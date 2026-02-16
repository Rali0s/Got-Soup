#include "core/service/alpha_service.hpp"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_map>
#include <utility>
#include <unordered_set>
#include <vector>

#include "core/model/app_meta.hpp"
#include "core/util/canonical.hpp"
#include "core/util/hash.hpp"

namespace alpha {
namespace {

bool looks_like_path(std::string_view value) {
  return value.find('/') != std::string_view::npos || value.find('\\') != std::string_view::npos ||
         value.ends_with(".dat");
}

bool is_absolute_path(std::string_view value) {
  if (value.empty()) {
    return false;
  }
  return std::filesystem::path{std::string{value}}.is_absolute();
}

std::string mode_to_string(AnonymityMode mode) {
  return mode == AnonymityMode::I2P ? "I2P" : "Tor";
}

std::string soup_address_from_cid(std::string_view cid) {
  if (cid.empty()) {
    return std::string{alpha::kAddressPrefix} + "000000000000000000000000000000000000000";
  }
  return std::string{alpha::kAddressPrefix} + util::sha256_like_hex(cid).substr(0, 39);
}

std::string recipe_segment_label(const RecipeSummary& recipe) {
  if (recipe.core_topic) {
    return "CORE";
  }
  return "COMMUNITY";
}

std::int64_t parse_int64_default(std::string_view text, std::int64_t fallback) {
  std::int64_t parsed = 0;
  const auto result = std::from_chars(text.data(), text.data() + text.size(), parsed);
  if (result.ec != std::errc()) {
    return fallback;
  }
  return parsed;
}

std::vector<std::string> split_csv(std::string_view csv) {
  std::vector<std::string> values;
  std::string current;
  for (char c : csv) {
    if (c == ',') {
      const std::string trimmed = util::trim_copy(current);
      if (!trimmed.empty()) {
        values.push_back(trimmed);
      }
      current.clear();
      continue;
    }
    current.push_back(c);
  }
  const std::string trimmed = util::trim_copy(current);
  if (!trimmed.empty()) {
    values.push_back(trimmed);
  }
  return values;
}

std::string join_csv(std::vector<std::string> values) {
  for (auto& value : values) {
    value = util::trim_copy(value);
  }
  values.erase(std::remove_if(values.begin(), values.end(), [](const std::string& value) {
                 return value.empty();
               }),
               values.end());
  std::ranges::sort(values);
  values.erase(std::unique(values.begin(), values.end()), values.end());
  std::ostringstream out;
  for (std::size_t i = 0; i < values.size(); ++i) {
    if (i > 0) {
      out << ',';
    }
    out << values[i];
  }
  return out.str();
}

std::string preview_merkle_root(std::vector<std::string> leaves) {
  if (leaves.empty()) {
    return util::sha256_like_hex("merkle-empty");
  }
  while (leaves.size() > 1U) {
    if ((leaves.size() % 2U) != 0U) {
      leaves.push_back(leaves.back());
    }
    std::vector<std::string> next;
    next.reserve(leaves.size() / 2U);
    for (std::size_t i = 0; i < leaves.size(); i += 2U) {
      next.push_back(util::sha256_like_hex(leaves[i] + "|" + leaves[i + 1U]));
    }
    leaves = std::move(next);
  }
  return leaves.front();
}

std::string join_parts(const std::vector<std::string>& values) {
  std::ostringstream out;
  for (std::size_t i = 0; i < values.size(); ++i) {
    if (i > 0) {
      out << ',';
    }
    out << values[i];
  }
  return out.str();
}

ModerationPolicy moderation_policy_from_profile(const CommunityProfile& profile) {
  ModerationPolicy policy;
  policy.moderation_enabled = profile.moderation_enabled;
  policy.require_finality_for_actions = profile.moderation_require_finality;
  policy.min_confirmations_for_enforcement = profile.moderation_min_confirmations;
  policy.max_flags_before_auto_hide = profile.moderation_auto_hide_flags;
  policy.role_model = "single-signer";
  policy.moderator_cids = profile.moderator_cids;
  return policy;
}

bool should_use_testnet(bool alpha_test_mode, AnonymityMode mode) {
  return alpha_test_mode || mode == AnonymityMode::I2P;
}

bool is_png_file(const std::filesystem::path& path) {
  std::string ext = path.extension().string();
  std::ranges::transform(ext, ext.begin(), [](unsigned char c) {
    return static_cast<char>(std::tolower(c));
  });
  return ext == ".png";
}

bool should_rebuild_local_store(std::string_view message) {
  return message.find("Chain ID mismatch") != std::string_view::npos ||
         message.find("Network ID mismatch") != std::string_view::npos ||
         message.find("Community mismatch") != std::string_view::npos ||
         message.find("Failed to parse") != std::string_view::npos ||
         message.find("Event ID mismatch") != std::string_view::npos;
}

bool has_duplicate_reward_claim_error(std::string_view message) {
  return message.find("Duplicate reward claim for block.") != std::string_view::npos;
}

Result quarantine_and_reset_store_dir(std::string_view app_data_dir, std::string_view store_dir,
                                      std::string_view reason) {
  std::error_code ec;
  const std::filesystem::path target{std::string{store_dir}};
  const std::filesystem::path app_root{std::string{app_data_dir}};
  const std::filesystem::path recovery_root = app_root / "recovery";
  std::filesystem::create_directories(recovery_root, ec);
  if (ec) {
    return Result::failure("Unable to create recovery directory: " + ec.message());
  }

  if (std::filesystem::exists(target, ec) && !ec) {
    const std::string folder_name =
        target.filename().empty() ? "store" : target.filename().string();
    const std::filesystem::path quarantine =
        recovery_root / (folder_name + "-quarantine-" + std::to_string(util::unix_timestamp_now()));
    std::filesystem::rename(target, quarantine, ec);
    if (ec) {
      ec.clear();
      std::filesystem::remove_all(target, ec);
      if (ec) {
        return Result::failure("Unable to reset corrupted store path: " + ec.message());
      }
    }
  }

  ec.clear();
  std::filesystem::create_directories(target, ec);
  if (ec) {
    return Result::failure("Unable to recreate store directory: " + ec.message());
  }

  return Result::success("Local store reset: " + std::string{reason});
}

std::optional<std::filesystem::path> find_named_asset_upwards(std::string_view filename, std::size_t max_levels) {
  std::error_code ec;
  std::filesystem::path dir = std::filesystem::current_path(ec);
  if (ec) {
    return std::nullopt;
  }

  for (std::size_t level = 0; level <= max_levels; ++level) {
    const std::filesystem::path candidate = dir / std::string{filename};
    if (std::filesystem::exists(candidate, ec) && std::filesystem::is_regular_file(candidate, ec)) {
      return candidate;
    }

    if (!dir.has_parent_path()) {
      break;
    }
    const std::filesystem::path parent = dir.parent_path();
    if (parent == dir) {
      break;
    }
    dir = parent;
  }

  return std::nullopt;
}

std::optional<std::filesystem::path> find_named_asset_in_subdir_upwards(std::string_view subdir,
                                                                         std::string_view filename,
                                                                         std::size_t max_levels) {
  std::error_code ec;
  std::filesystem::path dir = std::filesystem::current_path(ec);
  if (ec) {
    return std::nullopt;
  }

  for (std::size_t level = 0; level <= max_levels; ++level) {
    const std::filesystem::path candidate = dir / std::string{subdir} / std::string{filename};
    if (std::filesystem::exists(candidate, ec) && std::filesystem::is_regular_file(candidate, ec)) {
      return candidate;
    }

    if (!dir.has_parent_path()) {
      break;
    }
    const std::filesystem::path parent = dir.parent_path();
    if (parent == dir) {
      break;
    }
    dir = parent;
  }

  return std::nullopt;
}

std::optional<std::filesystem::path> find_about_asset_upwards(std::size_t max_levels) {
  if (const auto exact = find_named_asset_upwards("about.png", max_levels); exact.has_value()) {
    return exact;
  }
  if (const auto exact = find_named_asset_in_subdir_upwards("Art", "about.png", max_levels);
      exact.has_value()) {
    return exact;
  }

  std::error_code ec;
  std::filesystem::path dir = std::filesystem::current_path(ec);
  if (ec) {
    return std::nullopt;
  }

  for (std::size_t level = 0; level <= max_levels; ++level) {
    std::vector<std::filesystem::path> candidates;
    for (const auto& entry : std::filesystem::directory_iterator(dir, ec)) {
      if (ec) {
        break;
      }
      if (!entry.is_regular_file(ec) || ec) {
        continue;
      }
      const std::string filename = entry.path().filename().string();
      if (filename == "tomato_soup.png") {
        continue;
      }
      if (!is_png_file(entry.path())) {
        continue;
      }
      candidates.push_back(entry.path());
    }
    if (!candidates.empty()) {
      std::ranges::sort(candidates);
      return candidates.front();
    }

    if (!dir.has_parent_path()) {
      break;
    }
    const std::filesystem::path parent = dir.parent_path();
    if (parent == dir) {
      break;
    }
    dir = parent;
  }

  return std::nullopt;
}

std::optional<std::filesystem::path> find_leaf_asset_upwards(std::size_t max_levels) {
  if (const auto exact = find_named_asset_upwards("leaf_icon.png", max_levels); exact.has_value()) {
    return exact;
  }
  if (const auto exact = find_named_asset_in_subdir_upwards("Art", "leaf_icon.png", max_levels);
      exact.has_value()) {
    return exact;
  }
  if (const auto exact = find_named_asset_upwards("leaf.png", max_levels); exact.has_value()) {
    return exact;
  }
  if (const auto exact = find_named_asset_in_subdir_upwards("Art", "leaf.png", max_levels);
      exact.has_value()) {
    return exact;
  }

  std::error_code ec;
  std::filesystem::path dir = std::filesystem::current_path(ec);
  if (ec) {
    return std::nullopt;
  }

  for (std::size_t level = 0; level <= max_levels; ++level) {
    for (const auto& entry : std::filesystem::directory_iterator(dir, ec)) {
      if (ec) {
        break;
      }
      if (!entry.is_regular_file(ec) || ec || !is_png_file(entry.path())) {
        continue;
      }
      std::string filename = entry.path().filename().string();
      std::ranges::transform(filename, filename.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
      });
      if (filename.find("leaf") != std::string::npos) {
        return entry.path();
      }
    }
    if (!dir.has_parent_path()) {
      break;
    }
    const std::filesystem::path parent = dir.parent_path();
    if (parent == dir) {
      break;
    }
    dir = parent;
  }
  return std::nullopt;
}

void seed_default_assets(std::string_view app_data_dir) {
  std::error_code ec;
  const std::filesystem::path assets_dir = std::filesystem::path{std::string{app_data_dir}} / "assets";
  std::filesystem::create_directories(assets_dir, ec);
  if (ec) {
    return;
  }

  const std::filesystem::path splash_dest = assets_dir / "tomato_soup.png";
  if (!std::filesystem::exists(splash_dest, ec)) {
    std::optional<std::filesystem::path> splash_src = find_named_asset_upwards("tomato_soup.png", 4);
    if (!splash_src.has_value()) {
      splash_src = find_named_asset_in_subdir_upwards("Art", "tomato_soup.png", 4);
    }
    if (splash_src.has_value()) {
      std::filesystem::copy_file(*splash_src, splash_dest, std::filesystem::copy_options::skip_existing, ec);
    }
  }

  const std::filesystem::path about_dest = assets_dir / "about.png";
  if (!std::filesystem::exists(about_dest, ec)) {
    std::optional<std::filesystem::path> about_src = find_about_asset_upwards(4);
    if (!about_src.has_value()) {
      std::vector<std::filesystem::path> local_candidates;
      for (const auto& entry : std::filesystem::directory_iterator(assets_dir, ec)) {
        if (ec) {
          break;
        }
        if (!entry.is_regular_file(ec) || ec) {
          continue;
        }
        const std::string filename = entry.path().filename().string();
        if (filename == "about.png" || filename == "tomato_soup.png") {
          continue;
        }
        if (!is_png_file(entry.path())) {
          continue;
        }
        local_candidates.push_back(entry.path());
      }
      if (!local_candidates.empty()) {
        std::ranges::sort(local_candidates);
        about_src = local_candidates.front();
      }
    }
    if (about_src.has_value()) {
      std::filesystem::copy_file(*about_src, about_dest, std::filesystem::copy_options::skip_existing, ec);
    }
  }

  const std::filesystem::path leaf_dest = assets_dir / "leaf_icon.png";
  if (!std::filesystem::exists(leaf_dest, ec)) {
    if (const auto leaf_src = find_leaf_asset_upwards(4); leaf_src.has_value()) {
      std::filesystem::copy_file(*leaf_src, leaf_dest, std::filesystem::copy_options::skip_existing, ec);
    }
  }
}

}  // namespace

Result AlphaService::init(const InitConfig& config) {
  config_ = config;
  if (config_.p2p_mainnet_port == 0) {
    config_.p2p_mainnet_port = 4001;
  }
  if (config_.p2p_testnet_port == 0) {
    config_.p2p_testnet_port = 14001;
  }

  if (config_.app_data_dir.empty()) {
    return Result::failure("Init failed: app_data_dir is required.");
  }

  if (config_.passphrase.empty()) {
    return Result::failure("Init failed: passphrase is required.");
  }

  std::error_code ec;
  std::filesystem::create_directories(config_.app_data_dir, ec);
  if (ec) {
    return Result::failure("Init failed: unable to create app_data_dir: " + ec.message());
  }
  std::filesystem::create_directories(std::filesystem::path{config_.app_data_dir} / "assets", ec);
  if (ec) {
    return Result::failure("Init failed: unable to create assets dir: " + ec.message());
  }
  seed_default_assets(config_.app_data_dir);

  const Result crypto_init =
      crypto_.initialize(config_.app_data_dir, config_.passphrase, config_.production_swap);
  if (!crypto_init.ok) {
    return crypto_init;
  }

  communities_dir_ = (std::filesystem::path{config_.app_data_dir} / "communities").string();
  std::filesystem::create_directories(communities_dir_, ec);
  if (ec) {
    return Result::failure("Init failed: unable to create communities dir: " + ec.message());
  }

  profile_state_path_ = (std::filesystem::path{config_.app_data_dir} / "profile-state.dat").string();
  const Result profile_state_result = load_profile_state();
  if (!profile_state_result.ok) {
    return profile_state_result;
  }

  alpha_test_mode_ = config_.alpha_test_mode;
  active_mode_ = config_.mode;
  if (config_.seed_peers_mainnet.empty()) {
    config_.seed_peers_mainnet = config_.seed_peers;
  }
  if (config_.seed_peers_testnet.empty()) {
    config_.seed_peers_testnet = config_.seed_peers_mainnet;
  }
  if (config_.seed_peers_mainnet.empty()) {
    config_.seed_peers_mainnet = {"seed.got-soup.local:4001", "24.188.147.247:4001"};
  }
  if (config_.seed_peers_testnet.empty()) {
    config_.seed_peers_testnet = {"seed.got-soup.local:14001"};
  }

  if (config_.genesis_psz_timestamp.empty()) {
    config_.genesis_psz_timestamp =
        should_use_testnet(alpha_test_mode_, active_mode_) ? config_.testnet_genesis_psz_timestamp
                                                            : config_.mainnet_genesis_psz_timestamp;
  }
  tor_enabled_ = true;
  i2p_enabled_ = true;
  validation_interval_ticks_ = config_.validation_interval_ticks == 0 ? 10 : config_.validation_interval_ticks;
  ticks_since_last_validation_ = 0;
  wallet_destroyed_ = false;
  wallet_recovery_required_ = false;
  wallet_last_unlocked_unix_ = crypto_.last_unlocked_unix();
  wallet_last_locked_unix_ = crypto_.last_locked_unix();

  store_.set_block_timing(config_.block_interval_seconds == 0 ? 150 : config_.block_interval_seconds);
  store_.set_block_reward_units(config_.block_reward_units <= 0 ? 115 : config_.block_reward_units);
  store_.set_chain_policy(config_.chain_policy);
  store_.set_validation_limits(config_.validation_limits);
  store_.set_moderation_policy(config_.default_moderation_policy);
  store_.set_state_options(config_.blockdata_format_version, config_.enable_snapshots,
                           config_.snapshot_interval_blocks, config_.enable_pruning,
                           config_.prune_keep_recent_blocks);
  if (!config_.genesis_psz_timestamp.empty()) {
    store_.set_genesis_psz_timestamp(config_.genesis_psz_timestamp);
  }

  tor_provider_ = make_anonymity_provider(AnonymityMode::Tor);
  i2p_provider_ = make_anonymity_provider(AnonymityMode::I2P);

  tor_provider_->set_alpha_test_mode(alpha_test_mode_);
  i2p_provider_->set_alpha_test_mode(alpha_test_mode_);

  Result provider_result = ensure_provider_state(AnonymityMode::Tor, tor_enabled_);
  if (!provider_result.ok) {
    return provider_result;
  }

  provider_result = ensure_provider_state(AnonymityMode::I2P, i2p_enabled_);
  if (!provider_result.ok) {
    return provider_result;
  }

  const std::string community_selector =
      config_.community_profile_path.empty() ? "recipes" : config_.community_profile_path;
  const Result community_result = use_community_profile(community_selector, "", "");
  if (!community_result.ok) {
    return community_result;
  }

  const Result initial_backtest = run_backtest_validation();
  if (!initial_backtest.ok) {
    return initial_backtest;
  }

  initialized_ = true;
  return Result::success("SoupNet service initialized with node status controls, peers.dat and community profiles.");
}

Result AlphaService::create_recipe(const RecipeDraft& draft) {
  if (const Result unlocked = ensure_wallet_unlocked("create_recipe"); !unlocked.ok) {
    return unlocked;
  }
  if (draft.title.empty()) {
    return Result::failure("Recipe title is required.");
  }

  if (draft.markdown.empty()) {
    return Result::failure("Recipe markdown content is required.");
  }

  std::int64_t post_value = 0;
  if (draft.value_units < 0) {
    return Result::failure("Recipe post value cannot be negative.");
  }

  if (draft.core_topic) {
    post_value = draft.value_units;
    if (post_value > 0 && store_.reward_balance(crypto_.identity().cid.value) < post_value) {
      return Result::failure("Insufficient reward balance to publish this core topic value.");
    }
  } else {
    const Result spend_check = validate_and_apply_post_cost(draft.value_units, post_value);
    if (!spend_check.ok) {
      return spend_check;
    }
  }

  const std::string recipe_id =
      "rcp-" +
      crypto_.hash_bytes(current_community_.community_id + draft.title + draft.markdown +
                         std::to_string(util::unix_timestamp_now()))
          .substr(0, 16);

  EventEnvelope event = make_event(
      EventKind::RecipeCreated,
      {{"recipe_id", recipe_id},
       {"category", draft.category.empty() ? "General" : draft.category},
       {"title", draft.title},
       {"markdown", draft.markdown},
       {"post_value", std::to_string(post_value)},
       {"core_topic", draft.core_topic ? "1" : "0"},
       {"menu_segment", draft.menu_segment.empty()
                            ? (draft.core_topic ? "core-menu" : "community-post")
                            : draft.menu_segment}});

  return append_locally_and_queue(event);
}

Result AlphaService::create_thread(const ThreadDraft& draft) {
  if (const Result unlocked = ensure_wallet_unlocked("create_thread"); !unlocked.ok) {
    return unlocked;
  }
  if (draft.recipe_id.empty()) {
    return Result::failure("Thread creation requires recipe_id.");
  }

  if (draft.title.empty()) {
    return Result::failure("Thread title is required.");
  }

  std::int64_t post_value = 0;
  const Result spend_check = validate_and_apply_post_cost(draft.value_units, post_value);
  if (!spend_check.ok) {
    return spend_check;
  }

  const std::string thread_id =
      "thr-" +
      crypto_.hash_bytes(current_community_.community_id + draft.recipe_id + draft.title +
                         std::to_string(util::unix_timestamp_now()))
          .substr(0, 16);

  EventEnvelope event = make_event(
      EventKind::ThreadCreated,
      {{"thread_id", thread_id},
       {"recipe_id", draft.recipe_id},
       {"title", draft.title},
       {"markdown", draft.markdown},
       {"post_value", std::to_string(post_value)}});

  return append_locally_and_queue(event);
}

Result AlphaService::create_reply(const ReplyDraft& draft) {
  if (const Result unlocked = ensure_wallet_unlocked("create_reply"); !unlocked.ok) {
    return unlocked;
  }
  if (draft.thread_id.empty()) {
    return Result::failure("Reply creation requires thread_id.");
  }

  if (draft.markdown.empty()) {
    return Result::failure("Reply markdown content is required.");
  }

  std::int64_t post_value = 0;
  const Result spend_check = validate_and_apply_post_cost(draft.value_units, post_value);
  if (!spend_check.ok) {
    return spend_check;
  }

  const std::string reply_id =
      "rpl-" +
      crypto_.hash_bytes(current_community_.community_id + draft.thread_id + draft.markdown +
                         std::to_string(util::unix_timestamp_now()))
          .substr(0, 16);

  EventEnvelope event =
      make_event(EventKind::ReplyCreated,
                 {{"reply_id", reply_id},
                  {"thread_id", draft.thread_id},
                  {"markdown", draft.markdown},
                  {"post_value", std::to_string(post_value)}});

  return append_locally_and_queue(event);
}

Result AlphaService::add_review(const ReviewDraft& draft) {
  if (const Result unlocked = ensure_wallet_unlocked("add_review"); !unlocked.ok) {
    return unlocked;
  }
  if (draft.recipe_id.empty()) {
    return Result::failure("Review requires recipe_id.");
  }

  if (draft.rating < 1 || draft.rating > 5) {
    return Result::failure("Review rating must be between 1 and 5.");
  }

  std::int64_t post_value = 0;
  const Result spend_check = validate_and_apply_post_cost(draft.value_units, post_value);
  if (!spend_check.ok) {
    return spend_check;
  }

  const std::string review_id =
      "rev-" + crypto_.hash_bytes(current_community_.community_id + draft.recipe_id +
                                   std::to_string(draft.rating) + draft.markdown)
                   .substr(0, 16);

  EventEnvelope event = make_event(
      EventKind::ReviewAdded,
      {{"review_id", review_id},
       {"recipe_id", draft.recipe_id},
       {"rating", std::to_string(draft.rating)},
       {"markdown", draft.markdown},
       {"post_value", std::to_string(post_value)}});

  return append_locally_and_queue(event);
}

Result AlphaService::add_thumb_up(std::string_view recipe_id) {
  if (const Result unlocked = ensure_wallet_unlocked("add_thumb_up"); !unlocked.ok) {
    return unlocked;
  }
  const std::string target{recipe_id};
  if (target.empty()) {
    return Result::failure("Thumbs up requires recipe_id.");
  }

  const auto recipes = store_.query_recipes({.text = {}, .category = {}});
  const auto found = std::ranges::find_if(recipes, [&target](const RecipeSummary& summary) {
    return summary.recipe_id == target;
  });
  if (found == recipes.end()) {
    return Result::failure("Thumbs up target recipe was not found.");
  }

  const std::string thumb_id =
      "thm-" +
      crypto_.hash_bytes(current_community_.community_id + target +
                         std::to_string(util::unix_timestamp_now()))
          .substr(0, 16);

  EventEnvelope event = make_event(
      EventKind::ThumbsUpAdded,
      {{"thumb_id", thumb_id},
       {"recipe_id", target}});

  return append_locally_and_queue(event);
}

Result AlphaService::transfer_rewards(const RewardTransferDraft& draft) {
  if (const Result unlocked = ensure_wallet_unlocked("transfer_rewards"); !unlocked.ok) {
    return unlocked;
  }
  const std::string target_name = sanitize_display_name(draft.to_display_name);
  if (target_name.empty()) {
    return Result::failure("Reward transfer requires a target display name.");
  }
  if (draft.amount <= 0) {
    return Result::failure("Reward transfer amount must be positive.");
  }

  const std::int64_t fee = store_.transfer_burn_fee(draft.amount);
  const std::uint64_t nonce = store_.next_transfer_nonce(crypto_.identity().cid.value);
  const std::int64_t local_balance = store_.reward_balance(crypto_.identity().cid.value);
  if (local_balance < (draft.amount + fee)) {
    return Result::failure("Insufficient reward balance for transfer.");
  }

  const auto target_cid = resolve_display_name_to_cid(target_name);
  if (!target_cid.has_value()) {
    return Result::failure("Target display name is unknown or ambiguous.");
  }

  const std::string transfer_id =
      "xfr-" + crypto_.hash_bytes(current_community_.community_id + crypto_.identity().cid.value +
                                  *target_cid + std::to_string(draft.amount) +
                                  std::to_string(util::unix_timestamp_now()))
                   .substr(0, 16);
  const std::string witness_root =
      util::sha256_like_hex(crypto_.identity().cid.value + "|" + *target_cid + "|" +
                            std::to_string(draft.amount) + "|" + std::to_string(fee) + "|" +
                            std::to_string(nonce));

  EventEnvelope event = make_event(
      EventKind::RewardTransferred,
      {{"transfer_id", transfer_id},
       {"to_cid", *target_cid},
       {"to_display_name", target_name},
       {"amount", std::to_string(draft.amount)},
       {"fee", std::to_string(fee)},
       {"nonce", std::to_string(nonce)},
       {"witness_root", witness_root},
       {"memo", draft.memo}});

  return append_locally_and_queue(event);
}

Result AlphaService::transfer_rewards_to_address(const RewardTransferAddressDraft& draft) {
  if (const Result unlocked = ensure_wallet_unlocked("transfer_rewards_to_address"); !unlocked.ok) {
    return unlocked;
  }
  const std::string target_address = util::trim_copy(draft.to_address);
  if (target_address.empty()) {
    return Result::failure("Reward transfer requires a target address.");
  }
  if (!target_address.starts_with(alpha::kAddressPrefix)) {
    return Result::failure("Invalid address prefix for target address.");
  }
  if (draft.amount <= 0) {
    return Result::failure("Reward transfer amount must be positive.");
  }

  const std::int64_t fee = store_.transfer_burn_fee(draft.amount);
  const std::uint64_t nonce = store_.next_transfer_nonce(crypto_.identity().cid.value);
  const std::int64_t local_balance = store_.reward_balance(crypto_.identity().cid.value);
  if (local_balance < (draft.amount + fee)) {
    return Result::failure("Insufficient reward balance for transfer.");
  }

  const auto target_cid = resolve_address_to_cid(target_address);
  if (!target_cid.has_value()) {
    return Result::failure("Target address is unknown in current community.");
  }

  const std::string transfer_id =
      "xfr-" + crypto_.hash_bytes(current_community_.community_id + crypto_.identity().cid.value +
                                  *target_cid + std::to_string(draft.amount) +
                                  std::to_string(util::unix_timestamp_now()))
                   .substr(0, 16);
  const std::string witness_root =
      util::sha256_like_hex(crypto_.identity().cid.value + "|" + *target_cid + "|" +
                            std::to_string(draft.amount) + "|" + std::to_string(fee) + "|" +
                            std::to_string(nonce));

  EventEnvelope event = make_event(
      EventKind::RewardTransferred,
      {{"transfer_id", transfer_id},
       {"to_cid", *target_cid},
       {"to_address", target_address},
       {"amount", std::to_string(draft.amount)},
       {"fee", std::to_string(fee)},
       {"nonce", std::to_string(nonce)},
       {"witness_root", witness_root},
       {"memo", draft.memo}});

  return append_locally_and_queue(event);
}

std::vector<RecipeSummary> AlphaService::search(const SearchQuery& query) const {
  return store_.query_recipes(query);
}

std::vector<ThreadSummary> AlphaService::threads(std::string_view recipe_id) const {
  return store_.query_threads(recipe_id);
}

std::vector<ReplySummary> AlphaService::replies(std::string_view thread_id) const {
  return store_.query_replies(thread_id);
}

std::vector<RewardTransactionSummary> AlphaService::reward_transactions() const {
  std::vector<RewardTransactionSummary> out;
  out.reserve(store_.all_events().size());

  for (const auto& event : store_.all_events()) {
    if (event.kind != EventKind::RewardTransferred) {
      continue;
    }
    const auto payload = util::parse_canonical_map(event.payload);
    if (!payload.contains("to_cid") || !payload.contains("amount")) {
      continue;
    }

    RewardTransactionSummary tx;
    tx.transfer_id = payload.contains("transfer_id") ? payload.at("transfer_id") : "";
    tx.event_id = event.event_id;
    tx.from_cid = event.author_cid;
    tx.to_cid = payload.at("to_cid");
    tx.from_address = soup_address_from_cid(tx.from_cid);
    tx.to_address = payload.contains("to_address") ? payload.at("to_address") : soup_address_from_cid(tx.to_cid);
    tx.amount = parse_int64_default(payload.at("amount"), 0);
    tx.fee = parse_int64_default(payload.contains("fee") ? payload.at("fee") : "0", 0);
    tx.memo = payload.contains("memo") ? payload.at("memo") : "";
    tx.unix_ts = event.unix_ts;
    if (const auto metrics = store_.confirmation_for_object(event.event_id); metrics.has_value()) {
      const auto parsed = util::parse_canonical_map(*metrics);
      const auto cc_it = parsed.find("confirmation_count");
      const auto age_it = parsed.find("confirmation_age_seconds");
      if (cc_it != parsed.end()) {
        tx.confirmation_count =
            static_cast<std::uint64_t>(std::max<std::int64_t>(0, parse_int64_default(cc_it->second, 0)));
      }
      if (age_it != parsed.end()) {
        tx.confirmation_age_seconds = parse_int64_default(age_it->second, 0);
      }
    }
    out.push_back(std::move(tx));
  }

  std::ranges::sort(out, [](const RewardTransactionSummary& a, const RewardTransactionSummary& b) {
    if (a.unix_ts == b.unix_ts) {
      return a.event_id > b.event_id;
    }
    return a.unix_ts > b.unix_ts;
  });
  return out;
}

std::vector<EventEnvelope> AlphaService::sync_tick() {
  const Result block_check = store_.routine_block_check(util::unix_timestamp_now());
  if (!block_check.ok) {
    return {};
  }

  const Result claim_result = try_claim_confirmed_block_rewards();
  if (!claim_result.ok) {
    return {};
  }

  ++ticks_since_last_validation_;
  if (ticks_since_last_validation_ >= validation_interval_ticks_) {
    const Result validation = run_backtest_validation();
    if (!validation.ok) {
      // Keep network tick alive even if validation reports issues.
    }
    ticks_since_last_validation_ = 0;
  }

  return p2p_node_.sync_tick();
}

Result AlphaService::ingest_remote_event(const EventEnvelope& event) {
  if (!p2p_node_.ingest_remote_event(event)) {
    return Result::success("Duplicate or ignored remote event.");
  }

  if (event.signature.empty()) {
    return Result::failure("Remote event signature is missing.");
  }

  return store_.append_event(event);
}

Result AlphaService::set_transport_enabled(AnonymityMode mode, bool enabled) {
  if (mode == AnonymityMode::Tor) {
    tor_enabled_ = enabled;
  } else {
    i2p_enabled_ = enabled;
  }

  const Result provider_result = ensure_provider_state(mode, enabled);
  if (!provider_result.ok) {
    return provider_result;
  }

  if (!tor_enabled_ && !i2p_enabled_) {
    p2p_node_.stop();
    return Result::success("All anonymity transports disabled; P2P node stopped.");
  }

  if (active_mode_ == AnonymityMode::Tor && !tor_enabled_ && i2p_enabled_) {
    active_mode_ = AnonymityMode::I2P;
  } else if (active_mode_ == AnonymityMode::I2P && !i2p_enabled_ && tor_enabled_) {
    active_mode_ = AnonymityMode::Tor;
  }

  if (!current_community_.profile_path.empty()) {
    return use_community_profile(current_community_.profile_path, current_community_.display_name,
                                 current_community_.description);
  }
  return restart_network();
}

Result AlphaService::set_active_transport(AnonymityMode mode) {
  if (mode == AnonymityMode::Tor && !tor_enabled_) {
    return Result::failure("Cannot activate Tor: Tor toggle is OFF.");
  }

  if (mode == AnonymityMode::I2P && !i2p_enabled_) {
    return Result::failure("Cannot activate I2P: I2P toggle is OFF.");
  }

  active_mode_ = mode;
  if (!current_community_.profile_path.empty()) {
    return use_community_profile(current_community_.profile_path, current_community_.display_name,
                                 current_community_.description);
  }
  return restart_network();
}

Result AlphaService::set_alpha_test_mode(bool enabled) {
  alpha_test_mode_ = enabled;

  tor_provider_->set_alpha_test_mode(enabled);
  i2p_provider_->set_alpha_test_mode(enabled);

  if (tor_enabled_) {
    const Result tor_restart = ensure_provider_state(AnonymityMode::Tor, true);
    if (!tor_restart.ok) {
      return tor_restart;
    }
  }

  if (i2p_enabled_) {
    const Result i2p_restart = ensure_provider_state(AnonymityMode::I2P, true);
    if (!i2p_restart.ok) {
      return i2p_restart;
    }
  }

  if (!current_community_.profile_path.empty()) {
    return use_community_profile(current_community_.profile_path, current_community_.display_name,
                                 current_community_.description);
  }
  return restart_network();
}

Result AlphaService::add_peer(std::string_view peer) {
  const Result add_result = p2p_node_.add_peer(peer);
  if (!add_result.ok) {
    return add_result;
  }

  return p2p_node_.save_peers_dat(peers_dat_path_);
}

Result AlphaService::reload_peers_dat() {
  const Result load_result = p2p_node_.load_peers_dat(peers_dat_path_);
  if (!load_result.ok) {
    return load_result;
  }

  const Result save_result = p2p_node_.save_peers_dat(peers_dat_path_);
  if (!save_result.ok) {
    return save_result;
  }

  return restart_network();
}

Result AlphaService::set_profile_display_name(std::string_view display_name) {
  if (const Result unlocked = ensure_wallet_unlocked("set_profile_display_name"); !unlocked.ok) {
    return unlocked;
  }
  const std::string sanitized = sanitize_display_name(display_name);
  if (sanitized.empty()) {
    return Result::failure("Display name is required and must contain letters or numbers.");
  }

  if (local_display_name_immortalized_ && !local_display_name_.empty() &&
      sanitized != local_display_name_) {
    return Result::failure("Display name is immortalized and cannot be changed for this CID.");
  }

  if (reject_duplicate_names_) {
    const auto observed = observed_display_names_by_cid();
    const std::string own_cid = crypto_.identity().cid.value;
    const std::string requested = util::lowercase_copy(sanitized);
    for (const auto& [cid, name] : observed) {
      if (cid == own_cid) {
        continue;
      }
      if (util::lowercase_copy(name) == requested) {
        return Result::failure("Duplicate name rejected: already used by CID " + cid);
      }
    }
  }

  local_display_name_ = sanitized;
  local_display_name_immortalized_ = true;
  const Result persist = save_profile_state();
  if (!persist.ok) {
    return persist;
  }

  EventEnvelope event = make_event(
      EventKind::ProfileUpdated,
      {{"display_name", local_display_name_},
       {"display_name_immortalized", "1"},
       {"duplicate_policy", reject_duplicate_names_ ? "reject" : "allow"}});

  return append_locally_and_queue(event);
}

Result AlphaService::set_immortal_name_with_cipher(std::string_view display_name,
                                                   std::string_view cipher_password,
                                                   std::string_view cipher_salt) {
  if (const Result unlocked = ensure_wallet_unlocked("set_immortal_name_with_cipher"); !unlocked.ok) {
    return unlocked;
  }
  const std::string pass = util::trim_copy(cipher_password);
  if (pass.empty()) {
    return Result::failure("Immortal name requires cipher password.");
  }

  const Result cipher_result = set_profile_cipher_password(pass, cipher_salt);
  if (!cipher_result.ok) {
    return cipher_result;
  }

  const Result name_result = set_profile_display_name(display_name);
  if (!name_result.ok) {
    return name_result;
  }

  const Result sync_result = update_key_to_peers();
  if (!sync_result.ok) {
    return sync_result;
  }

  return Result::success("Immortal name processed with required cipher and peer update.");
}

Result AlphaService::set_duplicate_name_policy(bool reject_duplicates) {
  if (const Result unlocked = ensure_wallet_unlocked("set_duplicate_name_policy"); !unlocked.ok) {
    return unlocked;
  }
  reject_duplicate_names_ = reject_duplicates;
  const Result persist = save_profile_state();
  if (!persist.ok) {
    return persist;
  }

  std::vector<std::pair<std::string, std::string>> fields{
      {"duplicate_policy", reject_duplicate_names_ ? "reject" : "allow"},
      {"display_name_immortalized", local_display_name_immortalized_ ? "1" : "0"},
  };
  if (!local_display_name_.empty()) {
    fields.emplace_back("display_name", local_display_name_);
  }

  EventEnvelope event = make_event(EventKind::ProfileUpdated, std::move(fields));
  return append_locally_and_queue(event);
}

Result AlphaService::set_profile_cipher_password(std::string_view password, std::string_view salt) {
  if (const Result unlocked = ensure_wallet_unlocked("set_profile_cipher_password"); !unlocked.ok) {
    return unlocked;
  }
  const std::string pass = util::trim_copy(password);
  if (pass.empty()) {
    return Result::failure("Cipher key update failed: password is required.");
  }

  std::string applied_salt = util::trim_copy(salt);
  if (applied_salt.empty()) {
    applied_salt = current_community_.community_id + ":" + crypto_.identity().cid.value;
  }

  current_community_.cipher_key = crypto_.derive_vault_key(pass, "community-cipher:" + applied_salt);
  const Result write = write_community_profile_file(current_community_);
  if (!write.ok) {
    return write;
  }

  return Result::success("Community cipher key updated.", applied_salt);
}

Result AlphaService::update_key_to_peers() {
  if (const Result unlocked = ensure_wallet_unlocked("update_key_to_peers"); !unlocked.ok) {
    return unlocked;
  }
  EventEnvelope event = make_event(EventKind::KeyRotated,
                                   {{"action", "announce"},
                                    {"cid", crypto_.identity().cid.value},
                                    {"public_key", crypto_.identity().public_key}});
  return append_locally_and_queue(event);
}

Result AlphaService::add_moderator(std::string_view cid) {
  if (const Result unlocked = ensure_wallet_unlocked("add_moderator"); !unlocked.ok) {
    return unlocked;
  }
  if (const Result auth = ensure_local_moderator("add_moderator"); !auth.ok) {
    return auth;
  }

  const std::string target_cid = sanitize_cid(cid);
  if (target_cid.empty()) {
    return Result::failure("Add moderator requires a non-empty CID.");
  }

  EventEnvelope event = make_event(EventKind::ModeratorAdded,
                                   {{"target_cid", target_cid},
                                    {"action", "moderator-add"}});
  return append_locally_and_queue(event);
}

Result AlphaService::remove_moderator(std::string_view cid) {
  if (const Result unlocked = ensure_wallet_unlocked("remove_moderator"); !unlocked.ok) {
    return unlocked;
  }
  if (const Result auth = ensure_local_moderator("remove_moderator"); !auth.ok) {
    return auth;
  }

  const std::string target_cid = sanitize_cid(cid);
  if (target_cid.empty()) {
    return Result::failure("Remove moderator requires a non-empty CID.");
  }

  EventEnvelope event = make_event(EventKind::ModeratorRemoved,
                                   {{"target_cid", target_cid},
                                    {"action", "moderator-remove"}});
  return append_locally_and_queue(event);
}

Result AlphaService::flag_content(std::string_view object_id, std::string_view reason) {
  if (const Result unlocked = ensure_wallet_unlocked("flag_content"); !unlocked.ok) {
    return unlocked;
  }
  const std::string target_id = util::trim_copy(object_id);
  if (target_id.empty()) {
    return Result::failure("Flag content requires an object_id.");
  }
  const std::string reason_text = util::trim_copy(reason);

  EventEnvelope event = make_event(EventKind::ContentFlagged,
                                   {{"object_id", target_id},
                                    {"reason", reason_text.empty() ? "flagged" : reason_text}});
  return append_locally_and_queue(event);
}

Result AlphaService::set_content_hidden(std::string_view object_id, bool hidden, std::string_view reason) {
  if (const Result unlocked = ensure_wallet_unlocked("set_content_hidden"); !unlocked.ok) {
    return unlocked;
  }
  if (const Result auth = ensure_local_moderator("set_content_hidden"); !auth.ok) {
    return auth;
  }
  const std::string target_id = util::trim_copy(object_id);
  if (target_id.empty()) {
    return Result::failure("Set content hidden requires an object_id.");
  }
  const std::string reason_text = util::trim_copy(reason);

  const EventKind kind = hidden ? EventKind::ContentHidden : EventKind::ContentUnhidden;
  EventEnvelope event =
      make_event(kind, {{"object_id", target_id},
                        {"hidden", hidden ? "1" : "0"},
                        {"reason", reason_text.empty() ? (hidden ? "hidden" : "unhidden") : reason_text}});
  return append_locally_and_queue(event);
}

Result AlphaService::pin_core_topic(std::string_view recipe_id, bool pinned) {
  if (const Result unlocked = ensure_wallet_unlocked("pin_core_topic"); !unlocked.ok) {
    return unlocked;
  }
  if (const Result auth = ensure_local_moderator("pin_core_topic"); !auth.ok) {
    return auth;
  }
  const std::string target_recipe = util::trim_copy(recipe_id);
  if (target_recipe.empty()) {
    return Result::failure("Pin core topic requires a recipe_id.");
  }

  const EventKind kind = pinned ? EventKind::CoreTopicPinned : EventKind::CoreTopicUnpinned;
  EventEnvelope event =
      make_event(kind, {{"recipe_id", target_recipe}, {"pinned", pinned ? "1" : "0"}});
  return append_locally_and_queue(event);
}

Result AlphaService::export_key_backup(std::string_view backup_path, std::string_view password,
                                       std::string_view salt) {
  if (const Result unlocked = ensure_wallet_unlocked("export_key_backup"); !unlocked.ok) {
    return unlocked;
  }
  const std::string resolved = resolve_data_path(backup_path, "backup/identity-backup.dat");
  const Result result = crypto_.export_identity_backup(resolved, password, salt);
  if (result.ok) {
    last_key_backup_path_ = resolved;
    (void)save_profile_state();
  }
  return result;
}

Result AlphaService::import_key_backup(std::string_view backup_path, std::string_view password) {
  const std::string resolved = resolve_data_path(backup_path, "backup/identity-backup.dat");
  const std::string previous_cid = crypto_.identity().cid.value;
  const Result imported = crypto_.import_identity_backup(resolved, password, config_.passphrase);
  if (!imported.ok) {
    return imported;
  }

  local_display_name_.clear();
  local_display_name_immortalized_ = false;
  wallet_destroyed_ = false;
  wallet_recovery_required_ = false;
  wallet_last_unlocked_unix_ = crypto_.last_unlocked_unix();
  wallet_last_locked_unix_ = crypto_.last_locked_unix();
  last_key_backup_path_ = resolved;
  const Result save_state = save_profile_state();
  if (!save_state.ok) {
    return save_state;
  }

  const Result restart = restart_network();
  if (!restart.ok) {
    return restart;
  }

  EventEnvelope event = make_event(EventKind::KeyRotated,
                                   {{"action", "import"},
                                    {"previous_cid", previous_cid},
                                    {"current_cid", crypto_.identity().cid.value}});
  return append_locally_and_queue(event);
}

Result AlphaService::lock_wallet() {
  const Result lock = crypto_.lock_identity();
  if (!lock.ok) {
    return lock;
  }
  wallet_last_locked_unix_ = crypto_.last_locked_unix();
  wallet_recovery_required_ = false;
  (void)save_profile_state();
  return Result::success("Wallet locked.");
}

Result AlphaService::unlock_wallet(std::string_view passphrase) {
  const Result unlock = crypto_.unlock_identity(passphrase);
  if (!unlock.ok) {
    wallet_recovery_required_ = true;
    return unlock;
  }
  wallet_last_unlocked_unix_ = crypto_.last_unlocked_unix();
  wallet_recovery_required_ = false;
  wallet_destroyed_ = false;
  (void)save_profile_state();
  return restart_network();
}

Result AlphaService::recover_wallet(std::string_view backup_path, std::string_view backup_password,
                                    std::string_view new_local_passphrase) {
  const std::string local_pass = util::trim_copy(new_local_passphrase);
  if (local_pass.empty()) {
    return Result::failure("Wallet recovery failed: new local passphrase is required.");
  }

  config_.passphrase = local_pass;
  const std::string resolved = resolve_data_path(backup_path, "backup/identity-backup.dat");
  const Result imported = crypto_.import_identity_backup(resolved, backup_password, config_.passphrase);
  if (!imported.ok) {
    wallet_recovery_required_ = true;
    return imported;
  }

  wallet_recovery_required_ = false;
  wallet_destroyed_ = false;
  wallet_last_unlocked_unix_ = crypto_.last_unlocked_unix();
  last_key_backup_path_ = resolved;
  (void)save_profile_state();
  return restart_network();
}

Result AlphaService::nuke_key(std::string_view confirmation_phrase) {
  const std::string confirm = util::trim_copy(confirmation_phrase);
  if (confirm != "NUKE-KEY" && confirm != "NUKE") {
    return Result::failure("Nuke key requires confirmation text: NUKE-KEY");
  }

  const std::string previous_cid = crypto_.identity().cid.value;
  const Result nuked = crypto_.nuke_identity(config_.passphrase, config_.production_swap);
  if (!nuked.ok) {
    return nuked;
  }

  local_display_name_.clear();
  local_display_name_immortalized_ = false;
  wallet_destroyed_ = true;
  wallet_recovery_required_ = true;
  wallet_last_unlocked_unix_ = crypto_.last_unlocked_unix();
  wallet_last_locked_unix_ = crypto_.last_locked_unix();
  const Result save_state = save_profile_state();
  if (!save_state.ok) {
    return save_state;
  }

  const Result restart = restart_network();
  if (!restart.ok) {
    return restart;
  }

  EventEnvelope event = make_event(EventKind::KeyRotated,
                                   {{"action", "nuke"},
                                    {"previous_cid", previous_cid},
                                    {"current_cid", crypto_.identity().cid.value}});
  return append_locally_and_queue(event);
}

Result AlphaService::run_backtest_validation() {
  auto content_id_fn = [this](std::string_view payload) {
    return crypto_.content_id(payload);
  };

  return store_.backtest_validate(content_id_fn, current_community_.community_id);
}

Result AlphaService::use_community_profile(std::string_view community_or_path,
                                           std::string_view display_name,
                                           std::string_view description) {
  const Result community_result =
      load_or_create_community_profile(community_or_path, display_name, description);
  if (!community_result.ok) {
    return community_result;
  }

  const bool testnet = should_use_testnet(alpha_test_mode_, active_mode_);
  const std::string network_suffix = testnet ? "testnet" : "mainnet";
  const std::string chain_id = testnet ? config_.testnet_chain_id : config_.mainnet_chain_id;
  const std::string network_id = testnet ? "testnet" : "mainnet";
  const std::string genesis_merkle =
      testnet ? config_.testnet_genesis_merkle_root : config_.mainnet_genesis_merkle_root;
  const std::string genesis_block_hash =
      testnet ? config_.testnet_genesis_block_hash : config_.mainnet_genesis_block_hash;
  const std::string genesis_psz =
      testnet ? config_.testnet_genesis_psz_timestamp : config_.mainnet_genesis_psz_timestamp;

  store_.set_chain_identity(chain_id, network_id);
  store_.set_genesis_hashes(genesis_merkle, genesis_block_hash);
  store_.set_chain_policy(config_.chain_policy);
  store_.set_validation_limits(config_.validation_limits);
  store_.set_moderation_policy(moderation_policy_from_profile(current_community_));
  store_.set_state_options(config_.blockdata_format_version, config_.enable_snapshots,
                           config_.snapshot_interval_blocks, config_.enable_pruning,
                           config_.prune_keep_recent_blocks);

  store_.set_block_reward_units(current_community_.block_reward_units <= 0
                                    ? (config_.block_reward_units <= 0 ? 115 : config_.block_reward_units)
                                    : current_community_.block_reward_units);
  if (!genesis_psz.empty()) {
    // Mainnet/Testnet release genesis is hardcoded and authoritative for this node.
    store_.set_genesis_psz_timestamp(genesis_psz);
    if (current_community_.genesis_psz_timestamp != genesis_psz) {
      current_community_.genesis_psz_timestamp = genesis_psz;
      const Result persist_profile = write_community_profile_file(current_community_);
      if (!persist_profile.ok) {
        return persist_profile;
      }
    }
  } else if (!config_.genesis_psz_timestamp.empty()) {
    store_.set_genesis_psz_timestamp(config_.genesis_psz_timestamp);
    if (current_community_.genesis_psz_timestamp != config_.genesis_psz_timestamp) {
      current_community_.genesis_psz_timestamp = config_.genesis_psz_timestamp;
      const Result persist_profile = write_community_profile_file(current_community_);
      if (!persist_profile.ok) {
        return persist_profile;
      }
    }
  }

  const std::string effective_store_path =
      resolve_data_path(current_community_.store_path + "-" + network_suffix, "db-" + current_community_.community_id);
  const std::string store_key =
      crypto_.derive_vault_key(config_.passphrase, "store:" + current_community_.community_id + ":" + network_suffix);
  Result store_result = store_.open(effective_store_path, store_key);
  if (!store_result.ok && should_rebuild_local_store(store_result.message)) {
    const Result reset =
        quarantine_and_reset_store_dir(config_.app_data_dir, effective_store_path, store_result.message);
    if (!reset.ok) {
      return reset;
    }
    store_result = store_.open(effective_store_path, store_key);
  }
  if (!store_result.ok) {
    return store_result;
  }
  const auto& existing_blocks = store_.all_blocks();
  if (!existing_blocks.empty()) {
    const auto& genesis_block = existing_blocks.front();
    bool genesis_mismatch = false;
    if (!genesis_merkle.empty() && !genesis_block.merkle_root.empty() &&
        genesis_block.merkle_root != genesis_merkle) {
      genesis_mismatch = true;
    }
    if (!genesis_block_hash.empty() && !genesis_block.block_hash.empty() &&
        genesis_block.block_hash != genesis_block_hash) {
      genesis_mismatch = true;
    }
    if (!genesis_psz.empty() && !genesis_block.psz_timestamp.empty() &&
        genesis_block.psz_timestamp != genesis_psz) {
      genesis_mismatch = true;
    }
    if (genesis_mismatch) {
      const Result reset = quarantine_and_reset_store_dir(config_.app_data_dir, effective_store_path,
                                                          "Genesis release spec mismatch.");
      if (!reset.ok) {
        return reset;
      }
      const Result reopen = store_.open(effective_store_path, store_key);
      if (!reopen.ok) {
        return reopen;
      }
    }
  }

  const Result block_check = store_.routine_block_check(util::unix_timestamp_now());
  if (!block_check.ok) {
    return block_check;
  }

  const std::string base_peers_path = config_.peers_dat_path.empty() ? current_community_.peers_dat_path
                                                                      : config_.peers_dat_path;
  peers_dat_path_ = resolve_data_path(base_peers_path + "." + network_suffix + ".dat",
                                      "peers-" + current_community_.community_id + "." + network_suffix + ".dat");

  // Reset peer state when switching communities, then load external peers file.
  p2p_node_ = P2PNode{};

  const Result load_peers = p2p_node_.load_peers_dat(peers_dat_path_);
  if (!load_peers.ok) {
    return load_peers;
  }

  const Result network_result = restart_network();
  if (!network_result.ok) {
    return network_result;
  }

  const Result save_peers = p2p_node_.save_peers_dat(peers_dat_path_);
  if (!save_peers.ok) {
    return save_peers;
  }

  Result validation = run_backtest_validation();
  if (!validation.ok && has_duplicate_reward_claim_error(validation.message)) {
    const Result rollback = store_.rollback_to_last_checkpoint("duplicate reward-claim conflict");
    if (!rollback.ok) {
      return rollback;
    }
    const Result rebuilt_block_check = store_.routine_block_check(util::unix_timestamp_now());
    if (!rebuilt_block_check.ok) {
      return rebuilt_block_check;
    }
    validation = run_backtest_validation();
  }
  if (!validation.ok && should_rebuild_local_store(validation.message)) {
    const Result reset =
        quarantine_and_reset_store_dir(config_.app_data_dir, effective_store_path, validation.message);
    if (!reset.ok) {
      return reset;
    }
    const Result reopen = store_.open(effective_store_path, store_key);
    if (!reopen.ok) {
      return reopen;
    }
    const Result rebuilt_block_check = store_.routine_block_check(util::unix_timestamp_now());
    if (!rebuilt_block_check.ok) {
      return rebuilt_block_check;
    }
    validation = run_backtest_validation();
  }

  return validation;
}

ProfileSummary AlphaService::profile() const {
  const auto observed = observed_display_names_by_cid();
  const std::string own_cid = crypto_.identity().cid.value;

  std::string display_name = local_display_name_;
  const auto own_it = observed.find(own_cid);
  if (display_name.empty() && own_it != observed.end()) {
    display_name = own_it->second;
  }
  if (display_name.empty()) {
    display_name = "SoupNet User";
  }

  std::size_t duplicate_count = 0;
  const std::string target = util::lowercase_copy(display_name);
  for (const auto& [cid, name] : observed) {
    if (cid == own_cid) {
      continue;
    }
    if (util::lowercase_copy(name) == target) {
      ++duplicate_count;
    }
  }

  std::string bio = "Pseudonymous contributor in community `" + current_community_.community_id + "`.\n";
  bio += std::string{"Duplicate-name policy: "} + (reject_duplicate_names_ ? "REJECT" : "ALLOW") + "\n";
  bio += std::string{"Display name state: "} +
         (local_display_name_immortalized_ ? "IMMORTALIZED" : "not set");
  bio += "\nReward balance: " + std::to_string(store_.reward_balance(own_cid));

  return {
      .cid = crypto_.identity().cid,
      .display_name = display_name,
      .bio_markdown = bio,
      .display_name_immortalized = local_display_name_immortalized_,
      .reject_duplicate_names = reject_duplicate_names_,
      .duplicate_name_detected = duplicate_count > 0,
      .duplicate_name_count = duplicate_count,
  };
}

AnonymityStatus AlphaService::anonymity_status() const {
  if (active_mode_ == AnonymityMode::I2P) {
    return i2p_provider_ != nullptr ? i2p_provider_->status()
                                    : AnonymityStatus{
                                          .running = false,
                                          .mode = "I2P",
                                          .version = "unavailable",
                                          .details = "I2P provider not initialized.",
                                          .last_started_unix = 0,
                                          .last_stopped_unix = 0,
                                          .update_count = 0,
                                          .endpoint = {},
                                      };
  }

  return tor_provider_ != nullptr ? tor_provider_->status()
                                  : AnonymityStatus{
                                        .running = false,
                                        .mode = "Tor",
                                        .version = "unavailable",
                                        .details = "Tor provider not initialized.",
                                        .last_started_unix = 0,
                                        .last_stopped_unix = 0,
                                        .update_count = 0,
                                        .endpoint = {},
                                    };
}

NodeStatusReport AlphaService::node_status() const {
  NodeStatusReport report;
  report.tor = tor_provider_ != nullptr ? tor_provider_->status()
                                        : AnonymityStatus{
                                              .running = false,
                                              .mode = "Tor",
                                              .version = "unavailable",
                                              .details = "Tor provider missing.",
                                              .last_started_unix = 0,
                                              .last_stopped_unix = 0,
                                              .update_count = 0,
                                              .endpoint = {},
                                          };
  report.i2p = i2p_provider_ != nullptr ? i2p_provider_->status()
                                        : AnonymityStatus{
                                              .running = false,
                                              .mode = "I2P",
                                              .version = "unavailable",
                                              .details = "I2P provider missing.",
                                              .last_started_unix = 0,
                                              .last_stopped_unix = 0,
                                              .update_count = 0,
                                              .endpoint = {},
                                          };
  report.tor_enabled = tor_enabled_;
  report.i2p_enabled = i2p_enabled_;
  report.active_mode = active_mode_;
  report.alpha_test_mode = alpha_test_mode_;
  report.p2p = p2p_node_.runtime_status();
  report.db = store_.health_report();
  report.local_reward_balance = store_.reward_balance(crypto_.identity().cid.value);
  report.reward_balances = reward_balances();
  report.moderation = store_.moderation_status();
  report.p2p_mainnet_port = config_.p2p_mainnet_port;
  report.p2p_testnet_port = config_.p2p_testnet_port;
  report.data_dir = config_.app_data_dir;
  report.chain_policy = config_.chain_policy;
  report.validation_limits = config_.validation_limits;
  report.genesis = active_genesis_spec();
  report.wallet = {
      .locked = wallet_locked(),
      .destroyed = wallet_destroyed_,
      .recovery_required = wallet_recovery_required_,
      .vault_path = crypto_.vault_path(),
      .backup_last_path = last_key_backup_path_,
      .last_unlocked_unix = wallet_last_unlocked_unix_,
      .last_locked_unix = wallet_last_locked_unix_,
  };
  report.peers_dat_path = peers_dat_path_;
  report.peers = p2p_node_.peers();
  report.community = current_community_;
  report.known_communities = community_profiles();
  report.core_phase_status = crypto_.core_phase_status();
  return report;
}

std::int64_t AlphaService::local_reward_balance() const {
  return store_.reward_balance(crypto_.identity().cid.value);
}

std::vector<RewardBalanceSummary> AlphaService::reward_balances() const {
  std::vector<RewardBalanceSummary> balances = store_.reward_balances();
  const auto names_by_cid = observed_display_names_by_cid();
  for (auto& entry : balances) {
    if (const auto it = names_by_cid.find(entry.cid); it != names_by_cid.end()) {
      entry.display_name = it->second;
    }
  }
  return balances;
}

ReceiveAddressInfo AlphaService::receive_info() const {
  ReceiveAddressInfo info;
  info.cid = crypto_.identity().cid.value;
  info.display_name = local_display_name_;
  info.address = soup_address_from_cid(info.cid);
  info.public_key = crypto_.identity().public_key;
  info.private_key = crypto_.identity().private_key;
  return info;
}

std::string AlphaService::hashspec_console() const {
  const auto& blocks = store_.all_blocks();
  const auto& events = store_.all_events();
  std::ostringstream text;
  text << "HashSpec Console\n\n";
  if (blocks.empty()) {
    text << "No blocks found.\n";
    return text.str();
  }

  std::unordered_map<std::string, std::string> payload_hash_by_event;
  payload_hash_by_event.reserve(events.size());
  for (const auto& event : events) {
    payload_hash_by_event[event.event_id] = util::sha256_like_hex(event.payload);
  }

  const auto& latest = blocks.back();
  const std::uint64_t next_index = latest.index + 1U;
  const std::int64_t next_open_unix = latest.opened_unix + static_cast<std::int64_t>(config_.block_interval_seconds);
  const std::string prev_hash = latest.block_hash.empty() ? "genesis" : latest.block_hash;

  std::vector<std::string> next_event_ids;
  if (latest.reserved && latest.event_ids.empty()) {
    next_event_ids = {};
  } else {
    next_event_ids = {};
  }

  std::vector<std::string> merkle_leaves;
  std::vector<std::string> content_parts;
  for (const auto& event_id : next_event_ids) {
    const auto it = payload_hash_by_event.find(event_id);
    const std::string payload_hash = it == payload_hash_by_event.end() ? "missing" : it->second;
    merkle_leaves.push_back(util::sha256_like_hex(event_id + ":" + payload_hash));
    content_parts.push_back(event_id + ":" + payload_hash);
  }
  const std::string anticipated_merkle = preview_merkle_root(merkle_leaves);
  const std::string anticipated_content_hash = util::sha256_like_hex(join_parts(content_parts));

  std::ostringstream block_input;
  block_input << next_index << "|" << next_open_unix << "|" << 1 << "|" << 0 << "|" << 0 << "|"
              << prev_hash << "|" << anticipated_merkle << "|" << anticipated_content_hash << "|";
  const std::string anticipated_block_hash = util::sha256_like_hex(block_input.str());

  const bool testnet = should_use_testnet(alpha_test_mode_, active_mode_);
  const int difficulty_nibbles = testnet ? 3 : 4;
  const std::string pow_material =
      current_community_.community_id + "|" + crypto_.identity().cid.value + "|" + std::to_string(next_index) +
      "|" + anticipated_block_hash + "|" + anticipated_merkle;

  text << "Chain: " << (testnet ? config_.testnet_chain_id : config_.mainnet_chain_id) << "\n";
  text << "Network: " << (testnet ? "testnet" : "mainnet") << "\n";
  text << "Latest Block Index: " << latest.index << "\n";
  text << "Latest Block Hash: " << latest.block_hash << "\n";
  text << "Latest Merkle Root: " << latest.merkle_root << "\n\n";

  text << "Next Block Anticipation\n";
  text << "- Next Index: " << next_index << "\n";
  text << "- Prev Hash: " << prev_hash << "\n";
  text << "- Provisional Event Count: " << next_event_ids.size() << "\n";
  text << "- Anticipated Merkle Root: " << anticipated_merkle << "\n";
  text << "- Anticipated Content Hash: " << anticipated_content_hash << "\n";
  text << "- Anticipated Block Hash: " << anticipated_block_hash << "\n\n";

  text << "PoW Preview\n";
  text << "- Difficulty (leading zero nibbles): " << difficulty_nibbles << "\n";
  text << "- Material: " << pow_material << "\n";
  text << "- Samples:\n";
  for (std::uint64_t attempt = 0; attempt < 5U; ++attempt) {
    const std::string sample = util::sha256_like_hex(pow_material + "|" + std::to_string(attempt));
    text << "  nonce " << attempt << " => " << sample << "\n";
  }

  std::uint64_t found_nonce = 0;
  std::string found_hash;
  constexpr std::uint64_t kPreviewAttempts = 200000;
  for (std::uint64_t attempt = 0; attempt < kPreviewAttempts; ++attempt) {
    const std::string candidate = util::sha256_like_hex(pow_material + "|" + std::to_string(attempt));
    if (util::has_leading_zero_nibbles(candidate, difficulty_nibbles)) {
      found_nonce = attempt;
      found_hash = candidate;
      break;
    }
  }

  if (found_hash.empty()) {
    text << "- Match not found in first " << kPreviewAttempts << " attempts.\n";
  } else {
    text << "- First match nonce: " << found_nonce << "\n";
    text << "- First match hash: " << found_hash << "\n";
  }
  return text.str();
}

std::string AlphaService::soup_address() const {
  return soup_address_from_cid(crypto_.identity().cid.value);
}

std::string AlphaService::public_key() const {
  return crypto_.identity().public_key;
}

std::string AlphaService::private_key() const {
  return crypto_.identity().private_key;
}

MessageSignatureSummary AlphaService::sign_message(std::string_view message) const {
  MessageSignatureSummary summary;
  summary.message = std::string{message};
  summary.signature = crypto_.sign(message);
  summary.public_key = crypto_.identity().public_key;
  summary.cid = crypto_.identity().cid.value;
  summary.address = soup_address_from_cid(summary.cid);
  summary.wallet_locked = wallet_locked();
  return summary;
}

bool AlphaService::verify_message_signature(std::string_view message, std::string_view signature,
                                            std::string_view public_key) const {
  if (message.empty() || signature.empty() || public_key.empty()) {
    return false;
  }
  return crypto_.verify(message, signature, public_key);
}

ModerationStatus AlphaService::moderation_status() const {
  return store_.moderation_status();
}

std::vector<CommunityProfile> AlphaService::community_profiles() const {
  std::vector<CommunityProfile> profiles;

  std::error_code ec;
  if (!std::filesystem::exists(communities_dir_, ec) || ec) {
    if (!current_community_.community_id.empty()) {
      profiles.push_back(current_community_);
    }
    return profiles;
  }

  for (const auto& entry : std::filesystem::directory_iterator(communities_dir_, ec)) {
    if (ec) {
      break;
    }

    if (!entry.is_regular_file()) {
      continue;
    }

    if (entry.path().extension() != ".dat") {
      continue;
    }

    const auto parsed = parse_community_profile_file(entry.path().string());
    if (parsed.has_value()) {
      profiles.push_back(*parsed);
    }
  }

  if (profiles.empty() && !current_community_.community_id.empty()) {
    profiles.push_back(current_community_);
  }

  std::ranges::sort(profiles, [](const CommunityProfile& lhs, const CommunityProfile& rhs) {
    return lhs.community_id < rhs.community_id;
  });

  return profiles;
}

std::vector<std::string> AlphaService::reference_parent_menus() const {
  std::vector<std::string> parents = reference_engine_.parent_menus();
  if (std::ranges::find(parents, std::string{"Forum"}) == parents.end()) {
    parents.push_back("Forum");
  }

  std::ranges::sort(parents, [](const std::string& lhs, const std::string& rhs) {
    if (lhs == "General") {
      return true;
    }
    if (rhs == "General") {
      return false;
    }
    if (lhs == "Forum") {
      return true;
    }
    if (rhs == "Forum") {
      return false;
    }
    return lhs < rhs;
  });

  return parents;
}

std::vector<std::string> AlphaService::reference_secondary_menus(std::string_view parent) const {
  if (parent == "Forum") {
    return {"Core Menu", "Community Posts", "Threads", "Replies", "Recipes", "Moderation"};
  }

  return reference_engine_.secondary_menus(parent);
}

std::vector<std::string> AlphaService::reference_openings(std::string_view parent,
                                                          std::string_view secondary,
                                                          std::string_view query) const {
  if (parent != "Forum") {
    return reference_engine_.openings(parent, secondary, query);
  }

  std::vector<std::string> keys;
  const std::string query_text{query};

  if (secondary == "Recipes") {
    const auto recipes = store_.query_recipes({.text = query_text, .category = {}});
    for (const auto& recipe : recipes) {
      keys.push_back("forum::recipe::" + recipe.recipe_id);
    }
    return keys;
  }

  if (secondary == "Core Menu") {
      const auto recipes = store_.query_recipes({.text = query_text, .category = {}});
      for (const auto& recipe : recipes) {
      if (!recipe.core_topic) {
        continue;
      }
      keys.push_back("forum::recipe::" + recipe.recipe_id);
    }
    return keys;
  }

  if (secondary == "Community Posts") {
    const auto recipes = store_.query_recipes({.text = query_text, .category = {}});
    for (const auto& recipe : recipes) {
      if (recipe.core_topic) {
        continue;
      }
      keys.push_back("forum::recipe::" + recipe.recipe_id);
    }
    return keys;
  }

  if (secondary == "Threads") {
    const auto threads_all = store_.query_threads("");
    for (const auto& thread : threads_all) {
      if (!query_text.empty() &&
          !util::contains_case_insensitive(thread.title, query_text) &&
          !util::contains_case_insensitive(thread.thread_id, query_text) &&
          !util::contains_case_insensitive(thread.recipe_id, query_text)) {
        continue;
      }

      keys.push_back("forum::thread::" + thread.thread_id);
    }
    return keys;
  }

  if (secondary == "Replies") {
    const auto threads_all = store_.query_threads("");
    for (const auto& thread : threads_all) {
      const auto replies_all = store_.query_replies(thread.thread_id);
      for (const auto& reply : replies_all) {
        if (!query_text.empty() &&
            !util::contains_case_insensitive(reply.reply_id, query_text) &&
            !util::contains_case_insensitive(reply.author_cid, query_text) &&
            !util::contains_case_insensitive(reply.markdown, query_text)) {
          continue;
        }
        keys.push_back("forum::reply::" + reply.reply_id);
      }
    }
    return keys;
  }

  if (secondary == "Moderation") {
    keys.push_back("forum::moderation::summary");
    const auto moderation = store_.moderation_status();
    for (const auto& object : moderation.objects) {
      const std::string key = "forum::moderation::object::" + object.object_id;
      if (!query_text.empty() && !util::contains_case_insensitive(key, query_text) &&
          !util::contains_case_insensitive(object.object_id, query_text)) {
        continue;
      }
      keys.push_back(key);
    }
    return keys;
  }

  return reference_engine_.openings(parent, secondary, query);
}

std::optional<refpad::WikiEntry> AlphaService::reference_lookup(std::string_view key) const {
  const DbHealthReport health = store_.health_report();

  if (key.starts_with("forum::recipe::")) {
    const std::string recipe_id = std::string{key.substr(std::string_view{"forum::recipe::"}.size())};
    const auto recipes = store_.query_recipes({.text = {}, .category = {}});
    for (const auto& recipe : recipes) {
      if (recipe.recipe_id != recipe_id) {
        continue;
      }

      const auto recipe_threads = store_.query_threads(recipe.recipe_id);
      std::string body;
      body += "Community: " + current_community_.community_id + "\n";
      body += "Chain: " + health.chain_id + " (" + health.network_id + ")\n";
      body += "Recipe ID: " + recipe.recipe_id + "\n";
      body += "Confirmation Event ID: " + recipe.source_event_id + "\n";
      if (const auto confirmation = store_.confirmation_for_object(recipe.recipe_id); confirmation.has_value()) {
        body += "Universal Confirmation: " + *confirmation + "\n";
      }
      body += "Consensus Hash: " + health.consensus_hash + "\n";
      body += "Category: " + recipe.category + "\n";
      body += "Segment: " + recipe_segment_label(recipe) + "\n";
      body += "Menu Segment: " + recipe.menu_segment + "\n";
      body += "Post Value: " + std::to_string(recipe.value_units) + "\n";
      body += "Confirmations: " + std::to_string(recipe.confirmation_count) + "\n";
      body += "Finality Threshold: " + std::to_string(health.confirmation_threshold) + "\n";
      body += "Age (s): " + std::to_string(recipe.confirmation_age_seconds) + "\n";
      body += "Author CID: " + recipe.author_cid + "\n";
      body += "Thumbs Up: " + std::to_string(recipe.thumbs_up_count) + "\n";
      body += "Average Rating: " + std::to_string(recipe.average_rating) + "\n";
      body += "Review Count: " + std::to_string(recipe.review_count) + "\n";
      body += "Thread Count: " + std::to_string(recipe_threads.size()) + "\n";

      return refpad::WikiEntry{
          .parent_menu = "Forum",
          .secondary_menu = recipe.core_topic ? "Core Menu" : "Community Posts",
          .key = std::string{key},
          .title = "[" + recipe_segment_label(recipe) + "] Recipe: " + recipe.title,
          .body = body,
      };
    }

    return std::nullopt;
  }

  if (key.starts_with("forum::thread::")) {
    const std::string thread_id = std::string{key.substr(std::string_view{"forum::thread::"}.size())};
    const auto threads_all = store_.query_threads("");
    for (const auto& thread : threads_all) {
      if (thread.thread_id != thread_id) {
        continue;
      }

      const auto replies_all = store_.query_replies(thread.thread_id);
      std::string body;
        body += "Community: " + current_community_.community_id + "\n";
        body += "Chain: " + health.chain_id + " (" + health.network_id + ")\n";
        body += "Thread ID: " + thread.thread_id + "\n";
        body += "Confirmation Event ID: " + thread.source_event_id + "\n";
        if (const auto confirmation = store_.confirmation_for_object(thread.thread_id); confirmation.has_value()) {
          body += "Universal Confirmation: " + *confirmation + "\n";
        }
        body += "Consensus Hash: " + health.consensus_hash + "\n";
        body += "Recipe ID: " + thread.recipe_id + "\n";
      body += "Post Value: " + std::to_string(thread.value_units) + "\n";
      body += "Confirmations: " + std::to_string(thread.confirmation_count) + "\n";
      body += "Finality Threshold: " + std::to_string(health.confirmation_threshold) + "\n";
      body += "Age (s): " + std::to_string(thread.confirmation_age_seconds) + "\n";
      body += "Author CID: " + thread.author_cid + "\n";
      body += "Reply Count: " + std::to_string(replies_all.size()) + "\n\n";
      body += "Replies\n";
      for (const auto& reply : replies_all) {
        body += "- [" + reply.reply_id + "] " + reply.author_cid + "\n";
      }

      return refpad::WikiEntry{
          .parent_menu = "Forum",
          .secondary_menu = "Threads",
          .key = std::string{key},
          .title = "Thread: " + thread.title,
          .body = body,
      };
    }

    return std::nullopt;
  }

  if (key.starts_with("forum::reply::")) {
    const std::string reply_id = std::string{key.substr(std::string_view{"forum::reply::"}.size())};
    const auto threads_all = store_.query_threads("");
    for (const auto& thread : threads_all) {
      const auto replies_all = store_.query_replies(thread.thread_id);
      for (const auto& reply : replies_all) {
        if (reply.reply_id != reply_id) {
          continue;
        }

        std::string body;
        body += "Community: " + current_community_.community_id + "\n";
        body += "Chain: " + health.chain_id + " (" + health.network_id + ")\n";
        body += "Reply ID: " + reply.reply_id + "\n";
        body += "Confirmation Event ID: " + reply.source_event_id + "\n";
        if (const auto confirmation = store_.confirmation_for_object(reply.reply_id); confirmation.has_value()) {
          body += "Universal Confirmation: " + *confirmation + "\n";
        }
        body += "Consensus Hash: " + health.consensus_hash + "\n";
        body += "Thread ID: " + reply.thread_id + "\n";
        body += "Post Value: " + std::to_string(reply.value_units) + "\n";
        body += "Confirmations: " + std::to_string(reply.confirmation_count) + "\n";
        body += "Finality Threshold: " + std::to_string(health.confirmation_threshold) + "\n";
        body += "Age (s): " + std::to_string(reply.confirmation_age_seconds) + "\n";
        body += "Author CID: " + reply.author_cid + "\n\n";
        body += reply.markdown;

        return refpad::WikiEntry{
            .parent_menu = "Forum",
            .secondary_menu = "Replies",
            .key = std::string{key},
            .title = "Reply: " + reply.reply_id,
            .body = body,
        };
      }
    }

    return std::nullopt;
  }

  if (key == "forum::moderation::summary") {
    const ModerationStatus moderation = store_.moderation_status();
    std::string body;
    body += "Community: " + current_community_.community_id + "\n";
    body += "Moderation Enabled: " + std::string{moderation.enabled ? "YES" : "NO"} + "\n";
    body += "Require Finality: " +
            std::string{moderation.policy.require_finality_for_actions ? "YES" : "NO"} + "\n";
    body += "Min Confirmations: " +
            std::to_string(moderation.policy.min_confirmations_for_enforcement) + "\n";
    body += "Auto Hide Flags: " + std::to_string(moderation.policy.max_flags_before_auto_hide) + "\n";
    body += "Role Model: " + moderation.policy.role_model + "\n";
    body += "Invalid Moderation Events: " + std::to_string(moderation.invalid_event_count) + "\n";
    body += "Active Moderators: " + std::to_string(moderation.active_moderators.size()) + "\n";
    for (const auto& moderator_cid : moderation.active_moderators) {
      body += "- " + moderator_cid + "\n";
    }
    body += "\nModerated Objects: " + std::to_string(moderation.objects.size()) + "\n";

    return refpad::WikiEntry{
        .parent_menu = "Forum",
        .secondary_menu = "Moderation",
        .key = std::string{key},
        .title = "Moderation Summary",
        .body = body,
    };
  }

  if (key.starts_with("forum::moderation::object::")) {
    const std::string object_id =
        std::string{key.substr(std::string_view{"forum::moderation::object::"}.size())};
    const ModerationStatus moderation = store_.moderation_status();
    for (const auto& object : moderation.objects) {
      if (object.object_id != object_id) {
        continue;
      }
      std::string body;
      body += "Object ID: " + object.object_id + "\n";
      body += "Flags: " + std::to_string(object.flag_count) + "\n";
      body += "Hidden: " + std::string{object.hidden ? "YES" : "NO"} + "\n";
      body += "Auto Hidden: " + std::string{object.auto_hidden ? "YES" : "NO"} + "\n";
      body += "Core Topic Pinned: " + std::string{object.core_topic_pinned ? "YES" : "NO"} + "\n";
      body += "Consensus Hash: " + health.consensus_hash + "\n";
      return refpad::WikiEntry{
          .parent_menu = "Forum",
          .secondary_menu = "Moderation",
          .key = std::string{key},
          .title = "Moderation Object: " + object.object_id,
          .body = body,
      };
    }
    return std::nullopt;
  }

  return reference_engine_.lookup(key);
}

Result AlphaService::try_claim_confirmed_block_rewards() {
  if (wallet_locked()) {
    return Result::success("Wallet locked; reward claims paused.");
  }
  const std::string local_cid = crypto_.identity().cid.value;
  if (local_cid.empty()) {
    return Result::failure("Reward claim failed: local CID is empty.");
  }

  const auto claimable_blocks = store_.claimable_confirmed_blocks(local_cid);
  if (claimable_blocks.empty()) {
    return Result::success("No claimable confirmed blocks.");
  }

  const bool testnet = should_use_testnet(alpha_test_mode_, active_mode_);
  const int difficulty_nibbles = testnet ? 3 : 4;
  bool claimed_any = false;
  for (const auto& block : claimable_blocks) {
    const std::int64_t reward_units = store_.next_claim_reward(block.index);
    if (reward_units <= 0) {
      continue;
    }

    const std::string pow_material =
        current_community_.community_id + "|" + local_cid + "|" + std::to_string(block.index) + "|" +
        block.block_hash + "|" + block.merkle_root;
    std::uint64_t pow_nonce = 0;
    std::string pow_hash;
    constexpr std::uint64_t kMaxPowAttempts = 2500000;
    for (std::uint64_t attempt = 0; attempt < kMaxPowAttempts; ++attempt) {
      const std::string candidate = util::sha256_like_hex(pow_material + "|" + std::to_string(attempt));
      if (util::has_leading_zero_nibbles(candidate, difficulty_nibbles)) {
        pow_nonce = attempt;
        pow_hash = candidate;
        break;
      }
    }
    if (pow_hash.empty()) {
      continue;
    }

    const std::string claim_id =
        "clm-" + crypto_.hash_bytes(current_community_.community_id + local_cid + std::to_string(block.index) +
                                    block.block_hash)
                     .substr(0, 16);
    const std::string witness_root =
        util::sha256_like_hex(local_cid + "|" + std::to_string(block.index) + "|" +
                              std::to_string(reward_units) + "|" + pow_hash);

    EventEnvelope claim = make_event(
        EventKind::BlockRewardClaimed,
        {{"claim_id", claim_id},
         {"block_index", std::to_string(block.index)},
         {"reward", std::to_string(reward_units)},
         {"pow_difficulty", std::to_string(difficulty_nibbles)},
         {"pow_nonce", std::to_string(pow_nonce)},
         {"pow_material", pow_material},
         {"pow_hash", pow_hash},
         {"witness_root", witness_root},
         {"block_hash", block.block_hash},
         {"merkle_root", block.merkle_root},
         {"psz_timestamp", block.psz_timestamp}});

    const Result append = store_.append_event(claim);
    if (!append.ok) {
      return append;
    }
    p2p_node_.queue_local_event(claim);
    claimed_any = true;
  }

  if (!claimed_any) {
    return Result::success("No reward claims generated.");
  }

  return run_backtest_validation();
}

Result AlphaService::validate_and_apply_post_cost(std::int64_t requested_units,
                                                  std::int64_t& out_applied_units) const {
  if (requested_units < 0) {
    return Result::failure("Post value cannot be negative.");
  }

  const std::int64_t minimum_required = std::max<std::int64_t>(0, current_community_.minimum_post_value);
  out_applied_units = std::max<std::int64_t>(requested_units, minimum_required);
  const std::int64_t balance = store_.reward_balance(crypto_.identity().cid.value);
  if (out_applied_units > 0 && balance < out_applied_units) {
    return Result::failure("Insufficient reward balance for this post value requirement.");
  }

  return Result::success();
}

std::optional<std::string> AlphaService::resolve_display_name_to_cid(std::string_view display_name) const {
  const std::string normalized = util::lowercase_copy(sanitize_display_name(display_name));
  if (normalized.empty()) {
    return std::nullopt;
  }

  const auto observed = observed_display_names_by_cid();
  std::optional<std::string> match;
  for (const auto& [cid, name] : observed) {
    if (util::lowercase_copy(name) != normalized) {
      continue;
    }
    if (match.has_value() && *match != cid) {
      return std::nullopt;
    }
    match = cid;
  }
  return match;
}

std::optional<std::string> AlphaService::resolve_address_to_cid(std::string_view address) const {
  const std::string needle = util::trim_copy(address);
  if (needle.empty()) {
    return std::nullopt;
  }

  if (soup_address_from_cid(crypto_.identity().cid.value) == needle) {
    return crypto_.identity().cid.value;
  }

  for (const auto& balance : store_.reward_balances()) {
    if (soup_address_from_cid(balance.cid) == needle) {
      return balance.cid;
    }
  }

  const auto names = observed_display_names_by_cid();
  for (const auto& [cid, _] : names) {
    if (soup_address_from_cid(cid) == needle) {
      return cid;
    }
  }

  return std::nullopt;
}

Result AlphaService::append_locally_and_queue(const EventEnvelope& event) {
  if (event.signature.empty()) {
    return Result::failure("Local event signature is empty. Unlock wallet and retry.");
  }
  const Result append_result = store_.append_event(event);
  if (!append_result.ok) {
    return append_result;
  }

  const Result validation = run_backtest_validation();
  if (!validation.ok) {
    return validation;
  }

  p2p_node_.queue_local_event(event);
  return Result::success("Event appended and queued for sync.", event.event_id);
}

EventEnvelope AlphaService::make_event(EventKind kind,
                                       std::vector<std::pair<std::string, std::string>> payload_fields) {
  const std::int64_t now = util::unix_timestamp_now();
  std::int64_t event_unix_ts = now;
  if (event_unix_ts <= last_local_event_unix_ts_) {
    const std::int64_t target = last_local_event_unix_ts_ + 1;
    while (event_unix_ts < target) {
      std::this_thread::sleep_for(std::chrono::milliseconds(2));
      event_unix_ts = util::unix_timestamp_now();
    }
  }
  last_local_event_unix_ts_ = event_unix_ts;
  const auto genesis = active_genesis_spec();
  payload_fields.emplace_back("author_cid", crypto_.identity().cid.value);
  payload_fields.emplace_back("community_id", current_community_.community_id);
  payload_fields.emplace_back("chain_id", genesis.chain_id);
  payload_fields.emplace_back("network_id", genesis.network_id);
  payload_fields.emplace_back("kind", std::to_string(static_cast<int>(kind)));
  payload_fields.emplace_back("unix_ts", std::to_string(event_unix_ts));

  const std::string payload = util::canonical_join(std::move(payload_fields));
  const std::string event_id = crypto_.content_id(payload);

  return {
      .event_id = event_id,
      .kind = kind,
      .author_cid = crypto_.identity().cid.value,
      .unix_ts = event_unix_ts,
      .payload = payload,
      .signature = crypto_.sign(payload),
  };
}

Result AlphaService::restart_network() {
  p2p_node_.stop();

  if (!tor_enabled_ && !i2p_enabled_) {
    return Result::success("No active anonymity providers; P2P node remains offline.");
  }

  if (active_mode_ == AnonymityMode::Tor && !tor_enabled_ && i2p_enabled_) {
    active_mode_ = AnonymityMode::I2P;
  }
  if (active_mode_ == AnonymityMode::I2P && !i2p_enabled_ && tor_enabled_) {
    active_mode_ = AnonymityMode::Tor;
  }

  const ProxyEndpoint endpoint = active_proxy_endpoint();
  if (endpoint.host.empty() || endpoint.port == 0) {
    return Result::failure("Unable to restart P2P node: no active proxy endpoint.");
  }

  const bool testnet = should_use_testnet(alpha_test_mode_, active_mode_);
  const std::uint16_t p2p_port = testnet ? config_.p2p_testnet_port : config_.p2p_mainnet_port;
  const std::string network_name = testnet ? "testnet" : "mainnet";
  std::vector<std::string> seeds =
      testnet ? config_.seed_peers_testnet : config_.seed_peers_mainnet;
  if (seeds.empty()) {
    seeds = config_.seed_peers;
  }
  if (alpha_test_mode_) {
    seeds.push_back("127.0.0.1:" + std::to_string(p2p_port));
  }

  return p2p_node_.start(seeds, endpoint, crypto_.identity().cid.value, alpha_test_mode_, p2p_port,
                         network_name);
}

Result AlphaService::ensure_provider_state(AnonymityMode mode, bool enabled) {
  IAnonymityProvider* provider =
      (mode == AnonymityMode::Tor) ? tor_provider_.get() : i2p_provider_.get();

  if (provider == nullptr) {
    return Result::failure("Provider missing for mode " + mode_to_string(mode));
  }

  provider->set_alpha_test_mode(alpha_test_mode_);

  if (!enabled) {
    provider->stop();
    return Result::success(mode_to_string(mode) + " provider stopped.");
  }

  const AnonymityStatus status = provider->status();
  if (status.running) {
    return Result::success(mode_to_string(mode) + " provider already running.");
  }

  return provider->start();
}

ProxyEndpoint AlphaService::active_proxy_endpoint() const {
  if (active_mode_ == AnonymityMode::I2P && i2p_enabled_ && i2p_provider_ != nullptr) {
    return i2p_provider_->proxy_endpoint();
  }

  if (tor_enabled_ && tor_provider_ != nullptr) {
    return tor_provider_->proxy_endpoint();
  }

  if (i2p_enabled_ && i2p_provider_ != nullptr) {
    return i2p_provider_->proxy_endpoint();
  }

  return {};
}

Result AlphaService::load_or_create_community_profile(std::string_view profile_path_or_id,
                                                      std::string_view display_name,
                                                      std::string_view description) {
  std::string profile_path;
  std::string proposed_id;

  if (profile_path_or_id.empty()) {
    proposed_id = "recipes";
  } else if (looks_like_path(profile_path_or_id)) {
    profile_path = resolve_data_path(profile_path_or_id, std::string{profile_path_or_id});
    proposed_id = sanitize_community_id(std::filesystem::path(profile_path).stem().string());
  } else {
    proposed_id = sanitize_community_id(profile_path_or_id);
  }

  if (proposed_id.empty()) {
    proposed_id = "community";
  }

  if (profile_path.empty()) {
    profile_path = (std::filesystem::path{communities_dir_} / (proposed_id + ".dat")).string();
  }

  if (std::filesystem::exists(profile_path)) {
    const auto loaded = parse_community_profile_file(profile_path);
    if (!loaded.has_value()) {
      return Result::failure("Community profile exists but could not be parsed: " + profile_path);
    }

    current_community_ = *loaded;
    return Result::success("Loaded community profile: " + current_community_.community_id);
  }

  CommunityProfile created;
  created.community_id = proposed_id;
  created.display_name = display_name.empty() ? ("Community " + proposed_id) : std::string{display_name};
  created.description = description.empty() ? "Modular got-soup community profile." : std::string{description};
  created.profile_path = profile_path;
  created.cipher_key = crypto_.derive_vault_key(config_.passphrase, "community:" + created.community_id);
  created.peers_dat_path =
      (std::filesystem::path{config_.app_data_dir} / ("peers-" + created.community_id + ".dat")).string();
  created.store_path =
      (std::filesystem::path{config_.app_data_dir} / ("db-" + created.community_id)).string();
  created.minimum_post_value = std::max<std::int64_t>(0, config_.minimum_post_value);
  created.block_reward_units = config_.block_reward_units <= 0 ? 115 : config_.block_reward_units;
  created.moderation_enabled = config_.default_moderation_policy.moderation_enabled;
  created.moderation_require_finality = config_.default_moderation_policy.require_finality_for_actions;
  created.moderation_min_confirmations =
      std::max<std::uint64_t>(1, config_.default_moderation_policy.min_confirmations_for_enforcement);
  created.moderation_auto_hide_flags =
      std::max<std::size_t>(1, config_.default_moderation_policy.max_flags_before_auto_hide);
  created.moderator_cids = config_.default_moderators.empty() ? config_.default_moderation_policy.moderator_cids
                                                               : config_.default_moderators;
  created.moderator_cids.push_back(crypto_.identity().cid.value);
  created.moderator_cids = split_csv(join_csv(created.moderator_cids));

  if (!config_.genesis_psz_timestamp.empty()) {
    created.genesis_psz_timestamp = config_.genesis_psz_timestamp;
  } else if (should_use_testnet(alpha_test_mode_, active_mode_)) {
    created.genesis_psz_timestamp = config_.testnet_genesis_psz_timestamp;
  } else {
    created.genesis_psz_timestamp = config_.mainnet_genesis_psz_timestamp;
  }

  const Result write_result = write_community_profile_file(created);
  if (!write_result.ok) {
    return write_result;
  }

  current_community_ = created;
  return Result::success("Created community profile: " + current_community_.community_id);
}

std::optional<CommunityProfile> AlphaService::parse_community_profile_file(std::string_view path) const {
  std::ifstream in(std::string{path});
  if (!in) {
    return std::nullopt;
  }

  std::unordered_map<std::string, std::string> fields;
  std::string line;
  while (std::getline(in, line)) {
    const std::string trimmed = util::trim_copy(line);
    if (trimmed.empty() || trimmed.front() == '#') {
      continue;
    }

    const auto split = trimmed.find('=');
    if (split == std::string::npos) {
      continue;
    }

    const std::string key = util::trim_copy(trimmed.substr(0, split));
    const std::string value = util::trim_copy(trimmed.substr(split + 1));
    fields[key] = value;
  }

  if (!fields.contains("community_id")) {
    return std::nullopt;
  }

  CommunityProfile profile;
  profile.community_id = sanitize_community_id(fields["community_id"]);
  profile.display_name = fields.contains("display_name") ? fields["display_name"] : profile.community_id;
  profile.description = fields.contains("description") ? fields["description"] : "";
  profile.profile_path = std::string{path};
  profile.cipher_key =
      fields.contains("cipher_key") ? fields["cipher_key"]
                                      : crypto_.derive_vault_key(config_.passphrase, "community:" + profile.community_id);
  profile.peers_dat_path =
      fields.contains("peers_dat_path")
          ? resolve_data_path(fields["peers_dat_path"], "peers-" + profile.community_id + ".dat")
          : (std::filesystem::path{config_.app_data_dir} / ("peers-" + profile.community_id + ".dat")).string();
  profile.store_path =
      fields.contains("store_path")
          ? resolve_data_path(fields["store_path"], "db-" + profile.community_id)
          : (std::filesystem::path{config_.app_data_dir} / ("db-" + profile.community_id)).string();
  profile.minimum_post_value = fields.contains("minimum_post_value")
                                   ? std::max<std::int64_t>(0, parse_int64_default(fields["minimum_post_value"], 0))
                                   : std::max<std::int64_t>(0, config_.minimum_post_value);
  profile.block_reward_units = fields.contains("block_reward_units")
                                   ? std::max<std::int64_t>(1, parse_int64_default(fields["block_reward_units"], 50))
                                   : std::max<std::int64_t>(1, config_.block_reward_units <= 0 ? 115
                                                                                                 : config_.block_reward_units);
  profile.moderation_enabled = !fields.contains("moderation_enabled") || fields["moderation_enabled"] != "0";
  profile.moderation_require_finality =
      !fields.contains("moderation_require_finality") || fields["moderation_require_finality"] != "0";
  profile.moderation_min_confirmations = fields.contains("moderation_min_confirmations")
                                             ? static_cast<std::uint64_t>(
                                                   std::max<std::int64_t>(
                                                       1, parse_int64_default(fields["moderation_min_confirmations"],
                                                                              static_cast<std::int64_t>(
                                                                                  config_.default_moderation_policy
                                                                                      .min_confirmations_for_enforcement))))
                                             : std::max<std::uint64_t>(
                                                   1, config_.default_moderation_policy.min_confirmations_for_enforcement);
  profile.moderation_auto_hide_flags =
      fields.contains("moderation_auto_hide_flags")
          ? static_cast<std::size_t>(
                std::max<std::int64_t>(
                    1, parse_int64_default(fields["moderation_auto_hide_flags"],
                                           static_cast<std::int64_t>(
                                               config_.default_moderation_policy.max_flags_before_auto_hide))))
          : std::max<std::size_t>(1, config_.default_moderation_policy.max_flags_before_auto_hide);
  if (fields.contains("moderators")) {
    profile.moderator_cids = split_csv(fields["moderators"]);
  } else {
    profile.moderator_cids =
        config_.default_moderators.empty() ? config_.default_moderation_policy.moderator_cids
                                           : config_.default_moderators;
  }
  if (profile.moderator_cids.empty()) {
    profile.moderator_cids.push_back(crypto_.identity().cid.value);
  }
  profile.moderator_cids = split_csv(join_csv(profile.moderator_cids));
  profile.genesis_psz_timestamp =
      fields.contains("genesis_psz_timestamp")
          ? fields["genesis_psz_timestamp"]
          : (!config_.genesis_psz_timestamp.empty()
                 ? config_.genesis_psz_timestamp
                 : (should_use_testnet(alpha_test_mode_, active_mode_)
                        ? config_.testnet_genesis_psz_timestamp
                        : config_.mainnet_genesis_psz_timestamp));

  if (profile.community_id.empty()) {
    return std::nullopt;
  }

  return profile;
}

Result AlphaService::write_community_profile_file(const CommunityProfile& profile) const {
  if (profile.profile_path.empty()) {
    return Result::failure("Community profile write failed: empty profile path.");
  }

  std::error_code ec;
  const std::filesystem::path file_path{profile.profile_path};
  if (file_path.has_parent_path()) {
    std::filesystem::create_directories(file_path.parent_path(), ec);
    if (ec) {
      return Result::failure("Unable to create community profile directory: " + ec.message());
    }
  }

  std::ofstream out(file_path, std::ios::out | std::ios::trunc);
  if (!out) {
    return Result::failure("Unable to write community profile file: " + profile.profile_path);
  }

  out << "# got-soup community profile\n";
  out << "community_id=" << profile.community_id << '\n';
  out << "display_name=" << profile.display_name << '\n';
  out << "description=" << profile.description << '\n';
  out << "cipher_key=" << profile.cipher_key << '\n';
  out << "peers_dat_path=" << profile.peers_dat_path << '\n';
  out << "store_path=" << profile.store_path << '\n';
  out << "minimum_post_value=" << profile.minimum_post_value << '\n';
  out << "block_reward_units=" << profile.block_reward_units << '\n';
  out << "moderation_enabled=" << (profile.moderation_enabled ? "1" : "0") << '\n';
  out << "moderation_require_finality=" << (profile.moderation_require_finality ? "1" : "0") << '\n';
  out << "moderation_min_confirmations=" << profile.moderation_min_confirmations << '\n';
  out << "moderation_auto_hide_flags=" << profile.moderation_auto_hide_flags << '\n';
  out << "moderators=" << join_csv(profile.moderator_cids) << '\n';
  out << "genesis_psz_timestamp=" << profile.genesis_psz_timestamp << '\n';

  if (!out.good()) {
    return Result::failure("Failed writing community profile file: " + profile.profile_path);
  }

  return Result::success("Community profile written.");
}

std::string AlphaService::sanitize_community_id(std::string_view id) const {
  std::string cleaned;
  cleaned.reserve(id.size());

  for (char c : id) {
    if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')) {
      cleaned.push_back(c);
      continue;
    }

    if (c >= 'A' && c <= 'Z') {
      cleaned.push_back(static_cast<char>(c - 'A' + 'a'));
      continue;
    }

    if (c == '_' || c == '-' || c == ' ') {
      cleaned.push_back('-');
    }
  }

  while (!cleaned.empty() && cleaned.front() == '-') {
    cleaned.erase(cleaned.begin());
  }
  while (!cleaned.empty() && cleaned.back() == '-') {
    cleaned.pop_back();
  }

  return cleaned;
}

std::string AlphaService::sanitize_display_name(std::string_view value) const {
  std::string cleaned = util::trim_copy(value);
  if (cleaned.size() > 48) {
    cleaned.resize(48);
  }
  return cleaned;
}

std::string AlphaService::sanitize_cid(std::string_view cid) const {
  return util::trim_copy(cid);
}

std::string AlphaService::resolve_data_path(std::string_view input_path,
                                            std::string_view fallback_name) const {
  std::string path = util::trim_copy(input_path);
  if (path.empty()) {
    path = std::string{fallback_name};
  }
  if (path.empty()) {
    return config_.app_data_dir;
  }
  if (is_absolute_path(path)) {
    return path;
  }
  return (std::filesystem::path{config_.app_data_dir} / path).string();
}

bool AlphaService::is_local_moderator() const {
  const std::string local_cid = crypto_.identity().cid.value;
  if (local_cid.empty()) {
    return false;
  }
  if (store_.is_moderator(local_cid)) {
    return true;
  }
  return std::ranges::find(current_community_.moderator_cids, local_cid) != current_community_.moderator_cids.end();
}

Result AlphaService::ensure_local_moderator(std::string_view operation) const {
  if (!current_community_.moderation_enabled) {
    return Result::failure("Moderation is disabled for this community.");
  }
  if (!is_local_moderator()) {
    return Result::failure("Moderator authority required before `" + std::string{operation} + "`.");
  }
  return Result::success();
}

bool AlphaService::wallet_locked() const {
  return !crypto_.ready();
}

Result AlphaService::ensure_wallet_unlocked(std::string_view operation) const {
  if (wallet_locked()) {
    return Result::failure("Wallet is locked; unlock required before `" + std::string{operation} + "`.");
  }
  return Result::success();
}

GenesisSpec AlphaService::active_genesis_spec() const {
  const bool testnet = should_use_testnet(alpha_test_mode_, active_mode_);
  const std::string network_id = testnet ? "testnet" : "mainnet";
  const std::string chain_id = testnet ? config_.testnet_chain_id : config_.mainnet_chain_id;
  const std::string psz = !config_.genesis_psz_timestamp.empty()
                              ? config_.genesis_psz_timestamp
                              : (testnet ? config_.testnet_genesis_psz_timestamp
                                         : config_.mainnet_genesis_psz_timestamp);
  const std::string merkle =
      testnet ? config_.testnet_genesis_merkle_root : config_.mainnet_genesis_merkle_root;
  const std::string block_hash =
      testnet ? config_.testnet_genesis_block_hash : config_.mainnet_genesis_block_hash;
  std::vector<std::string> seeds = testnet ? config_.seed_peers_testnet : config_.seed_peers_mainnet;
  if (seeds.empty()) {
    seeds = config_.seed_peers;
  }
  const std::vector<InitialAllocation> allocations =
      testnet ? config_.testnet_initial_allocations : config_.mainnet_initial_allocations;
  return {
      .chain_id = chain_id,
      .network_id = network_id,
      .psz_timestamp = psz,
      .merkle_root = merkle,
      .block_hash = block_hash,
      .seed_peers = seeds,
      .initial_allocations = allocations,
  };
}

Result AlphaService::load_profile_state() {
  local_display_name_.clear();
  local_display_name_immortalized_ = false;
  reject_duplicate_names_ = true;
  wallet_destroyed_ = false;
  wallet_recovery_required_ = false;
  last_key_backup_path_.clear();
  wallet_last_locked_unix_ = 0;
  wallet_last_unlocked_unix_ = crypto_.last_unlocked_unix();

  std::ifstream in(profile_state_path_);
  if (!in) {
    return Result::success("Profile state will be created on first update.");
  }

  std::unordered_map<std::string, std::string> fields;
  std::string line;
  while (std::getline(in, line)) {
    const std::string trimmed = util::trim_copy(line);
    if (trimmed.empty() || trimmed.front() == '#') {
      continue;
    }

    const auto split = trimmed.find('=');
    if (split == std::string::npos) {
      continue;
    }

    fields[util::trim_copy(trimmed.substr(0, split))] = util::trim_copy(trimmed.substr(split + 1));
  }

  if (fields.contains("display_name")) {
    local_display_name_ = sanitize_display_name(fields["display_name"]);
  }
  if (fields.contains("display_name_immortalized")) {
    local_display_name_immortalized_ = fields["display_name_immortalized"] == "1" ||
                                       fields["display_name_immortalized"] == "true";
  }
  if (fields.contains("duplicate_policy")) {
    reject_duplicate_names_ = fields["duplicate_policy"] != "allow";
  }
  if (fields.contains("wallet_destroyed")) {
    wallet_destroyed_ = fields["wallet_destroyed"] == "1" || fields["wallet_destroyed"] == "true";
  }
  if (fields.contains("wallet_recovery_required")) {
    wallet_recovery_required_ =
        fields["wallet_recovery_required"] == "1" || fields["wallet_recovery_required"] == "true";
  }
  if (fields.contains("last_key_backup_path")) {
    last_key_backup_path_ = fields["last_key_backup_path"];
  }
  if (fields.contains("wallet_last_locked_unix")) {
    wallet_last_locked_unix_ = parse_int64_default(fields["wallet_last_locked_unix"], 0);
  }
  if (fields.contains("wallet_last_unlocked_unix")) {
    wallet_last_unlocked_unix_ = parse_int64_default(fields["wallet_last_unlocked_unix"], 0);
  }

  return Result::success("Profile state loaded.");
}

Result AlphaService::save_profile_state() const {
  if (profile_state_path_.empty()) {
    return Result::failure("Profile state path is not configured.");
  }

  std::error_code ec;
  const std::filesystem::path path{profile_state_path_};
  if (path.has_parent_path()) {
    std::filesystem::create_directories(path.parent_path(), ec);
    if (ec) {
      return Result::failure("Unable to create profile state directory: " + ec.message());
    }
  }

  std::ofstream out(path, std::ios::out | std::ios::trunc);
  if (!out) {
    return Result::failure("Unable to write profile state file.");
  }

  out << "# got-soup profile state\n";
  out << "display_name=" << local_display_name_ << '\n';
  out << "display_name_immortalized=" << (local_display_name_immortalized_ ? "1" : "0") << '\n';
  out << "duplicate_policy=" << (reject_duplicate_names_ ? "reject" : "allow") << '\n';
  out << "wallet_destroyed=" << (wallet_destroyed_ ? "1" : "0") << '\n';
  out << "wallet_recovery_required=" << (wallet_recovery_required_ ? "1" : "0") << '\n';
  out << "last_key_backup_path=" << last_key_backup_path_ << '\n';
  out << "wallet_last_locked_unix=" << wallet_last_locked_unix_ << '\n';
  out << "wallet_last_unlocked_unix=" << wallet_last_unlocked_unix_ << '\n';
  if (!out.good()) {
    return Result::failure("Failed writing profile state file.");
  }

  return Result::success("Profile state saved.");
}

std::unordered_map<std::string, std::string> AlphaService::observed_display_names_by_cid() const {
  std::unordered_map<std::string, std::string> names;
  for (const auto& event : store_.all_events()) {
    if (event.kind != EventKind::ProfileUpdated) {
      continue;
    }
    const auto payload = util::parse_canonical_map(event.payload);
    const auto name_it = payload.find("display_name");
    if (name_it == payload.end()) {
      continue;
    }
    const std::string name = sanitize_display_name(name_it->second);
    if (name.empty()) {
      continue;
    }
    names[event.author_cid] = name;
  }

  if (!local_display_name_.empty()) {
    names[crypto_.identity().cid.value] = local_display_name_;
  }

  return names;
}

}  // namespace alpha

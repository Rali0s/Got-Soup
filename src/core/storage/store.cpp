#include "core/storage/store.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <functional>
#include <limits>
#include <sstream>
#include <system_error>
#include <unordered_set>
#include <utility>

#include "core/util/canonical.hpp"
#include "core/util/hash.hpp"

namespace alpha {
namespace {

constexpr std::string_view kEventLogFile = "events.log";
constexpr std::string_view kBlockLogFile = "blockdata.dat";
constexpr std::string_view kLegacyBlockLogFile = "blocks.log";
constexpr std::string_view kInvalidEventLogFile = "invalid-events.log";
constexpr std::string_view kSnapshotFile = "state.snapshot";
constexpr std::string_view kCheckpointsFile = "checkpoints.dat";
constexpr std::string_view kBlockHeaderPrefix = "# got-soup blockdata";

std::string to_hex(std::string_view bytes) {
  static constexpr char kHex[] = "0123456789abcdef";
  std::string out;
  out.reserve(bytes.size() * 2U);
  for (unsigned char c : bytes) {
    out.push_back(kHex[(c >> 4U) & 0x0FU]);
    out.push_back(kHex[c & 0x0FU]);
  }
  return out;
}

int from_hex_digit(char c) {
  if (c >= '0' && c <= '9') {
    return c - '0';
  }
  if (c >= 'a' && c <= 'f') {
    return c - 'a' + 10;
  }
  if (c >= 'A' && c <= 'F') {
    return c - 'A' + 10;
  }
  return -1;
}

std::string from_hex(std::string_view hex) {
  if ((hex.size() % 2U) != 0U) {
    return {};
  }

  std::string out;
  out.reserve(hex.size() / 2U);
  for (std::size_t i = 0; i < hex.size(); i += 2U) {
    const int hi = from_hex_digit(hex[i]);
    const int lo = from_hex_digit(hex[i + 1U]);
    if (hi < 0 || lo < 0) {
      return {};
    }
    out.push_back(static_cast<char>((hi << 4U) | lo));
  }
  return out;
}

std::string event_kind_to_string(EventKind kind) {
  switch (kind) {
    case EventKind::RecipeCreated:
      return "RecipeCreated";
    case EventKind::ThreadCreated:
      return "ThreadCreated";
    case EventKind::ReplyCreated:
      return "ReplyCreated";
    case EventKind::ReviewAdded:
      return "ReviewAdded";
    case EventKind::ThumbsUpAdded:
      return "ThumbsUpAdded";
    case EventKind::BlockRewardClaimed:
      return "BlockRewardClaimed";
    case EventKind::RewardTransferred:
      return "RewardTransferred";
    case EventKind::ProfileUpdated:
      return "ProfileUpdated";
    case EventKind::KeyRotated:
      return "KeyRotated";
    case EventKind::ModeratorAdded:
      return "ModeratorAdded";
    case EventKind::ModeratorRemoved:
      return "ModeratorRemoved";
    case EventKind::ContentFlagged:
      return "ContentFlagged";
    case EventKind::ContentHidden:
      return "ContentHidden";
    case EventKind::ContentUnhidden:
      return "ContentUnhidden";
    case EventKind::CoreTopicPinned:
      return "CoreTopicPinned";
    case EventKind::CoreTopicUnpinned:
      return "CoreTopicUnpinned";
    case EventKind::PolicyUpdated:
      return "PolicyUpdated";
  }
  return "RecipeCreated";
}

EventKind event_kind_from_string(std::string_view text) {
  if (text == "ThreadCreated") {
    return EventKind::ThreadCreated;
  }
  if (text == "ReplyCreated") {
    return EventKind::ReplyCreated;
  }
  if (text == "ReviewAdded") {
    return EventKind::ReviewAdded;
  }
  if (text == "ThumbsUpAdded") {
    return EventKind::ThumbsUpAdded;
  }
  if (text == "BlockRewardClaimed") {
    return EventKind::BlockRewardClaimed;
  }
  if (text == "RewardTransferred") {
    return EventKind::RewardTransferred;
  }
  if (text == "ProfileUpdated") {
    return EventKind::ProfileUpdated;
  }
  if (text == "KeyRotated") {
    return EventKind::KeyRotated;
  }
  if (text == "ModeratorAdded") {
    return EventKind::ModeratorAdded;
  }
  if (text == "ModeratorRemoved") {
    return EventKind::ModeratorRemoved;
  }
  if (text == "ContentFlagged") {
    return EventKind::ContentFlagged;
  }
  if (text == "ContentHidden") {
    return EventKind::ContentHidden;
  }
  if (text == "ContentUnhidden") {
    return EventKind::ContentUnhidden;
  }
  if (text == "CoreTopicPinned") {
    return EventKind::CoreTopicPinned;
  }
  if (text == "CoreTopicUnpinned") {
    return EventKind::CoreTopicUnpinned;
  }
  if (text == "PolicyUpdated") {
    return EventKind::PolicyUpdated;
  }
  return EventKind::RecipeCreated;
}

std::string serialize_event_line(const EventEnvelope& event) {
  std::ostringstream out;
  out << event.event_id << '\t' << event_kind_to_string(event.kind) << '\t' << event.author_cid
      << '\t' << event.unix_ts << '\t' << to_hex(event.payload) << '\t' << event.signature << '\n';
  return out.str();
}

bool parse_int64(std::string_view text, std::int64_t& out) {
  std::int64_t value = 0;
  const auto result = std::from_chars(text.data(), text.data() + text.size(), value);
  if (result.ec != std::errc()) {
    return false;
  }
  out = value;
  return true;
}

bool parse_event_line(std::string_view line, EventEnvelope& out) {
  std::array<std::string_view, 6> fields{};
  std::size_t field_index = 0;
  std::size_t start = 0;
  for (std::size_t i = 0; i <= line.size(); ++i) {
    if (i == line.size() || line[i] == '\t') {
      if (field_index >= fields.size()) {
        return false;
      }
      fields[field_index++] = line.substr(start, i - start);
      start = i + 1U;
    }
  }

  if (field_index != fields.size()) {
    return false;
  }

  out.event_id = std::string{fields[0]};
  out.kind = event_kind_from_string(fields[1]);
  out.author_cid = std::string{fields[2]};
  if (!parse_int64(fields[3], out.unix_ts)) {
    return false;
  }
  out.payload = from_hex(fields[4]);
  out.signature = std::string{fields[5]};
  return !out.event_id.empty() && !out.payload.empty();
}

int parse_int_or_zero(std::string_view value) {
  int parsed = 0;
  const auto result = std::from_chars(value.data(), value.data() + value.size(), parsed);
  if (result.ec != std::errc()) {
    return 0;
  }
  return parsed;
}

std::int64_t parse_int64_or_zero(std::string_view value) {
  std::int64_t parsed = 0;
  const auto result = std::from_chars(value.data(), value.data() + value.size(), parsed);
  if (result.ec != std::errc()) {
    return 0;
  }
  return parsed;
}

bool parse_boolish(std::string_view value) {
  return value == "1" || value == "true" || value == "TRUE" || value == "yes" || value == "YES";
}

std::string stable_hash(std::string_view payload) {
  return util::sha256_like_hex(payload);
}

std::string join_event_ids(const std::vector<std::string>& event_ids) {
  std::ostringstream out;
  for (std::size_t i = 0; i < event_ids.size(); ++i) {
    if (i > 0) {
      out << ',';
    }
    out << event_ids[i];
  }
  return out.str();
}

std::vector<std::string> split_event_ids(std::string_view text) {
  std::vector<std::string> ids;
  std::string current;
  for (char c : text) {
    if (c == ',') {
      if (!current.empty()) {
        ids.push_back(current);
        current.clear();
      }
      continue;
    }
    current.push_back(c);
  }
  if (!current.empty()) {
    ids.push_back(current);
  }
  return ids;
}

bool is_post_kind(EventKind kind) {
  return kind == EventKind::RecipeCreated || kind == EventKind::ThreadCreated ||
         kind == EventKind::ReplyCreated || kind == EventKind::ReviewAdded ||
         kind == EventKind::ThumbsUpAdded;
}

bool is_moderation_event(EventKind kind) {
  return kind == EventKind::ModeratorAdded || kind == EventKind::ModeratorRemoved ||
         kind == EventKind::ContentFlagged || kind == EventKind::ContentHidden ||
         kind == EventKind::ContentUnhidden || kind == EventKind::CoreTopicPinned ||
         kind == EventKind::CoreTopicUnpinned || kind == EventKind::PolicyUpdated;
}

std::string compute_merkle_root(std::vector<std::string> leaves) {
  if (leaves.empty()) {
    return stable_hash("merkle-empty");
  }

  while (leaves.size() > 1) {
    if ((leaves.size() % 2U) != 0U) {
      leaves.push_back(leaves.back());
    }

    std::vector<std::string> next;
    next.reserve(leaves.size() / 2U);
    for (std::size_t i = 0; i < leaves.size(); i += 2U) {
      next.push_back(stable_hash(leaves[i] + "|" + leaves[i + 1U]));
    }
    leaves = std::move(next);
  }

  return leaves.front();
}

std::string serialize_block_line(const Store::BlockRecord& block) {
  std::ostringstream out;
  out << block.index << '\t' << block.opened_unix << '\t' << (block.reserved ? 1 : 0) << '\t'
      << (block.confirmed ? 1 : 0) << '\t' << (block.backfilled ? 1 : 0) << '\t' << block.prev_hash
      << '\t' << block.merkle_root << '\t' << block.content_hash << '\t' << block.block_hash << '\t'
      << to_hex(block.psz_timestamp) << '\t' << to_hex(join_event_ids(block.event_ids)) << '\n';
  return out.str();
}

bool parse_uint64(std::string_view text, std::uint64_t& out) {
  std::uint64_t value = 0;
  const auto result = std::from_chars(text.data(), text.data() + text.size(), value);
  if (result.ec != std::errc()) {
    return false;
  }
  out = value;
  return true;
}

bool parse_block_line(std::string_view line, Store::BlockRecord& out) {
  std::vector<std::string_view> fields;
  fields.reserve(12);
  std::size_t start = 0;
  for (std::size_t i = 0; i <= line.size(); ++i) {
    if (i == line.size() || line[i] == '\t') {
      fields.push_back(line.substr(start, i - start));
      start = i + 1U;
    }
  }

  if (fields.size() != 9 && fields.size() != 11) {
    return false;
  }

  if (!parse_uint64(fields[0], out.index)) {
    return false;
  }
  if (!parse_int64(fields[1], out.opened_unix)) {
    return false;
  }
  out.reserved = parse_boolish(fields[2]);
  out.confirmed = parse_boolish(fields[3]);
  out.backfilled = parse_boolish(fields[4]);
  out.prev_hash = std::string{fields[5]};
  if (fields.size() == 11) {
    out.merkle_root = std::string{fields[6]};
    out.content_hash = std::string{fields[7]};
    out.block_hash = std::string{fields[8]};
    out.psz_timestamp = from_hex(fields[9]);
  } else {
    out.merkle_root = std::string{fields[6]};
    out.content_hash = std::string{fields[6]};
    out.block_hash = std::string{fields[7]};
    out.psz_timestamp.clear();
  }

  const std::string encoded_event_ids = from_hex(fields.back());
  out.event_ids = split_event_ids(encoded_event_ids);
  return true;
}

}  // namespace

Result Store::open(std::string_view app_data_dir, std::string_view vault_key) {
  (void)vault_key;
  app_data_dir_ = std::string{app_data_dir};

  std::error_code ec;
  std::filesystem::create_directories(app_data_dir_, ec);
  if (ec) {
    return Result::failure("Failed to create store directory: " + ec.message());
  }

  event_log_path_ = (std::filesystem::path{app_data_dir_} / std::string{kEventLogFile}).string();
  block_log_path_ = (std::filesystem::path{app_data_dir_} / std::string{kBlockLogFile}).string();
  invalid_event_log_path_ = (std::filesystem::path{app_data_dir_} / std::string{kInvalidEventLogFile}).string();
  snapshot_path_ = (std::filesystem::path{app_data_dir_} / std::string{kSnapshotFile}).string();
  checkpoints_path_ = (std::filesystem::path{app_data_dir_} / std::string{kCheckpointsFile}).string();
  invalid_event_drop_count_ = 0;
  recovered_from_corruption_ = false;
  checkpoint_count_ = 0;

  const Result events_result = load_event_log();
  if (!events_result.ok) {
    return events_result;
  }

  const Result blocks_result = load_block_log();
  if (!blocks_result.ok) {
    return blocks_result;
  }

  const std::int64_t now = util::unix_timestamp_now();
  ensure_genesis_block(now);
  assign_unassigned_events_to_blocks();
  ensure_block_slots_until(now);
  recompute_block_hashes();
  const Result materialized = materialize_views();
  if (!materialized.ok) {
    return materialized;
  }
  const Result persist_blocks = persist_block_log();
  if (!persist_blocks.ok) {
    return persist_blocks;
  }
  const Result checkpoint_result = persist_checkpoints();
  if (!checkpoint_result.ok) {
    return checkpoint_result;
  }
  const Result snapshot_result = persist_snapshot();
  if (!snapshot_result.ok) {
    return snapshot_result;
  }

  backtest_ok_ = true;
  backtest_details_ = "Backtest pending first scheduled run.";
  last_backtest_unix_ = 0;
  return Result::success("Store opened with block timeline.");
}

void Store::set_block_timing(std::uint64_t block_interval_seconds) {
  block_interval_seconds_ = block_interval_seconds == 0 ? 150 : block_interval_seconds;
}

void Store::set_genesis_psz_timestamp(std::string_view psz_timestamp) {
  genesis_psz_timestamp_ = std::string{psz_timestamp};
}

void Store::set_block_reward_units(std::int64_t units) {
  block_reward_units_ = units <= 0 ? 115 : units;
  max_token_supply_units_ = 69359946;
  per_block_subsidy_decay_fraction_ = 0.0000016435998841934918L;
  min_subsidy_units_ = 1;
  difficulty_adjustment_interval_blocks_ = 864;
}

void Store::set_chain_identity(std::string_view chain_id, std::string_view network_id) {
  if (!chain_id.empty()) {
    chain_id_ = std::string{chain_id};
  }
  if (!network_id.empty()) {
    network_id_ = std::string{network_id};
  }
}

void Store::set_genesis_hashes(std::string_view merkle_root, std::string_view block_hash) {
  hardcoded_genesis_merkle_root_ = std::string{merkle_root};
  hardcoded_genesis_block_hash_ = std::string{block_hash};
}

void Store::set_chain_policy(const ChainPolicy& policy) {
  chain_policy_ = policy;
  if (chain_policy_.confirmation_threshold == 0) {
    chain_policy_.confirmation_threshold = 1;
  }
  if (chain_policy_.max_reorg_depth == 0) {
    chain_policy_.max_reorg_depth = 1;
  }
  if (chain_policy_.checkpoint_interval_blocks == 0) {
    chain_policy_.checkpoint_interval_blocks = 288;
  }
  if (chain_policy_.checkpoint_confirmations == 0) {
    chain_policy_.checkpoint_confirmations = 24;
  }
  if (chain_policy_.fork_choice_rule.empty()) {
    chain_policy_.fork_choice_rule = "most-work-then-oldest";
  }
}

void Store::set_validation_limits(const ValidationLimits& limits) {
  validation_limits_ = limits;
  validation_limits_.max_block_events = std::max<std::size_t>(1, validation_limits_.max_block_events);
  validation_limits_.max_block_bytes = std::max<std::size_t>(1024, validation_limits_.max_block_bytes);
  validation_limits_.max_event_bytes = std::max<std::size_t>(256, validation_limits_.max_event_bytes);
  validation_limits_.max_future_drift_seconds =
      std::max<std::int64_t>(0, validation_limits_.max_future_drift_seconds);
  validation_limits_.max_past_drift_seconds =
      std::max<std::int64_t>(0, validation_limits_.max_past_drift_seconds);
}

void Store::set_moderation_policy(const ModerationPolicy& policy) {
  moderation_policy_ = policy;
  moderation_policy_.min_confirmations_for_enforcement =
      std::max<std::uint64_t>(1, moderation_policy_.min_confirmations_for_enforcement);
  moderation_policy_.max_flags_before_auto_hide =
      std::max<std::size_t>(1, moderation_policy_.max_flags_before_auto_hide);
  if (moderation_policy_.role_model.empty()) {
    moderation_policy_.role_model = "single-signer";
  }

  std::unordered_set<std::string> unique;
  std::vector<std::string> sanitized;
  sanitized.reserve(moderation_policy_.moderator_cids.size());
  for (const auto& cid : moderation_policy_.moderator_cids) {
    const std::string trimmed = util::trim_copy(cid);
    if (trimmed.empty()) {
      continue;
    }
    if (unique.insert(trimmed).second) {
      sanitized.push_back(trimmed);
    }
  }
  std::ranges::sort(sanitized);
  moderation_policy_.moderator_cids = std::move(sanitized);
}

void Store::set_state_options(std::uint32_t blockdata_format_version, bool enable_snapshots,
                              std::uint64_t snapshot_interval_blocks, bool enable_pruning,
                              std::uint64_t prune_keep_recent_blocks) {
  blockdata_format_version_ = blockdata_format_version == 0 ? 2 : blockdata_format_version;
  enable_snapshots_ = enable_snapshots;
  snapshot_interval_blocks_ = snapshot_interval_blocks == 0 ? 128 : snapshot_interval_blocks;
  enable_pruning_ = enable_pruning;
  prune_keep_recent_blocks_ = prune_keep_recent_blocks == 0 ? 4096 : prune_keep_recent_blocks;
}

Result Store::append_event(const EventEnvelope& event) {
  if (event.event_id.empty()) {
    record_invalid_event("", "append_event failed: missing event id.");
    return Result::failure("append_event failed: missing event id.");
  }
  if (event.payload.empty()) {
    record_invalid_event(event.event_id, "append_event failed: missing payload.");
    return Result::failure("append_event failed: missing payload.");
  }
  if (event.signature.empty()) {
    record_invalid_event(event.event_id, "append_event failed: missing signature.");
    return Result::failure("append_event failed: missing signature.");
  }
  if (event.payload.size() > validation_limits_.max_event_bytes) {
    record_invalid_event(event.event_id, "append_event failed: payload exceeds max_event_bytes.");
    return Result::failure("append_event failed: payload exceeds max_event_bytes.");
  }

  const std::int64_t now = util::unix_timestamp_now();
  if (event.unix_ts > (now + validation_limits_.max_future_drift_seconds)) {
    record_invalid_event(event.event_id, "append_event failed: timestamp exceeds future drift limit.");
    return Result::failure("append_event failed: timestamp exceeds future drift limit.");
  }
  if (event.unix_ts < (now - validation_limits_.max_past_drift_seconds)) {
    record_invalid_event(event.event_id, "append_event failed: timestamp exceeds past drift limit.");
    return Result::failure("append_event failed: timestamp exceeds past drift limit.");
  }

  if (has_event(event.event_id)) {
    return Result::success("Event already exists (idempotent append).");
  }

  events_.push_back(event);
  const Result persist = persist_event(event);
  if (!persist.ok) {
    return persist;
  }

  assign_unassigned_events_to_blocks();
  ensure_block_slots_until(util::unix_timestamp_now());
  recompute_block_hashes();
  const Result block_persist = persist_block_log();
  if (!block_persist.ok) {
    return block_persist;
  }
  const Result materialized = materialize_views();
  if (!materialized.ok) {
    return materialized;
  }
  const Result checkpoints = persist_checkpoints();
  if (!checkpoints.ok) {
    return checkpoints;
  }
  return persist_snapshot();
}

bool Store::has_event(std::string_view event_id) const {
  return std::ranges::any_of(events_, [event_id](const EventEnvelope& event) {
    return event.event_id == event_id;
  });
}

Result Store::materialize_views() {
  recipes_.clear();
  threads_.clear();
  replies_by_thread_.clear();
  review_totals_.clear();
  thumbs_up_totals_.clear();
  reward_balances_.clear();
  claimed_blocks_.clear();
  transfer_nonce_by_cid_.clear();
  invalid_economic_events_.clear();
  invalid_moderation_events_.clear();
  moderators_.clear();
  moderation_flag_counts_.clear();
  moderation_hidden_objects_.clear();
  moderation_auto_hidden_objects_.clear();
  moderation_core_topic_overrides_.clear();
  issued_reward_total_ = 0;
  burned_fee_total_ = 0;

  const auto parse_post_value = [](const std::unordered_map<std::string, std::string>& payload) {
    if (payload.contains("post_value")) {
      return parse_int64_or_zero(payload.at("post_value"));
    }
    if (payload.contains("value_units")) {
      return parse_int64_or_zero(payload.at("value_units"));
    }
    if (payload.contains("value")) {
      return parse_int64_or_zero(payload.at("value"));
    }
    return std::int64_t{0};
  };

  std::vector<const EventEnvelope*> ordered_events;
  ordered_events.reserve(events_.size());
  for (const auto& event : events_) {
    ordered_events.push_back(&event);
  }
  const auto block_index_for = [this](std::string_view event_id) {
    const auto it = event_to_block_.find(std::string{event_id});
    if (it == event_to_block_.end()) {
      return std::numeric_limits<std::size_t>::max();
    }
    return it->second;
  };
  const auto economic_priority = [](EventKind kind) {
    switch (kind) {
      case EventKind::BlockRewardClaimed:
        return 0;
      case EventKind::RewardTransferred:
        return 1;
      default:
        return 2;
    }
  };
  std::ranges::sort(ordered_events, [&block_index_for, &economic_priority](const EventEnvelope* lhs,
                                                                            const EventEnvelope* rhs) {
    const std::size_t lhs_block = block_index_for(lhs->event_id);
    const std::size_t rhs_block = block_index_for(rhs->event_id);
    if (lhs_block != rhs_block) {
      return lhs_block < rhs_block;
    }
    if (lhs->unix_ts != rhs->unix_ts) {
      return lhs->unix_ts < rhs->unix_ts;
    }
    const int lhs_priority = economic_priority(lhs->kind);
    const int rhs_priority = economic_priority(rhs->kind);
    if (lhs_priority != rhs_priority) {
      return lhs_priority < rhs_priority;
    }
    return lhs->event_id < rhs->event_id;
  });

  const std::optional<std::uint64_t> confirmed_tip = latest_confirmed_block_index();
  for (const auto& cid : moderation_policy_.moderator_cids) {
    const std::string trimmed = util::trim_copy(cid);
    if (!trimmed.empty()) {
      moderators_.insert(trimmed);
    }
  }

  const auto event_confirmations = [this, &confirmed_tip](const EventEnvelope& event) {
    const auto it = event_to_block_.find(event.event_id);
    if (it == event_to_block_.end() || !confirmed_tip.has_value()) {
      return std::uint64_t{0};
    }
    const std::size_t block_pos = it->second;
    if (block_pos >= blocks_.size()) {
      return std::uint64_t{0};
    }
    const std::uint64_t block_index = blocks_[block_pos].index;
    if (*confirmed_tip < block_index) {
      return std::uint64_t{0};
    }
    return (*confirmed_tip - block_index) + 1U;
  };

  const auto moderation_event_is_effective = [this, &event_confirmations](const EventEnvelope& event) {
    if (!moderation_policy_.moderation_enabled) {
      return false;
    }
    if (!moderation_policy_.require_finality_for_actions) {
      return true;
    }
    return event_confirmations(event) >= moderation_policy_.min_confirmations_for_enforcement;
  };

  std::int64_t issued_so_far = 0;
  for (const EventEnvelope* event_ptr : ordered_events) {
    const EventEnvelope& event = *event_ptr;
    const auto payload = util::parse_canonical_map(event.payload);

    if (event.kind == EventKind::BlockRewardClaimed) {
      std::uint64_t block_index = 0;
      if (!payload.contains("block_index") || !parse_uint64(payload.at("block_index"), block_index)) {
        invalid_economic_events_[event.event_id] = "Reward claim missing valid block_index.";
        continue;
      }
      const std::int64_t reward =
          payload.contains("reward") ? parse_int64_or_zero(payload.at("reward")) : std::int64_t{0};
      const std::int64_t expected_reward = expected_claim_reward_for_block(block_index, issued_so_far);
      if (reward <= 0 || reward != expected_reward) {
        invalid_economic_events_[event.event_id] = "Reward claim amount does not match deterministic schedule.";
        continue;
      }
      const auto block_it = std::ranges::find_if(blocks_, [block_index](const BlockRecord& block) {
        return block.index == block_index;
      });
      if (block_it == blocks_.end() || !block_it->confirmed || !confirmed_tip.has_value() ||
          block_index > *confirmed_tip) {
        invalid_economic_events_[event.event_id] = "Reward claim references an unconfirmed block.";
        continue;
      }

      if (claimed_blocks_.contains(block_index)) {
        invalid_economic_events_[event.event_id] = "Duplicate reward claim for block.";
        continue;
      }

      const int difficulty = payload.contains("pow_difficulty")
                                 ? static_cast<int>(parse_int64_or_zero(payload.at("pow_difficulty")))
                                 : pow_difficulty_nibbles_;
      const std::string pow_nonce = payload.contains("pow_nonce") ? payload.at("pow_nonce") : "";
      const std::string pow_hash = payload.contains("pow_hash") ? payload.at("pow_hash") : "";
      const std::string pow_material = payload.contains("pow_material") ? payload.at("pow_material") : "";
      const std::string expected_pow_hash = stable_hash(pow_material + "|" + pow_nonce);
      if (pow_hash.empty() || pow_hash != expected_pow_hash ||
          !util::has_leading_zero_nibbles(pow_hash, difficulty)) {
        invalid_economic_events_[event.event_id] = "Reward claim PoW is invalid.";
        continue;
      }

      const std::string expected_witness = stable_hash(event.author_cid + "|" + std::to_string(block_index) + "|" +
                                                       std::to_string(reward) + "|" + pow_hash);
      if (!payload.contains("witness_root") || payload.at("witness_root") != expected_witness) {
        invalid_economic_events_[event.event_id] = "Reward claim witness is invalid.";
        continue;
      }

      claimed_blocks_[block_index] = event.author_cid;
      reward_balances_[event.author_cid] += reward;
      issued_so_far += reward;
      issued_reward_total_ = issued_so_far;
      continue;
    }

    if (event.kind == EventKind::RewardTransferred) {
      const std::string to_cid = payload.contains("to_cid") ? payload.at("to_cid") : "";
      const std::int64_t amount =
          payload.contains("amount") ? parse_int64_or_zero(payload.at("amount")) : std::int64_t{0};
      const std::int64_t fee = payload.contains("fee") ? parse_int64_or_zero(payload.at("fee")) : 0;
      std::uint64_t nonce = 0;
      const bool nonce_ok = payload.contains("nonce") && parse_uint64(payload.at("nonce"), nonce);
      if (to_cid.empty() || amount <= 0 || fee < 0 || !nonce_ok || nonce == 0) {
        invalid_economic_events_[event.event_id] = "Reward transfer has invalid target or amount.";
        continue;
      }
      const std::uint64_t expected_nonce = transfer_nonce_by_cid_[event.author_cid] + 1U;
      if (nonce != expected_nonce) {
        invalid_economic_events_[event.event_id] = "Reward transfer nonce is invalid.";
        continue;
      }
      const std::int64_t expected_fee = transfer_burn_fee_internal(amount);
      if (fee != expected_fee) {
        invalid_economic_events_[event.event_id] = "Reward transfer fee is invalid.";
        continue;
      }

      const std::string expected_witness =
          stable_hash(event.author_cid + "|" + to_cid + "|" + std::to_string(amount) + "|" +
                      std::to_string(fee) + "|" + std::to_string(nonce));
      if (!payload.contains("witness_root") || payload.at("witness_root") != expected_witness) {
        invalid_economic_events_[event.event_id] = "Reward transfer witness is invalid.";
        continue;
      }

      if (reward_balances_[event.author_cid] < (amount + fee)) {
        invalid_economic_events_[event.event_id] = "Reward transfer exceeds sender balance.";
        continue;
      }
      reward_balances_[event.author_cid] -= (amount + fee);
      reward_balances_[to_cid] += amount;
      burned_fee_total_ += fee;
      transfer_nonce_by_cid_[event.author_cid] = nonce;
      continue;
    }

    if (is_post_kind(event.kind)) {
      const std::int64_t post_value = parse_post_value(payload);
      if (post_value < 0) {
        invalid_economic_events_[event.event_id] = "Post value cannot be negative.";
        continue;
      }
      if (post_value > 0) {
        if (reward_balances_[event.author_cid] < post_value) {
          invalid_economic_events_[event.event_id] = "Insufficient balance for post value spend.";
          continue;
        }
        reward_balances_[event.author_cid] -= post_value;
        burned_fee_total_ += post_value;
      }
    }
  }

  for (const EventEnvelope* event_ptr : ordered_events) {
    const EventEnvelope& event = *event_ptr;
    if (invalid_economic_events_.contains(event.event_id) && is_post_kind(event.kind)) {
      continue;
    }

    const auto payload = util::parse_canonical_map(event.payload);

    if (is_moderation_event(event.kind)) {
      if (!moderation_event_is_effective(event)) {
        continue;
      }

      const auto moderator_required = [](EventKind kind) {
        return kind == EventKind::ModeratorAdded || kind == EventKind::ModeratorRemoved ||
               kind == EventKind::ContentHidden || kind == EventKind::ContentUnhidden ||
               kind == EventKind::CoreTopicPinned || kind == EventKind::CoreTopicUnpinned ||
               kind == EventKind::PolicyUpdated;
      };
      const auto object_id_from_payload = [&payload, &event]() -> std::string {
        if (payload.contains("object_id")) {
          return payload.at("object_id");
        }
        if (payload.contains("recipe_id")) {
          return payload.at("recipe_id");
        }
        if (payload.contains("thread_id")) {
          return payload.at("thread_id");
        }
        if (payload.contains("reply_id")) {
          return payload.at("reply_id");
        }
        if (payload.contains("target_id")) {
          return payload.at("target_id");
        }
        return event.event_id;
      };

      if (moderator_required(event.kind) && !moderators_.contains(event.author_cid)) {
        invalid_moderation_events_[event.event_id] =
            "Moderator authority required for moderation event by " + event.author_cid + ".";
        continue;
      }

      switch (event.kind) {
        case EventKind::ModeratorAdded: {
          const std::string target_cid = payload.contains("target_cid") ? util::trim_copy(payload.at("target_cid")) : "";
          if (target_cid.empty()) {
            invalid_moderation_events_[event.event_id] = "ModeratorAdded missing target_cid.";
            break;
          }
          moderators_.insert(target_cid);
          break;
        }
        case EventKind::ModeratorRemoved: {
          const std::string target_cid = payload.contains("target_cid") ? util::trim_copy(payload.at("target_cid")) : "";
          if (target_cid.empty()) {
            invalid_moderation_events_[event.event_id] = "ModeratorRemoved missing target_cid.";
            break;
          }
          if (!moderators_.contains(target_cid)) {
            invalid_moderation_events_[event.event_id] = "ModeratorRemoved references unknown target_cid.";
            break;
          }
          if (moderators_.size() <= 1U) {
            invalid_moderation_events_[event.event_id] = "ModeratorRemoved would leave community without moderators.";
            break;
          }
          moderators_.erase(target_cid);
          break;
        }
        case EventKind::ContentFlagged: {
          const std::string object_id = object_id_from_payload();
          if (object_id.empty()) {
            invalid_moderation_events_[event.event_id] = "ContentFlagged missing object_id.";
            break;
          }
          const std::size_t new_count = ++moderation_flag_counts_[object_id];
          if (new_count >= moderation_policy_.max_flags_before_auto_hide) {
            moderation_hidden_objects_.insert(object_id);
            moderation_auto_hidden_objects_.insert(object_id);
          }
          break;
        }
        case EventKind::ContentHidden: {
          const std::string object_id = object_id_from_payload();
          if (object_id.empty()) {
            invalid_moderation_events_[event.event_id] = "ContentHidden missing object_id.";
            break;
          }
          moderation_hidden_objects_.insert(object_id);
          moderation_auto_hidden_objects_.erase(object_id);
          break;
        }
        case EventKind::ContentUnhidden: {
          const std::string object_id = object_id_from_payload();
          if (object_id.empty()) {
            invalid_moderation_events_[event.event_id] = "ContentUnhidden missing object_id.";
            break;
          }
          moderation_hidden_objects_.erase(object_id);
          moderation_auto_hidden_objects_.erase(object_id);
          break;
        }
        case EventKind::CoreTopicPinned: {
          const std::string recipe_id = payload.contains("recipe_id") ? payload.at("recipe_id") : "";
          if (recipe_id.empty()) {
            invalid_moderation_events_[event.event_id] = "CoreTopicPinned missing recipe_id.";
            break;
          }
          moderation_core_topic_overrides_[recipe_id] = true;
          break;
        }
        case EventKind::CoreTopicUnpinned: {
          const std::string recipe_id = payload.contains("recipe_id") ? payload.at("recipe_id") : "";
          if (recipe_id.empty()) {
            invalid_moderation_events_[event.event_id] = "CoreTopicUnpinned missing recipe_id.";
            break;
          }
          moderation_core_topic_overrides_[recipe_id] = false;
          break;
        }
        case EventKind::PolicyUpdated: {
          if (payload.contains("max_flags_before_auto_hide")) {
            const std::int64_t parsed = parse_int64_or_zero(payload.at("max_flags_before_auto_hide"));
            if (parsed > 0) {
              moderation_policy_.max_flags_before_auto_hide = static_cast<std::size_t>(parsed);
            }
          }
          if (payload.contains("min_confirmations_for_enforcement")) {
            const std::int64_t parsed = parse_int64_or_zero(payload.at("min_confirmations_for_enforcement"));
            if (parsed > 0) {
              moderation_policy_.min_confirmations_for_enforcement = static_cast<std::uint64_t>(parsed);
            }
          }
          if (payload.contains("require_finality_for_actions")) {
            moderation_policy_.require_finality_for_actions = parse_boolish(payload.at("require_finality_for_actions"));
          }
          break;
        }
        default:
          break;
      }
      continue;
    }

    switch (event.kind) {
      case EventKind::RecipeCreated: {
        RecipeSummary summary;
        summary.recipe_id = payload.contains("recipe_id") ? payload.at("recipe_id") : event.event_id;
        summary.source_event_id = event.event_id;
        summary.title = payload.contains("title") ? payload.at("title") : "Untitled recipe";
        summary.category = payload.contains("category") ? payload.at("category") : "General";
        summary.author_cid = event.author_cid;
        summary.updated_unix = event.unix_ts;
        summary.core_topic = (payload.contains("core_topic") && parse_boolish(payload.at("core_topic"))) ||
                             (payload.contains("moderator_core") &&
                              parse_boolish(payload.at("moderator_core")));
        summary.menu_segment =
            payload.contains("menu_segment")
                ? payload.at("menu_segment")
                : (summary.core_topic ? "core-menu" : "community-post");
        summary.value_units = parse_post_value(payload);

        const auto review_it = review_totals_.find(summary.recipe_id);
        if (review_it != review_totals_.end() && review_it->second.second > 0) {
          summary.review_count = review_it->second.second;
          summary.average_rating =
              static_cast<double>(review_it->second.first) / static_cast<double>(review_it->second.second);
        }
        const auto thumbs_it = thumbs_up_totals_.find(summary.recipe_id);
        if (thumbs_it != thumbs_up_totals_.end()) {
          summary.thumbs_up_count = thumbs_it->second;
        }

        recipes_[summary.recipe_id] = summary;
        break;
      }

      case EventKind::ThreadCreated: {
        ThreadSummary thread;
        thread.thread_id = payload.contains("thread_id") ? payload.at("thread_id") : event.event_id;
        thread.source_event_id = event.event_id;
        thread.recipe_id = payload.contains("recipe_id") ? payload.at("recipe_id") : "";
        thread.title = payload.contains("title") ? payload.at("title") : "Untitled thread";
        thread.author_cid = event.author_cid;
        thread.updated_unix = event.unix_ts;
        thread.value_units = parse_post_value(payload);

        threads_[thread.thread_id] = thread;
        break;
      }

      case EventKind::ReplyCreated: {
        ReplySummary reply;
        reply.reply_id = payload.contains("reply_id") ? payload.at("reply_id") : event.event_id;
        reply.source_event_id = event.event_id;
        reply.thread_id = payload.contains("thread_id") ? payload.at("thread_id") : "";
        reply.author_cid = event.author_cid;
        reply.markdown = payload.contains("markdown") ? payload.at("markdown") : "";
        reply.updated_unix = event.unix_ts;
        reply.value_units = parse_post_value(payload);

        if (!reply.thread_id.empty()) {
          replies_by_thread_[reply.thread_id].push_back(reply);
        }
        break;
      }

      case EventKind::ReviewAdded: {
        const std::string recipe_id = payload.contains("recipe_id") ? payload.at("recipe_id") : "";
        if (!recipe_id.empty()) {
          auto& totals = review_totals_[recipe_id];
          totals.first += parse_int_or_zero(payload.contains("rating") ? payload.at("rating") : "0");
          totals.second += 1;
        }
        break;
      }

      case EventKind::ThumbsUpAdded: {
        const std::string recipe_id = payload.contains("recipe_id") ? payload.at("recipe_id") : "";
        if (!recipe_id.empty()) {
          thumbs_up_totals_[recipe_id] += 1;
        }
        break;
      }

      case EventKind::BlockRewardClaimed:
      case EventKind::RewardTransferred:
      case EventKind::ProfileUpdated:
      case EventKind::KeyRotated:
      case EventKind::ModeratorAdded:
      case EventKind::ModeratorRemoved:
      case EventKind::ContentFlagged:
      case EventKind::ContentHidden:
      case EventKind::ContentUnhidden:
      case EventKind::CoreTopicPinned:
      case EventKind::CoreTopicUnpinned:
      case EventKind::PolicyUpdated:
        break;
    }
  }

  for (auto& [recipe_id, summary] : recipes_) {
    const auto review_it = review_totals_.find(recipe_id);
    if (review_it != review_totals_.end() && review_it->second.second > 0) {
      summary.review_count = review_it->second.second;
      summary.average_rating =
          static_cast<double>(review_it->second.first) / static_cast<double>(review_it->second.second);
    }

    const auto thumbs_it = thumbs_up_totals_.find(recipe_id);
    if (thumbs_it != thumbs_up_totals_.end()) {
      summary.thumbs_up_count = thumbs_it->second;
    }
  }

  for (auto& [thread_id, replies] : replies_by_thread_) {
    const auto thread_it = threads_.find(thread_id);
    if (thread_it != threads_.end()) {
      thread_it->second.reply_count = static_cast<int>(replies.size());
    }
  }

  for (const auto& [recipe_id, core_topic] : moderation_core_topic_overrides_) {
    const auto recipe_it = recipes_.find(recipe_id);
    if (recipe_it == recipes_.end()) {
      continue;
    }
    recipe_it->second.core_topic = core_topic;
    recipe_it->second.menu_segment = core_topic ? "core-menu" : "community-post";
  }

  std::vector<std::string> threads_to_remove;
  for (const auto& [thread_id, thread] : threads_) {
    if (moderation_hidden_objects_.contains(thread_id) || moderation_hidden_objects_.contains(thread.recipe_id)) {
      threads_to_remove.push_back(thread_id);
    }
  }
  for (const auto& thread_id : threads_to_remove) {
    threads_.erase(thread_id);
    replies_by_thread_.erase(thread_id);
  }

  for (auto it = replies_by_thread_.begin(); it != replies_by_thread_.end();) {
    auto& replies = it->second;
    replies.erase(std::remove_if(replies.begin(), replies.end(), [this](const ReplySummary& reply) {
                    return moderation_hidden_objects_.contains(reply.reply_id) ||
                           moderation_hidden_objects_.contains(reply.thread_id);
                  }),
                  replies.end());
    if (replies.empty()) {
      it = replies_by_thread_.erase(it);
    } else {
      ++it;
    }
  }

  for (auto it = recipes_.begin(); it != recipes_.end();) {
    if (moderation_hidden_objects_.contains(it->first)) {
      it = recipes_.erase(it);
      continue;
    }
    ++it;
  }

  for (auto& [thread_id, thread] : threads_) {
    const auto replies_it = replies_by_thread_.find(thread_id);
    thread.reply_count = (replies_it != replies_by_thread_.end()) ? static_cast<int>(replies_it->second.size()) : 0;
  }

  apply_confirmation_metrics();
  return Result::success("Materialized view updated.");
}

Result Store::routine_block_check(std::int64_t now_unix) {
  ensure_genesis_block(now_unix);
  ensure_block_slots_until(now_unix);
  assign_unassigned_events_to_blocks();

  bool updated = false;
  for (auto& block : blocks_) {
    if (!block.confirmed && (now_unix - block.opened_unix) >= static_cast<std::int64_t>(block_interval_seconds_)) {
      block.confirmed = true;
      updated = true;
    }
  }

  if (updated) {
    recompute_block_hashes();
  } else {
    // Hashes may still need recompute if new events were assigned.
    recompute_block_hashes();
  }

  apply_confirmation_metrics();
  prune_blocks_if_needed();
  const Result block_persist = persist_block_log();
  if (!block_persist.ok) {
    return block_persist;
  }
  const Result checkpoints = persist_checkpoints();
  if (!checkpoints.ok) {
    return checkpoints;
  }
  return persist_snapshot();
}

Result Store::backtest_validate(const std::function<std::string(std::string_view)>& content_id_fn,
                                std::string_view expected_community_id) {
  std::size_t issues = 0;
  std::ostringstream details;
  std::unordered_map<std::string, std::string> event_payload_hash;

  for (const auto& event : events_) {
    if (event.event_id != content_id_fn(event.payload)) {
      ++issues;
      details << "Event ID mismatch: " << event.event_id << "\n";
    }
    if (event.payload.size() > validation_limits_.max_event_bytes) {
      ++issues;
      details << "Event payload exceeds max_event_bytes: " << event.event_id << "\n";
    }
    const std::int64_t now = util::unix_timestamp_now();
    if (event.unix_ts > now + validation_limits_.max_future_drift_seconds) {
      ++issues;
      details << "Event timestamp exceeds future drift: " << event.event_id << "\n";
    }
    if (event.unix_ts < now - validation_limits_.max_past_drift_seconds) {
      ++issues;
      details << "Event timestamp exceeds past drift: " << event.event_id << "\n";
    }

    const auto payload = util::parse_canonical_map(event.payload);
    if (!expected_community_id.empty() && payload.contains("community_id") &&
        payload.at("community_id") != expected_community_id) {
      ++issues;
      details << "Community mismatch in event: " << event.event_id << "\n";
    }
    if (payload.contains("chain_id") && payload.at("chain_id") != chain_id_) {
      ++issues;
      details << "Chain ID mismatch in event: " << event.event_id << "\n";
    }
    if (payload.contains("network_id") && payload.at("network_id") != network_id_) {
      ++issues;
      details << "Network ID mismatch in event: " << event.event_id << "\n";
    }

    switch (event.kind) {
      case EventKind::RecipeCreated:
        if (!payload.contains("recipe_id") || payload.at("recipe_id").empty()) {
          ++issues;
          details << "Recipe event missing recipe_id: " << event.event_id << "\n";
        }
        break;
      case EventKind::ThreadCreated:
        if (!payload.contains("thread_id") || !payload.contains("recipe_id") ||
            payload.at("thread_id").empty() || payload.at("recipe_id").empty()) {
          ++issues;
          details << "Thread event missing IDs: " << event.event_id << "\n";
        }
        break;
      case EventKind::ReplyCreated:
        if (!payload.contains("reply_id") || !payload.contains("thread_id") ||
            payload.at("reply_id").empty() || payload.at("thread_id").empty()) {
          ++issues;
          details << "Reply event missing IDs: " << event.event_id << "\n";
        }
        break;
      case EventKind::BlockRewardClaimed: {
        std::uint64_t block_index = 0;
        const std::int64_t reward = payload.contains("reward") ? parse_int64_or_zero(payload.at("reward")) : 0;
        if (!payload.contains("block_index") || !parse_uint64(payload.at("block_index"), block_index)) {
          ++issues;
          details << "Reward claim missing block_index: " << event.event_id << "\n";
        }
        if (reward <= 0) {
          ++issues;
          details << "Reward claim missing positive reward: " << event.event_id << "\n";
        }
        break;
      }
      case EventKind::RewardTransferred: {
        const std::int64_t amount = payload.contains("amount") ? parse_int64_or_zero(payload.at("amount")) : 0;
        if (!payload.contains("to_cid") || payload.at("to_cid").empty() || amount <= 0) {
          ++issues;
          details << "Reward transfer missing target or amount: " << event.event_id << "\n";
        }
        break;
      }
      case EventKind::ModeratorAdded:
      case EventKind::ModeratorRemoved:
        if (!payload.contains("target_cid") || payload.at("target_cid").empty()) {
          ++issues;
          details << "Moderator event missing target_cid: " << event.event_id << "\n";
        }
        break;
      case EventKind::ContentFlagged:
      case EventKind::ContentHidden:
      case EventKind::ContentUnhidden:
        if ((!payload.contains("object_id") || payload.at("object_id").empty()) &&
            (!payload.contains("recipe_id") || payload.at("recipe_id").empty()) &&
            (!payload.contains("thread_id") || payload.at("thread_id").empty()) &&
            (!payload.contains("reply_id") || payload.at("reply_id").empty())) {
          ++issues;
          details << "Content moderation event missing object_id: " << event.event_id << "\n";
        }
        break;
      case EventKind::CoreTopicPinned:
      case EventKind::CoreTopicUnpinned:
        if (!payload.contains("recipe_id") || payload.at("recipe_id").empty()) {
          ++issues;
          details << "Core topic moderation event missing recipe_id: " << event.event_id << "\n";
        }
        break;
      case EventKind::PolicyUpdated:
        break;
      default:
        break;
    }

    if (is_post_kind(event.kind)) {
      const std::int64_t post_value = payload.contains("post_value") ? parse_int64_or_zero(payload.at("post_value"))
                                                                      : std::int64_t{0};
      if (post_value < 0) {
        ++issues;
        details << "Post value is negative: " << event.event_id << "\n";
      }
    }

    if (event.signature.empty()) {
      ++issues;
      details << "Empty signature: " << event.event_id << "\n";
    }

    event_payload_hash[event.event_id] = stable_hash(event.payload);
  }

  std::unordered_set<std::string> block_event_ids;
  for (std::size_t i = 0; i < blocks_.size(); ++i) {
    const auto& block = blocks_[i];
    if (block.index == 0 && block.psz_timestamp.empty()) {
      ++issues;
      details << "Genesis block missing pszTimestamp metadata.\n";
    }
    if (i == 0 && block.prev_hash != "genesis") {
      ++issues;
      details << "Genesis block prev_hash must be `genesis`.\n";
    }
    if (i > 0 && block.prev_hash != blocks_[i - 1U].block_hash) {
      ++issues;
      details << "Block prev_hash mismatch at index " << block.index << "\n";
    }

    std::vector<std::string> leaves;
    std::vector<std::string> content_parts;
    leaves.reserve(block.event_ids.size());
    content_parts.reserve(block.event_ids.size());
    for (const auto& event_id : block.event_ids) {
      const std::string payload_hash =
          event_payload_hash.contains(event_id) ? event_payload_hash.at(event_id) : std::string{"missing"};
      leaves.push_back(stable_hash(event_id + ":" + payload_hash));
      content_parts.push_back(event_id + ":" + payload_hash);
    }

    std::string expected_merkle = compute_merkle_root(leaves);
    const std::string expected_content = stable_hash(join_event_ids(content_parts));
    std::ostringstream digest_input;
    digest_input << block.index << "|" << block.opened_unix << "|" << (block.reserved ? 1 : 0) << "|"
                 << (block.confirmed ? 1 : 0) << "|" << (block.backfilled ? 1 : 0) << "|" << block.prev_hash
                 << "|" << expected_merkle << "|" << expected_content << "|" << block.psz_timestamp;
    std::string expected_block_hash = stable_hash(digest_input.str());
    if (block.index == 0 && block.event_ids.empty()) {
      if (!hardcoded_genesis_merkle_root_.empty()) {
        expected_merkle = hardcoded_genesis_merkle_root_;
      }
      if (!hardcoded_genesis_block_hash_.empty()) {
        expected_block_hash = hardcoded_genesis_block_hash_;
      }
    }

    if (block.merkle_root != expected_merkle) {
      ++issues;
      details << "Merkle root mismatch at block " << block.index << "\n";
    }
    if (block.content_hash != expected_content) {
      ++issues;
      details << "Content hash mismatch at block " << block.index << "\n";
    }
    if (block.block_hash != expected_block_hash) {
      ++issues;
      details << "Block hash mismatch at block " << block.index << "\n";
    }
    if (block.event_ids.size() > validation_limits_.max_block_events) {
      ++issues;
      details << "Block event count exceeds configured max at block " << block.index << "\n";
    }
    if (block_event_bytes(block) > validation_limits_.max_block_bytes) {
      ++issues;
      details << "Block byte size exceeds configured max at block " << block.index << "\n";
    }

    for (const auto& event_id : block.event_ids) {
      if (!has_event(event_id)) {
        ++issues;
        details << "Block references missing event: " << event_id << "\n";
      }
      if (!block_event_ids.insert(event_id).second) {
        ++issues;
        details << "Duplicate event assignment in blocks: " << event_id << "\n";
      }
    }
  }

  for (const auto& event : events_) {
    if (!block_event_ids.contains(event.event_id)) {
      ++issues;
      details << "Event not assigned to any block: " << event.event_id << "\n";
    }
  }

  for (const auto& [event_id, reason] : invalid_economic_events_) {
    ++issues;
    details << "Economic validation failure: " << event_id << " (" << reason << ")\n";
  }
  for (const auto& [event_id, reason] : invalid_moderation_events_) {
    ++issues;
    details << "Moderation validation failure: " << event_id << " (" << reason << ")\n";
  }

  last_backtest_unix_ = util::unix_timestamp_now();
  if (issues == 0) {
    backtest_ok_ = true;
    backtest_details_ = "Backtest validation passed. Event and block timelines are immutable and coherent.";
    return Result::success(backtest_details_);
  }

  backtest_ok_ = false;
  backtest_details_ = details.str();
  if (backtest_details_.empty()) {
    backtest_details_ = "Backtest failed with unknown validation issue.";
  }
  return Result::failure(backtest_details_);
}

std::optional<Store::BlockRecord> Store::block_for_event(std::string_view event_id) const {
  const auto it = event_to_block_.find(std::string{event_id});
  if (it == event_to_block_.end()) {
    return std::nullopt;
  }
  if (it->second >= blocks_.size()) {
    return std::nullopt;
  }
  return blocks_[it->second];
}

std::optional<std::string> Store::confirmation_for_object(std::string_view object_id) const {
  const std::string target{object_id};
  if (target.empty()) {
    return std::nullopt;
  }

  const std::string global = consensus_hash();
  for (const auto& event : events_) {
    const auto payload = util::parse_canonical_map(event.payload);
    auto matches = [&](std::string_view key) {
      return payload.contains(std::string{key}) && payload.at(std::string{key}) == target;
    };

    if (!matches("recipe_id") && !matches("thread_id") && !matches("reply_id")) {
      continue;
    }

    const auto block = block_for_event(event.event_id);
    if (!block.has_value()) {
      return "event=" + event.event_id + " hash=" + stable_hash(global + event.event_id);
    }

    const auto metrics = confirmation_metrics_for_event(event.event_id, event.unix_ts);
    const std::uint64_t confirmations = metrics.has_value() ? metrics->first : 0;
    const std::int64_t age_seconds =
        metrics.has_value() ? metrics->second : std::max<std::int64_t>(0, util::unix_timestamp_now() - event.unix_ts);

    return "event=" + event.event_id + " block=" + std::to_string(block->index) +
           " confirmations=" + std::to_string(confirmations) + " age_s=" + std::to_string(age_seconds) +
           " finality_threshold=" + std::to_string(chain_policy_.confirmation_threshold) +
           " merkle=" + block->merkle_root +
           " hash=" + stable_hash(global + "|" + event.event_id + "|" + block->block_hash);
  }

  return std::nullopt;
}

std::int64_t Store::reward_balance(std::string_view cid) const {
  const auto it = reward_balances_.find(std::string{cid});
  if (it == reward_balances_.end()) {
    return 0;
  }
  return it->second;
}

std::vector<RewardBalanceSummary> Store::reward_balances() const {
  std::vector<RewardBalanceSummary> balances;
  balances.reserve(reward_balances_.size());
  for (const auto& [cid, balance] : reward_balances_) {
    balances.push_back({
        .cid = cid,
        .display_name = {},
        .balance = balance,
    });
  }

  std::ranges::sort(balances, [](const RewardBalanceSummary& lhs, const RewardBalanceSummary& rhs) {
    if (lhs.balance != rhs.balance) {
      return lhs.balance > rhs.balance;
    }
    return lhs.cid < rhs.cid;
  });
  return balances;
}

bool Store::has_block_claim(std::uint64_t block_index) const {
  return claimed_blocks_.contains(block_index);
}

std::vector<Store::BlockRecord> Store::claimable_confirmed_blocks(std::string_view cid) const {
  (void)cid;
  std::vector<BlockRecord> claimable;
  const auto latest_confirmed = latest_confirmed_block_index();
  if (!latest_confirmed.has_value()) {
    return claimable;
  }
  for (const auto& block : blocks_) {
    if (block.index == 0 || !block.confirmed) {
      continue;
    }
    const std::uint64_t confirmations = (*latest_confirmed >= block.index)
                                            ? ((*latest_confirmed - block.index) + 1U)
                                            : 0U;
    if (confirmations < chain_policy_.confirmation_threshold) {
      continue;
    }
    if (has_block_claim(block.index)) {
      continue;
    }
    claimable.push_back(block);
  }

  std::ranges::sort(claimable, [](const BlockRecord& lhs, const BlockRecord& rhs) {
    return lhs.index < rhs.index;
  });
  return claimable;
}

std::int64_t Store::next_claim_reward(std::uint64_t block_index) const {
  return expected_claim_reward_for_block(block_index, issued_reward_total_);
}

std::uint64_t Store::next_transfer_nonce(std::string_view cid) const {
  const auto it = transfer_nonce_by_cid_.find(std::string{cid});
  if (it == transfer_nonce_by_cid_.end()) {
    return 1;
  }
  return it->second + 1U;
}

std::int64_t Store::transfer_burn_fee(std::int64_t amount) const {
  return transfer_burn_fee_internal(amount);
}

ModerationStatus Store::moderation_status() const {
  ModerationStatus status;
  status.enabled = moderation_policy_.moderation_enabled;
  status.policy = moderation_policy_;
  status.invalid_event_count = invalid_moderation_events_.size();
  status.active_moderators.assign(moderators_.begin(), moderators_.end());
  std::ranges::sort(status.active_moderators);

  std::unordered_set<std::string> object_ids;
  object_ids.reserve(moderation_flag_counts_.size() + moderation_hidden_objects_.size() +
                     moderation_core_topic_overrides_.size());
  for (const auto& [object_id, flag_count] : moderation_flag_counts_) {
    (void)flag_count;
    object_ids.insert(object_id);
  }
  object_ids.insert(moderation_hidden_objects_.begin(), moderation_hidden_objects_.end());
  for (const auto& [recipe_id, pinned] : moderation_core_topic_overrides_) {
    (void)pinned;
    object_ids.insert(recipe_id);
  }

  std::vector<std::string> ordered_ids(object_ids.begin(), object_ids.end());
  std::ranges::sort(ordered_ids);
  status.objects.reserve(ordered_ids.size());
  for (const auto& object_id : ordered_ids) {
    ModerationObjectState object;
    object.object_id = object_id;
    if (const auto it = moderation_flag_counts_.find(object_id); it != moderation_flag_counts_.end()) {
      object.flag_count = it->second;
    }
    object.hidden = moderation_hidden_objects_.contains(object_id);
    object.auto_hidden = moderation_auto_hidden_objects_.contains(object_id);
    if (const auto it = moderation_core_topic_overrides_.find(object_id);
        it != moderation_core_topic_overrides_.end()) {
      object.core_topic_pinned = it->second;
    }
    status.objects.push_back(std::move(object));
  }
  return status;
}

bool Store::is_moderator(std::string_view cid) const {
  if (cid.empty()) {
    return false;
  }
  return moderators_.contains(std::string{cid});
}

std::vector<RecipeSummary> Store::query_recipes(const SearchQuery& query) const {
  std::vector<RecipeSummary> results;
  for (const auto& [recipe_id, summary] : recipes_) {
    (void)recipe_id;

    if (!query.category.empty() && summary.category != query.category) {
      continue;
    }

    if (!query.text.empty()) {
      const bool hit_title = util::contains_case_insensitive(summary.title, query.text);
      const bool hit_id = util::contains_case_insensitive(summary.recipe_id, query.text);
      if (!hit_title && !hit_id) {
        continue;
      }
    }

    results.push_back(summary);
  }

  std::ranges::sort(results, [](const RecipeSummary& lhs, const RecipeSummary& rhs) {
    if (lhs.core_topic != rhs.core_topic) {
      return lhs.core_topic > rhs.core_topic;
    }
    if (lhs.updated_unix != rhs.updated_unix) {
      return lhs.updated_unix > rhs.updated_unix;
    }
    return lhs.recipe_id < rhs.recipe_id;
  });

  return results;
}

std::vector<ThreadSummary> Store::query_threads(std::string_view recipe_id) const {
  std::vector<ThreadSummary> results;
  for (const auto& [thread_id, thread] : threads_) {
    (void)thread_id;
    if (recipe_id.empty() || thread.recipe_id == recipe_id) {
      results.push_back(thread);
    }
  }

  std::ranges::sort(results, [](const ThreadSummary& lhs, const ThreadSummary& rhs) {
    if (lhs.updated_unix != rhs.updated_unix) {
      return lhs.updated_unix > rhs.updated_unix;
    }
    return lhs.thread_id < rhs.thread_id;
  });

  return results;
}

std::vector<ReplySummary> Store::query_replies(std::string_view thread_id) const {
  const auto it = replies_by_thread_.find(std::string{thread_id});
  if (it == replies_by_thread_.end()) {
    return {};
  }

  auto replies = it->second;
  std::ranges::sort(replies, [](const ReplySummary& lhs, const ReplySummary& rhs) {
    if (lhs.updated_unix != rhs.updated_unix) {
      return lhs.updated_unix < rhs.updated_unix;
    }
    return lhs.reply_id < rhs.reply_id;
  });

  return replies;
}

std::string Store::schema_sql() const {
  return "CREATE TABLE identity_keys (...);\n"
         "CREATE TABLE invites (...);\n"
         "CREATE TABLE events (...);\n"
         "CREATE TABLE blocks (...);\n"
         "CREATE TABLE checkpoints (...);\n"
         "CREATE TABLE snapshots (...);\n"
         "CREATE TABLE rewards_ledger (...);\n"
         "CREATE TABLE moderation_policy (...);\n"
         "CREATE TABLE moderation_actions (...);\n"
         "CREATE TABLE moderation_state (...);\n"
         "CREATE TABLE recipes_view (...);\n"
         "CREATE TABLE threads_view (...);\n"
         "CREATE TABLE replies_view (...);\n"
         "CREATE TABLE reviews_view (...);\n"
         "CREATE TABLE thumbs_view (...);\n"
         "CREATE TABLE peers (...);\n";
}

DbHealthReport Store::health_report() const {
  DbHealthReport report;
  report.healthy = true;
  report.details = "Store health check passed.";
  report.data_dir = app_data_dir_;
  report.events_file = event_log_path_;
  report.blockdata_file = block_log_path_;
  report.snapshot_file = snapshot_path_;
  report.blockdata_format_version = blockdata_format_version_;
  report.recovered_from_corruption = recovered_from_corruption_;
  report.invalid_event_drop_count = invalid_event_drop_count_;
  report.event_count = events_.size();
  report.recipe_count = recipes_.size();
  report.thread_count = threads_.size();

  std::size_t reply_count = 0;
  for (const auto& [thread_id, replies] : replies_by_thread_) {
    (void)thread_id;
    reply_count += replies.size();
  }
  report.reply_count = reply_count;

  std::error_code ec;
  if (!event_log_path_.empty() && std::filesystem::exists(event_log_path_, ec) && !ec) {
    report.event_log_size_bytes = std::filesystem::file_size(event_log_path_, ec);
  }

  if (ec) {
    report.healthy = false;
    report.details = "Store health warning: unable to inspect event log size (" + ec.message() + ").";
  }

  report.consensus_hash = consensus_hash();
  report.timeline_hash = timeline_hash();
  report.block_count = blocks_.size();
  report.block_interval_seconds = block_interval_seconds_;
  report.backtest_ok = backtest_ok_;
  report.backtest_details = backtest_details_;
  report.last_backtest_unix = last_backtest_unix_;
  report.invalid_economic_event_count = invalid_economic_events_.size();
  report.chain_id = chain_id_;
  report.network_id = network_id_;
  report.confirmation_threshold = chain_policy_.confirmation_threshold;
  report.fork_choice_rule = chain_policy_.fork_choice_rule;
  report.max_reorg_depth = chain_policy_.max_reorg_depth;
  report.checkpoint_interval_blocks = chain_policy_.checkpoint_interval_blocks;
  report.checkpoint_confirmations = chain_policy_.checkpoint_confirmations;
  report.checkpoint_count = checkpoint_count_;
  report.max_block_events = validation_limits_.max_block_events;
  report.max_block_bytes = validation_limits_.max_block_bytes;
  report.max_event_bytes = validation_limits_.max_event_bytes;
  report.max_future_drift_seconds = validation_limits_.max_future_drift_seconds;
  report.max_past_drift_seconds = validation_limits_.max_past_drift_seconds;
  report.moderation_enabled = moderation_policy_.moderation_enabled;
  report.moderation_min_confirmations = moderation_policy_.min_confirmations_for_enforcement;
  report.moderator_count = moderators_.size();
  report.flagged_object_count = moderation_flag_counts_.size();
  report.hidden_object_count = moderation_hidden_objects_.size();
  report.pinned_core_topic_count = moderation_core_topic_overrides_.size();
  report.invalid_moderation_event_count = invalid_moderation_events_.size();

  std::int64_t reward_supply = 0;
  for (const auto& [cid, balance] : reward_balances_) {
    (void)cid;
    reward_supply += balance;
  }
  report.reward_supply = reward_supply;
  report.issued_reward_total = issued_reward_total_;
  report.burned_fee_total = burned_fee_total_;
  report.max_token_supply = max_token_supply_units_;

  std::size_t reserved_blocks = 0;
  std::size_t confirmed_blocks = 0;
  std::size_t backfilled_blocks = 0;
  std::int64_t last_block_unix = 0;
  for (const auto& block : blocks_) {
    if (block.reserved && block.event_ids.empty()) {
      ++reserved_blocks;
    }
    if (block.confirmed) {
      ++confirmed_blocks;
    }
    if (block.backfilled) {
      ++backfilled_blocks;
    }
    if (block.opened_unix > last_block_unix) {
      last_block_unix = block.opened_unix;
    }
  }
  report.reserved_block_count = reserved_blocks;
  report.confirmed_block_count = confirmed_blocks;
  report.backfilled_block_count = backfilled_blocks;
  report.last_block_unix = last_block_unix;
  if (!blocks_.empty()) {
    report.genesis_psz_timestamp = blocks_.front().psz_timestamp;
    report.latest_merkle_root = blocks_.back().merkle_root;
  } else {
    report.genesis_psz_timestamp = genesis_psz_timestamp_;
    report.latest_merkle_root.clear();
  }

  for (const auto& event : events_) {
    if (event.kind == EventKind::BlockRewardClaimed) {
      ++report.reward_claim_event_count;
    } else if (event.kind == EventKind::RewardTransferred) {
      ++report.reward_transfer_event_count;
    }
  }

  if (!backtest_ok_) {
    report.healthy = false;
    report.details = "Store health warning: backtest validation failed.";
  }
  if (recovered_from_corruption_) {
    report.healthy = false;
    report.details = "Store health warning: blockdata recovery mode is active (corruption detected).";
  }
  if (!invalid_economic_events_.empty()) {
    report.healthy = false;
    report.details =
        "Store health warning: " + std::to_string(invalid_economic_events_.size()) +
        " economically-invalid events detected.";
  }
  if (!invalid_moderation_events_.empty()) {
    report.healthy = false;
    report.details =
        "Store health warning: " + std::to_string(invalid_moderation_events_.size()) +
        " moderation-invalid events detected.";
  }
  if (invalid_event_drop_count_ > 0) {
    report.healthy = false;
    report.details = "Store health warning: dropped " + std::to_string(invalid_event_drop_count_) +
                     " invalid event(s).";
  }

  return report;
}

Result Store::load_event_log() {
  events_.clear();

  std::ifstream in(event_log_path_);
  if (!in) {
    return Result::success("Event log will be created on first write.");
  }

  std::string line;
  while (std::getline(in, line)) {
    if (line.empty() || line.front() == '#') {
      continue;
    }

    EventEnvelope event;
    if (parse_event_line(line, event)) {
      events_.push_back(std::move(event));
    } else {
      record_invalid_event("load-event-log", "Failed to parse event line.");
    }
  }

  return materialize_views();
}

Result Store::persist_event(const EventEnvelope& event) const {
  std::ofstream out(event_log_path_, std::ios::out | std::ios::app);
  if (!out) {
    return Result::failure("Failed to write event log file.");
  }

  out << serialize_event_line(event);
  if (!out.good()) {
    return Result::failure("Failed to flush event log file.");
  }

  return Result::success();
}

Result Store::persist_event_log() const {
  std::ofstream out(event_log_path_, std::ios::out | std::ios::trunc);
  if (!out) {
    return Result::failure("Failed to rewrite event log file.");
  }

  for (const auto& event : events_) {
    out << serialize_event_line(event);
  }

  if (!out.good()) {
    return Result::failure("Failed flushing rewritten event log file.");
  }

  return Result::success();
}

Result Store::rollback_to_last_checkpoint(std::string_view reason) {
  ensure_genesis_block(util::unix_timestamp_now());

  const std::optional<std::uint64_t> confirmed_tip = latest_confirmed_block_index();
  std::uint64_t checkpoint_index = 0;
  if (confirmed_tip.has_value()) {
    for (const auto& block : blocks_) {
      if (!block.confirmed) {
        continue;
      }
      if (chain_policy_.checkpoint_interval_blocks > 0 &&
          (block.index % chain_policy_.checkpoint_interval_blocks) != 0U) {
        continue;
      }
      const std::uint64_t confirmations = (*confirmed_tip >= block.index) ? ((*confirmed_tip - block.index) + 1U) : 0U;
      if (confirmations < chain_policy_.checkpoint_confirmations) {
        continue;
      }
      checkpoint_index = std::max(checkpoint_index, block.index);
    }
  }

  std::vector<BlockRecord> retained_blocks;
  retained_blocks.reserve(blocks_.size());
  std::unordered_set<std::string> retained_event_ids;
  for (const auto& block : blocks_) {
    if (block.index > checkpoint_index) {
      continue;
    }
    retained_blocks.push_back(block);
    for (const auto& event_id : block.event_ids) {
      retained_event_ids.insert(event_id);
    }
  }

  if (retained_blocks.empty()) {
    BlockRecord genesis;
    genesis.index = 0;
    genesis.opened_unix = util::unix_timestamp_now();
    genesis.reserved = true;
    genesis.confirmed = false;
    genesis.backfilled = false;
    genesis.psz_timestamp = genesis_psz_timestamp_;
    retained_blocks.push_back(std::move(genesis));
  }

  events_.erase(std::remove_if(events_.begin(), events_.end(), [&retained_event_ids](const EventEnvelope& event) {
                  return !retained_event_ids.contains(event.event_id);
                }),
                events_.end());
  blocks_ = std::move(retained_blocks);

  rebuild_event_to_block_index();
  recompute_block_hashes();

  const Result materialized = materialize_views();
  if (!materialized.ok) {
    return materialized;
  }

  const Result persist_events = persist_event_log();
  if (!persist_events.ok) {
    return persist_events;
  }
  const Result persist_blocks = persist_block_log();
  if (!persist_blocks.ok) {
    return persist_blocks;
  }
  const Result persist_checkpoints_result = persist_checkpoints();
  if (!persist_checkpoints_result.ok) {
    return persist_checkpoints_result;
  }
  const Result snapshot = persist_snapshot();
  if (!snapshot.ok) {
    return snapshot;
  }

  std::ostringstream msg;
  msg << "Rolled back chain to checkpoint block " << checkpoint_index;
  if (!reason.empty()) {
    msg << " (" << reason << ")";
  }
  return Result::success(msg.str());
}

Result Store::load_block_log() {
  blocks_.clear();
  event_to_block_.clear();

  std::ifstream in(block_log_path_);
  bool loaded_legacy = false;
  if (!in) {
    const std::filesystem::path legacy =
        std::filesystem::path{block_log_path_}.parent_path() / std::string{kLegacyBlockLogFile};
    in.open(legacy);
    if (!in) {
      return Result::success("Block data file will be created on first write.");
    }
    loaded_legacy = true;
  }

  std::size_t parse_errors = 0;
  std::string line;
  while (std::getline(in, line)) {
    if (line.empty()) {
      continue;
    }
    if (line.front() == '#') {
      if (line.rfind(std::string{kBlockHeaderPrefix}, 0) == 0) {
        std::istringstream header(line);
        std::string token;
        while (header >> token) {
          if (token.starts_with("version=")) {
            const std::string v = token.substr(std::string{"version="}.size());
            const int parsed = parse_int_or_zero(v);
            if (parsed > 0) {
              blockdata_format_version_ = static_cast<std::uint32_t>(parsed);
            }
          } else if (token.starts_with("chain_id=")) {
            chain_id_ = token.substr(std::string{"chain_id="}.size());
          } else if (token.starts_with("network=")) {
            network_id_ = token.substr(std::string{"network="}.size());
          }
        }
      }
      continue;
    }

    BlockRecord block;
    if (parse_block_line(line, block)) {
      blocks_.push_back(std::move(block));
    } else {
      ++parse_errors;
    }
  }

  std::ranges::sort(blocks_, [](const BlockRecord& lhs, const BlockRecord& rhs) {
    return lhs.index < rhs.index;
  });
  rebuild_event_to_block_index();
  if (parse_errors > 0) {
    recovered_from_corruption_ = true;
    record_invalid_event("load-block-log", "Failed to parse " + std::to_string(parse_errors) +
                                               " blockdata line(s).");
  }
  if (blockdata_format_version_ < 2) {
    recovered_from_corruption_ = true;
    blockdata_format_version_ = 2;
    record_invalid_event("load-block-log", "Migrated blockdata format to version 2.");
  }
  if (loaded_legacy) {
    return Result::success("Loaded legacy blocks.log data; will persist as blockdata.dat.");
  }
  return Result::success("Block data loaded.");
}

Result Store::persist_block_log() const {
  std::ofstream out(block_log_path_, std::ios::out | std::ios::trunc);
  if (!out) {
    return Result::failure("Failed to write block log file.");
  }

  out << kBlockHeaderPrefix << " version=" << blockdata_format_version_ << " chain_id=" << chain_id_
      << " network=" << network_id_ << "\n";
  for (const auto& block : blocks_) {
    out << serialize_block_line(block);
  }
  if (!out.good()) {
    return Result::failure("Failed flushing block log file.");
  }
  return Result::success();
}

void Store::ensure_genesis_block(std::int64_t now_unix) {
  if (!blocks_.empty()) {
    if (blocks_.front().psz_timestamp.empty() && !genesis_psz_timestamp_.empty()) {
      blocks_.front().psz_timestamp = genesis_psz_timestamp_;
    }
    return;
  }

  if (genesis_psz_timestamp_.empty()) {
    genesis_psz_timestamp_ =
        "SoupNet::P2P Tomato Soup " + network_id_ + " genesis | " + std::to_string(now_unix);
  }

  BlockRecord genesis;
  genesis.index = 0;
  genesis.opened_unix = now_unix;
  genesis.reserved = true;
  genesis.confirmed = false;
  genesis.backfilled = false;
  genesis.psz_timestamp = genesis_psz_timestamp_;
  genesis.merkle_root = hardcoded_genesis_merkle_root_;
  genesis.block_hash = hardcoded_genesis_block_hash_;
  blocks_.push_back(std::move(genesis));
}

void Store::ensure_block_slots_until(std::int64_t now_unix) {
  if (block_interval_seconds_ == 0) {
    block_interval_seconds_ = 150;
  }
  ensure_genesis_block(now_unix);

  std::size_t created = 0;
  constexpr std::size_t kMaxReservePerCheck = 256;
  while (!blocks_.empty() &&
         (now_unix - blocks_.back().opened_unix) >= static_cast<std::int64_t>(block_interval_seconds_) &&
         created < kMaxReservePerCheck) {
    BlockRecord reserved;
    reserved.index = blocks_.back().index + 1;
    reserved.opened_unix =
        blocks_.back().opened_unix + static_cast<std::int64_t>(block_interval_seconds_);
    reserved.reserved = true;
    reserved.confirmed = false;
    reserved.backfilled = false;
    reserved.psz_timestamp.clear();
    blocks_.push_back(std::move(reserved));
    ++created;
  }
}

void Store::assign_unassigned_events_to_blocks() {
  if (events_.empty()) {
    rebuild_event_to_block_index();
    return;
  }

  ensure_genesis_block(events_.front().unix_ts);

  std::unordered_set<std::string> assigned;
  for (const auto& block : blocks_) {
    for (const auto& event_id : block.event_ids) {
      assigned.insert(event_id);
    }
  }

  for (const auto& event : events_) {
    if (assigned.contains(event.event_id)) {
      continue;
    }

    const std::size_t event_bytes = event.event_id.size() + event.payload.size() + event.signature.size() + 24U;
    auto slot_it = std::ranges::find_if(blocks_, [this, event_bytes](const BlockRecord& block) {
      if (block.confirmed) {
        return false;
      }
      if (block.event_ids.size() >= validation_limits_.max_block_events) {
        return false;
      }
      return (block_event_bytes(block) + event_bytes) <= validation_limits_.max_block_bytes;
    });

    if (slot_it == blocks_.end()) {
      BlockRecord appended;
      appended.index = blocks_.empty() ? 0 : blocks_.back().index + 1;
      appended.opened_unix = event.unix_ts;
      appended.reserved = true;
      appended.confirmed = false;
      appended.backfilled = false;
      appended.psz_timestamp.clear();
      blocks_.push_back(std::move(appended));
      slot_it = std::prev(blocks_.end());
    }

    if (slot_it->reserved) {
      slot_it->backfilled = true;
    }
    slot_it->reserved = false;
    slot_it->event_ids.push_back(event.event_id);
    assigned.insert(event.event_id);
  }

  rebuild_event_to_block_index();
}

void Store::rebuild_event_to_block_index() {
  event_to_block_.clear();
  for (std::size_t i = 0; i < blocks_.size(); ++i) {
    for (const auto& event_id : blocks_[i].event_ids) {
      event_to_block_[event_id] = i;
    }
  }
}

void Store::recompute_block_hashes() {
  rebuild_event_to_block_index();

  std::unordered_map<std::string, std::string> event_payload_hash;
  event_payload_hash.reserve(events_.size());
  for (const auto& event : events_) {
    event_payload_hash[event.event_id] = stable_hash(event.payload);
  }

  std::string prev_hash = "genesis";
  for (auto& block : blocks_) {
    if (block.index == 0 && block.psz_timestamp.empty()) {
      if (genesis_psz_timestamp_.empty()) {
        genesis_psz_timestamp_ =
            "SoupNet::P2P Tomato Soup " + network_id_ + " genesis | " + std::to_string(block.opened_unix);
      }
      block.psz_timestamp = genesis_psz_timestamp_;
    }

    std::vector<std::string> merkle_leaves;
    std::vector<std::string> content_parts;
    merkle_leaves.reserve(block.event_ids.size());
    content_parts.reserve(block.event_ids.size());
    for (const auto& event_id : block.event_ids) {
      if (event_payload_hash.contains(event_id)) {
        const std::string payload_hash = event_payload_hash[event_id];
        merkle_leaves.push_back(stable_hash(event_id + ":" + payload_hash));
        content_parts.push_back(event_id + ":" + payload_hash);
      } else {
        merkle_leaves.push_back(stable_hash(event_id + ":missing"));
        content_parts.push_back(event_id + ":missing");
      }
    }

    block.merkle_root = compute_merkle_root(merkle_leaves);
    block.content_hash = stable_hash(join_event_ids(content_parts));
    block.prev_hash = prev_hash;

    std::ostringstream digest_input;
    digest_input << block.index << "|" << block.opened_unix << "|" << (block.reserved ? 1 : 0) << "|"
                 << (block.confirmed ? 1 : 0) << "|" << (block.backfilled ? 1 : 0) << "|" << block.prev_hash
                 << "|" << block.merkle_root << "|" << block.content_hash << "|" << block.psz_timestamp;
    block.block_hash = stable_hash(digest_input.str());
    if (block.index == 0 && block.event_ids.empty()) {
      if (!hardcoded_genesis_merkle_root_.empty()) {
        block.merkle_root = hardcoded_genesis_merkle_root_;
      }
      if (!hardcoded_genesis_block_hash_.empty()) {
        block.block_hash = hardcoded_genesis_block_hash_;
      }
    }
    prev_hash = block.block_hash;
  }
}

std::int64_t Store::scheduled_reward_for_block(std::uint64_t block_index) const {
  if (block_index == 0) {
    return 0;
  }
  const long double decay = std::clamp(per_block_subsidy_decay_fraction_, 0.0L, 0.9999999999L);
  const long double multiplier = 1.0L - decay;
  const long double exponent = static_cast<long double>(block_index - 1U);
  const long double raw = static_cast<long double>(block_reward_units_) * std::pow(multiplier, exponent);
  const std::int64_t floor_units = std::max<std::int64_t>(1, min_subsidy_units_);
  const std::int64_t reward = static_cast<std::int64_t>(raw);
  return std::max<std::int64_t>(floor_units, reward);
}

std::int64_t Store::expected_claim_reward_for_block(std::uint64_t block_index,
                                                    std::int64_t issued_so_far) const {
  if (issued_so_far >= max_token_supply_units_) {
    return 0;
  }
  const std::int64_t scheduled = scheduled_reward_for_block(block_index);
  if (scheduled <= 0) {
    return 0;
  }
  const std::int64_t remaining = max_token_supply_units_ - issued_so_far;
  return std::min<std::int64_t>(scheduled, remaining);
}

std::int64_t Store::transfer_burn_fee_internal(std::int64_t amount) const {
  if (amount <= 0) {
    return 0;
  }
  const std::int64_t percent_fee = amount / 100;  // 1%
  return std::max<std::int64_t>(1, percent_fee);
}

std::optional<std::uint64_t> Store::latest_confirmed_block_index() const {
  std::optional<std::uint64_t> latest;
  for (const auto& block : blocks_) {
    if (!block.confirmed) {
      continue;
    }
    if (!latest.has_value() || block.index > *latest) {
      latest = block.index;
    }
  }
  return latest;
}

std::optional<std::pair<std::uint64_t, std::int64_t>>
Store::confirmation_metrics_for_event(std::string_view source_event_id, std::int64_t updated_unix) const {
  const auto block = block_for_event(source_event_id);
  if (!block.has_value()) {
    return std::nullopt;
  }

  const auto latest_confirmed = latest_confirmed_block_index();
  if (!latest_confirmed.has_value() || !block->confirmed || *latest_confirmed < block->index) {
    return std::pair<std::uint64_t, std::int64_t>{0U,
                                                   std::max<std::int64_t>(
                                                       0, util::unix_timestamp_now() - updated_unix)};
  }

  const std::uint64_t confirmations = (*latest_confirmed - block->index) + 1U;
  const std::int64_t age_seconds = std::max<std::int64_t>(0, util::unix_timestamp_now() - updated_unix);
  return std::pair<std::uint64_t, std::int64_t>{confirmations, age_seconds};
}

void Store::apply_confirmation_metrics() {
  for (auto& [recipe_id, recipe] : recipes_) {
    (void)recipe_id;
    const auto metrics = confirmation_metrics_for_event(recipe.source_event_id, recipe.updated_unix);
    if (metrics.has_value()) {
      recipe.confirmation_count = metrics->first;
      recipe.confirmation_age_seconds = metrics->second;
    } else {
      recipe.confirmation_count = 0;
      recipe.confirmation_age_seconds =
          std::max<std::int64_t>(0, util::unix_timestamp_now() - recipe.updated_unix);
    }
  }

  for (auto& [thread_id, thread] : threads_) {
    (void)thread_id;
    const auto metrics = confirmation_metrics_for_event(thread.source_event_id, thread.updated_unix);
    if (metrics.has_value()) {
      thread.confirmation_count = metrics->first;
      thread.confirmation_age_seconds = metrics->second;
    } else {
      thread.confirmation_count = 0;
      thread.confirmation_age_seconds =
          std::max<std::int64_t>(0, util::unix_timestamp_now() - thread.updated_unix);
    }
  }

  for (auto& [thread_id, replies] : replies_by_thread_) {
    (void)thread_id;
    for (auto& reply : replies) {
      const auto metrics = confirmation_metrics_for_event(reply.source_event_id, reply.updated_unix);
      if (metrics.has_value()) {
        reply.confirmation_count = metrics->first;
        reply.confirmation_age_seconds = metrics->second;
      } else {
        reply.confirmation_count = 0;
        reply.confirmation_age_seconds =
            std::max<std::int64_t>(0, util::unix_timestamp_now() - reply.updated_unix);
      }
    }
  }
}

Result Store::persist_snapshot() {
  if (!enable_snapshots_ || snapshot_path_.empty()) {
    return Result::success("Snapshots disabled.");
  }
  if (!blocks_.empty() && snapshot_interval_blocks_ > 1 &&
      (blocks_.back().index % snapshot_interval_blocks_) != 0U &&
      std::filesystem::exists(snapshot_path_)) {
    return Result::success("Snapshot interval not reached.");
  }

  std::ofstream out(snapshot_path_, std::ios::out | std::ios::trunc);
  if (!out) {
    return Result::failure("Failed to write snapshot file.");
  }

  out << "format=got-soup-snapshot-v1\n";
  out << "chain_id=" << chain_id_ << "\n";
  out << "network=" << network_id_ << "\n";
  out << "blockdata_format_version=" << blockdata_format_version_ << "\n";
  out << "event_count=" << events_.size() << "\n";
  out << "block_count=" << blocks_.size() << "\n";
  out << "consensus_hash=" << consensus_hash() << "\n";
  out << "timeline_hash=" << timeline_hash() << "\n";
  out << "tip_block_index=" << (blocks_.empty() ? 0 : blocks_.back().index) << "\n";
  out << "checkpoint_count=" << checkpoint_count_ << "\n";
  out << "invalid_event_drop_count=" << invalid_event_drop_count_ << "\n";
  out << "created_unix=" << util::unix_timestamp_now() << "\n";
  if (!out.good()) {
    return Result::failure("Failed flushing snapshot file.");
  }
  last_snapshot_unix_ = util::unix_timestamp_now();
  return Result::success("Snapshot persisted.");
}

Result Store::persist_checkpoints() {
  if (checkpoints_path_.empty()) {
    return Result::success("Checkpoints path not configured.");
  }

  std::ofstream out(checkpoints_path_, std::ios::out | std::ios::trunc);
  if (!out) {
    return Result::failure("Failed to write checkpoints file.");
  }

  out << "# got-soup checkpoints\n";
  out << "chain_id=" << chain_id_ << "\n";
  out << "network=" << network_id_ << "\n";
  out << "policy_interval=" << chain_policy_.checkpoint_interval_blocks << "\n";
  out << "policy_confirmations=" << chain_policy_.checkpoint_confirmations << "\n";

  checkpoint_count_ = 0;
  const auto latest_confirmed = latest_confirmed_block_index();
  if (latest_confirmed.has_value()) {
    for (const auto& block : blocks_) {
      if (!block.confirmed || block.index == 0) {
        continue;
      }
      if ((block.index % chain_policy_.checkpoint_interval_blocks) != 0U) {
        continue;
      }
      const std::uint64_t confirmations =
          (*latest_confirmed >= block.index) ? ((*latest_confirmed - block.index) + 1U) : 0U;
      if (confirmations < chain_policy_.checkpoint_confirmations) {
        continue;
      }
      ++checkpoint_count_;
      out << block.index << "\t" << block.block_hash << "\t" << block.merkle_root << "\n";
    }
  }

  if (!out.good()) {
    return Result::failure("Failed flushing checkpoints file.");
  }
  return Result::success("Checkpoints persisted.");
}

void Store::prune_blocks_if_needed() {
  if (!enable_pruning_ || blocks_.size() <= prune_keep_recent_blocks_) {
    return;
  }

  std::size_t keep = std::max<std::size_t>(2, static_cast<std::size_t>(prune_keep_recent_blocks_));
  if (keep >= blocks_.size()) {
    return;
  }

  const std::size_t target_remove = blocks_.size() - keep;
  std::size_t removed = 0;
  auto it = std::next(blocks_.begin());
  while (it != blocks_.end() && removed < target_remove) {
    if (it->event_ids.empty() && it->confirmed) {
      it = blocks_.erase(it);
      ++removed;
    } else {
      ++it;
    }
  }

  if (removed > 0) {
    last_prune_unix_ = util::unix_timestamp_now();
    rebuild_event_to_block_index();
  }
}

std::size_t Store::block_event_bytes(const BlockRecord& block) const {
  std::size_t total = 0;
  for (const auto& event_id : block.event_ids) {
    total += event_id.size();
    const auto it =
        std::ranges::find_if(events_, [&event_id](const EventEnvelope& event) { return event.event_id == event_id; });
    if (it != events_.end()) {
      total += it->payload.size() + it->signature.size() + 24U;
    } else {
      total += 64U;
    }
  }
  return total;
}

void Store::record_invalid_event(std::string_view event_id, std::string_view reason) {
  if (invalid_event_log_path_.empty()) {
    return;
  }

  std::ofstream out(invalid_event_log_path_, std::ios::out | std::ios::app);
  if (!out) {
    return;
  }
  ++invalid_event_drop_count_;
  out << util::unix_timestamp_now() << "\t" << event_id << "\t" << reason << "\n";
}

std::string Store::consensus_hash() const {
  std::vector<std::string> chunks;
  chunks.reserve(events_.size());
  for (const auto& event : events_) {
    chunks.push_back(event.event_id + ":" + stable_hash(event.payload));
  }
  std::ranges::sort(chunks);

  std::ostringstream out;
  for (const auto& chunk : chunks) {
    out << chunk << "\n";
  }
  return stable_hash(out.str());
}

std::string Store::timeline_hash() const {
  std::ostringstream out;
  for (const auto& block : blocks_) {
    out << block.index << ":" << block.block_hash << "\n";
  }
  return stable_hash(out.str());
}

}  // namespace alpha

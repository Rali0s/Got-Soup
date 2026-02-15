#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace alpha {

struct Result {
  bool ok = false;
  std::string message;
  std::string data;

  static Result success(std::string msg = {}, std::string payload = {}) {
    return {true, std::move(msg), std::move(payload)};
  }

  static Result failure(std::string msg) {
    return {false, std::move(msg), {}};
  }
};

struct Cid {
  std::string value;

  [[nodiscard]] bool empty() const { return value.empty(); }
};

struct InviteToken {
  std::string token_id;
  Cid issued_to;
  std::string issuer_cid;
  std::int64_t issued_unix = 0;
  std::int64_t expires_unix = 0;
  std::string signature;
};

enum class EventKind {
  RecipeCreated,
  ThreadCreated,
  ReplyCreated,
  ReviewAdded,
  ThumbsUpAdded,
  BlockRewardClaimed,
  RewardTransferred,
  ProfileUpdated,
  KeyRotated,
  ModeratorAdded,
  ModeratorRemoved,
  ContentFlagged,
  ContentHidden,
  ContentUnhidden,
  CoreTopicPinned,
  CoreTopicUnpinned,
  PolicyUpdated,
};

struct EventEnvelope {
  std::string event_id;
  EventKind kind = EventKind::RecipeCreated;
  std::string author_cid;
  std::int64_t unix_ts = 0;
  std::string payload;
  std::string signature;
};

struct RecipeDraft {
  std::string category;
  std::string title;
  std::string markdown;
  bool core_topic = false;
  std::string menu_segment;
  std::int64_t value_units = 0;
};

struct ThreadDraft {
  std::string recipe_id;
  std::string title;
  std::string markdown;
  std::int64_t value_units = 0;
};

struct ReplyDraft {
  std::string thread_id;
  std::string markdown;
  std::int64_t value_units = 0;
};

struct ReviewDraft {
  std::string recipe_id;
  int rating = 0;
  std::string markdown;
  std::int64_t value_units = 0;
};

struct RewardTransferDraft {
  std::string to_display_name;
  std::int64_t amount = 0;
  std::string memo;
};

struct SearchQuery {
  std::string text;
  std::string category;
};

struct RecipeSummary {
  std::string recipe_id;
  std::string source_event_id;
  std::string title;
  std::string category;
  std::string author_cid;
  std::int64_t updated_unix = 0;
  double average_rating = 0.0;
  int review_count = 0;
  int thumbs_up_count = 0;
  bool core_topic = false;
  std::string menu_segment;
  std::int64_t value_units = 0;
  std::uint64_t confirmation_count = 0;
  std::int64_t confirmation_age_seconds = 0;
};

struct ThreadSummary {
  std::string thread_id;
  std::string source_event_id;
  std::string recipe_id;
  std::string title;
  std::string author_cid;
  std::int64_t updated_unix = 0;
  int reply_count = 0;
  std::int64_t value_units = 0;
  std::uint64_t confirmation_count = 0;
  std::int64_t confirmation_age_seconds = 0;
};

struct ReplySummary {
  std::string reply_id;
  std::string source_event_id;
  std::string thread_id;
  std::string author_cid;
  std::string markdown;
  std::int64_t updated_unix = 0;
  std::int64_t value_units = 0;
  std::uint64_t confirmation_count = 0;
  std::int64_t confirmation_age_seconds = 0;
};

struct RewardBalanceSummary {
  std::string cid;
  std::string display_name;
  std::int64_t balance = 0;
};

struct ProfileSummary {
  Cid cid;
  std::string display_name;
  std::string bio_markdown;
  bool display_name_immortalized = false;
  bool reject_duplicate_names = true;
  bool duplicate_name_detected = false;
  std::size_t duplicate_name_count = 0;
};

enum class AnonymityMode {
  Tor,
  I2P,
};

struct DbHealthReport {
  bool healthy = false;
  std::string details;
  std::string data_dir;
  std::string events_file;
  std::string blockdata_file;
  std::string snapshot_file;
  std::uint32_t blockdata_format_version = 1;
  bool recovered_from_corruption = false;
  std::size_t invalid_event_drop_count = 0;
  std::size_t event_count = 0;
  std::size_t recipe_count = 0;
  std::size_t thread_count = 0;
  std::size_t reply_count = 0;
  std::uintmax_t event_log_size_bytes = 0;
  std::string consensus_hash;
  std::string timeline_hash;
  std::size_t block_count = 0;
  std::size_t reserved_block_count = 0;
  std::size_t confirmed_block_count = 0;
  std::size_t backfilled_block_count = 0;
  std::uint64_t block_interval_seconds = 0;
  int pow_current_difficulty_nibbles = 0;
  int pow_min_difficulty_nibbles = 0;
  int pow_max_difficulty_nibbles = 0;
  std::uint64_t pow_target_solve_seconds = 0;
  std::uint64_t pow_retarget_window_claims = 0;
  std::uint64_t pow_retarget_count = 0;
  std::int64_t pow_last_window_avg_solve_seconds = 0;
  std::int64_t pow_last_retarget_unix = 0;
  std::int64_t last_block_unix = 0;
  std::string genesis_psz_timestamp;
  std::string latest_merkle_root;
  bool backtest_ok = false;
  std::string backtest_details;
  std::int64_t last_backtest_unix = 0;
  std::int64_t reward_supply = 0;
  std::int64_t issued_reward_total = 0;
  std::int64_t burned_fee_total = 0;
  std::int64_t max_token_supply = 0;
  std::size_t reward_claim_event_count = 0;
  std::size_t reward_transfer_event_count = 0;
  std::size_t invalid_economic_event_count = 0;
  std::string chain_id;
  std::string network_id;
  std::uint64_t confirmation_threshold = 1;
  std::string fork_choice_rule;
  std::uint64_t max_reorg_depth = 0;
  std::uint64_t checkpoint_interval_blocks = 0;
  std::uint64_t checkpoint_confirmations = 0;
  std::size_t checkpoint_count = 0;
  std::size_t max_block_events = 0;
  std::size_t max_block_bytes = 0;
  std::size_t max_event_bytes = 0;
  std::int64_t max_future_drift_seconds = 0;
  std::int64_t max_past_drift_seconds = 0;
  bool moderation_enabled = false;
  std::uint64_t moderation_min_confirmations = 0;
  std::size_t moderator_count = 0;
  std::size_t flagged_object_count = 0;
  std::size_t hidden_object_count = 0;
  std::size_t pinned_core_topic_count = 0;
  std::size_t invalid_moderation_event_count = 0;
};

struct NodeRuntimeStats {
  bool running = false;
  bool alpha_test_mode = false;
  std::string network;
  std::string bind_host;
  std::uint16_t bind_port = 0;
  std::uint16_t proxy_port = 0;
  std::size_t peer_count = 0;
  std::size_t outbound_queue = 0;
  std::size_t seen_event_count = 0;
  std::uint64_t sync_tick_count = 0;
};

struct CommunityProfile {
  std::string community_id;
  std::string display_name;
  std::string description;
  std::string profile_path;
  std::string cipher_key;
  std::string peers_dat_path;
  std::string store_path;
  std::int64_t minimum_post_value = 0;
  std::int64_t block_reward_units = 50;
  std::string genesis_psz_timestamp;
  bool moderation_enabled = true;
  bool moderation_require_finality = true;
  std::uint64_t moderation_min_confirmations = 6;
  std::size_t moderation_auto_hide_flags = 3;
  std::vector<std::string> moderator_cids;
};

struct InitialAllocation {
  std::string identity;
  std::int64_t amount = 0;
};

struct ChainPolicy {
  std::uint64_t confirmation_threshold = 1;
  std::string fork_choice_rule = "most-work-then-oldest";
  std::uint64_t max_reorg_depth = 6;
  std::uint64_t checkpoint_interval_blocks = 288;
  std::uint64_t checkpoint_confirmations = 24;
};

struct ValidationLimits {
  std::size_t max_block_events = 512;
  std::size_t max_block_bytes = 1U << 20U;  // 1 MiB
  std::size_t max_event_bytes = 64U << 10U; // 64 KiB
  std::int64_t max_future_drift_seconds = 120;
  std::int64_t max_past_drift_seconds = 7 * 24 * 60 * 60;
};

struct ModerationPolicy {
  bool moderation_enabled = true;
  bool require_finality_for_actions = true;
  std::uint64_t min_confirmations_for_enforcement = 6;
  std::size_t max_flags_before_auto_hide = 3;
  std::string role_model = "single-signer";
  std::vector<std::string> moderator_cids;
};

struct ModerationObjectState {
  std::string object_id;
  std::size_t flag_count = 0;
  bool hidden = false;
  bool auto_hidden = false;
  bool core_topic_pinned = false;
};

struct ModerationStatus {
  bool enabled = false;
  ModerationPolicy policy{};
  std::vector<std::string> active_moderators;
  std::vector<ModerationObjectState> objects;
  std::size_t invalid_event_count = 0;
};

struct GenesisSpec {
  std::string chain_id;
  std::string network_id;
  std::string psz_timestamp;
  std::string merkle_root;
  std::string block_hash;
  std::vector<std::string> seed_peers;
  std::vector<InitialAllocation> initial_allocations;
};

struct WalletStatus {
  bool locked = false;
  bool destroyed = false;
  bool recovery_required = false;
  std::string vault_path;
  std::string backup_last_path;
  std::int64_t last_unlocked_unix = 0;
  std::int64_t last_locked_unix = 0;
};

struct InitConfig {
  std::string app_data_dir;
  std::string passphrase;
  AnonymityMode mode = AnonymityMode::Tor;
  std::vector<std::string> seed_peers;
  std::vector<std::string> seed_peers_mainnet;
  std::vector<std::string> seed_peers_testnet;

  bool alpha_test_mode = false;
  std::string peers_dat_path;
  std::string community_profile_path;
  bool production_swap = true;
  std::uint64_t block_interval_seconds = 25;
  std::uint64_t validation_interval_ticks = 10;
  std::int64_t block_reward_units = 50;
  std::int64_t minimum_post_value = 0;
  std::string genesis_psz_timestamp;
  std::string mainnet_chain_id = "got-soup-mainnet-v1";
  std::string testnet_chain_id = "got-soup-testnet-v1";
  std::string mainnet_genesis_psz_timestamp =
      "Got Soup::P2P Tomato Soup mainnet genesis | 2026-02-14";
  std::string testnet_genesis_psz_timestamp =
      "Got Soup::P2P Tomato Soup testnet genesis | 2026-02-14";
  std::string mainnet_genesis_merkle_root =
      "31fa9d91e27f722cada145e858f90dcec257d92d2f9105cb4df7a88f3bf0b5f4";
  std::string testnet_genesis_merkle_root =
      "15857bf7a332e27ac17388b05300a0b3b493f0fda96e1dae3e2b9fec3fb8b6bd";
  std::string mainnet_genesis_block_hash =
      "e96890f8c3254ed8926ab119747931cd4f595ccdde71badc857bb2ba7e78b50d";
  std::string testnet_genesis_block_hash =
      "ead35284e7ce7d379a08e0555e70a6e238a652e6fbdbae6a6b3fbfaf5eb4cd30";
  std::vector<InitialAllocation> mainnet_initial_allocations;
  std::vector<InitialAllocation> testnet_initial_allocations;

  ChainPolicy chain_policy{};
  ValidationLimits validation_limits{};
  std::uint64_t pow_target_solve_seconds = 0;  // 0 -> follows block_interval_seconds
  std::uint64_t pow_retarget_window_claims = 120;
  int pow_min_difficulty_nibbles = 1;
  int pow_max_difficulty_nibbles = 12;
  int pow_mainnet_initial_difficulty_nibbles = 4;
  int pow_testnet_initial_difficulty_nibbles = 3;
  ModerationPolicy default_moderation_policy{};
  std::vector<std::string> default_moderators;
  std::uint32_t blockdata_format_version = 2;
  bool enable_snapshots = true;
  std::uint64_t snapshot_interval_blocks = 128;
  bool enable_pruning = false;
  std::uint64_t prune_keep_recent_blocks = 4096;
  std::uint16_t p2p_mainnet_port = 4001;
  std::uint16_t p2p_testnet_port = 14001;
};

}  // namespace alpha

#pragma once

#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "core/model/types.hpp"

namespace alpha {

class Store {
public:
  struct BlockRecord {
    std::uint64_t index = 0;
    std::int64_t opened_unix = 0;
    bool reserved = true;
    bool confirmed = false;
    bool backfilled = false;
    std::vector<std::string> event_ids;
    std::string psz_timestamp;
    std::string prev_hash;
    std::string merkle_root;
    std::string content_hash;
    std::string block_hash;
  };

  Result open(std::string_view app_data_dir, std::string_view vault_key);
  void set_block_timing(std::uint64_t block_interval_seconds);
  void set_genesis_psz_timestamp(std::string_view psz_timestamp);
  void set_block_reward_units(std::int64_t units);
  void set_chain_identity(std::string_view chain_id, std::string_view network_id);
  void set_genesis_hashes(std::string_view merkle_root, std::string_view block_hash);
  void set_chain_policy(const ChainPolicy& policy);
  void set_validation_limits(const ValidationLimits& limits);
  void set_moderation_policy(const ModerationPolicy& policy);
  void set_state_options(std::uint32_t blockdata_format_version, bool enable_snapshots,
                         std::uint64_t snapshot_interval_blocks, bool enable_pruning,
                         std::uint64_t prune_keep_recent_blocks);

  Result append_event(const EventEnvelope& event);
  [[nodiscard]] bool has_event(std::string_view event_id) const;

  Result materialize_views();
  Result routine_block_check(std::int64_t now_unix);
  Result backtest_validate(const std::function<std::string(std::string_view)>& content_id_fn,
                           std::string_view expected_community_id);

  [[nodiscard]] std::vector<RecipeSummary> query_recipes(const SearchQuery& query) const;
  [[nodiscard]] std::vector<ThreadSummary> query_threads(std::string_view recipe_id) const;
  [[nodiscard]] std::vector<ReplySummary> query_replies(std::string_view thread_id) const;
  [[nodiscard]] std::optional<BlockRecord> block_for_event(std::string_view event_id) const;
  [[nodiscard]] std::optional<std::string> confirmation_for_object(std::string_view object_id) const;
  [[nodiscard]] std::int64_t reward_balance(std::string_view cid) const;
  [[nodiscard]] std::vector<RewardBalanceSummary> reward_balances() const;
  [[nodiscard]] std::vector<BlockRecord> claimable_confirmed_blocks(std::string_view cid) const;
  [[nodiscard]] bool has_block_claim(std::uint64_t block_index) const;
  [[nodiscard]] std::int64_t next_claim_reward(std::uint64_t block_index) const;
  [[nodiscard]] std::uint64_t next_transfer_nonce(std::string_view cid) const;
  [[nodiscard]] std::int64_t transfer_burn_fee(std::int64_t amount) const;
  [[nodiscard]] ModerationStatus moderation_status() const;
  [[nodiscard]] bool is_moderator(std::string_view cid) const;

  [[nodiscard]] const std::vector<EventEnvelope>& all_events() const { return events_; }
  [[nodiscard]] const std::vector<BlockRecord>& all_blocks() const { return blocks_; }
  [[nodiscard]] std::string schema_sql() const;
  [[nodiscard]] DbHealthReport health_report() const;

private:
  std::string app_data_dir_;
  std::string event_log_path_;
  std::string block_log_path_;
  std::string invalid_event_log_path_;
  std::string snapshot_path_;
  std::string checkpoints_path_;

  std::vector<EventEnvelope> events_;
  std::vector<BlockRecord> blocks_;
  std::unordered_map<std::string, std::size_t> event_to_block_;
  std::unordered_map<std::string, RecipeSummary> recipes_;
  std::unordered_map<std::string, ThreadSummary> threads_;
  std::unordered_map<std::string, std::vector<ReplySummary>> replies_by_thread_;
  std::unordered_map<std::string, std::pair<int, int>> review_totals_;
  std::unordered_map<std::string, int> thumbs_up_totals_;
  std::unordered_map<std::string, std::int64_t> reward_balances_;
  std::unordered_map<std::uint64_t, std::string> claimed_blocks_;
  std::unordered_map<std::string, std::uint64_t> transfer_nonce_by_cid_;
  std::unordered_map<std::string, std::string> invalid_economic_events_;
  std::unordered_map<std::string, std::string> invalid_moderation_events_;
  std::unordered_set<std::string> moderators_;
  std::unordered_map<std::string, std::size_t> moderation_flag_counts_;
  std::unordered_set<std::string> moderation_hidden_objects_;
  std::unordered_set<std::string> moderation_auto_hidden_objects_;
  std::unordered_map<std::string, bool> moderation_core_topic_overrides_;
  std::int64_t issued_reward_total_ = 0;
  std::int64_t burned_fee_total_ = 0;
  std::uint64_t block_interval_seconds_ = 150;
  std::int64_t block_reward_units_ = 115;
  std::int64_t max_token_supply_units_ = 69359946;
  long double per_block_subsidy_decay_fraction_ = 0.0000016435998841934918L;
  std::int64_t min_subsidy_units_ = 1;
  std::uint64_t difficulty_adjustment_interval_blocks_ = 864;
  int pow_difficulty_nibbles_ = 4;
  std::string chain_id_ = "got-soup-mainnet-v1";
  std::string network_id_ = "mainnet";
  std::string genesis_psz_timestamp_;
  std::string hardcoded_genesis_merkle_root_;
  std::string hardcoded_genesis_block_hash_;
  ChainPolicy chain_policy_{};
  ValidationLimits validation_limits_{};
  ModerationPolicy moderation_policy_{};
  std::uint32_t blockdata_format_version_ = 2;
  bool enable_snapshots_ = true;
  std::uint64_t snapshot_interval_blocks_ = 128;
  bool enable_pruning_ = false;
  std::uint64_t prune_keep_recent_blocks_ = 4096;
  std::size_t invalid_event_drop_count_ = 0;
  bool recovered_from_corruption_ = false;
  std::size_t checkpoint_count_ = 0;
  std::int64_t last_snapshot_unix_ = 0;
  std::int64_t last_prune_unix_ = 0;
  bool backtest_ok_ = false;
  std::string backtest_details_ = "Backtest has not run.";
  std::int64_t last_backtest_unix_ = 0;

  Result load_event_log();
  Result persist_event(const EventEnvelope& event) const;
  Result load_block_log();
  Result persist_block_log() const;
  Result persist_snapshot();
  Result persist_checkpoints();
  void prune_blocks_if_needed();
  void ensure_genesis_block(std::int64_t now_unix);
  void ensure_block_slots_until(std::int64_t now_unix);
  void assign_unassigned_events_to_blocks();
  void rebuild_event_to_block_index();
  void recompute_block_hashes();
  [[nodiscard]] std::int64_t scheduled_reward_for_block(std::uint64_t block_index) const;
  [[nodiscard]] std::int64_t expected_claim_reward_for_block(std::uint64_t block_index,
                                                             std::int64_t issued_so_far) const;
  [[nodiscard]] std::int64_t transfer_burn_fee_internal(std::int64_t amount) const;
  [[nodiscard]] std::optional<std::uint64_t> latest_confirmed_block_index() const;
  [[nodiscard]] std::optional<std::pair<std::uint64_t, std::int64_t>>
  confirmation_metrics_for_event(std::string_view source_event_id, std::int64_t updated_unix) const;
  void apply_confirmation_metrics();
  [[nodiscard]] std::string consensus_hash() const;
  [[nodiscard]] std::string timeline_hash() const;
  [[nodiscard]] std::size_t block_event_bytes(const BlockRecord& block) const;
  void record_invalid_event(std::string_view event_id, std::string_view reason);
};

}  // namespace alpha

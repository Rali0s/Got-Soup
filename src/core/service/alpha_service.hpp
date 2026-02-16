#pragma once

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "core/crypto/crypto.hpp"
#include "core/model/types.hpp"
#include "core/p2p/node.hpp"
#include "core/reference_engine.hpp"
#include "core/storage/store.hpp"
#include "core/transport/anonymity_provider.hpp"

namespace alpha {

struct NodeStatusReport {
  AnonymityStatus tor;
  AnonymityStatus i2p;
  bool tor_enabled = false;
  bool i2p_enabled = false;
  AnonymityMode active_mode = AnonymityMode::Tor;
  bool alpha_test_mode = false;

  NodeRuntimeStats p2p;
  DbHealthReport db;
  std::int64_t local_reward_balance = 0;
  std::vector<RewardBalanceSummary> reward_balances;
  ModerationStatus moderation;
  std::uint16_t p2p_mainnet_port = 4001;
  std::uint16_t p2p_testnet_port = 14001;
  std::string data_dir;
  ChainPolicy chain_policy;
  ValidationLimits validation_limits;
  GenesisSpec genesis;
  WalletStatus wallet;

  std::string peers_dat_path;
  std::vector<std::string> peers;

  CommunityProfile community;
  std::vector<CommunityProfile> known_communities;

  std::string core_phase_status;
};

class AlphaService {
public:
  Result init(const InitConfig& config);

  Result create_recipe(const RecipeDraft& draft);
  Result create_thread(const ThreadDraft& draft);
  Result create_reply(const ReplyDraft& draft);
  Result add_review(const ReviewDraft& draft);
  Result add_thumb_up(std::string_view recipe_id);
  Result transfer_rewards(const RewardTransferDraft& draft);
  Result transfer_rewards_to_address(const RewardTransferAddressDraft& draft);

  std::vector<RecipeSummary> search(const SearchQuery& query) const;
  std::vector<ThreadSummary> threads(std::string_view recipe_id) const;
  std::vector<ReplySummary> replies(std::string_view thread_id) const;
  std::vector<RewardTransactionSummary> reward_transactions() const;

  std::vector<EventEnvelope> sync_tick();
  Result ingest_remote_event(const EventEnvelope& event);

  Result set_transport_enabled(AnonymityMode mode, bool enabled);
  Result set_active_transport(AnonymityMode mode);
  Result set_alpha_test_mode(bool enabled);

  Result add_peer(std::string_view peer);
  Result reload_peers_dat();

  Result set_profile_display_name(std::string_view display_name);
  Result set_immortal_name_with_cipher(std::string_view display_name, std::string_view cipher_password,
                                       std::string_view cipher_salt);
  Result set_duplicate_name_policy(bool reject_duplicates);
  Result set_profile_cipher_password(std::string_view password, std::string_view salt);
  Result update_key_to_peers();
  Result add_moderator(std::string_view cid);
  Result remove_moderator(std::string_view cid);
  Result flag_content(std::string_view object_id, std::string_view reason);
  Result set_content_hidden(std::string_view object_id, bool hidden, std::string_view reason);
  Result pin_core_topic(std::string_view recipe_id, bool pinned);
  Result export_key_backup(std::string_view backup_path, std::string_view password, std::string_view salt);
  Result import_key_backup(std::string_view backup_path, std::string_view password);
  Result lock_wallet();
  Result unlock_wallet(std::string_view passphrase);
  Result recover_wallet(std::string_view backup_path, std::string_view backup_password,
                        std::string_view new_local_passphrase);
  Result nuke_key(std::string_view confirmation_phrase);
  Result run_backtest_validation();

  Result use_community_profile(std::string_view community_or_path, std::string_view display_name,
                               std::string_view description);

  [[nodiscard]] ProfileSummary profile() const;
  [[nodiscard]] AnonymityStatus anonymity_status() const;
  [[nodiscard]] NodeStatusReport node_status() const;
  [[nodiscard]] std::int64_t local_reward_balance() const;
  [[nodiscard]] std::vector<RewardBalanceSummary> reward_balances() const;
  [[nodiscard]] ReceiveAddressInfo receive_info() const;
  [[nodiscard]] std::string hashspec_console() const;
  [[nodiscard]] std::string soup_address() const;
  [[nodiscard]] std::string public_key() const;
  [[nodiscard]] std::string private_key() const;
  [[nodiscard]] MessageSignatureSummary sign_message(std::string_view message) const;
  [[nodiscard]] bool verify_message_signature(std::string_view message, std::string_view signature,
                                              std::string_view public_key) const;
  [[nodiscard]] ModerationStatus moderation_status() const;

  [[nodiscard]] std::vector<CommunityProfile> community_profiles() const;
  [[nodiscard]] CommunityProfile current_community() const { return current_community_; }

  [[nodiscard]] std::vector<std::string> reference_parent_menus() const;
  [[nodiscard]] std::vector<std::string> reference_secondary_menus(std::string_view parent) const;
  [[nodiscard]] std::vector<std::string> reference_openings(std::string_view parent,
                                                            std::string_view secondary,
                                                            std::string_view query) const;
  [[nodiscard]] std::optional<refpad::WikiEntry> reference_lookup(std::string_view key) const;

private:
  Result append_locally_and_queue(const EventEnvelope& event);
  EventEnvelope make_event(EventKind kind,
                           std::vector<std::pair<std::string, std::string>> payload_fields);
  Result try_claim_confirmed_block_rewards();
  Result validate_and_apply_post_cost(std::int64_t requested_units, std::int64_t& out_applied_units) const;
  std::optional<std::string> resolve_display_name_to_cid(std::string_view display_name) const;
  std::optional<std::string> resolve_address_to_cid(std::string_view address) const;

  Result restart_network();
  Result ensure_provider_state(AnonymityMode mode, bool enabled);
  ProxyEndpoint active_proxy_endpoint() const;

  Result load_or_create_community_profile(std::string_view profile_path_or_id,
                                          std::string_view display_name,
                                          std::string_view description);
  std::optional<CommunityProfile> parse_community_profile_file(std::string_view path) const;
  Result write_community_profile_file(const CommunityProfile& profile) const;
  std::string sanitize_community_id(std::string_view id) const;
  std::string sanitize_display_name(std::string_view value) const;
  std::string resolve_data_path(std::string_view input_path, std::string_view fallback_name) const;
  std::string sanitize_cid(std::string_view cid) const;
  bool is_local_moderator() const;
  Result ensure_local_moderator(std::string_view operation) const;
  bool wallet_locked() const;
  Result ensure_wallet_unlocked(std::string_view operation) const;
  GenesisSpec active_genesis_spec() const;
  Result load_profile_state();
  Result save_profile_state() const;
  std::unordered_map<std::string, std::string> observed_display_names_by_cid() const;

  InitConfig config_;
  std::string communities_dir_;
  std::string peers_dat_path_;
  std::string profile_state_path_;

  bool initialized_ = false;
  bool tor_enabled_ = true;
  bool i2p_enabled_ = true;
  bool alpha_test_mode_ = false;
  AnonymityMode active_mode_ = AnonymityMode::Tor;
  bool local_display_name_immortalized_ = false;
  bool reject_duplicate_names_ = true;
  std::string local_display_name_;
  bool wallet_destroyed_ = false;
  bool wallet_recovery_required_ = false;
  std::string last_key_backup_path_;
  std::int64_t wallet_last_unlocked_unix_ = 0;
  std::int64_t wallet_last_locked_unix_ = 0;
  std::int64_t last_local_event_unix_ts_ = 0;
  std::uint64_t validation_interval_ticks_ = 10;
  std::uint64_t ticks_since_last_validation_ = 0;

  CryptoEngine crypto_;
  Store store_;
  std::unique_ptr<IAnonymityProvider> tor_provider_;
  std::unique_ptr<IAnonymityProvider> i2p_provider_;
  P2PNode p2p_node_;
  refpad::ReferenceEngine reference_engine_;
  CommunityProfile current_community_;
};

}  // namespace alpha

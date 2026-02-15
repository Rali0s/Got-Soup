#pragma once

#include <optional>
#include <string_view>
#include <vector>

#include "core/model/types.hpp"
#include "core/reference_engine.hpp"
#include "core/service/alpha_service.hpp"

namespace alpha {

class CoreApi {
public:
  Result init(const InitConfig& config);

  Result create_recipe(const RecipeDraft& draft);
  Result create_thread(const ThreadDraft& draft);
  Result create_reply(const ReplyDraft& draft);
  Result add_review(const ReviewDraft& draft);
  Result add_thumb_up(std::string_view recipe_id);
  Result transfer_rewards(const RewardTransferDraft& draft);

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

  std::vector<RecipeSummary> search(const SearchQuery& query);
  std::vector<EventEnvelope> sync_tick();

  ProfileSummary profile() const;
  AnonymityStatus anonymity_status() const;
  NodeStatusReport node_status() const;
  std::int64_t local_reward_balance() const;
  std::vector<RewardBalanceSummary> reward_balances() const;
  ModerationStatus moderation_status() const;

  std::vector<CommunityProfile> community_profiles() const;
  CommunityProfile current_community() const;

  std::vector<ThreadSummary> threads(std::string_view recipe_id) const;
  std::vector<ReplySummary> replies(std::string_view thread_id) const;

  std::vector<std::string> reference_parent_menus() const;
  std::vector<std::string> reference_secondary_menus(std::string_view parent) const;
  std::vector<std::string> reference_openings(std::string_view parent, std::string_view secondary,
                                              std::string_view query) const;
  std::optional<refpad::WikiEntry> reference_lookup(std::string_view key) const;

private:
  AlphaService service_;
};

}  // namespace alpha

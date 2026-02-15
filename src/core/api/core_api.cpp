#include "core/api/core_api.hpp"

namespace alpha {

Result CoreApi::init(const InitConfig& config) {
  return service_.init(config);
}

Result CoreApi::create_recipe(const RecipeDraft& draft) {
  return service_.create_recipe(draft);
}

Result CoreApi::create_thread(const ThreadDraft& draft) {
  return service_.create_thread(draft);
}

Result CoreApi::create_reply(const ReplyDraft& draft) {
  return service_.create_reply(draft);
}

Result CoreApi::add_review(const ReviewDraft& draft) {
  return service_.add_review(draft);
}

Result CoreApi::add_thumb_up(std::string_view recipe_id) {
  return service_.add_thumb_up(recipe_id);
}

Result CoreApi::transfer_rewards(const RewardTransferDraft& draft) {
  return service_.transfer_rewards(draft);
}

Result CoreApi::set_transport_enabled(AnonymityMode mode, bool enabled) {
  return service_.set_transport_enabled(mode, enabled);
}

Result CoreApi::set_active_transport(AnonymityMode mode) {
  return service_.set_active_transport(mode);
}

Result CoreApi::set_alpha_test_mode(bool enabled) {
  return service_.set_alpha_test_mode(enabled);
}

Result CoreApi::add_peer(std::string_view peer) {
  return service_.add_peer(peer);
}

Result CoreApi::reload_peers_dat() {
  return service_.reload_peers_dat();
}

Result CoreApi::set_profile_display_name(std::string_view display_name) {
  return service_.set_profile_display_name(display_name);
}

Result CoreApi::set_immortal_name_with_cipher(std::string_view display_name,
                                              std::string_view cipher_password,
                                              std::string_view cipher_salt) {
  return service_.set_immortal_name_with_cipher(display_name, cipher_password, cipher_salt);
}

Result CoreApi::set_duplicate_name_policy(bool reject_duplicates) {
  return service_.set_duplicate_name_policy(reject_duplicates);
}

Result CoreApi::set_profile_cipher_password(std::string_view password, std::string_view salt) {
  return service_.set_profile_cipher_password(password, salt);
}

Result CoreApi::update_key_to_peers() {
  return service_.update_key_to_peers();
}

Result CoreApi::add_moderator(std::string_view cid) {
  return service_.add_moderator(cid);
}

Result CoreApi::remove_moderator(std::string_view cid) {
  return service_.remove_moderator(cid);
}

Result CoreApi::flag_content(std::string_view object_id, std::string_view reason) {
  return service_.flag_content(object_id, reason);
}

Result CoreApi::set_content_hidden(std::string_view object_id, bool hidden, std::string_view reason) {
  return service_.set_content_hidden(object_id, hidden, reason);
}

Result CoreApi::pin_core_topic(std::string_view recipe_id, bool pinned) {
  return service_.pin_core_topic(recipe_id, pinned);
}

Result CoreApi::export_key_backup(std::string_view backup_path, std::string_view password,
                                  std::string_view salt) {
  return service_.export_key_backup(backup_path, password, salt);
}

Result CoreApi::import_key_backup(std::string_view backup_path, std::string_view password) {
  return service_.import_key_backup(backup_path, password);
}

Result CoreApi::lock_wallet() {
  return service_.lock_wallet();
}

Result CoreApi::unlock_wallet(std::string_view passphrase) {
  return service_.unlock_wallet(passphrase);
}

Result CoreApi::recover_wallet(std::string_view backup_path, std::string_view backup_password,
                               std::string_view new_local_passphrase) {
  return service_.recover_wallet(backup_path, backup_password, new_local_passphrase);
}

Result CoreApi::nuke_key(std::string_view confirmation_phrase) {
  return service_.nuke_key(confirmation_phrase);
}

Result CoreApi::run_backtest_validation() {
  return service_.run_backtest_validation();
}

Result CoreApi::use_community_profile(std::string_view community_or_path, std::string_view display_name,
                                      std::string_view description) {
  return service_.use_community_profile(community_or_path, display_name, description);
}

std::vector<RecipeSummary> CoreApi::search(const SearchQuery& query) {
  return service_.search(query);
}

std::vector<EventEnvelope> CoreApi::sync_tick() {
  return service_.sync_tick();
}

ProfileSummary CoreApi::profile() const {
  return service_.profile();
}

AnonymityStatus CoreApi::anonymity_status() const {
  return service_.anonymity_status();
}

NodeStatusReport CoreApi::node_status() const {
  return service_.node_status();
}

std::int64_t CoreApi::local_reward_balance() const {
  return service_.local_reward_balance();
}

std::vector<RewardBalanceSummary> CoreApi::reward_balances() const {
  return service_.reward_balances();
}

ModerationStatus CoreApi::moderation_status() const {
  return service_.moderation_status();
}

std::vector<CommunityProfile> CoreApi::community_profiles() const {
  return service_.community_profiles();
}

CommunityProfile CoreApi::current_community() const {
  return service_.current_community();
}

std::vector<ThreadSummary> CoreApi::threads(std::string_view recipe_id) const {
  return service_.threads(recipe_id);
}

std::vector<ReplySummary> CoreApi::replies(std::string_view thread_id) const {
  return service_.replies(thread_id);
}

std::vector<std::string> CoreApi::reference_parent_menus() const {
  return service_.reference_parent_menus();
}

std::vector<std::string> CoreApi::reference_secondary_menus(std::string_view parent) const {
  return service_.reference_secondary_menus(parent);
}

std::vector<std::string> CoreApi::reference_openings(std::string_view parent,
                                                     std::string_view secondary,
                                                     std::string_view query) const {
  return service_.reference_openings(parent, secondary, query);
}

std::optional<refpad::WikiEntry> CoreApi::reference_lookup(std::string_view key) const {
  return service_.reference_lookup(key);
}

}  // namespace alpha

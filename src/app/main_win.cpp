#include <windows.h>
#include <commctrl.h>

#include <algorithm>
#include <string>
#include <string_view>
#include <vector>

#include "core/api/core_api.hpp"
#include "core/model/app_meta.hpp"

namespace {

constexpr wchar_t kWindowClassName[] = L"GotSoupMainWindow";
constexpr wchar_t kWindowTitle[] = L"got-soup::P2P Tomato Soup - Recipe Forum";

constexpr int kSearchEditId = 1001;
constexpr int kCloseButtonId = 1002;
constexpr int kParentMenuId = 1003;
constexpr int kSecondaryMenuId = 1004;
constexpr int kOpeningListId = 1005;
constexpr int kMainTabsId = 1006;
constexpr int kRecipesListId = 1007;
constexpr int kRecipeDetailId = 1008;
constexpr int kRecipeThumbUpId = 1033;
constexpr int kRecipeRateComboId = 1034;
constexpr int kRecipeRateButtonId = 1035;
constexpr int kForumViewId = 1009;
constexpr int kForumThreadTitleId = 1010;
constexpr int kForumThreadBodyId = 1011;
constexpr int kForumCreateThreadId = 1012;
constexpr int kForumReplyBodyId = 1013;
constexpr int kForumCreateReplyId = 1014;
constexpr int kUploadTitleId = 1015;
constexpr int kUploadCategoryId = 1016;
constexpr int kUploadBodyId = 1017;
constexpr int kUploadSubmitId = 1018;
constexpr int kProfileViewId = 1019;
constexpr int kProfileNameEditId = 1037;
constexpr int kProfileSetNameId = 1038;
constexpr int kProfileDupPolicyToggleId = 1039;
constexpr int kProfileApplyPolicyId = 1040;
constexpr int kProfileCipherPasswordId = 1041;
constexpr int kProfileCipherSaltId = 1042;
constexpr int kProfileCipherApplyId = 1043;
constexpr int kProfileUpdateKeyId = 1044;
constexpr int kProfileExportPathId = 1045;
constexpr int kProfileExportPasswordId = 1046;
constexpr int kProfileExportSaltId = 1047;
constexpr int kProfileExportButtonId = 1048;
constexpr int kProfileImportPathId = 1049;
constexpr int kProfileImportPasswordId = 1050;
constexpr int kProfileImportButtonId = 1051;
constexpr int kProfileNukeButtonId = 1052;
constexpr int kRewardsViewId = 1053;
constexpr int kSettingsViewId = 1054;
constexpr int kSettingsLockWalletId = 1055;
constexpr int kSettingsUnlockPassId = 1056;
constexpr int kSettingsUnlockWalletId = 1057;
constexpr int kSettingsRecoverPathId = 1058;
constexpr int kSettingsRecoverBackupPassId = 1059;
constexpr int kSettingsRecoverLocalPassId = 1060;
constexpr int kSettingsRecoverWalletId = 1061;
constexpr int kSettingsValidateNowId = 1062;
constexpr int kAboutViewId = 1020;
constexpr int kNodeStatusViewId = 1021;
constexpr int kNodeTorToggleId = 1022;
constexpr int kNodeI2PToggleId = 1023;
constexpr int kNodeLocalhostToggleId = 1024;
constexpr int kNodeModeComboId = 1025;
constexpr int kNodeApplyId = 1026;
constexpr int kNodeRefreshId = 1027;
constexpr int kNodePeerEditId = 1028;
constexpr int kNodeAddPeerId = 1029;
constexpr int kNodeCommunityIdId = 1030;
constexpr int kNodeCommunityNameId = 1031;
constexpr int kNodeCommunityApplyId = 1032;

constexpr int kMenuCloseId = 2001;
constexpr int kMenuAboutId = 2002;

enum class TabIndex : int {
  Recipes = 0,
  Forum = 1,
  Upload = 2,
  Profile = 3,
  Rewards = 4,
  NodeStatus = 5,
  Settings = 6,
  About = 7,
};

struct AppState {
  alpha::CoreApi api;

  std::vector<std::string> opening_keys;
  std::vector<alpha::RecipeSummary> recipes;

  HWND search_edit = nullptr;
  HWND close_button = nullptr;
  HWND parent_menu = nullptr;
  HWND secondary_menu = nullptr;
  HWND opening_list = nullptr;

  HWND tab_control = nullptr;
  HWND recipes_list = nullptr;
  HWND recipe_detail = nullptr;
  HWND recipe_thumb_up = nullptr;
  HWND recipe_rate_combo = nullptr;
  HWND recipe_rate_button = nullptr;
  HWND forum_view = nullptr;
  HWND forum_thread_title = nullptr;
  HWND forum_thread_body = nullptr;
  HWND forum_create_thread = nullptr;
  HWND forum_reply_body = nullptr;
  HWND forum_create_reply = nullptr;
  HWND upload_title = nullptr;
  HWND upload_category = nullptr;
  HWND upload_body = nullptr;
  HWND upload_submit = nullptr;
  HWND profile_view = nullptr;
  HWND profile_name_edit = nullptr;
  HWND profile_set_name_button = nullptr;
  HWND profile_duplicate_policy_toggle = nullptr;
  HWND profile_apply_policy_button = nullptr;
  HWND profile_cipher_password_edit = nullptr;
  HWND profile_cipher_salt_edit = nullptr;
  HWND profile_cipher_apply_button = nullptr;
  HWND profile_update_key_button = nullptr;
  HWND profile_export_path_edit = nullptr;
  HWND profile_export_password_edit = nullptr;
  HWND profile_export_salt_edit = nullptr;
  HWND profile_export_button = nullptr;
  HWND profile_import_path_edit = nullptr;
  HWND profile_import_password_edit = nullptr;
  HWND profile_import_button = nullptr;
  HWND profile_nuke_button = nullptr;
  HWND rewards_view = nullptr;
  HWND about_view = nullptr;
  HWND settings_view = nullptr;
  HWND settings_lock_wallet_button = nullptr;
  HWND settings_unlock_password_edit = nullptr;
  HWND settings_unlock_wallet_button = nullptr;
  HWND settings_recover_path_edit = nullptr;
  HWND settings_recover_backup_password_edit = nullptr;
  HWND settings_recover_local_password_edit = nullptr;
  HWND settings_recover_wallet_button = nullptr;
  HWND settings_validate_now_button = nullptr;

  HWND node_status_view = nullptr;
  HWND node_tor_toggle = nullptr;
  HWND node_i2p_toggle = nullptr;
  HWND node_localhost_toggle = nullptr;
  HWND node_mode_combo = nullptr;
  HWND node_apply_button = nullptr;
  HWND node_refresh_button = nullptr;
  HWND node_peer_edit = nullptr;
  HWND node_peer_add_button = nullptr;
  HWND node_community_id_edit = nullptr;
  HWND node_community_name_edit = nullptr;
  HWND node_community_apply_button = nullptr;
};

std::wstring utf8_to_wide(std::string_view text) {
  if (text.empty()) {
    return L"";
  }

  const int required =
      MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
  if (required <= 0) {
    return L"";
  }

  std::wstring out(static_cast<size_t>(required), L'\0');
  MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), out.data(), required);
  return out;
}

std::string wide_to_utf8(const std::wstring& text) {
  if (text.empty()) {
    return {};
  }

  const int required =
      WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0,
                          nullptr, nullptr);
  if (required <= 0) {
    return {};
  }

  std::string out(static_cast<size_t>(required), '\0');
  WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), out.data(), required,
                      nullptr, nullptr);
  return out;
}

std::wstring read_window_text(HWND control) {
  const int len = GetWindowTextLengthW(control);
  if (len <= 0) {
    return L"";
  }

  std::wstring out(static_cast<size_t>(len + 1), L'\0');
  GetWindowTextW(control, out.data(), len + 1);
  out.resize(static_cast<size_t>(len));
  return out;
}

std::string selected_combo_text(HWND control) {
  const LRESULT selected = SendMessageW(control, CB_GETCURSEL, 0, 0);
  if (selected == CB_ERR) {
    return {};
  }

  const LRESULT len = SendMessageW(control, CB_GETLBTEXTLEN, selected, 0);
  if (len == CB_ERR || len <= 0) {
    return {};
  }

  std::wstring out(static_cast<size_t>(len + 1), L'\0');
  SendMessageW(control, CB_GETLBTEXT, selected, reinterpret_cast<LPARAM>(out.data()));
  out.resize(static_cast<size_t>(len));
  return wide_to_utf8(out);
}

void set_edit_text(HWND control, std::string_view text) {
  const std::wstring wide = utf8_to_wide(text);
  SetWindowTextW(control, wide.c_str());
}

bool checkbox_checked(HWND control) {
  return SendMessageW(control, BM_GETCHECK, 0, 0) == BST_CHECKED;
}

void set_checkbox(HWND control, bool checked) {
  SendMessageW(control, BM_SETCHECK, checked ? BST_CHECKED : BST_UNCHECKED, 0);
}

void set_combo_to_mode(HWND combo, alpha::AnonymityMode mode) {
  const std::wstring target = (mode == alpha::AnonymityMode::I2P) ? L"I2P" : L"Tor";
  const LRESULT count = SendMessageW(combo, CB_GETCOUNT, 0, 0);
  for (LRESULT i = 0; i < count; ++i) {
    const LRESULT text_len = SendMessageW(combo, CB_GETLBTEXTLEN, i, 0);
    if (text_len <= 0) {
      continue;
    }

    std::wstring buffer(static_cast<size_t>(text_len + 1), L'\0');
    SendMessageW(combo, CB_GETLBTEXT, i, reinterpret_cast<LPARAM>(buffer.data()));
    buffer.resize(static_cast<size_t>(text_len));
    if (buffer == target) {
      SendMessageW(combo, CB_SETCURSEL, i, 0);
      return;
    }
  }
}

std::string build_about_text(const AppState* state) {
  const auto node = state->api.node_status();
  const std::string about_png = node.data_dir + "/assets/about.png";
  const std::string splash_png = node.data_dir + "/assets/tomato_soup.png";

  std::string about_text;
  about_text += std::string{alpha::kAppDisplayName} + "\r\n\r\n";
  about_text += "Current Tor Version: " + node.tor.version + "\r\n";
  about_text += "Current I2P Version: " + node.i2p.version + "\r\n";
  about_text += "Current P2P:Soup Version Build Release: " + std::string{alpha::kAppVersion} +
                " (" + std::string{alpha::kBuildRelease} + ")\r\n";
  about_text += "Authors: " + std::string{alpha::kAuthorList} + "\r\n\r\n";
  about_text += "Credits\r\n";
  about_text += "- Core: C++23 modular alpha_core\r\n";
  about_text += "- UI: Native Win32 / Cocoa / GTK path\r\n";
  about_text += "- Planned deps: libp2p, libsodium, SQLCipher, libtor, i2pd\r\n\r\n";
  about_text += "Assets\r\n";
  about_text += "- About PNG (transparent): " + about_png + "\r\n";
  about_text += "- Splash PNG: " + splash_png + "\r\n\r\n";
  about_text += "Chain\r\n";
  about_text += "- Chain ID: " + node.genesis.chain_id + "\r\n";
  about_text += "- Network: " + node.genesis.network_id + "\r\n";
  about_text += "- Genesis Merkle: " + node.genesis.merkle_root + "\r\n";
  about_text += "- Genesis Block: " + node.genesis.block_hash + "\r\n\r\n";
  about_text += "Core Phase 1:\r\n" + node.core_phase_status + "\r\n";
  return about_text;
}

const alpha::RecipeSummary* selected_recipe(const AppState* state) {
  const LRESULT index = SendMessageW(state->recipes_list, LB_GETCURSEL, 0, 0);
  if (index == LB_ERR || static_cast<size_t>(index) >= state->recipes.size()) {
    return nullptr;
  }
  return &state->recipes[static_cast<size_t>(index)];
}

void refresh_profile_and_about(AppState* state) {
  const auto profile = state->api.profile();
  const auto node = state->api.node_status();

  set_edit_text(state->profile_name_edit, profile.display_name);
  set_checkbox(state->profile_duplicate_policy_toggle, profile.reject_duplicate_names);

  std::string profile_text;
  profile_text += "CID: " + profile.cid.value + "\r\n";
  profile_text += "Display Name: " + profile.display_name + "\r\n\r\n";
  profile_text += "Display Name State: ";
  profile_text += (profile.display_name_immortalized ? "IMMORTALIZED" : "not set");
  profile_text += "\r\n";
  profile_text += "Duplicate Name Policy: ";
  profile_text += (profile.reject_duplicate_names ? "REJECT" : "ALLOW");
  profile_text += "\r\n";
  profile_text += "Duplicate State: ";
  profile_text += (profile.duplicate_name_detected ? "DUPLICATE DETECTED" : "UNIQUE");
  profile_text += " (count=" + std::to_string(profile.duplicate_name_count) + ")\r\n\r\n";
  profile_text += "Bio:\r\n" + profile.bio_markdown + "\r\n\r\n";
  profile_text += "Community: " + node.community.community_id + "\r\n";
  profile_text += "Community Profile: " + node.community.profile_path + "\r\n";
  profile_text += "Active Transport: ";
  profile_text += (node.active_mode == alpha::AnonymityMode::I2P ? "I2P" : "Tor");
  profile_text += "\r\n";
  set_edit_text(state->profile_view, profile_text);

  set_edit_text(state->about_view, build_about_text(state));
}

void refresh_forum_view(AppState* state) {
  std::string forum;
  forum += "Forum Summary\r\n\r\n";

  const alpha::RecipeSummary* recipe = selected_recipe(state);
  if (recipe == nullptr) {
    forum += "Select a recipe in Recipes tab to inspect forum threads.\r\n";
    set_edit_text(state->forum_view, forum);
    return;
  }

  const auto threads = state->api.threads(recipe->recipe_id);

  forum += "Recipe: " + recipe->title + "\r\n";
  forum += "Recipe ID: " + recipe->recipe_id + "\r\n";
  forum += "Segment: ";
  forum += (recipe->core_topic ? "CORE TOPIC" : "COMMUNITY POST");
  forum += "\r\n";
  forum += "Threads: " + std::to_string(threads.size()) + "\r\n\r\n";

  for (const auto& thread : threads) {
    forum += "- " + thread.title + " [thread_id=" + thread.thread_id + "]";
    forum += " (replies: " + std::to_string(thread.reply_count) + ")\r\n";
  }

  if (threads.empty()) {
    forum += "No threads yet. Use the fields below to create a thread.\r\n";
  } else {
    const auto latest_replies = state->api.replies(threads.front().thread_id);
    forum += "\r\nLatest thread target for reply: " + threads.front().thread_id + "\r\n";
    if (latest_replies.empty()) {
      forum += "No replies yet. Use the reply field below.\r\n";
    } else {
      forum += "Replies in latest thread:\r\n";
      for (const auto& reply : latest_replies) {
        forum += "  * [" + reply.reply_id + "] " + reply.author_cid + "\r\n";
      }
    }
  }

  set_edit_text(state->forum_view, forum);
}

void refresh_recipe_detail(AppState* state) {
  std::string detail;
  const alpha::RecipeSummary* recipe = selected_recipe(state);
  if (recipe != nullptr) {
    detail += "Recipe\r\n";
    detail += "ID: " + recipe->recipe_id + "\r\n";
    detail += "Title: " + recipe->title + "\r\n";
    detail += "Category: " + recipe->category + "\r\n";
    detail += "Segment: ";
    detail += (recipe->core_topic ? "CORE TOPIC" : "COMMUNITY POST");
    detail += "\r\n";
    detail += "Menu Segment: " + recipe->menu_segment + "\r\n";
    detail += "Author CID: " + recipe->author_cid + "\r\n";
    detail += "Thumbs Up: " + std::to_string(recipe->thumbs_up_count) + "\r\n";
    detail += "Rating: " + std::to_string(recipe->average_rating) + " (" +
              std::to_string(recipe->review_count) + " review(s))\r\n\r\n";
  } else {
    detail += "No recipe selected.\r\n\r\n";
  }

  const LRESULT wiki_index = SendMessageW(state->opening_list, LB_GETCURSEL, 0, 0);
  if (wiki_index != LB_ERR && static_cast<size_t>(wiki_index) < state->opening_keys.size()) {
    const auto& key = state->opening_keys[static_cast<size_t>(wiki_index)];
    const auto wiki = state->api.reference_lookup(key);
    if (wiki.has_value()) {
      detail += "Internal Reference\r\n";
      detail += "[" + wiki->key + "] " + wiki->title + "\r\n\r\n";
      detail += wiki->body;
    }
  }

  set_edit_text(state->recipe_detail, detail);
}

void refresh_recipe_list(AppState* state) {
  const std::string query = wide_to_utf8(read_window_text(state->search_edit));
  state->recipes = state->api.search({.text = query, .category = {}});

  SendMessageW(state->recipes_list, LB_RESETCONTENT, 0, 0);
  for (const auto& recipe : state->recipes) {
    std::string line;
    line += recipe.core_topic ? "[CORE] " : "[POST] ";
    line += recipe.title + " [" + recipe.category + "]";
    line += " 👍" + std::to_string(recipe.thumbs_up_count);
    const std::wstring wide = utf8_to_wide(line);
    SendMessageW(state->recipes_list, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(wide.c_str()));
  }

  if (!state->recipes.empty()) {
    SendMessageW(state->recipes_list, LB_SETCURSEL, 0, 0);
  }

  refresh_recipe_detail(state);
  refresh_forum_view(state);
}

void refresh_rewards_view(AppState* state);
void refresh_settings_view(AppState* state);

void refresh_node_status_view(AppState* state) {
  (void)state->api.sync_tick();
  const auto node = state->api.node_status();

  set_checkbox(state->node_tor_toggle, node.tor_enabled);
  set_checkbox(state->node_i2p_toggle, node.i2p_enabled);
  set_checkbox(state->node_localhost_toggle, node.alpha_test_mode);
  set_combo_to_mode(state->node_mode_combo, node.active_mode);

  std::string text;
  text += "Node Status\r\n\r\n";
  text += "Core Phase: " + node.core_phase_status + "\r\n\r\n";

  text += "Active transport: ";
  text += (node.active_mode == alpha::AnonymityMode::I2P ? "I2P" : "Tor");
  text += "\r\n";
  text += "Alpha test mode: ";
  text += (node.alpha_test_mode ? "ON" : "OFF");
  text += "\r\n\r\n";

  text += "Tor: ";
  text += (node.tor.running ? "running" : "stopped");
  text += " | updates=" + std::to_string(node.tor.update_count);
  text += " | endpoint=" + node.tor.endpoint.host + ":" + std::to_string(node.tor.endpoint.port);
  text += "\r\n";
  text += "  " + node.tor.details + "\r\n";

  text += "I2P: ";
  text += (node.i2p.running ? "running" : "stopped");
  text += " | updates=" + std::to_string(node.i2p.update_count);
  text += " | endpoint=" + node.i2p.endpoint.host + ":" + std::to_string(node.i2p.endpoint.port);
  text += "\r\n";
  text += "  " + node.i2p.details + "\r\n\r\n";

  text += "P2P runtime: ";
  text += (node.p2p.running ? "running" : "stopped");
  text += "\r\n";
  text += "Network: " + node.p2p.network + "\r\n";
  text += "Configured Ports: mainnet=" + std::to_string(node.p2p_mainnet_port);
  text += " testnet=" + std::to_string(node.p2p_testnet_port) + "\r\n";
  text += "Bind: " + node.p2p.bind_host + ":" + std::to_string(node.p2p.bind_port) + "\r\n";
  text += "Proxy Port: " + std::to_string(node.p2p.proxy_port) + "\r\n";
  text += "Peers: " + std::to_string(node.p2p.peer_count) + "\r\n";
  text += "Outbound queue: " + std::to_string(node.p2p.outbound_queue) + "\r\n";
  text += "Seen events: " + std::to_string(node.p2p.seen_event_count) + "\r\n";
  text += "Sync ticks: " + std::to_string(node.p2p.sync_tick_count) + "\r\n\r\n";

  text += "Peers.dat: " + node.peers_dat_path + "\r\n";
  for (const auto& peer : node.peers) {
    text += "- " + peer + "\r\n";
  }
  text += "\r\n";

  text += "DB health: ";
  text += (node.db.healthy ? "healthy" : "warning");
  text += "\r\n";
  text += "DB details: " + node.db.details + "\r\n";
  text += "Events: " + std::to_string(node.db.event_count);
  text += " | Recipes: " + std::to_string(node.db.recipe_count);
  text += " | Threads: " + std::to_string(node.db.thread_count);
  text += " | Replies: " + std::to_string(node.db.reply_count) + "\r\n";
  text += "Event log bytes: " + std::to_string(node.db.event_log_size_bytes) + "\r\n\r\n";

  text += "Consensus Hash: " + node.db.consensus_hash + "\r\n";
  text += "Timeline Hash: " + node.db.timeline_hash + "\r\n";
  text += "Chain ID: " + node.db.chain_id + " (" + node.db.network_id + ")\r\n";
  text += "Genesis pszTimestamp: " + node.db.genesis_psz_timestamp + "\r\n";
  text += "Latest Merkle Root: " + node.db.latest_merkle_root + "\r\n";
  text += "Blocks: " + std::to_string(node.db.block_count);
  text += " | Reserved: " + std::to_string(node.db.reserved_block_count);
  text += " | Confirmed: " + std::to_string(node.db.confirmed_block_count);
  text += " | Backfilled: " + std::to_string(node.db.backfilled_block_count) + "\r\n";
  text += "Rewards: supply=" + std::to_string(node.db.reward_supply);
  text += " | local=" + std::to_string(node.local_reward_balance);
  text += " | claims=" + std::to_string(node.db.reward_claim_event_count);
  text += " | transfers=" + std::to_string(node.db.reward_transfer_event_count) + "\r\n";
  text += "Issued=" + std::to_string(node.db.issued_reward_total);
  text += " | Burned=" + std::to_string(node.db.burned_fee_total);
  text += " | Cap=" + std::to_string(node.db.max_token_supply) + "\r\n";
  text += "Invalid economic events: " + std::to_string(node.db.invalid_economic_event_count) + "\r\n";
  text += "Dropped invalid events: " + std::to_string(node.db.invalid_event_drop_count) + "\r\n";
  text += "Block interval (sec): " + std::to_string(node.db.block_interval_seconds) + "\r\n";
  text += "Finality threshold: " + std::to_string(node.db.confirmation_threshold) + "\r\n";
  text += "Fork choice: " + node.db.fork_choice_rule + "\r\n";
  text += "Max reorg depth: " + std::to_string(node.db.max_reorg_depth) + "\r\n";
  text += "Checkpoint interval: " + std::to_string(node.db.checkpoint_interval_blocks) + "\r\n";
  text += "Checkpoint confirmations: " + std::to_string(node.db.checkpoint_confirmations) + "\r\n";
  text += "Checkpoint count: " + std::to_string(node.db.checkpoint_count) + "\r\n";
  text += "Blockdata format: v" + std::to_string(node.db.blockdata_format_version) + "\r\n";
  text += "Recovered from corruption: ";
  text += (node.db.recovered_from_corruption ? "YES" : "NO");
  text += "\r\n";
  text += "Last block unix: " + std::to_string(node.db.last_block_unix) + "\r\n";
  text += "Backtest: ";
  text += (node.db.backtest_ok ? "PASS" : "FAIL");
  text += " (last=" + std::to_string(node.db.last_backtest_unix) + ")\r\n";
  text += "Backtest details: " + node.db.backtest_details + "\r\n\r\n";

  text += "Community\r\n";
  text += "ID: " + node.community.community_id + "\r\n";
  text += "Name: " + node.community.display_name + "\r\n";
  text += "Profile file: " + node.community.profile_path + "\r\n";
  text += "Cipher key: " + node.community.cipher_key + "\r\n";
  text += "Store path: " + node.community.store_path + "\r\n";
  text += "Min Post Value: " + std::to_string(node.community.minimum_post_value) + "\r\n";
  text += "Block Reward Units: " + std::to_string(node.community.block_reward_units) + "\r\n";
  text += "\r\nWallet\r\n";
  text += "Locked: ";
  text += (node.wallet.locked ? "YES" : "NO");
  text += "\r\nDestroyed: ";
  text += (node.wallet.destroyed ? "YES" : "NO");
  text += "\r\nRecovery Required: ";
  text += (node.wallet.recovery_required ? "YES" : "NO");
  text += "\r\nVault: " + node.wallet.vault_path + "\r\n";

  if (!node.reward_balances.empty()) {
    text += "\r\nReward balances\r\n";
    for (const auto& balance : node.reward_balances) {
      std::string label = balance.display_name.empty() ? balance.cid : balance.display_name + " (" + balance.cid + ")";
      text += "- " + label + ": " + std::to_string(balance.balance) + "\r\n";
    }
  }

  if (!node.known_communities.empty()) {
    text += "\r\nKnown communities\r\n";
    for (const auto& community : node.known_communities) {
      text += "- " + community.community_id + " (" + community.profile_path + ")\r\n";
    }
  }

  set_edit_text(state->node_status_view, text);
  refresh_rewards_view(state);
  refresh_settings_view(state);
}

void refresh_rewards_view(AppState* state) {
  const auto node = state->api.node_status();

  std::string text;
  text += "Rewards (PoW)\r\n\r\n";
  text += "Network: " + node.p2p.network + "\r\n";
  text += "Block Interval (sec): " + std::to_string(node.db.block_interval_seconds) + "\r\n";
  text += "Genesis pszTimestamp: " + node.db.genesis_psz_timestamp + "\r\n";
  text += "Latest Merkle Root: " + node.db.latest_merkle_root + "\r\n\r\n";

  text += "Tokenomics\r\n";
  text += "Max Supply: " + std::to_string(node.db.max_token_supply) + "\r\n";
  text += "Issued: " + std::to_string(node.db.issued_reward_total) + "\r\n";
  text += "Burned Fees: " + std::to_string(node.db.burned_fee_total) + "\r\n";
  text += "Circulating: " + std::to_string(node.db.reward_supply) + "\r\n";
  text += "Local Balance: " + std::to_string(node.local_reward_balance) + "\r\n\r\n";

  text += "PoW Claims\r\n";
  text += "Reward Claim Events: " + std::to_string(node.db.reward_claim_event_count) + "\r\n";
  text += "Transfer Events: " + std::to_string(node.db.reward_transfer_event_count) + "\r\n";
  text += "Invalid Economic Events: " + std::to_string(node.db.invalid_economic_event_count) + "\r\n";
  text += "Finality Threshold: " + std::to_string(node.db.confirmation_threshold) + "\r\n";
  text += "Mining occurs automatically in sync ticks for confirmed unclaimed blocks.\r\n\r\n";

  text += "Balances\r\n";
  for (const auto& balance : node.reward_balances) {
    std::string label = balance.display_name.empty() ? balance.cid : balance.display_name + " (" + balance.cid + ")";
    text += "- " + label + ": " + std::to_string(balance.balance) + "\r\n";
  }

  set_edit_text(state->rewards_view, text);
}

void refresh_settings_view(AppState* state) {
  const auto node = state->api.node_status();
  std::string text;
  text += "Settings Panel\r\n\r\n";
  text += "Data Dir: " + node.data_dir + "\r\n";
  text += "Events: " + node.db.events_file + "\r\n";
  text += "Blockdata: " + node.db.blockdata_file + "\r\n";
  text += "Snapshot: " + node.db.snapshot_file + "\r\n";
  text += "Peers: " + node.peers_dat_path + "\r\n";
  text += "Vault: " + node.wallet.vault_path + "\r\n";
  text += "Backup: " + node.wallet.backup_last_path + "\r\n\r\n";

  text += "Wallet\r\n";
  text += "Locked: ";
  text += (node.wallet.locked ? "YES" : "NO");
  text += "\r\nDestroyed: ";
  text += (node.wallet.destroyed ? "YES" : "NO");
  text += "\r\nRecovery required: ";
  text += (node.wallet.recovery_required ? "YES" : "NO");
  text += "\r\nLast unlock unix: " + std::to_string(node.wallet.last_unlocked_unix);
  text += "\r\nLast lock unix: " + std::to_string(node.wallet.last_locked_unix) + "\r\n\r\n";

  text += "Finality / Fork Policy\r\n";
  text += "Confirmation threshold: " + std::to_string(node.chain_policy.confirmation_threshold) + "\r\n";
  text += "Fork choice: " + node.chain_policy.fork_choice_rule + "\r\n";
  text += "Max reorg depth: " + std::to_string(node.chain_policy.max_reorg_depth) + "\r\n";
  text += "Checkpoint interval: " + std::to_string(node.chain_policy.checkpoint_interval_blocks) + "\r\n";
  text += "Checkpoint confirmations: " + std::to_string(node.chain_policy.checkpoint_confirmations) + "\r\n\r\n";

  text += "Validation Limits\r\n";
  text += "Max block events: " + std::to_string(node.validation_limits.max_block_events) + "\r\n";
  text += "Max block bytes: " + std::to_string(node.validation_limits.max_block_bytes) + "\r\n";
  text += "Max event bytes: " + std::to_string(node.validation_limits.max_event_bytes) + "\r\n";
  text += "Future drift seconds: " + std::to_string(node.validation_limits.max_future_drift_seconds) + "\r\n";
  text += "Past drift seconds: " + std::to_string(node.validation_limits.max_past_drift_seconds) + "\r\n\r\n";

  text += "Genesis Spec\r\n";
  text += "Chain ID: " + node.genesis.chain_id + "\r\n";
  text += "Network ID: " + node.genesis.network_id + "\r\n";
  text += "pszTimestamp: " + node.genesis.psz_timestamp + "\r\n";
  text += "Merkle Root: " + node.genesis.merkle_root + "\r\n";
  text += "Block Hash: " + node.genesis.block_hash + "\r\n";
  text += "Seed peers: " + std::to_string(node.genesis.seed_peers.size()) + "\r\n";
  for (const auto& seed : node.genesis.seed_peers) {
    text += "- " + seed + "\r\n";
  }

  set_edit_text(state->settings_view, text);
}

void rebuild_secondary_menu(AppState* state) {
  const std::string parent = selected_combo_text(state->parent_menu);
  const auto secondary = state->api.reference_secondary_menus(parent);

  SendMessageW(state->secondary_menu, CB_RESETCONTENT, 0, 0);
  for (const auto& menu_name : secondary) {
    const std::wstring wide = utf8_to_wide(menu_name);
    SendMessageW(state->secondary_menu, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(wide.c_str()));
  }

  if (!secondary.empty()) {
    SendMessageW(state->secondary_menu, CB_SETCURSEL, 0, 0);
  }
}

void rebuild_parent_menu(AppState* state) {
  const auto parents = state->api.reference_parent_menus();

  SendMessageW(state->parent_menu, CB_RESETCONTENT, 0, 0);
  for (const auto& menu_name : parents) {
    const std::wstring wide = utf8_to_wide(menu_name);
    SendMessageW(state->parent_menu, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(wide.c_str()));
  }

  if (!parents.empty()) {
    SendMessageW(state->parent_menu, CB_SETCURSEL, 0, 0);
  }

  rebuild_secondary_menu(state);
}

void rebuild_opening_list(AppState* state) {
  const std::string parent = selected_combo_text(state->parent_menu);
  const std::string secondary = selected_combo_text(state->secondary_menu);
  const std::string query = wide_to_utf8(read_window_text(state->search_edit));

  state->opening_keys = state->api.reference_openings(parent, secondary, query);

  SendMessageW(state->opening_list, LB_RESETCONTENT, 0, 0);
  for (const auto& key : state->opening_keys) {
    std::string display = key;
    const auto entry = state->api.reference_lookup(key);
    if (entry.has_value()) {
      display = entry->title;
    }

    const std::wstring wide = utf8_to_wide(display);
    SendMessageW(state->opening_list, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(wide.c_str()));
  }

  if (!state->opening_keys.empty()) {
    SendMessageW(state->opening_list, LB_SETCURSEL, 0, 0);
  }

  refresh_recipe_detail(state);
}

void refresh_tab_visibility(AppState* state) {
  const int tab_index = TabCtrl_GetCurSel(state->tab_control);

  const auto show = [](HWND control, bool visible) {
    ShowWindow(control, visible ? SW_SHOW : SW_HIDE);
  };

  const bool is_recipes = tab_index == static_cast<int>(TabIndex::Recipes);
  const bool is_forum = tab_index == static_cast<int>(TabIndex::Forum);
  const bool is_upload = tab_index == static_cast<int>(TabIndex::Upload);
  const bool is_profile = tab_index == static_cast<int>(TabIndex::Profile);
  const bool is_rewards = tab_index == static_cast<int>(TabIndex::Rewards);
  const bool is_node = tab_index == static_cast<int>(TabIndex::NodeStatus);
  const bool is_settings = tab_index == static_cast<int>(TabIndex::Settings);
  const bool is_about = tab_index == static_cast<int>(TabIndex::About);

  show(state->recipes_list, is_recipes);
  show(state->recipe_detail, is_recipes);
  show(state->recipe_thumb_up, is_recipes);
  show(state->recipe_rate_combo, is_recipes);
  show(state->recipe_rate_button, is_recipes);

  show(state->forum_view, is_forum);
  show(state->forum_thread_title, is_forum);
  show(state->forum_thread_body, is_forum);
  show(state->forum_create_thread, is_forum);
  show(state->forum_reply_body, is_forum);
  show(state->forum_create_reply, is_forum);

  show(state->upload_title, is_upload);
  show(state->upload_category, is_upload);
  show(state->upload_body, is_upload);
  show(state->upload_submit, is_upload);

  show(state->profile_view, is_profile);
  show(state->profile_name_edit, is_profile);
  show(state->profile_set_name_button, is_profile);
  show(state->profile_duplicate_policy_toggle, is_profile);
  show(state->profile_apply_policy_button, is_profile);
  show(state->profile_cipher_password_edit, is_profile);
  show(state->profile_cipher_salt_edit, is_profile);
  show(state->profile_cipher_apply_button, is_profile);
  show(state->profile_update_key_button, is_profile);
  show(state->profile_export_path_edit, is_profile);
  show(state->profile_export_password_edit, is_profile);
  show(state->profile_export_salt_edit, is_profile);
  show(state->profile_export_button, is_profile);
  show(state->profile_import_path_edit, is_profile);
  show(state->profile_import_password_edit, is_profile);
  show(state->profile_import_button, is_profile);
  show(state->profile_nuke_button, is_profile);

  show(state->rewards_view, is_rewards);

  show(state->node_status_view, is_node);
  show(state->node_tor_toggle, is_node);
  show(state->node_i2p_toggle, is_node);
  show(state->node_localhost_toggle, is_node);
  show(state->node_mode_combo, is_node);
  show(state->node_apply_button, is_node);
  show(state->node_refresh_button, is_node);
  show(state->node_peer_edit, is_node);
  show(state->node_peer_add_button, is_node);
  show(state->node_community_id_edit, is_node);
  show(state->node_community_name_edit, is_node);
  show(state->node_community_apply_button, is_node);

  show(state->settings_view, is_settings);
  show(state->settings_lock_wallet_button, is_settings);
  show(state->settings_unlock_password_edit, is_settings);
  show(state->settings_unlock_wallet_button, is_settings);
  show(state->settings_recover_path_edit, is_settings);
  show(state->settings_recover_backup_password_edit, is_settings);
  show(state->settings_recover_local_password_edit, is_settings);
  show(state->settings_recover_wallet_button, is_settings);
  show(state->settings_validate_now_button, is_settings);

  show(state->about_view, is_about);
}

void layout_controls(AppState* state, int width, int height) {
  constexpr int margin = 10;
  constexpr int top_height = 28;
  constexpr int combo_height = 28;
  constexpr int close_width = 90;
  constexpr int section_gap = 8;
  constexpr int left_width = 260;

  const int search_width = std::max(140, width - (margin * 3) - close_width);
  MoveWindow(state->search_edit, margin, margin, search_width, top_height, TRUE);
  MoveWindow(state->close_button, margin * 2 + search_width, margin, close_width, top_height, TRUE);

  const int body_y = margin + top_height + margin;
  const int body_h = std::max(120, height - body_y - margin);

  MoveWindow(state->parent_menu, margin, body_y, left_width, combo_height, TRUE);
  MoveWindow(state->secondary_menu, margin, body_y + combo_height + section_gap, left_width, combo_height,
             TRUE);

  const int opening_y = body_y + (combo_height * 2) + (section_gap * 2);
  const int opening_h = std::max(80, body_h - (combo_height * 2) - (section_gap * 2));
  MoveWindow(state->opening_list, margin, opening_y, left_width, opening_h, TRUE);

  const int tab_x = margin * 2 + left_width;
  const int tab_w = std::max(260, width - tab_x - margin);
  const int tab_h = body_h;
  MoveWindow(state->tab_control, tab_x, body_y, tab_w, tab_h, TRUE);

  RECT tab_rect{};
  GetClientRect(state->tab_control, &tab_rect);
  TabCtrl_AdjustRect(state->tab_control, FALSE, &tab_rect);
  MapWindowPoints(state->tab_control, GetParent(state->tab_control), reinterpret_cast<LPPOINT>(&tab_rect), 2);

  const int page_w = tab_rect.right - tab_rect.left;
  const int page_h = tab_rect.bottom - tab_rect.top;

  const int recipe_list_w = std::max(180, page_w / 3);
  MoveWindow(state->recipes_list, tab_rect.left, tab_rect.top, recipe_list_w, page_h, TRUE);
  const int recipe_right_x = tab_rect.left + recipe_list_w + 8;
  const int recipe_right_w = std::max(120, page_w - recipe_list_w - 8);
  const int action_h = 24;
  const int action_y = tab_rect.top + page_h - action_h;
  MoveWindow(state->recipe_detail, recipe_right_x, tab_rect.top, recipe_right_w,
             std::max(80, page_h - action_h - 8), TRUE);
  MoveWindow(state->recipe_thumb_up, recipe_right_x, action_y, 120, action_h, TRUE);
  MoveWindow(state->recipe_rate_combo, recipe_right_x + 126, action_y - 1, 70, action_h + 2, TRUE);
  MoveWindow(state->recipe_rate_button, recipe_right_x + 202, action_y, 80, action_h, TRUE);

  const int forum_summary_h = std::max(120, page_h / 2 - 8);
  MoveWindow(state->forum_view, tab_rect.left, tab_rect.top, page_w, forum_summary_h, TRUE);

  const int forum_controls_y = tab_rect.top + forum_summary_h + 8;
  MoveWindow(state->forum_thread_title, tab_rect.left, forum_controls_y, std::max(120, page_w - 130),
             24, TRUE);
  MoveWindow(state->forum_create_thread, tab_rect.left + std::max(120, page_w - 130) + 8,
             forum_controls_y, 120, 24, TRUE);
  MoveWindow(state->forum_thread_body, tab_rect.left, forum_controls_y + 28, page_w, 64, TRUE);
  MoveWindow(state->forum_reply_body, tab_rect.left, forum_controls_y + 98, std::max(120, page_w - 130),
             std::max(48, page_h - forum_summary_h - 106), TRUE);
  MoveWindow(state->forum_create_reply, tab_rect.left + std::max(120, page_w - 130) + 8,
             forum_controls_y + 98, 120, 24, TRUE);

  constexpr int label_h = 22;
  const int upload_top = tab_rect.top;
  MoveWindow(state->upload_title, tab_rect.left, upload_top, page_w, label_h, TRUE);
  MoveWindow(state->upload_category, tab_rect.left, upload_top + label_h + 6, page_w, label_h, TRUE);
  MoveWindow(state->upload_body, tab_rect.left, upload_top + (label_h * 2) + 12, page_w,
             std::max(80, page_h - (label_h * 2) - 54), TRUE);
  MoveWindow(state->upload_submit, tab_rect.left, tab_rect.top + page_h - 30, 140, 28, TRUE);

  const int profile_top = tab_rect.top;
  const int profile_row_h = 24;
  const int profile_gap = 6;
  const int profile_wide = std::max(120, page_w - 520);
  const int profile_medium = std::max(100, page_w / 4);

  MoveWindow(state->profile_name_edit, tab_rect.left, profile_top, profile_wide, profile_row_h, TRUE);
  MoveWindow(state->profile_set_name_button, tab_rect.left + profile_wide + 8, profile_top, 132, profile_row_h,
             TRUE);
  MoveWindow(state->profile_update_key_button, tab_rect.left + profile_wide + 146, profile_top, 130, profile_row_h,
             TRUE);

  MoveWindow(state->profile_duplicate_policy_toggle, tab_rect.left, profile_top + profile_row_h + profile_gap,
             240, profile_row_h, TRUE);
  MoveWindow(state->profile_apply_policy_button, tab_rect.left + 248, profile_top + profile_row_h + profile_gap,
             120, profile_row_h, TRUE);

  MoveWindow(state->profile_cipher_password_edit, tab_rect.left,
             profile_top + ((profile_row_h + profile_gap) * 2), profile_medium, profile_row_h, TRUE);
  MoveWindow(state->profile_cipher_salt_edit, tab_rect.left + profile_medium + 8,
             profile_top + ((profile_row_h + profile_gap) * 2), profile_medium, profile_row_h, TRUE);
  MoveWindow(state->profile_cipher_apply_button, tab_rect.left + (profile_medium * 2) + 16,
             profile_top + ((profile_row_h + profile_gap) * 2), 140, profile_row_h, TRUE);

  MoveWindow(state->profile_export_path_edit, tab_rect.left,
             profile_top + ((profile_row_h + profile_gap) * 3), std::max(180, page_w - 420), profile_row_h, TRUE);
  MoveWindow(state->profile_export_password_edit, tab_rect.left + std::max(180, page_w - 420) + 8,
             profile_top + ((profile_row_h + profile_gap) * 3), 120, profile_row_h, TRUE);
  MoveWindow(state->profile_export_salt_edit, tab_rect.left + std::max(180, page_w - 420) + 136,
             profile_top + ((profile_row_h + profile_gap) * 3), 90, profile_row_h, TRUE);
  MoveWindow(state->profile_export_button, tab_rect.left + std::max(180, page_w - 420) + 234,
             profile_top + ((profile_row_h + profile_gap) * 3), 90, profile_row_h, TRUE);

  MoveWindow(state->profile_import_path_edit, tab_rect.left,
             profile_top + ((profile_row_h + profile_gap) * 4), std::max(180, page_w - 330), profile_row_h, TRUE);
  MoveWindow(state->profile_import_password_edit, tab_rect.left + std::max(180, page_w - 330) + 8,
             profile_top + ((profile_row_h + profile_gap) * 4), 120, profile_row_h, TRUE);
  MoveWindow(state->profile_import_button, tab_rect.left + std::max(180, page_w - 330) + 136,
             profile_top + ((profile_row_h + profile_gap) * 4), 90, profile_row_h, TRUE);
  MoveWindow(state->profile_nuke_button, tab_rect.left + std::max(180, page_w - 330) + 234,
             profile_top + ((profile_row_h + profile_gap) * 4), 90, profile_row_h, TRUE);

  const int profile_text_top = profile_top + ((profile_row_h + profile_gap) * 5) + 2;
  MoveWindow(state->profile_view, tab_rect.left, profile_text_top, page_w,
             std::max<int>(80, page_h - (profile_text_top - tab_rect.top)), TRUE);

  const int status_top = tab_rect.top;
  MoveWindow(state->node_tor_toggle, tab_rect.left, status_top, 90, 22, TRUE);
  MoveWindow(state->node_i2p_toggle, tab_rect.left + 96, status_top, 90, 22, TRUE);
  MoveWindow(state->node_localhost_toggle, tab_rect.left + 192, status_top, 160, 22, TRUE);
  MoveWindow(state->node_mode_combo, tab_rect.left + 360, status_top - 2, 90, 26, TRUE);
  MoveWindow(state->node_apply_button, tab_rect.left + 458, status_top - 2, 84, 26, TRUE);
  MoveWindow(state->node_refresh_button, tab_rect.left + 548, status_top - 2, 84, 26, TRUE);

  MoveWindow(state->node_peer_edit, tab_rect.left, status_top + 30, std::max(120, page_w - 370), 24, TRUE);
  MoveWindow(state->node_peer_add_button, tab_rect.left + std::max(120, page_w - 370) + 8, status_top + 30,
             92, 24, TRUE);

  MoveWindow(state->node_community_id_edit, tab_rect.left, status_top + 60, 180, 24, TRUE);
  MoveWindow(state->node_community_name_edit, tab_rect.left + 188, status_top + 60,
             std::max(120, page_w - 380), 24, TRUE);
  MoveWindow(state->node_community_apply_button, tab_rect.left + std::max(120, page_w - 380) + 196,
             status_top + 60, 92, 24, TRUE);

  MoveWindow(state->node_status_view, tab_rect.left, status_top + 92, page_w,
             std::max(80, page_h - 92), TRUE);

  const int settings_top = tab_rect.top;
  MoveWindow(state->settings_lock_wallet_button, tab_rect.left, settings_top, 120, 24, TRUE);
  MoveWindow(state->settings_unlock_password_edit, tab_rect.left + 126, settings_top, 180, 24, TRUE);
  MoveWindow(state->settings_unlock_wallet_button, tab_rect.left + 312, settings_top, 120, 24, TRUE);
  MoveWindow(state->settings_recover_path_edit, tab_rect.left, settings_top + 30,
             std::max(140, page_w - 420), 24, TRUE);
  MoveWindow(state->settings_recover_backup_password_edit, tab_rect.left + std::max(140, page_w - 420) + 8,
             settings_top + 30, 120, 24, TRUE);
  MoveWindow(state->settings_recover_local_password_edit, tab_rect.left + std::max(140, page_w - 420) + 136,
             settings_top + 30, 120, 24, TRUE);
  MoveWindow(state->settings_recover_wallet_button, tab_rect.left + std::max(140, page_w - 420) + 264,
             settings_top + 30, 90, 24, TRUE);
  MoveWindow(state->settings_validate_now_button, tab_rect.left + 438, settings_top, 110, 24, TRUE);
  MoveWindow(state->settings_view, tab_rect.left, settings_top + 60, page_w, std::max(80, page_h - 60), TRUE);

  MoveWindow(state->rewards_view, tab_rect.left, tab_rect.top, page_w, page_h, TRUE);
  MoveWindow(state->about_view, tab_rect.left, tab_rect.top, page_w, page_h, TRUE);

  refresh_tab_visibility(state);
}

HMENU make_main_menu() {
  HMENU menu_bar = CreateMenu();

  HMENU file_menu = CreatePopupMenu();
  AppendMenuW(file_menu, MF_STRING, kMenuCloseId, L"Close");
  AppendMenuW(menu_bar, MF_POPUP, reinterpret_cast<UINT_PTR>(file_menu), L"File");

  HMENU help_menu = CreatePopupMenu();
  AppendMenuW(help_menu, MF_STRING, kMenuAboutId, L"About / Credits");
  AppendMenuW(menu_bar, MF_POPUP, reinterpret_cast<UINT_PTR>(help_menu), L"Help");

  return menu_bar;
}

void bootstrap_demo_data(AppState* state) {
  const auto recipes = state->api.search({.text = {}, .category = {}});
  if (!recipes.empty()) {
    return;
  }

  state->api.create_recipe({
      .category = "Core Topic",
      .title = "Tomato Soup Base",
      .markdown = "# Tomato Soup Base\n\nCore method for all tomato soup variations.",
      .core_topic = true,
      .menu_segment = "core-menu",
  });
  state->api.create_recipe({
      .category = "Ingredient",
      .title = "Essential Ingredients",
      .markdown = "- Tomatoes\n- Olive oil\n- Garlic\n- Salt",
      .core_topic = true,
      .menu_segment = "core-ingredients",
  });
  state->api.create_recipe({
      .category = "Community",
      .title = "Starter: P2P Tomato Soup",
      .markdown = "# Tomato Soup\n\n- 4 tomatoes\n- Olive oil\n- Salt\n\nSimmer 20 minutes.",
      .core_topic = false,
      .menu_segment = "community-post",
  });
}

void show_result_if_error(HWND hwnd, const alpha::Result& result, std::wstring_view title) {
  if (!result.ok) {
    MessageBoxW(hwnd, utf8_to_wide(result.message).c_str(), std::wstring(title).c_str(),
                MB_OK | MB_ICONERROR);
  }
}

void create_forum_thread_from_ui(HWND hwnd, AppState* state) {
  const LRESULT recipe_index = SendMessageW(state->recipes_list, LB_GETCURSEL, 0, 0);
  if (recipe_index == LB_ERR || static_cast<size_t>(recipe_index) >= state->recipes.size()) {
    MessageBoxW(hwnd, L"Select a recipe first, then create a thread.", L"Forum", MB_OK | MB_ICONWARNING);
    return;
  }

  const auto& recipe = state->recipes[static_cast<size_t>(recipe_index)];
  const std::string title = wide_to_utf8(read_window_text(state->forum_thread_title));
  const std::string body = wide_to_utf8(read_window_text(state->forum_thread_body));

  const alpha::Result result = state->api.create_thread({
      .recipe_id = recipe.recipe_id,
      .title = title,
      .markdown = body,
  });
  if (!result.ok) {
    show_result_if_error(hwnd, result, L"Create Thread");
    return;
  }

  SetWindowTextW(state->forum_thread_title, L"");
  SetWindowTextW(state->forum_thread_body, L"");
  refresh_forum_view(state);
  refresh_node_status_view(state);
  rebuild_opening_list(state);
}

void create_forum_reply_from_ui(HWND hwnd, AppState* state) {
  const LRESULT recipe_index = SendMessageW(state->recipes_list, LB_GETCURSEL, 0, 0);
  if (recipe_index == LB_ERR || static_cast<size_t>(recipe_index) >= state->recipes.size()) {
    MessageBoxW(hwnd, L"Select a recipe first, then create a reply.", L"Forum", MB_OK | MB_ICONWARNING);
    return;
  }

  const auto& recipe = state->recipes[static_cast<size_t>(recipe_index)];
  const auto threads = state->api.threads(recipe.recipe_id);
  if (threads.empty()) {
    MessageBoxW(hwnd, L"No thread exists yet. Create a thread first.", L"Forum", MB_OK | MB_ICONWARNING);
    return;
  }

  const std::string body = wide_to_utf8(read_window_text(state->forum_reply_body));
  const alpha::Result result = state->api.create_reply({
      .thread_id = threads.front().thread_id,
      .markdown = body,
  });
  if (!result.ok) {
    show_result_if_error(hwnd, result, L"Create Reply");
    return;
  }

  SetWindowTextW(state->forum_reply_body, L"");
  refresh_forum_view(state);
  refresh_node_status_view(state);
  rebuild_opening_list(state);
}

void thumb_up_selected_recipe(HWND hwnd, AppState* state) {
  const alpha::RecipeSummary* recipe = selected_recipe(state);
  if (recipe == nullptr) {
    MessageBoxW(hwnd, L"Select a recipe first.", L"Thumbs Up", MB_OK | MB_ICONWARNING);
    return;
  }

  const alpha::Result result = state->api.add_thumb_up(recipe->recipe_id);
  if (!result.ok) {
    show_result_if_error(hwnd, result, L"Thumbs Up");
    return;
  }

  refresh_recipe_list(state);
  refresh_node_status_view(state);
  rebuild_opening_list(state);
}

void rate_selected_recipe(HWND hwnd, AppState* state) {
  const alpha::RecipeSummary* recipe = selected_recipe(state);
  if (recipe == nullptr) {
    MessageBoxW(hwnd, L"Select a recipe first.", L"Rate Recipe", MB_OK | MB_ICONWARNING);
    return;
  }

  const std::string value = selected_combo_text(state->recipe_rate_combo);
  int rating = 0;
  if (!value.empty() && value.size() == 1 && value[0] >= '1' && value[0] <= '5') {
    rating = value[0] - '0';
  }
  if (rating < 1 || rating > 5) {
    MessageBoxW(hwnd, L"Choose a rating from 1 to 5.", L"Rate Recipe", MB_OK | MB_ICONWARNING);
    return;
  }

  const alpha::Result result = state->api.add_review({
      .recipe_id = recipe->recipe_id,
      .rating = rating,
      .markdown = "Rated via UI",
  });
  if (!result.ok) {
    show_result_if_error(hwnd, result, L"Rate Recipe");
    return;
  }

  refresh_recipe_list(state);
  refresh_node_status_view(state);
  rebuild_opening_list(state);
}

void apply_node_controls(HWND hwnd, AppState* state) {
  const bool tor_enabled = checkbox_checked(state->node_tor_toggle);
  const bool i2p_enabled = checkbox_checked(state->node_i2p_toggle);
  const bool localhost_mode = checkbox_checked(state->node_localhost_toggle);

  alpha::Result result = state->api.set_transport_enabled(alpha::AnonymityMode::Tor, tor_enabled);
  if (!result.ok) {
    show_result_if_error(hwnd, result, L"Node Controls");
    return;
  }

  result = state->api.set_transport_enabled(alpha::AnonymityMode::I2P, i2p_enabled);
  if (!result.ok) {
    show_result_if_error(hwnd, result, L"Node Controls");
    return;
  }

  const std::string selected_mode = selected_combo_text(state->node_mode_combo);
  const alpha::AnonymityMode mode =
      (selected_mode == "I2P") ? alpha::AnonymityMode::I2P : alpha::AnonymityMode::Tor;
  result = state->api.set_active_transport(mode);
  if (!result.ok) {
    show_result_if_error(hwnd, result, L"Node Controls");
    return;
  }

  result = state->api.set_alpha_test_mode(localhost_mode);
  if (!result.ok) {
    show_result_if_error(hwnd, result, L"Node Controls");
    return;
  }

  refresh_node_status_view(state);
  refresh_profile_and_about(state);
}

void add_peer_from_ui(HWND hwnd, AppState* state) {
  const std::string peer = wide_to_utf8(read_window_text(state->node_peer_edit));
  const alpha::Result result = state->api.add_peer(peer);
  if (!result.ok) {
    show_result_if_error(hwnd, result, L"Add Peer");
    return;
  }

  SetWindowTextW(state->node_peer_edit, L"");
  refresh_node_status_view(state);
}

void apply_community_from_ui(HWND hwnd, AppState* state) {
  const std::string community_id = wide_to_utf8(read_window_text(state->node_community_id_edit));
  const std::string community_name = wide_to_utf8(read_window_text(state->node_community_name_edit));

  const alpha::Result result = state->api.use_community_profile(community_id, community_name, "");
  if (!result.ok) {
    show_result_if_error(hwnd, result, L"Community Profile");
    return;
  }

  refresh_recipe_list(state);
  refresh_forum_view(state);
  refresh_node_status_view(state);
  refresh_profile_and_about(state);
}

void apply_profile_name_from_ui(HWND hwnd, AppState* state) {
  const std::string name = wide_to_utf8(read_window_text(state->profile_name_edit));
  const std::string cipher_password = wide_to_utf8(read_window_text(state->profile_cipher_password_edit));
  const std::string cipher_salt = wide_to_utf8(read_window_text(state->profile_cipher_salt_edit));

  const alpha::Result result =
      state->api.set_immortal_name_with_cipher(name, cipher_password, cipher_salt);
  if (!result.ok) {
    show_result_if_error(hwnd, result, L"Set Immortal");
    return;
  }

  SetWindowTextW(state->profile_cipher_password_edit, L"");

  refresh_profile_and_about(state);
  refresh_node_status_view(state);
}

void apply_duplicate_policy_from_ui(HWND hwnd, AppState* state) {
  const bool reject_duplicates = checkbox_checked(state->profile_duplicate_policy_toggle);
  const alpha::Result result = state->api.set_duplicate_name_policy(reject_duplicates);
  if (!result.ok) {
    show_result_if_error(hwnd, result, L"Duplicate Policy");
    return;
  }
  refresh_profile_and_about(state);
}

void apply_profile_cipher_from_ui(HWND hwnd, AppState* state) {
  const std::string password = wide_to_utf8(read_window_text(state->profile_cipher_password_edit));
  const std::string salt = wide_to_utf8(read_window_text(state->profile_cipher_salt_edit));
  const alpha::Result result = state->api.set_profile_cipher_password(password, salt);
  if (!result.ok) {
    show_result_if_error(hwnd, result, L"Cipher Key");
    return;
  }
  SetWindowTextW(state->profile_cipher_password_edit, L"");
  refresh_node_status_view(state);
  refresh_profile_and_about(state);
}

void update_key_to_peers_from_ui(HWND hwnd, AppState* state) {
  const alpha::Result result = state->api.update_key_to_peers();
  if (!result.ok) {
    show_result_if_error(hwnd, result, L"Update Key");
    return;
  }
  refresh_node_status_view(state);
  refresh_profile_and_about(state);
}

void export_key_backup_from_ui(HWND hwnd, AppState* state) {
  const std::string path = wide_to_utf8(read_window_text(state->profile_export_path_edit));
  const std::string password = wide_to_utf8(read_window_text(state->profile_export_password_edit));
  const std::string salt = wide_to_utf8(read_window_text(state->profile_export_salt_edit));
  const alpha::Result result = state->api.export_key_backup(path, password, salt);
  if (!result.ok) {
    show_result_if_error(hwnd, result, L"Export Key");
    return;
  }
  MessageBoxW(hwnd, utf8_to_wide("Key backup exported: " + result.data).c_str(), L"Export Key",
              MB_OK | MB_ICONINFORMATION);
}

void import_key_backup_from_ui(HWND hwnd, AppState* state) {
  const std::string path = wide_to_utf8(read_window_text(state->profile_import_path_edit));
  const std::string password = wide_to_utf8(read_window_text(state->profile_import_password_edit));
  const alpha::Result result = state->api.import_key_backup(path, password);
  if (!result.ok) {
    show_result_if_error(hwnd, result, L"Import Key");
    return;
  }
  refresh_recipe_list(state);
  refresh_forum_view(state);
  refresh_node_status_view(state);
  refresh_profile_and_about(state);
}

void nuke_key_from_ui(HWND hwnd, AppState* state) {
  const int confirm = MessageBoxW(
      hwnd,
      L"Nuke key will permanently destroy this local identity key. Existing posts remain signed by old CID.\n\nProceed?",
      L"Nuke Key",
      MB_YESNO | MB_ICONWARNING);
  if (confirm != IDYES) {
    return;
  }

  const alpha::Result result = state->api.nuke_key("NUKE-KEY");
  if (!result.ok) {
    show_result_if_error(hwnd, result, L"Nuke Key");
    return;
  }
  refresh_recipe_list(state);
  refresh_forum_view(state);
  refresh_node_status_view(state);
  refresh_profile_and_about(state);
}

void lock_wallet_from_ui(HWND hwnd, AppState* state) {
  const alpha::Result result = state->api.lock_wallet();
  if (!result.ok) {
    show_result_if_error(hwnd, result, L"Lock Wallet");
    return;
  }
  refresh_node_status_view(state);
  refresh_profile_and_about(state);
}

void unlock_wallet_from_ui(HWND hwnd, AppState* state) {
  const std::string pass = wide_to_utf8(read_window_text(state->settings_unlock_password_edit));
  const alpha::Result result = state->api.unlock_wallet(pass);
  if (!result.ok) {
    show_result_if_error(hwnd, result, L"Unlock Wallet");
    return;
  }
  SetWindowTextW(state->settings_unlock_password_edit, L"");
  refresh_node_status_view(state);
  refresh_profile_and_about(state);
}

void recover_wallet_from_ui(HWND hwnd, AppState* state) {
  const std::string path = wide_to_utf8(read_window_text(state->settings_recover_path_edit));
  const std::string backup_pass =
      wide_to_utf8(read_window_text(state->settings_recover_backup_password_edit));
  const std::string local_pass =
      wide_to_utf8(read_window_text(state->settings_recover_local_password_edit));
  const alpha::Result result = state->api.recover_wallet(path, backup_pass, local_pass);
  if (!result.ok) {
    show_result_if_error(hwnd, result, L"Recover Wallet");
    return;
  }
  SetWindowTextW(state->settings_recover_backup_password_edit, L"");
  SetWindowTextW(state->settings_recover_local_password_edit, L"");
  refresh_recipe_list(state);
  refresh_forum_view(state);
  refresh_node_status_view(state);
  refresh_profile_and_about(state);
}

void validate_now_from_ui(HWND hwnd, AppState* state) {
  const alpha::Result result = state->api.run_backtest_validation();
  if (!result.ok) {
    show_result_if_error(hwnd, result, L"Validate State");
    return;
  }
  refresh_node_status_view(state);
}

LRESULT CALLBACK window_proc(HWND hwnd, UINT message, WPARAM w_param, LPARAM l_param) {
  auto* state = reinterpret_cast<AppState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));

  switch (message) {
    case WM_CREATE: {
      auto* create = reinterpret_cast<CREATESTRUCTW*>(l_param);
      state = reinterpret_cast<AppState*>(create->lpCreateParams);
      SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(state));

      INITCOMMONCONTROLSEX common{};
      common.dwSize = sizeof(common);
      common.dwICC = ICC_TAB_CLASSES;
      InitCommonControlsEx(&common);

      HFONT ui_font = static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));

      state->search_edit =
          CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
                          0, 0, 0, 0, hwnd, reinterpret_cast<HMENU>(kSearchEditId), create->hInstance,
                          nullptr);

      state->close_button =
          CreateWindowW(L"BUTTON", L"Close", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 0, 0, 0, 0, hwnd,
                        reinterpret_cast<HMENU>(kCloseButtonId), create->hInstance, nullptr);

      state->parent_menu =
          CreateWindowExW(0, L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | WS_VSCROLL | CBS_DROPDOWNLIST,
                          0, 0, 0, 0, hwnd, reinterpret_cast<HMENU>(kParentMenuId), create->hInstance,
                          nullptr);

      state->secondary_menu =
          CreateWindowExW(0, L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | WS_VSCROLL | CBS_DROPDOWNLIST,
                          0, 0, 0, 0, hwnd, reinterpret_cast<HMENU>(kSecondaryMenuId), create->hInstance,
                          nullptr);

      state->opening_list =
          CreateWindowExW(WS_EX_CLIENTEDGE, L"LISTBOX", L"", WS_CHILD | WS_VISIBLE | WS_VSCROLL | LBS_NOTIFY,
                          0, 0, 0, 0, hwnd, reinterpret_cast<HMENU>(kOpeningListId), create->hInstance,
                          nullptr);

      state->tab_control = CreateWindowExW(0, WC_TABCONTROLW, L"", WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS,
                                           0, 0, 0, 0, hwnd, reinterpret_cast<HMENU>(kMainTabsId),
                                           create->hInstance, nullptr);

      TCITEMW tab{};
      tab.mask = TCIF_TEXT;
      std::wstring recipes = L"Recipes";
      tab.pszText = recipes.data();
      TabCtrl_InsertItem(state->tab_control, static_cast<int>(TabIndex::Recipes), &tab);
      std::wstring forum = L"Forum";
      tab.pszText = forum.data();
      TabCtrl_InsertItem(state->tab_control, static_cast<int>(TabIndex::Forum), &tab);
      std::wstring upload = L"Upload";
      tab.pszText = upload.data();
      TabCtrl_InsertItem(state->tab_control, static_cast<int>(TabIndex::Upload), &tab);
      std::wstring profile = L"Profile";
      tab.pszText = profile.data();
      TabCtrl_InsertItem(state->tab_control, static_cast<int>(TabIndex::Profile), &tab);
      std::wstring rewards = L"Rewards";
      tab.pszText = rewards.data();
      TabCtrl_InsertItem(state->tab_control, static_cast<int>(TabIndex::Rewards), &tab);
      std::wstring node = L"Node Status";
      tab.pszText = node.data();
      TabCtrl_InsertItem(state->tab_control, static_cast<int>(TabIndex::NodeStatus), &tab);
      std::wstring settings = L"Settings";
      tab.pszText = settings.data();
      TabCtrl_InsertItem(state->tab_control, static_cast<int>(TabIndex::Settings), &tab);
      std::wstring about = L"About";
      tab.pszText = about.data();
      TabCtrl_InsertItem(state->tab_control, static_cast<int>(TabIndex::About), &tab);

      state->recipes_list =
          CreateWindowExW(WS_EX_CLIENTEDGE, L"LISTBOX", L"", WS_CHILD | WS_VISIBLE | WS_VSCROLL | LBS_NOTIFY,
                          0, 0, 0, 0, hwnd, reinterpret_cast<HMENU>(kRecipesListId), create->hInstance,
                          nullptr);

      state->recipe_detail =
          CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_VSCROLL |
                                                           ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY,
                          0, 0, 0, 0, hwnd, reinterpret_cast<HMENU>(kRecipeDetailId), create->hInstance,
                          nullptr);
      state->recipe_thumb_up =
          CreateWindowW(L"BUTTON", L"Thumbs Up +1", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 0, 0, 0, 0,
                        hwnd, reinterpret_cast<HMENU>(kRecipeThumbUpId), create->hInstance, nullptr);
      state->recipe_rate_combo =
          CreateWindowExW(0, L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST, 0, 0, 0, 0,
                          hwnd, reinterpret_cast<HMENU>(kRecipeRateComboId), create->hInstance, nullptr);
      SendMessageW(state->recipe_rate_combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"1"));
      SendMessageW(state->recipe_rate_combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"2"));
      SendMessageW(state->recipe_rate_combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"3"));
      SendMessageW(state->recipe_rate_combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"4"));
      SendMessageW(state->recipe_rate_combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"5"));
      SendMessageW(state->recipe_rate_combo, CB_SETCURSEL, 4, 0);
      state->recipe_rate_button =
          CreateWindowW(L"BUTTON", L"Rate", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 0, 0, 0, 0, hwnd,
                        reinterpret_cast<HMENU>(kRecipeRateButtonId), create->hInstance, nullptr);

      state->forum_view =
          CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_VSCROLL |
                                                           ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY,
                          0, 0, 0, 0, hwnd, reinterpret_cast<HMENU>(kForumViewId), create->hInstance,
                          nullptr);

      state->forum_thread_title =
          CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"Thread title",
                          WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 0, 0, 0, 0, hwnd,
                          reinterpret_cast<HMENU>(kForumThreadTitleId), create->hInstance, nullptr);

      state->forum_thread_body =
          CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"Thread markdown",
                          WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL, 0, 0, 0,
                          0, hwnd, reinterpret_cast<HMENU>(kForumThreadBodyId), create->hInstance,
                          nullptr);

      state->forum_create_thread =
          CreateWindowW(L"BUTTON", L"Create Thread", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 0, 0, 0, 0,
                        hwnd, reinterpret_cast<HMENU>(kForumCreateThreadId), create->hInstance,
                        nullptr);

      state->forum_reply_body =
          CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"Reply markdown",
                          WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL, 0, 0, 0,
                          0, hwnd, reinterpret_cast<HMENU>(kForumReplyBodyId), create->hInstance,
                          nullptr);

      state->forum_create_reply =
          CreateWindowW(L"BUTTON", L"Create Reply", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 0, 0, 0, 0,
                        hwnd, reinterpret_cast<HMENU>(kForumCreateReplyId), create->hInstance, nullptr);

      state->upload_title =
          CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"Recipe title", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
                          0, 0, 0, 0, hwnd, reinterpret_cast<HMENU>(kUploadTitleId), create->hInstance,
                          nullptr);

      state->upload_category = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"Category",
                                               WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 0, 0, 0, 0, hwnd,
                                               reinterpret_cast<HMENU>(kUploadCategoryId), create->hInstance,
                                               nullptr);

      state->upload_body =
          CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"Recipe markdown", WS_CHILD | WS_VISIBLE | WS_VSCROLL |
                                                                           ES_MULTILINE | ES_AUTOVSCROLL,
                          0, 0, 0, 0, hwnd, reinterpret_cast<HMENU>(kUploadBodyId), create->hInstance,
                          nullptr);

      state->upload_submit =
          CreateWindowW(L"BUTTON", L"Upload Recipe", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 0, 0, 0, 0, hwnd,
                        reinterpret_cast<HMENU>(kUploadSubmitId), create->hInstance, nullptr);

      state->profile_view =
          CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_VSCROLL |
                                                           ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY,
                          0, 0, 0, 0, hwnd, reinterpret_cast<HMENU>(kProfileViewId), create->hInstance,
                          nullptr);
      state->profile_name_edit =
          CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"Display name (immutable)", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
                          0, 0, 0, 0, hwnd, reinterpret_cast<HMENU>(kProfileNameEditId), create->hInstance,
                          nullptr);
      state->profile_set_name_button =
          CreateWindowW(L"BUTTON", L"Set Immortal + Sync", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 0, 0, 0, 0,
                        hwnd, reinterpret_cast<HMENU>(kProfileSetNameId), create->hInstance, nullptr);
      state->profile_duplicate_policy_toggle =
          CreateWindowW(L"BUTTON", L"Reject Duplicate Names", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 0, 0,
                        0, 0, hwnd, reinterpret_cast<HMENU>(kProfileDupPolicyToggleId), create->hInstance,
                        nullptr);
      set_checkbox(state->profile_duplicate_policy_toggle, true);
      state->profile_apply_policy_button =
          CreateWindowW(L"BUTTON", L"Apply Policy", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 0, 0, 0, 0, hwnd,
                        reinterpret_cast<HMENU>(kProfileApplyPolicyId), create->hInstance, nullptr);
      state->profile_cipher_password_edit =
          CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL | ES_PASSWORD,
                          0, 0, 0, 0, hwnd, reinterpret_cast<HMENU>(kProfileCipherPasswordId), create->hInstance,
                          nullptr);
      state->profile_cipher_salt_edit =
          CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"cipher-salt", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
                          0, 0, 0, 0, hwnd, reinterpret_cast<HMENU>(kProfileCipherSaltId), create->hInstance,
                          nullptr);
      state->profile_cipher_apply_button =
          CreateWindowW(L"BUTTON", L"Apply Cipher Key", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 0, 0, 0, 0,
                        hwnd, reinterpret_cast<HMENU>(kProfileCipherApplyId), create->hInstance, nullptr);
      state->profile_update_key_button =
          CreateWindowW(L"BUTTON", L"Update Key to Peers", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 0, 0, 0, 0,
                        hwnd, reinterpret_cast<HMENU>(kProfileUpdateKeyId), create->hInstance, nullptr);
      state->profile_export_path_edit =
          CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"backup/identity-backup.dat",
                          WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 0, 0, 0, 0, hwnd,
                          reinterpret_cast<HMENU>(kProfileExportPathId), create->hInstance, nullptr);
      state->profile_export_password_edit =
          CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL | ES_PASSWORD,
                          0, 0, 0, 0, hwnd, reinterpret_cast<HMENU>(kProfileExportPasswordId),
                          create->hInstance, nullptr);
      state->profile_export_salt_edit =
          CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"salt", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 0, 0,
                          0, 0, hwnd, reinterpret_cast<HMENU>(kProfileExportSaltId), create->hInstance, nullptr);
      state->profile_export_button =
          CreateWindowW(L"BUTTON", L"Export Key", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 0, 0, 0, 0, hwnd,
                        reinterpret_cast<HMENU>(kProfileExportButtonId), create->hInstance, nullptr);
      state->profile_import_path_edit =
          CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"backup/identity-backup.dat",
                          WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 0, 0, 0, 0, hwnd,
                          reinterpret_cast<HMENU>(kProfileImportPathId), create->hInstance, nullptr);
      state->profile_import_password_edit =
          CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL | ES_PASSWORD,
                          0, 0, 0, 0, hwnd, reinterpret_cast<HMENU>(kProfileImportPasswordId),
                          create->hInstance, nullptr);
      state->profile_import_button =
          CreateWindowW(L"BUTTON", L"Import Key", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 0, 0, 0, 0, hwnd,
                        reinterpret_cast<HMENU>(kProfileImportButtonId), create->hInstance, nullptr);
      state->profile_nuke_button =
          CreateWindowW(L"BUTTON", L"Nuke Key", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 0, 0, 0, 0, hwnd,
                        reinterpret_cast<HMENU>(kProfileNukeButtonId), create->hInstance, nullptr);

      state->about_view =
          CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_VSCROLL |
                                                           ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY,
                          0, 0, 0, 0, hwnd, reinterpret_cast<HMENU>(kAboutViewId), create->hInstance,
                          nullptr);
      state->rewards_view =
          CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_VSCROLL |
                                                           ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY,
                          0, 0, 0, 0, hwnd, reinterpret_cast<HMENU>(kRewardsViewId), create->hInstance,
                          nullptr);

      state->node_status_view =
          CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_VSCROLL |
                                                           ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY,
                          0, 0, 0, 0, hwnd, reinterpret_cast<HMENU>(kNodeStatusViewId), create->hInstance,
                          nullptr);

      state->node_tor_toggle =
          CreateWindowW(L"BUTTON", L"Tor", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 0, 0, 0, 0, hwnd,
                        reinterpret_cast<HMENU>(kNodeTorToggleId), create->hInstance, nullptr);
      state->node_i2p_toggle =
          CreateWindowW(L"BUTTON", L"I2P", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 0, 0, 0, 0, hwnd,
                        reinterpret_cast<HMENU>(kNodeI2PToggleId), create->hInstance, nullptr);
      state->node_localhost_toggle =
          CreateWindowW(L"BUTTON", L"Localhost 127.0.0.1", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 0,
                        0, 0, 0, hwnd, reinterpret_cast<HMENU>(kNodeLocalhostToggleId), create->hInstance,
                        nullptr);

      state->node_mode_combo =
          CreateWindowExW(0, L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST, 0, 0, 0, 0, hwnd,
                          reinterpret_cast<HMENU>(kNodeModeComboId), create->hInstance, nullptr);
      SendMessageW(state->node_mode_combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Tor"));
      SendMessageW(state->node_mode_combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"I2P"));
      SendMessageW(state->node_mode_combo, CB_SETCURSEL, 0, 0);

      state->node_apply_button =
          CreateWindowW(L"BUTTON", L"Apply", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 0, 0, 0, 0, hwnd,
                        reinterpret_cast<HMENU>(kNodeApplyId), create->hInstance, nullptr);
      state->node_refresh_button =
          CreateWindowW(L"BUTTON", L"Refresh", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 0, 0, 0, 0, hwnd,
                        reinterpret_cast<HMENU>(kNodeRefreshId), create->hInstance, nullptr);

      state->node_peer_edit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"peer.host:port",
                                              WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 0, 0, 0, 0, hwnd,
                                              reinterpret_cast<HMENU>(kNodePeerEditId), create->hInstance,
                                              nullptr);
      state->node_peer_add_button =
          CreateWindowW(L"BUTTON", L"Add Peer", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 0, 0, 0, 0, hwnd,
                        reinterpret_cast<HMENU>(kNodeAddPeerId), create->hInstance, nullptr);

      state->node_community_id_edit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"recipes",
                                                      WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 0, 0, 0, 0,
                                                      hwnd, reinterpret_cast<HMENU>(kNodeCommunityIdId),
                                                      create->hInstance, nullptr);
      state->node_community_name_edit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"Recipe Community",
                                                        WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 0, 0, 0, 0,
                                                        hwnd, reinterpret_cast<HMENU>(kNodeCommunityNameId),
                                                        create->hInstance, nullptr);
      state->node_community_apply_button =
          CreateWindowW(L"BUTTON", L"Use Community", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 0, 0, 0, 0,
                        hwnd, reinterpret_cast<HMENU>(kNodeCommunityApplyId), create->hInstance, nullptr);

      state->settings_view =
          CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_VSCROLL |
                                                           ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY,
                          0, 0, 0, 0, hwnd, reinterpret_cast<HMENU>(kSettingsViewId), create->hInstance,
                          nullptr);
      state->settings_lock_wallet_button =
          CreateWindowW(L"BUTTON", L"Lock Wallet", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 0, 0, 0, 0, hwnd,
                        reinterpret_cast<HMENU>(kSettingsLockWalletId), create->hInstance, nullptr);
      state->settings_unlock_password_edit =
          CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL | ES_PASSWORD,
                          0, 0, 0, 0, hwnd, reinterpret_cast<HMENU>(kSettingsUnlockPassId), create->hInstance,
                          nullptr);
      state->settings_unlock_wallet_button =
          CreateWindowW(L"BUTTON", L"Unlock Wallet", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 0, 0, 0, 0,
                        hwnd, reinterpret_cast<HMENU>(kSettingsUnlockWalletId), create->hInstance, nullptr);
      state->settings_recover_path_edit =
          CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"backup/identity-backup.dat",
                          WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 0, 0, 0, 0, hwnd,
                          reinterpret_cast<HMENU>(kSettingsRecoverPathId), create->hInstance, nullptr);
      state->settings_recover_backup_password_edit =
          CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL | ES_PASSWORD,
                          0, 0, 0, 0, hwnd, reinterpret_cast<HMENU>(kSettingsRecoverBackupPassId),
                          create->hInstance, nullptr);
      state->settings_recover_local_password_edit =
          CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL | ES_PASSWORD,
                          0, 0, 0, 0, hwnd, reinterpret_cast<HMENU>(kSettingsRecoverLocalPassId),
                          create->hInstance, nullptr);
      state->settings_recover_wallet_button =
          CreateWindowW(L"BUTTON", L"Recover Wallet", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 0, 0, 0, 0,
                        hwnd, reinterpret_cast<HMENU>(kSettingsRecoverWalletId), create->hInstance,
                        nullptr);
      state->settings_validate_now_button =
          CreateWindowW(L"BUTTON", L"Validate Now", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 0, 0, 0, 0,
                        hwnd, reinterpret_cast<HMENU>(kSettingsValidateNowId), create->hInstance,
                        nullptr);

      const HWND controls[] = {
          state->search_edit,
          state->close_button,
          state->parent_menu,
          state->secondary_menu,
          state->opening_list,
          state->tab_control,
          state->recipes_list,
          state->recipe_detail,
          state->recipe_thumb_up,
          state->recipe_rate_combo,
          state->recipe_rate_button,
          state->forum_view,
          state->forum_thread_title,
          state->forum_thread_body,
          state->forum_create_thread,
          state->forum_reply_body,
          state->forum_create_reply,
          state->upload_title,
          state->upload_category,
          state->upload_body,
          state->upload_submit,
          state->profile_view,
          state->profile_name_edit,
          state->profile_set_name_button,
          state->profile_duplicate_policy_toggle,
          state->profile_apply_policy_button,
          state->profile_cipher_password_edit,
          state->profile_cipher_salt_edit,
          state->profile_cipher_apply_button,
          state->profile_update_key_button,
          state->profile_export_path_edit,
          state->profile_export_password_edit,
          state->profile_export_salt_edit,
          state->profile_export_button,
          state->profile_import_path_edit,
          state->profile_import_password_edit,
          state->profile_import_button,
          state->profile_nuke_button,
          state->rewards_view,
          state->about_view,
          state->node_status_view,
          state->node_tor_toggle,
          state->node_i2p_toggle,
          state->node_localhost_toggle,
          state->node_mode_combo,
          state->node_apply_button,
          state->node_refresh_button,
          state->node_peer_edit,
          state->node_peer_add_button,
          state->node_community_id_edit,
          state->node_community_name_edit,
          state->node_community_apply_button,
          state->settings_view,
          state->settings_lock_wallet_button,
          state->settings_unlock_password_edit,
          state->settings_unlock_wallet_button,
          state->settings_recover_path_edit,
          state->settings_recover_backup_password_edit,
          state->settings_recover_local_password_edit,
          state->settings_recover_wallet_button,
          state->settings_validate_now_button,
      };
      for (HWND control : controls) {
        SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(ui_font), TRUE);
      }

      SetMenu(hwnd, make_main_menu());

      const alpha::Result init = state->api.init({
          .app_data_dir = "got-soup-data-win",
          .passphrase = "got-soup-dev-passphrase",
          .mode = alpha::AnonymityMode::Tor,
          .seed_peers = {"seed.got-soup.local:4001"},
          .seed_peers_mainnet = {"seed.got-soup.local:4001"},
          .seed_peers_testnet = {"seed.got-soup.local:14001"},
          .alpha_test_mode = false,
          .peers_dat_path = {},
          .community_profile_path = "tomato-soup",
          .production_swap = true,
          .block_interval_seconds = 25,
          .validation_interval_ticks = 10,
          .block_reward_units = 50,
          .minimum_post_value = 0,
          .genesis_psz_timestamp = "",
          .mainnet_initial_allocations = {},
          .testnet_initial_allocations = {},
          .chain_policy = {
              .confirmation_threshold = 1,
              .fork_choice_rule = "most-work-then-oldest",
              .max_reorg_depth = 6,
              .checkpoint_interval_blocks = 288,
              .checkpoint_confirmations = 24,
          },
          .validation_limits = {
              .max_block_events = 512,
              .max_block_bytes = 1U << 20U,
              .max_event_bytes = 64U << 10U,
              .max_future_drift_seconds = 120,
              .max_past_drift_seconds = 7 * 24 * 60 * 60,
          },
          .default_moderation_policy = {
              .moderation_enabled = true,
              .require_finality_for_actions = true,
              .min_confirmations_for_enforcement = 1,
              .max_flags_before_auto_hide = 3,
              .role_model = "single-signer",
              .moderator_cids = {},
          },
          .default_moderators = {},
          .p2p_mainnet_port = 4001,
          .p2p_testnet_port = 14001,
      });
      if (!init.ok) {
        MessageBoxW(hwnd, utf8_to_wide(init.message).c_str(), L"Init Error", MB_OK | MB_ICONERROR);
      }

      bootstrap_demo_data(state);

      rebuild_parent_menu(state);
      rebuild_opening_list(state);
      refresh_recipe_list(state);
      refresh_profile_and_about(state);
      refresh_node_status_view(state);
      refresh_rewards_view(state);
      refresh_settings_view(state);

      RECT rect{};
      GetClientRect(hwnd, &rect);
      layout_controls(state, rect.right - rect.left, rect.bottom - rect.top);
      return 0;
    }

    case WM_SIZE:
      if (state != nullptr) {
        layout_controls(state, LOWORD(l_param), HIWORD(l_param));
      }
      return 0;

    case WM_NOTIFY:
      if (state != nullptr) {
        auto* notify = reinterpret_cast<LPNMHDR>(l_param);
        if (notify->idFrom == static_cast<UINT_PTR>(kMainTabsId) && notify->code == TCN_SELCHANGE) {
          refresh_tab_visibility(state);
          if (TabCtrl_GetCurSel(state->tab_control) == static_cast<int>(TabIndex::Rewards)) {
            refresh_rewards_view(state);
          } else if (TabCtrl_GetCurSel(state->tab_control) == static_cast<int>(TabIndex::Settings)) {
            refresh_settings_view(state);
          }
          return 0;
        }
      }
      break;

    case WM_COMMAND: {
      const int command_id = LOWORD(w_param);
      const int command_code = HIWORD(w_param);

      if (command_id == kSearchEditId && command_code == EN_CHANGE && state != nullptr) {
        rebuild_opening_list(state);
        refresh_recipe_list(state);
        return 0;
      }

      if (command_id == kParentMenuId && command_code == CBN_SELCHANGE && state != nullptr) {
        rebuild_secondary_menu(state);
        rebuild_opening_list(state);
        return 0;
      }

      if (command_id == kSecondaryMenuId && command_code == CBN_SELCHANGE && state != nullptr) {
        rebuild_opening_list(state);
        return 0;
      }

      if (command_id == kOpeningListId && command_code == LBN_SELCHANGE && state != nullptr) {
        refresh_recipe_detail(state);
        return 0;
      }

      if (command_id == kRecipesListId && command_code == LBN_SELCHANGE && state != nullptr) {
        refresh_recipe_detail(state);
        refresh_forum_view(state);
        return 0;
      }

      if (command_id == kForumCreateThreadId && command_code == BN_CLICKED && state != nullptr) {
        create_forum_thread_from_ui(hwnd, state);
        return 0;
      }

      if (command_id == kForumCreateReplyId && command_code == BN_CLICKED && state != nullptr) {
        create_forum_reply_from_ui(hwnd, state);
        return 0;
      }

      if (command_id == kUploadSubmitId && command_code == BN_CLICKED && state != nullptr) {
        const std::string title = wide_to_utf8(read_window_text(state->upload_title));
        const std::string category = wide_to_utf8(read_window_text(state->upload_category));
        const std::string body = wide_to_utf8(read_window_text(state->upload_body));

        const alpha::Result result = state->api.create_recipe({
            .category = category,
            .title = title,
            .markdown = body,
            .core_topic = false,
            .menu_segment = "community-post",
        });

        if (!result.ok) {
          MessageBoxW(hwnd, utf8_to_wide(result.message).c_str(), L"Upload Failed", MB_OK | MB_ICONERROR);
          return 0;
        }

        MessageBoxW(hwnd, L"Recipe uploaded into local event log and sync queue.", L"Upload Complete",
                    MB_OK | MB_ICONINFORMATION);
        refresh_recipe_list(state);
        refresh_node_status_view(state);
        TabCtrl_SetCurSel(state->tab_control, static_cast<int>(TabIndex::Recipes));
        refresh_tab_visibility(state);
        return 0;
      }

      if (command_id == kRecipeThumbUpId && command_code == BN_CLICKED && state != nullptr) {
        thumb_up_selected_recipe(hwnd, state);
        return 0;
      }

      if (command_id == kRecipeRateButtonId && command_code == BN_CLICKED && state != nullptr) {
        rate_selected_recipe(hwnd, state);
        return 0;
      }

      if (command_id == kNodeApplyId && command_code == BN_CLICKED && state != nullptr) {
        apply_node_controls(hwnd, state);
        refresh_profile_and_about(state);
        return 0;
      }

      if (command_id == kNodeRefreshId && command_code == BN_CLICKED && state != nullptr) {
        const alpha::Result reload = state->api.reload_peers_dat();
        if (!reload.ok) {
          show_result_if_error(hwnd, reload, L"Node Status Refresh");
        }
        refresh_node_status_view(state);
        refresh_profile_and_about(state);
        return 0;
      }

      if (command_id == kNodeAddPeerId && command_code == BN_CLICKED && state != nullptr) {
        add_peer_from_ui(hwnd, state);
        return 0;
      }

      if (command_id == kNodeCommunityApplyId && command_code == BN_CLICKED && state != nullptr) {
        apply_community_from_ui(hwnd, state);
        return 0;
      }

      if (command_id == kProfileSetNameId && command_code == BN_CLICKED && state != nullptr) {
        apply_profile_name_from_ui(hwnd, state);
        return 0;
      }

      if (command_id == kProfileApplyPolicyId && command_code == BN_CLICKED && state != nullptr) {
        apply_duplicate_policy_from_ui(hwnd, state);
        return 0;
      }

      if (command_id == kProfileCipherApplyId && command_code == BN_CLICKED && state != nullptr) {
        apply_profile_cipher_from_ui(hwnd, state);
        return 0;
      }

      if (command_id == kProfileUpdateKeyId && command_code == BN_CLICKED && state != nullptr) {
        update_key_to_peers_from_ui(hwnd, state);
        return 0;
      }

      if (command_id == kProfileExportButtonId && command_code == BN_CLICKED && state != nullptr) {
        export_key_backup_from_ui(hwnd, state);
        return 0;
      }

      if (command_id == kProfileImportButtonId && command_code == BN_CLICKED && state != nullptr) {
        import_key_backup_from_ui(hwnd, state);
        return 0;
      }

      if (command_id == kProfileNukeButtonId && command_code == BN_CLICKED && state != nullptr) {
        nuke_key_from_ui(hwnd, state);
        return 0;
      }

      if (command_id == kSettingsLockWalletId && command_code == BN_CLICKED && state != nullptr) {
        lock_wallet_from_ui(hwnd, state);
        return 0;
      }

      if (command_id == kSettingsUnlockWalletId && command_code == BN_CLICKED && state != nullptr) {
        unlock_wallet_from_ui(hwnd, state);
        return 0;
      }

      if (command_id == kSettingsRecoverWalletId && command_code == BN_CLICKED && state != nullptr) {
        recover_wallet_from_ui(hwnd, state);
        return 0;
      }

      if (command_id == kSettingsValidateNowId && command_code == BN_CLICKED && state != nullptr) {
        validate_now_from_ui(hwnd, state);
        return 0;
      }

      if (command_id == kCloseButtonId && command_code == BN_CLICKED) {
        PostMessageW(hwnd, WM_CLOSE, 0, 0);
        return 0;
      }

      if (command_id == kMenuCloseId) {
        PostMessageW(hwnd, WM_CLOSE, 0, 0);
        return 0;
      }

      if (command_id == kMenuAboutId && state != nullptr) {
        const std::string about = build_about_text(state);
        MessageBoxW(hwnd, utf8_to_wide(about).c_str(), L"About got-soup",
                    MB_OK | MB_ICONINFORMATION);
        return 0;
      }

      break;
    }

    case WM_DESTROY:
      PostQuitMessage(0);
      return 0;

    case WM_NCDESTROY:
      if (state != nullptr) {
        delete state;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
      }
      return 0;

    default:
      break;
  }

  return DefWindowProcW(hwnd, message, w_param, l_param);
}

}  // namespace

int WINAPI WinMain(HINSTANCE instance, HINSTANCE, LPSTR, int show_command) {
  {
    std::string splash;
    splash += "Got Soup::P2P Tomato Soup\r\n";
    splash += "Version: " + std::string{alpha::kAppVersion} + " (" + std::string{alpha::kBuildRelease} + ")\r\n";
    splash += "Network: mainnet (startup default)\r\n";
    splash += "Splash PNG: got-soup-data-win/assets/tomato_soup.png\r\n";
    MessageBoxW(nullptr, utf8_to_wide(splash).c_str(), L"Loading", MB_OK | MB_ICONINFORMATION);
  }

  WNDCLASSW wc{};
  wc.lpfnWndProc = window_proc;
  wc.hInstance = instance;
  wc.lpszClassName = kWindowClassName;
  wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
  wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);

  if (RegisterClassW(&wc) == 0) {
    return 1;
  }

  auto* state = new AppState();

  HWND hwnd = CreateWindowExW(0, kWindowClassName, kWindowTitle, WS_OVERLAPPEDWINDOW,
                              CW_USEDEFAULT, CW_USEDEFAULT, 1320, 800, nullptr, nullptr, instance,
                              state);
  if (hwnd == nullptr) {
    delete state;
    return 1;
  }

  ShowWindow(hwnd, show_command);
  UpdateWindow(hwnd);

  MSG msg;
  while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
    TranslateMessage(&msg);
    DispatchMessageW(&msg);
  }

  return static_cast<int>(msg.wParam);
}

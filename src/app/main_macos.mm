#import <Cocoa/Cocoa.h>

#include <algorithm>
#include <charconv>
#include <cstdlib>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

#include "core/api/core_api.hpp"
#include "core/model/app_meta.hpp"
#include "core/util/hash.hpp"

namespace {

NSString* to_ns_string(const std::string& text) {
  NSString* converted = [NSString stringWithUTF8String:text.c_str()];
  return converted != nil ? converted : @"";
}

std::string from_ns_string(NSString* text) {
  const char* utf8 = [text UTF8String];
  return utf8 != nullptr ? std::string{utf8} : std::string{};
}

std::string soup_address_from_cid(std::string_view cid) {
  if (cid.empty()) {
    return std::string{alpha::kAddressPrefix};
  }
  return std::string{alpha::kAddressPrefix} + alpha::util::sha256_like_hex(cid).substr(0, 39);
}

std::string basil_leaf_summary(std::int64_t basil_units) {
  const std::int64_t leafs = basil_units * alpha::kLeafsPerBasil;
  return std::to_string(basil_units) + " " + std::string{alpha::kCurrencyMajorName} + " " +
         std::string{alpha::kCurrencyMajorSymbol} + " (" + std::to_string(leafs) + " " +
         std::string{alpha::kCurrencyMinorName} + " " + std::string{alpha::kCurrencyMinorSymbol} + ")";
}

std::string compact_basil_amount(std::int64_t basil_units) {
  return std::to_string(basil_units) + " " + std::string{alpha::kCurrencyMajorSymbol};
}

std::string default_app_data_dir() {
  NSArray<NSURL*>* urls =
      [[NSFileManager defaultManager] URLsForDirectory:NSApplicationSupportDirectory inDomains:NSUserDomainMask];
  if (urls != nil && [urls count] > 0) {
    NSURL* app_support = [urls objectAtIndex:0];
    NSURL* soupnet_dir = [app_support URLByAppendingPathComponent:@"soupnet" isDirectory:YES];
    if (soupnet_dir != nil) {
      return from_ns_string([soupnet_dir path]);
    }
  }

  const char* home = std::getenv("HOME");
  if (home != nullptr && home[0] != '\0') {
    return std::string{home} + "/Library/Application Support/soupnet";
  }
  return "soupnet-data-macos";
}

void show_error_alert(NSWindow* window, const std::string& title, const std::string& message) {
  NSAlert* alert = [[NSAlert alloc] init];
  [alert setMessageText:to_ns_string(title)];
  [alert setInformativeText:to_ns_string(message)];
  [alert addButtonWithTitle:@"OK"];
  [alert beginSheetModalForWindow:window completionHandler:nil];
}

bool read_developer_mode_flag(const std::string& file_path) {
  std::ifstream in(file_path);
  if (!in.is_open()) {
    return false;
  }
  std::string line;
  while (std::getline(in, line)) {
    if (line == "developer=1") {
      return true;
    }
  }
  return false;
}

bool write_developer_mode_flag(const std::string& file_path, bool enabled) {
  std::ofstream out(file_path, std::ios::trunc);
  if (!out.is_open()) {
    return false;
  }
  out << "developer=" << (enabled ? "1" : "0") << "\n";
  return static_cast<bool>(out);
}

}  // namespace

@interface GotSoupMacController
    : NSObject <NSApplicationDelegate, NSTableViewDataSource, NSTableViewDelegate, NSSearchFieldDelegate>
@end

@implementation GotSoupMacController {
  NSWindow* _window;
  NSSearchField* _search_field;
  NSButton* _close_button;
  NSPopUpButton* _parent_menu;
  NSPopUpButton* _secondary_menu;
  NSTableView* _lookup_table;

  NSTabView* _tab_view;
  NSTextView* _recipes_text;
  NSButton* _recipes_thumb_button;
  NSPopUpButton* _recipes_rate_menu;
  NSButton* _recipes_rate_button;
  NSTextView* _forum_text;
  NSTextField* _forum_thread_title;
  NSTextView* _forum_thread_body;
  NSButton* _forum_create_thread_button;
  NSTextView* _forum_reply_body;
  NSButton* _forum_create_reply_button;
  NSTextField* _upload_title;
  NSTextField* _upload_category;
  NSTextView* _upload_body;
  NSButton* _upload_button;
  NSTextField* _profile_name_field;
  NSButton* _profile_set_name_button;
  NSButton* _profile_duplicate_policy_toggle;
  NSButton* _profile_apply_policy_button;
  NSSecureTextField* _profile_cipher_password_field;
  NSTextField* _profile_cipher_salt_field;
  NSButton* _profile_cipher_apply_button;
  NSButton* _profile_update_key_button;
  NSTextField* _profile_export_path_field;
  NSSecureTextField* _profile_export_password_field;
  NSTextField* _profile_export_salt_field;
  NSButton* _profile_export_button;
  NSTextField* _profile_import_path_field;
  NSSecureTextField* _profile_import_password_field;
  NSButton* _profile_import_button;
  NSButton* _profile_nuke_button;
  NSTextView* _profile_text;
  NSTextField* _wallet_available_value;
  NSTextField* _wallet_pending_value;
  NSTextField* _wallet_total_value;
  NSTextView* _wallet_recent_text;
  NSTextField* _send_address_field;
  NSTextField* _send_amount_field;
  NSTextField* _send_memo_field;
  NSButton* _send_button;
  NSTextField* _sign_message_field;
  NSButton* _sign_button;
  NSTextView* _signature_text;
  NSTextView* _receive_text;
  NSTextView* _transactions_text;
  NSTextView* _hashspec_text;
  NSTextView* _about_text;
  NSTextView* _settings_text;
  NSButton* _settings_lock_wallet_button;
  NSSecureTextField* _settings_unlock_password_field;
  NSButton* _settings_unlock_wallet_button;
  NSTextField* _settings_recover_path_field;
  NSSecureTextField* _settings_recover_backup_password_field;
  NSSecureTextField* _settings_recover_local_password_field;
  NSButton* _settings_recover_wallet_button;
  NSButton* _settings_validate_button;
  NSButton* _settings_dev_mode_button;
  NSImageView* _about_image_view;
  NSTextField* _about_leaf_glyph;

  NSButton* _node_tor_toggle;
  NSButton* _node_i2p_toggle;
  NSButton* _node_localhost_toggle;
  NSPopUpButton* _node_mode_menu;
  NSButton* _node_apply_button;
  NSButton* _node_refresh_button;
  NSTextField* _node_peer_field;
  NSButton* _node_add_peer_button;
  NSTextField* _node_community_id_field;
  NSTextField* _node_community_name_field;
  NSButton* _node_community_apply_button;
  NSTextView* _node_status_text;

  std::unique_ptr<alpha::CoreApi> _api;
  std::vector<std::string> _opening_keys;
  std::vector<alpha::RecipeSummary> _recipes;
  bool _developer_mode;
  std::string _ui_mode_file_path;
  NSTimer* _live_timer;
}

- (void)buildMenu {
  NSMenu* menu_bar = [[NSMenu alloc] init];
  [NSApp setMainMenu:menu_bar];

  NSMenuItem* app_root = [[NSMenuItem alloc] init];
  [menu_bar addItem:app_root];

  NSMenu* app_menu = [[NSMenu alloc] initWithTitle:@"SoupNet"];
  NSMenuItem* about_item = [[NSMenuItem alloc] initWithTitle:@"About / Credits"
                                                      action:@selector(showAboutCredits:)
                                               keyEquivalent:@""];
  [about_item setTarget:self];
  [app_menu addItem:about_item];
  [app_menu addItem:[NSMenuItem separatorItem]];

  NSMenuItem* quit_item = [[NSMenuItem alloc] initWithTitle:@"Quit SoupNet"
                                                     action:@selector(terminate:)
                                              keyEquivalent:@"q"];
  [app_menu addItem:quit_item];
  [app_root setSubmenu:app_menu];

  NSMenuItem* file_root = [[NSMenuItem alloc] init];
  [menu_bar addItem:file_root];

  NSMenu* file_menu = [[NSMenu alloc] initWithTitle:@"File"];
  NSMenuItem* close_item = [[NSMenuItem alloc] initWithTitle:@"Close"
                                                      action:@selector(onClose:)
                                               keyEquivalent:@"w"];
  [close_item setTarget:self];
  [file_menu addItem:close_item];
  [file_root setSubmenu:file_menu];
}

- (NSTextView*)makeReadOnlyTextViewInFrame:(NSRect)frame parent:(NSView*)parent {
  NSScrollView* scroll = [[NSScrollView alloc] initWithFrame:frame];
  [scroll setHasVerticalScroller:YES];
  [scroll setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];

  NSTextView* text_view = [[NSTextView alloc] initWithFrame:[scroll bounds]];
  [text_view setEditable:NO];
  [text_view setSelectable:YES];
  [[text_view textContainer] setWidthTracksTextView:YES];

  [scroll setDocumentView:text_view];
  [parent addSubview:scroll];
  return text_view;
}

- (void)refreshProfileAndAbout {
  const auto profile = _api->profile();
  const auto node = _api->node_status();

  [_profile_name_field setStringValue:to_ns_string(profile.display_name)];
  [_profile_duplicate_policy_toggle setState:profile.reject_duplicate_names ? NSControlStateValueOn
                                                                           : NSControlStateValueOff];

  const std::string soup_address = soup_address_from_cid(profile.cid.value);
  std::string profile_text;
  profile_text += "CID: " + profile.cid.value + "\n";
  profile_text += std::string{alpha::kNetworkDisplayName} + " Address: " + soup_address + "\n";
  profile_text += "Display Name: " + profile.display_name + "\n";
  profile_text += "Display Name State: ";
  profile_text += (profile.display_name_immortalized ? "IMMORTALIZED" : "not set");
  profile_text += "\n";
  profile_text += "Duplicate Name Policy: ";
  profile_text += (profile.reject_duplicate_names ? "REJECT" : "ALLOW");
  profile_text += "\n";
  profile_text += "Duplicate State: ";
  profile_text += (profile.duplicate_name_detected ? "DUPLICATE DETECTED" : "UNIQUE");
  profile_text += " (count=" + std::to_string(profile.duplicate_name_count) + ")\n\n";
  profile_text += "Bio:\n" + profile.bio_markdown + "\n\n";
  profile_text += "Community: " + node.community.community_id + "\n";
  profile_text += "Profile file: " + node.community.profile_path + "\n";
  profile_text += "Balance: " + basil_leaf_summary(node.local_reward_balance) + "\n";
  profile_text += "Active Transport: ";
  profile_text += (node.active_mode == alpha::AnonymityMode::I2P ? "I2P" : "Tor");
  profile_text += "\n";
  [_profile_text setString:to_ns_string(profile_text)];

  std::string about;
  about += std::string{alpha::kAppDisplayName} + "\n\n";
  about += "Current Tor Version: " + node.tor.version + "\n";
  about += "Current I2P Version: " + node.i2p.version + "\n";
  about += "Current P2P:Soup Version Build Release: " + std::string{alpha::kAppVersion} + " (" +
           std::string{alpha::kBuildRelease} + ")\n";
  about += "Authors: " + std::string{alpha::kAuthorList} + "\n\n";
  about += "Currency\n";
  about += "- " + std::string{alpha::kCurrencyMajorName} + " (1.0 " +
           std::string{alpha::kCurrencyMajorSymbol} + ")\n";
  about += "- " + std::string{alpha::kCurrencyMinorName} + " (0.0000000001 " +
           std::string{alpha::kCurrencyMinorSymbol} + ")\n";
  about += "- Address Prefix: " + std::string{alpha::kAddressPrefix} + "\n\n";
  about += "Credits\n";
  about += "- Core: C++23 alpha_core module\n";
  about += "- UI: Native Cocoa shell\n";
  about += "- Planned deps: libp2p, libsodium, SQLCipher, libtor, i2pd\n\n";
  about += "Reference\n";
  about += "- https://github.com/hendrayoga/smallchange\n\n";
  about += "Assets\n";
  about += "- About PNG (transparent): " + node.data_dir + "/assets/about.png\n";
  about += "- Splash PNG: " + node.data_dir + "/assets/tomato_soup.png\n\n";
  about += "- Leaf Icon (currency): " + node.data_dir + "/assets/leaf_icon.png\n\n";
  about += "Chain\n";
  about += "- Chain ID: " + node.genesis.chain_id + "\n";
  about += "- Network: " + node.genesis.network_id + "\n";
  about += "- Genesis Merkle: " + node.genesis.merkle_root + "\n";
  about += "- Genesis Block: " + node.genesis.block_hash + "\n\n";
  about += "Core Phase 1:\n" + node.core_phase_status + "\n";
  [_about_text setString:to_ns_string(about)];

  NSString* about_path = to_ns_string(node.data_dir + "/assets/about.png");
  NSImage* about_image = [[NSImage alloc] initWithContentsOfFile:about_path];
  if (about_image != nil) {
    [_about_image_view setImage:about_image];
    [_about_image_view setHidden:NO];
  } else {
    [_about_image_view setHidden:YES];
  }
  [_about_leaf_glyph setStringValue:@"❧"];
  [_about_leaf_glyph setTextColor:[NSColor systemGreenColor]];
}

- (void)refreshForumTab {
  std::string forum;
  forum += "Forum Summary\n\n";

  if (_recipes.empty()) {
    forum += "No recipes yet. Upload one from the Upload tab.\n";
    [_forum_text setString:to_ns_string(forum)];
    return;
  }

  const auto& recipe = _recipes.front();
  const auto threads = _api->threads(recipe.recipe_id);
  forum += "Recipe: " + recipe.title + "\n";
  forum += "Recipe ID: " + recipe.recipe_id + "\n";
  forum += "Segment: ";
  forum += (recipe.core_topic ? "CORE TOPIC" : "COMMUNITY POST");
  forum += "\n";
  forum += "Thread count: " + std::to_string(threads.size()) + "\n\n";

  for (const auto& thread : threads) {
    forum += "- " + thread.title + " [thread_id=" + thread.thread_id + "]";
    forum += " (replies: " + std::to_string(thread.reply_count) + ")\n";
  }

  if (threads.empty()) {
    forum += "No forum threads yet for this recipe. Use the fields below to create one.\n";
  } else {
    const auto replies = _api->replies(threads.front().thread_id);
    forum += "\nLatest thread target for reply: " + threads.front().thread_id + "\n";
    if (replies.empty()) {
      forum += "No replies yet. Add a reply below.\n";
    } else {
      forum += "Replies in latest thread:\n";
      for (const auto& reply : replies) {
        forum += "  * [" + reply.reply_id + "] " + reply.author_cid + "\n";
      }
    }
  }

  [_forum_text setString:to_ns_string(forum)];
}

- (void)refreshRecipesTab {
  const std::string query = from_ns_string([_search_field stringValue]);
  _recipes = _api->search({.text = query, .category = {}});

  std::string output;
  output += "Recipes\n\n";

  if (_recipes.empty()) {
    output += "No recipes found for current query.\n\n";
  }

  for (const auto& recipe : _recipes) {
    output += recipe.core_topic ? "[CORE] " : "[POST] ";
    output += recipe.title + " [" + recipe.category + "]";
    output += " 👍" + std::to_string(recipe.thumbs_up_count);
    output += " rating=" + std::to_string(recipe.average_rating);
    output += " (" + std::to_string(recipe.review_count) + ")\n";
  }

  NSInteger selected_row = [_lookup_table selectedRow];
  if (selected_row >= 0 && static_cast<size_t>(selected_row) < _opening_keys.size()) {
    const auto& key = _opening_keys[static_cast<size_t>(selected_row)];
    const auto wiki = _api->reference_lookup(key);
    if (wiki.has_value()) {
      output += "\nInternal Reference\n";
      output += "[" + wiki->key + "] " + wiki->title + "\n\n";
      output += wiki->body + "\n";
    }
  }

  [_recipes_text setString:to_ns_string(output)];
  [self refreshForumTab];
}

- (void)refreshNodeStatusTab {
  (void)_api->sync_tick();
  const auto node = _api->node_status();

  [_node_tor_toggle setState:node.tor_enabled ? NSControlStateValueOn : NSControlStateValueOff];
  [_node_i2p_toggle setState:node.i2p_enabled ? NSControlStateValueOn : NSControlStateValueOff];
  [_node_localhost_toggle setState:node.alpha_test_mode ? NSControlStateValueOn : NSControlStateValueOff];
  [_node_mode_menu selectItemWithTitle:(node.active_mode == alpha::AnonymityMode::I2P ? @"I2P" : @"Tor")];

  std::string text;
  text += "Node Status\n\n";
  text += "Core Phase: " + node.core_phase_status + "\n\n";

  text += "Active transport: ";
  text += (node.active_mode == alpha::AnonymityMode::I2P ? "I2P" : "Tor");
  text += "\n";
  text += "Alpha test mode: ";
  text += (node.alpha_test_mode ? "ON" : "OFF");
  text += "\n\n";

  text += "Tor: ";
  text += (node.tor.running ? "running" : "stopped");
  text += " | updates=" + std::to_string(node.tor.update_count);
  text += " | endpoint=" + node.tor.endpoint.host + ":" + std::to_string(node.tor.endpoint.port) + "\n";
  text += "  " + node.tor.details + "\n";

  text += "I2P: ";
  text += (node.i2p.running ? "running" : "stopped");
  text += " | updates=" + std::to_string(node.i2p.update_count);
  text += " | endpoint=" + node.i2p.endpoint.host + ":" + std::to_string(node.i2p.endpoint.port) + "\n";
  text += "  " + node.i2p.details + "\n\n";

  text += "P2P runtime: ";
  text += (node.p2p.running ? "running" : "stopped");
  text += "\n";
  text += "Network: " + node.p2p.network + "\n";
  text += "Configured Ports: mainnet=" + std::to_string(node.p2p_mainnet_port);
  text += " testnet=" + std::to_string(node.p2p_testnet_port) + "\n";
  text += "Bind: " + node.p2p.bind_host + ":" + std::to_string(node.p2p.bind_port) + "\n";
  text += "Proxy Port: " + std::to_string(node.p2p.proxy_port) + "\n";
  text += "Peers: " + std::to_string(node.p2p.peer_count) + "\n";
  text += "Outbound queue: " + std::to_string(node.p2p.outbound_queue) + "\n";
  text += "Seen events: " + std::to_string(node.p2p.seen_event_count) + "\n";
  text += "Sync ticks: " + std::to_string(node.p2p.sync_tick_count) + "\n\n";

  text += "Peers.dat: " + node.peers_dat_path + "\n";
  for (const auto& peer : node.peers) {
    text += "- " + peer + "\n";
  }

  text += "\nDB health: ";
  text += (node.db.healthy ? "healthy" : "warning");
  text += "\n";
  text += "DB details: " + node.db.details + "\n";
  text += "Events=" + std::to_string(node.db.event_count);
  text += " Recipes=" + std::to_string(node.db.recipe_count);
  text += " Threads=" + std::to_string(node.db.thread_count);
  text += " Replies=" + std::to_string(node.db.reply_count) + "\n";
  text += "Event log bytes: " + std::to_string(node.db.event_log_size_bytes) + "\n\n";

  text += "Consensus Hash: " + node.db.consensus_hash + "\n";
  text += "Timeline Hash: " + node.db.timeline_hash + "\n";
  text += "Chain ID: " + node.db.chain_id + " (" + node.db.network_id + ")\n";
  text += "Genesis pszTimestamp: " + node.db.genesis_psz_timestamp + "\n";
  text += "Latest Merkle Root: " + node.db.latest_merkle_root + "\n";
  text += "Blocks: " + std::to_string(node.db.block_count);
  text += " Reserved=" + std::to_string(node.db.reserved_block_count);
  text += " Confirmed=" + std::to_string(node.db.confirmed_block_count);
  text += " Backfilled=" + std::to_string(node.db.backfilled_block_count) + "\n";
  text += "Rewards: supply=" + basil_leaf_summary(node.db.reward_supply);
  text += " local=" + basil_leaf_summary(node.local_reward_balance);
  text += " claims=" + std::to_string(node.db.reward_claim_event_count);
  text += " transfers=" + std::to_string(node.db.reward_transfer_event_count) + "\n";
  text += "Issued=" + basil_leaf_summary(node.db.issued_reward_total);
  text += " Burned=" + basil_leaf_summary(node.db.burned_fee_total);
  text += " Cap=" + basil_leaf_summary(node.db.max_token_supply) + "\n";
  text += "Invalid economic events: " + std::to_string(node.db.invalid_economic_event_count) + "\n";
  text += "Dropped invalid events: " + std::to_string(node.db.invalid_event_drop_count) + "\n";
  text += "Block interval sec: " + std::to_string(node.db.block_interval_seconds) + "\n";
  text += "Finality threshold: " + std::to_string(node.db.confirmation_threshold) + "\n";
  text += "Fork choice: " + node.db.fork_choice_rule + "\n";
  text += "Max reorg depth: " + std::to_string(node.db.max_reorg_depth) + "\n";
  text += "Checkpoint interval: " + std::to_string(node.db.checkpoint_interval_blocks) + "\n";
  text += "Checkpoint confirmations: " + std::to_string(node.db.checkpoint_confirmations) + "\n";
  text += "Checkpoint count: " + std::to_string(node.db.checkpoint_count) + "\n";
  text += "Blockdata format: v" + std::to_string(node.db.blockdata_format_version) + "\n";
  text += "Recovered from corruption: ";
  text += (node.db.recovered_from_corruption ? "YES" : "NO");
  text += "\n";
  text += "Last block unix: " + std::to_string(node.db.last_block_unix) + "\n";
  text += "Backtest: ";
  text += (node.db.backtest_ok ? "PASS" : "FAIL");
  text += " (last=" + std::to_string(node.db.last_backtest_unix) + ")\n";
  text += "Backtest details: " + node.db.backtest_details + "\n\n";

  text += "Community\n";
  text += "ID: " + node.community.community_id + "\n";
  text += "Name: " + node.community.display_name + "\n";
  text += "Profile file: " + node.community.profile_path + "\n";
  text += "Cipher key: " + node.community.cipher_key + "\n";
  text += "Store path: " + node.community.store_path + "\n";
  text += "Min Post Value: " + std::to_string(node.community.minimum_post_value) + "\n";
  text += "Block Reward Units: " + std::to_string(node.community.block_reward_units) + "\n";
  text += "\nWallet\n";
  text += "Locked: ";
  text += (node.wallet.locked ? "YES" : "NO");
  text += "\nDestroyed: ";
  text += (node.wallet.destroyed ? "YES" : "NO");
  text += "\nRecovery Required: ";
  text += (node.wallet.recovery_required ? "YES" : "NO");
  text += "\nVault: " + node.wallet.vault_path + "\n";

  if (!node.reward_balances.empty()) {
    text += "\nReward balances\n";
    for (const auto& balance : node.reward_balances) {
      std::string label =
          balance.display_name.empty() ? balance.cid : (balance.display_name + " (" + balance.cid + ")");
      text += "- " + label + ": " + std::to_string(balance.balance) + "\n";
    }
  }

  if (!node.known_communities.empty()) {
    text += "\nKnown communities\n";
    for (const auto& community : node.known_communities) {
      text += "- " + community.community_id + " (" + community.profile_path + ")\n";
    }
  }

  [_node_status_text setString:to_ns_string(text)];
  [self refreshRewardsTab];
  [self refreshSendReceiveTransactionsTabs];
  [self refreshHashSpecTab];
  [self refreshSettingsTab];
}

- (void)refreshRewardsTab {
  const auto node = _api->node_status();
  const std::int64_t available = node.local_reward_balance;
  const std::int64_t pending = 0;
  const std::int64_t total = available + pending;

  [_wallet_available_value setStringValue:to_ns_string(compact_basil_amount(available))];
  [_wallet_pending_value setStringValue:to_ns_string(compact_basil_amount(pending))];
  [_wallet_total_value setStringValue:to_ns_string(compact_basil_amount(total))];

  const auto transactions = _api->reward_transactions();
  std::string tx_text;
  if (transactions.empty()) {
    tx_text += "No recent transactions.\n";
  } else {
    const std::size_t limit = std::min<std::size_t>(transactions.size(), 12U);
    for (std::size_t i = 0; i < limit; ++i) {
      const auto& tx = transactions[i];
      const bool outgoing = tx.amount < 0;
      tx_text += outgoing ? "- " : "+ ";
      tx_text += compact_basil_amount(outgoing ? -tx.amount : tx.amount);
      tx_text += "  unix:";
      tx_text += std::to_string(tx.unix_ts);
      tx_text += "\n";
      tx_text += tx.to_address.empty() ? tx.from_address : tx.to_address;
      tx_text += "\n";
      tx_text += "conf:";
      tx_text += std::to_string(tx.confirmation_count);
      tx_text += " fee:";
      tx_text += compact_basil_amount(tx.fee);
      tx_text += "\n\n";
    }
  }
  [_wallet_recent_text setString:to_ns_string(tx_text)];
}

- (void)refreshSendReceiveTransactionsTabs {
  const auto receive = _api->receive_info();
  std::string receive_text;
  receive_text += "Receive\n\n";
  receive_text += "Display Name: " + receive.display_name + "\n";
  receive_text += "CID: " + receive.cid + "\n";
  receive_text += "Address: " + receive.address + "\n\n";
  receive_text += "PubKey:\n" + receive.public_key + "\n\n";
  receive_text += "PrivKey:\n" + receive.private_key + "\n";
  [_receive_text setString:to_ns_string(receive_text)];

  const auto transactions = _api->reward_transactions();
  std::string tx_text;
  tx_text += "Transactions History\n\n";
  if (transactions.empty()) {
    tx_text += "No transfer events found.\n";
  } else {
    for (const auto& tx : transactions) {
      tx_text += "- TxID: " + tx.transfer_id + "\n";
      tx_text += "  Event: " + tx.event_id + "\n";
      tx_text += "  From: " + tx.from_address + "\n";
      tx_text += "  To: " + tx.to_address + "\n";
      tx_text += "  Amount: " + basil_leaf_summary(tx.amount) + "\n";
      tx_text += "  Fee Burn: " + basil_leaf_summary(tx.fee) + "\n";
      tx_text += "  Memo: " + tx.memo + "\n";
      tx_text += "  Unix: " + std::to_string(tx.unix_ts) + "\n";
      tx_text += "  Confirmations: " + std::to_string(tx.confirmation_count) + "\n\n";
    }
  }
  [_transactions_text setString:to_ns_string(tx_text)];
}

- (void)refreshHashSpecTab {
  [_hashspec_text setString:to_ns_string(_api->hashspec_console())];
}

- (void)refreshSettingsTab {
  const auto node = _api->node_status();
  std::string text;
  text += "Settings Panel\n\n";
  text += "Data Dir: " + node.data_dir + "\n";
  text += "Events: " + node.db.events_file + "\n";
  text += "Blockdata: " + node.db.blockdata_file + "\n";
  text += "Snapshot: " + node.db.snapshot_file + "\n";
  text += "Peers: " + node.peers_dat_path + "\n";
  text += "Vault: " + node.wallet.vault_path + "\n";
  text += "Backup: " + node.wallet.backup_last_path + "\n\n";

  text += "Wallet\n";
  text += "Locked: ";
  text += (node.wallet.locked ? "YES" : "NO");
  text += "\nDestroyed: ";
  text += (node.wallet.destroyed ? "YES" : "NO");
  text += "\nRecovery Required: ";
  text += (node.wallet.recovery_required ? "YES" : "NO");
  text += "\nLast unlock unix: " + std::to_string(node.wallet.last_unlocked_unix);
  text += "\nLast lock unix: " + std::to_string(node.wallet.last_locked_unix) + "\n\n";

  text += "Finality / Fork Policy\n";
  text += "Confirmation threshold: " + std::to_string(node.chain_policy.confirmation_threshold) + "\n";
  text += "Fork choice: " + node.chain_policy.fork_choice_rule + "\n";
  text += "Max reorg depth: " + std::to_string(node.chain_policy.max_reorg_depth) + "\n";
  text += "Checkpoint interval: " + std::to_string(node.chain_policy.checkpoint_interval_blocks) + "\n";
  text += "Checkpoint confirmations: " + std::to_string(node.chain_policy.checkpoint_confirmations) + "\n\n";

  text += "Validation Limits\n";
  text += "Max block events: " + std::to_string(node.validation_limits.max_block_events) + "\n";
  text += "Max block bytes: " + std::to_string(node.validation_limits.max_block_bytes) + "\n";
  text += "Max event bytes: " + std::to_string(node.validation_limits.max_event_bytes) + "\n";
  text += "Future drift seconds: " + std::to_string(node.validation_limits.max_future_drift_seconds) + "\n";
  text += "Past drift seconds: " + std::to_string(node.validation_limits.max_past_drift_seconds) + "\n\n";
  text += "UI Mode: ";
  text += (_developer_mode ? "Developer" : "User Friendly");
  text += " (restart required after change)\n\n";

  text += "Genesis\n";
  text += "Chain ID: " + node.genesis.chain_id + "\n";
  text += "Network ID: " + node.genesis.network_id + "\n";
  text += "pszTimestamp: " + node.genesis.psz_timestamp + "\n";
  text += "Merkle Root: " + node.genesis.merkle_root + "\n";
  text += "Block Hash: " + node.genesis.block_hash + "\n";
  text += "Seed peers: " + std::to_string(node.genesis.seed_peers.size()) + "\n";
  for (const auto& seed : node.genesis.seed_peers) {
    text += "- " + seed + "\n";
  }

  [_settings_dev_mode_button setTitle:to_ns_string(_developer_mode ? "Disable Developer Mode (Reboot)"
                                                                   : "Enable Developer Mode (Reboot)")];
  [_settings_text setString:to_ns_string(text)];
}

- (void)applyUiModeVisibility {
  const BOOL dev = _developer_mode ? YES : NO;
  [_search_field setHidden:!dev];
  [_parent_menu setHidden:!dev];
  [_secondary_menu setHidden:!dev];
  [[_lookup_table enclosingScrollView] setHidden:!dev];

  [_recipes_thumb_button setHidden:!dev];
  [_recipes_rate_menu setHidden:!dev];
  [_recipes_rate_button setHidden:!dev];

  [_upload_title setHidden:!dev];
  [_upload_category setHidden:!dev];
  [[_upload_body enclosingScrollView] setHidden:!dev];
  [_upload_button setHidden:!dev];

  [_profile_name_field setHidden:!dev];
  [_profile_set_name_button setHidden:!dev];
  [_profile_duplicate_policy_toggle setHidden:!dev];
  [_profile_apply_policy_button setHidden:!dev];
  [_profile_cipher_password_field setHidden:!dev];
  [_profile_cipher_salt_field setHidden:!dev];
  [_profile_cipher_apply_button setHidden:!dev];
  [_profile_update_key_button setHidden:!dev];
  [_profile_export_path_field setHidden:!dev];
  [_profile_export_password_field setHidden:!dev];
  [_profile_export_salt_field setHidden:!dev];
  [_profile_export_button setHidden:!dev];
  [_profile_import_path_field setHidden:!dev];
  [_profile_import_password_field setHidden:!dev];
  [_profile_import_button setHidden:!dev];
  [_profile_nuke_button setHidden:!dev];

  [_send_address_field setHidden:!dev];
  [_send_amount_field setHidden:!dev];
  [_send_memo_field setHidden:!dev];
  [_send_button setHidden:!dev];
  [_sign_message_field setHidden:!dev];
  [_sign_button setHidden:!dev];
  [[_signature_text enclosingScrollView] setHidden:!dev];

  [_node_tor_toggle setHidden:!dev];
  [_node_i2p_toggle setHidden:!dev];
  [_node_localhost_toggle setHidden:!dev];
  [_node_mode_menu setHidden:!dev];
  [_node_apply_button setHidden:!dev];
  [_node_refresh_button setHidden:!dev];
  [_node_peer_field setHidden:!dev];
  [_node_add_peer_button setHidden:!dev];
  [_node_community_id_field setHidden:!dev];
  [_node_community_name_field setHidden:!dev];
  [_node_community_apply_button setHidden:!dev];

  [_settings_lock_wallet_button setHidden:!dev];
  [_settings_unlock_password_field setHidden:!dev];
  [_settings_unlock_wallet_button setHidden:!dev];
  [_settings_recover_path_field setHidden:!dev];
  [_settings_recover_backup_password_field setHidden:!dev];
  [_settings_recover_local_password_field setHidden:!dev];
  [_settings_recover_wallet_button setHidden:!dev];
  [_settings_validate_button setHidden:!dev];
}

- (void)onLiveTick:(NSTimer*)timer {
  (void)timer;
  (void)_api->sync_tick();
  [self refreshRewardsTab];
  [self refreshHashSpecTab];
}

- (void)rebuildSecondaryMenu {
  [_secondary_menu removeAllItems];

  const std::string parent = from_ns_string([_parent_menu titleOfSelectedItem]);
  const auto secondary = _api->reference_secondary_menus(parent);
  for (const auto& item : secondary) {
    [_secondary_menu addItemWithTitle:to_ns_string(item)];
  }

  if (!secondary.empty()) {
    [_secondary_menu selectItemAtIndex:0];
  }
}

- (void)rebuildParentMenu {
  [_parent_menu removeAllItems];

  const auto parents = _api->reference_parent_menus();
  for (const auto& item : parents) {
    [_parent_menu addItemWithTitle:to_ns_string(item)];
  }

  if (!parents.empty()) {
    [_parent_menu selectItemAtIndex:0];
  }

  [self rebuildSecondaryMenu];
}

- (void)rebuildOpeningList {
  const std::string parent = from_ns_string([_parent_menu titleOfSelectedItem]);
  const std::string secondary = from_ns_string([_secondary_menu titleOfSelectedItem]);
  const std::string query = from_ns_string([_search_field stringValue]);

  _opening_keys = _api->reference_openings(parent, secondary, query);
  [_lookup_table reloadData];

  if (!_opening_keys.empty()) {
    [_lookup_table selectRowIndexes:[NSIndexSet indexSetWithIndex:0] byExtendingSelection:NO];
  }

  [self refreshRecipesTab];
}

- (void)buildTabsInFrame:(NSRect)frame parent:(NSView*)content {
  _tab_view = [[NSTabView alloc] initWithFrame:frame];
  [_tab_view setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
  [content addSubview:_tab_view];

  NSTabViewItem* recipes_item = [[NSTabViewItem alloc] initWithIdentifier:@"recipes"];
  [recipes_item setLabel:@"Home"];
  NSView* recipes_view = [[NSView alloc] initWithFrame:[_tab_view bounds]];
  [recipes_view setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
  _recipes_text = [self makeReadOnlyTextViewInFrame:NSMakeRect(0.0, 34.0, frame.size.width, frame.size.height - 34.0)
                                              parent:recipes_view];
  _recipes_thumb_button = [[NSButton alloc] initWithFrame:NSMakeRect(0.0, 4.0, 120.0, 24.0)];
  [_recipes_thumb_button setTitle:@"Thumbs Up +1"];
  [_recipes_thumb_button setButtonType:NSButtonTypeMomentaryPushIn];
  [_recipes_thumb_button setBezelStyle:NSBezelStyleRounded];
  [_recipes_thumb_button setTarget:self];
  [_recipes_thumb_button setAction:@selector(onRecipesThumbUp:)];
  [_recipes_thumb_button setAutoresizingMask:NSViewMaxXMargin | NSViewMaxYMargin];
  [recipes_view addSubview:_recipes_thumb_button];

  _recipes_rate_menu = [[NSPopUpButton alloc] initWithFrame:NSMakeRect(128.0, 4.0, 70.0, 24.0) pullsDown:NO];
  [_recipes_rate_menu addItemWithTitle:@"1"];
  [_recipes_rate_menu addItemWithTitle:@"2"];
  [_recipes_rate_menu addItemWithTitle:@"3"];
  [_recipes_rate_menu addItemWithTitle:@"4"];
  [_recipes_rate_menu addItemWithTitle:@"5"];
  [_recipes_rate_menu selectItemWithTitle:@"5"];
  [_recipes_rate_menu setAutoresizingMask:NSViewMaxXMargin | NSViewMaxYMargin];
  [recipes_view addSubview:_recipes_rate_menu];

  _recipes_rate_button = [[NSButton alloc] initWithFrame:NSMakeRect(204.0, 4.0, 84.0, 24.0)];
  [_recipes_rate_button setTitle:@"Rate"];
  [_recipes_rate_button setButtonType:NSButtonTypeMomentaryPushIn];
  [_recipes_rate_button setBezelStyle:NSBezelStyleRounded];
  [_recipes_rate_button setTarget:self];
  [_recipes_rate_button setAction:@selector(onRecipesRate:)];
  [_recipes_rate_button setAutoresizingMask:NSViewMaxXMargin | NSViewMaxYMargin];
  [recipes_view addSubview:_recipes_rate_button];
  [recipes_item setView:recipes_view];
  [_tab_view addTabViewItem:recipes_item];

  NSTabViewItem* forum_item = [[NSTabViewItem alloc] initWithIdentifier:@"forum"];
  [forum_item setLabel:@"Forum"];
  NSView* forum_view = [[NSView alloc] initWithFrame:[_tab_view bounds]];
  [forum_view setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];

  const CGFloat forum_summary_h = std::max<CGFloat>(160.0, frame.size.height * 0.42);
  const CGFloat forum_summary_y = frame.size.height - forum_summary_h;
  _forum_text = [self makeReadOnlyTextViewInFrame:NSMakeRect(0.0, forum_summary_y, frame.size.width, forum_summary_h)
                                           parent:forum_view];

  _forum_thread_title = [[NSTextField alloc] initWithFrame:NSMakeRect(0.0, forum_summary_y - 30.0,
                                                                       frame.size.width - 122.0, 24.0)];
  [_forum_thread_title setPlaceholderString:@"Thread title"];
  [_forum_thread_title setAutoresizingMask:NSViewWidthSizable | NSViewMinYMargin];
  [forum_view addSubview:_forum_thread_title];

  _forum_create_thread_button =
      [[NSButton alloc] initWithFrame:NSMakeRect(frame.size.width - 116.0, forum_summary_y - 30.0, 116.0, 24.0)];
  [_forum_create_thread_button setTitle:@"Create Thread"];
  [_forum_create_thread_button setButtonType:NSButtonTypeMomentaryPushIn];
  [_forum_create_thread_button setBezelStyle:NSBezelStyleRounded];
  [_forum_create_thread_button setTarget:self];
  [_forum_create_thread_button setAction:@selector(onCreateForumThread:)];
  [_forum_create_thread_button setAutoresizingMask:NSViewMinXMargin | NSViewMinYMargin];
  [forum_view addSubview:_forum_create_thread_button];

  NSScrollView* forum_thread_body_scroll = [[NSScrollView alloc] initWithFrame:NSMakeRect(
      0.0, forum_summary_y - 104.0, frame.size.width, 68.0)];
  [forum_thread_body_scroll setHasVerticalScroller:YES];
  [forum_thread_body_scroll setAutoresizingMask:NSViewWidthSizable | NSViewMinYMargin];
  _forum_thread_body = [[NSTextView alloc] initWithFrame:[forum_thread_body_scroll bounds]];
  [[_forum_thread_body textContainer] setWidthTracksTextView:YES];
  [forum_thread_body_scroll setDocumentView:_forum_thread_body];
  [forum_view addSubview:forum_thread_body_scroll];

  _forum_create_reply_button =
      [[NSButton alloc] initWithFrame:NSMakeRect(frame.size.width - 116.0, 6.0, 116.0, 24.0)];
  [_forum_create_reply_button setTitle:@"Create Reply"];
  [_forum_create_reply_button setButtonType:NSButtonTypeMomentaryPushIn];
  [_forum_create_reply_button setBezelStyle:NSBezelStyleRounded];
  [_forum_create_reply_button setTarget:self];
  [_forum_create_reply_button setAction:@selector(onCreateForumReply:)];
  [_forum_create_reply_button setAutoresizingMask:NSViewMinXMargin | NSViewMaxYMargin];
  [forum_view addSubview:_forum_create_reply_button];

  NSScrollView* forum_reply_body_scroll = [[NSScrollView alloc] initWithFrame:NSMakeRect(
      0.0, 34.0, frame.size.width - 122.0, std::max<CGFloat>(70.0, forum_summary_y - 140.0))];
  [forum_reply_body_scroll setHasVerticalScroller:YES];
  [forum_reply_body_scroll setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
  _forum_reply_body = [[NSTextView alloc] initWithFrame:[forum_reply_body_scroll bounds]];
  [[_forum_reply_body textContainer] setWidthTracksTextView:YES];
  [forum_reply_body_scroll setDocumentView:_forum_reply_body];
  [forum_view addSubview:forum_reply_body_scroll];

  [forum_item setView:forum_view];
  [_tab_view addTabViewItem:forum_item];

  NSTabViewItem* upload_item = [[NSTabViewItem alloc] initWithIdentifier:@"upload"];
  [upload_item setLabel:@"Upload"];
  NSView* upload_view = [[NSView alloc] initWithFrame:[_tab_view bounds]];
  [upload_view setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];

  _upload_title = [[NSTextField alloc] initWithFrame:NSMakeRect(0.0, frame.size.height - 34.0, frame.size.width, 24.0)];
  [_upload_title setPlaceholderString:@"Recipe title"];
  [_upload_title setAutoresizingMask:NSViewWidthSizable | NSViewMinYMargin];
  [upload_view addSubview:_upload_title];

  _upload_category = [[NSTextField alloc] initWithFrame:NSMakeRect(0.0, frame.size.height - 66.0, frame.size.width, 24.0)];
  [_upload_category setPlaceholderString:@"Category"];
  [_upload_category setAutoresizingMask:NSViewWidthSizable | NSViewMinYMargin];
  [upload_view addSubview:_upload_category];

  NSScrollView* upload_body_scroll = [[NSScrollView alloc] initWithFrame:NSMakeRect(0.0, 36.0, frame.size.width, frame.size.height - 108.0)];
  [upload_body_scroll setHasVerticalScroller:YES];
  [upload_body_scroll setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
  _upload_body = [[NSTextView alloc] initWithFrame:[upload_body_scroll bounds]];
  [[_upload_body textContainer] setWidthTracksTextView:YES];
  [upload_body_scroll setDocumentView:_upload_body];
  [upload_view addSubview:upload_body_scroll];

  _upload_button = [[NSButton alloc] initWithFrame:NSMakeRect(0.0, 0.0, 140.0, 28.0)];
  [_upload_button setTitle:@"Upload Recipe"];
  [_upload_button setButtonType:NSButtonTypeMomentaryPushIn];
  [_upload_button setBezelStyle:NSBezelStyleRounded];
  [_upload_button setTarget:self];
  [_upload_button setAction:@selector(onUploadRecipe:)];
  [_upload_button setAutoresizingMask:NSViewMaxXMargin | NSViewMaxYMargin];
  [upload_view addSubview:_upload_button];

  [upload_item setView:upload_view];
  [_tab_view addTabViewItem:upload_item];

  NSTabViewItem* profile_item = [[NSTabViewItem alloc] initWithIdentifier:@"profile"];
  [profile_item setLabel:@"Profile"];
  NSView* profile_view = [[NSView alloc] initWithFrame:[_tab_view bounds]];
  [profile_view setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
  const CGFloat row_h = 24.0;
  const CGFloat gap = 6.0;
  const CGFloat top = frame.size.height - row_h;

  _profile_name_field = [[NSTextField alloc] initWithFrame:NSMakeRect(0.0, top, frame.size.width - 278.0, row_h)];
  [_profile_name_field setPlaceholderString:@"Display name (immutable)"];
  [_profile_name_field setAutoresizingMask:NSViewWidthSizable | NSViewMinYMargin];
  [profile_view addSubview:_profile_name_field];

  _profile_set_name_button = [[NSButton alloc] initWithFrame:NSMakeRect(frame.size.width - 272.0, top, 132.0, row_h)];
  [_profile_set_name_button setTitle:@"Set Immortal + Sync"];
  [_profile_set_name_button setButtonType:NSButtonTypeMomentaryPushIn];
  [_profile_set_name_button setBezelStyle:NSBezelStyleRounded];
  [_profile_set_name_button setTarget:self];
  [_profile_set_name_button setAction:@selector(onProfileSetName:)];
  [_profile_set_name_button setAutoresizingMask:NSViewMinXMargin | NSViewMinYMargin];
  [profile_view addSubview:_profile_set_name_button];

  _profile_update_key_button = [[NSButton alloc] initWithFrame:NSMakeRect(frame.size.width - 134.0, top, 134.0, row_h)];
  [_profile_update_key_button setTitle:@"Update Key Peers"];
  [_profile_update_key_button setButtonType:NSButtonTypeMomentaryPushIn];
  [_profile_update_key_button setBezelStyle:NSBezelStyleRounded];
  [_profile_update_key_button setTarget:self];
  [_profile_update_key_button setAction:@selector(onProfileUpdateKey:)];
  [_profile_update_key_button setAutoresizingMask:NSViewMinXMargin | NSViewMinYMargin];
  [profile_view addSubview:_profile_update_key_button];

  const CGFloat row2 = top - (row_h + gap);
  _profile_duplicate_policy_toggle = [[NSButton alloc] initWithFrame:NSMakeRect(0.0, row2, 220.0, row_h)];
  [_profile_duplicate_policy_toggle setButtonType:NSButtonTypeSwitch];
  [_profile_duplicate_policy_toggle setTitle:@"Reject Duplicate Names"];
  [_profile_duplicate_policy_toggle setAutoresizingMask:NSViewMaxXMargin | NSViewMinYMargin];
  [profile_view addSubview:_profile_duplicate_policy_toggle];

  _profile_apply_policy_button = [[NSButton alloc] initWithFrame:NSMakeRect(226.0, row2, 120.0, row_h)];
  [_profile_apply_policy_button setTitle:@"Apply Policy"];
  [_profile_apply_policy_button setButtonType:NSButtonTypeMomentaryPushIn];
  [_profile_apply_policy_button setBezelStyle:NSBezelStyleRounded];
  [_profile_apply_policy_button setTarget:self];
  [_profile_apply_policy_button setAction:@selector(onProfileApplyPolicy:)];
  [_profile_apply_policy_button setAutoresizingMask:NSViewMaxXMargin | NSViewMinYMargin];
  [profile_view addSubview:_profile_apply_policy_button];

  const CGFloat row3 = top - ((row_h + gap) * 2.0);
  _profile_cipher_password_field =
      [[NSSecureTextField alloc] initWithFrame:NSMakeRect(0.0, row3, std::max<CGFloat>(120.0, frame.size.width * 0.30), row_h)];
  [_profile_cipher_password_field setPlaceholderString:@"Cipher password"];
  [_profile_cipher_password_field setAutoresizingMask:NSViewMaxXMargin | NSViewMinYMargin];
  [profile_view addSubview:_profile_cipher_password_field];

  _profile_cipher_salt_field =
      [[NSTextField alloc] initWithFrame:NSMakeRect(std::max<CGFloat>(120.0, frame.size.width * 0.30) + 8.0, row3,
                                                    std::max<CGFloat>(120.0, frame.size.width * 0.22), row_h)];
  [_profile_cipher_salt_field setPlaceholderString:@"Cipher salt"];
  [_profile_cipher_salt_field setAutoresizingMask:NSViewMaxXMargin | NSViewMinYMargin];
  [profile_view addSubview:_profile_cipher_salt_field];

  _profile_cipher_apply_button = [[NSButton alloc] initWithFrame:NSMakeRect(std::max<CGFloat>(120.0, frame.size.width * 0.52) + 16.0,
                                                                             row3, 134.0, row_h)];
  [_profile_cipher_apply_button setTitle:@"Apply Cipher"];
  [_profile_cipher_apply_button setButtonType:NSButtonTypeMomentaryPushIn];
  [_profile_cipher_apply_button setBezelStyle:NSBezelStyleRounded];
  [_profile_cipher_apply_button setTarget:self];
  [_profile_cipher_apply_button setAction:@selector(onProfileApplyCipher:)];
  [_profile_cipher_apply_button setAutoresizingMask:NSViewMaxXMargin | NSViewMinYMargin];
  [profile_view addSubview:_profile_cipher_apply_button];

  const CGFloat row4 = top - ((row_h + gap) * 3.0);
  _profile_export_path_field =
      [[NSTextField alloc] initWithFrame:NSMakeRect(0.0, row4, std::max<CGFloat>(160.0, frame.size.width - 430.0), row_h)];
  [_profile_export_path_field setStringValue:@"backup/identity-backup.dat"];
  [_profile_export_path_field setAutoresizingMask:NSViewWidthSizable | NSViewMinYMargin];
  [profile_view addSubview:_profile_export_path_field];

  _profile_export_password_field =
      [[NSSecureTextField alloc] initWithFrame:NSMakeRect(std::max<CGFloat>(160.0, frame.size.width - 430.0) + 8.0, row4, 120.0, row_h)];
  [_profile_export_password_field setPlaceholderString:@"Password"];
  [_profile_export_password_field setAutoresizingMask:NSViewMinXMargin | NSViewMinYMargin];
  [profile_view addSubview:_profile_export_password_field];

  _profile_export_salt_field =
      [[NSTextField alloc] initWithFrame:NSMakeRect(std::max<CGFloat>(160.0, frame.size.width - 430.0) + 136.0, row4, 80.0, row_h)];
  [_profile_export_salt_field setPlaceholderString:@"Salt"];
  [_profile_export_salt_field setAutoresizingMask:NSViewMinXMargin | NSViewMinYMargin];
  [profile_view addSubview:_profile_export_salt_field];

  _profile_export_button =
      [[NSButton alloc] initWithFrame:NSMakeRect(std::max<CGFloat>(160.0, frame.size.width - 430.0) + 224.0, row4, 90.0, row_h)];
  [_profile_export_button setTitle:@"Export"];
  [_profile_export_button setButtonType:NSButtonTypeMomentaryPushIn];
  [_profile_export_button setBezelStyle:NSBezelStyleRounded];
  [_profile_export_button setTarget:self];
  [_profile_export_button setAction:@selector(onProfileExportKey:)];
  [_profile_export_button setAutoresizingMask:NSViewMinXMargin | NSViewMinYMargin];
  [profile_view addSubview:_profile_export_button];

  const CGFloat row5 = top - ((row_h + gap) * 4.0);
  _profile_import_path_field =
      [[NSTextField alloc] initWithFrame:NSMakeRect(0.0, row5, std::max<CGFloat>(160.0, frame.size.width - 340.0), row_h)];
  [_profile_import_path_field setStringValue:@"backup/identity-backup.dat"];
  [_profile_import_path_field setAutoresizingMask:NSViewWidthSizable | NSViewMinYMargin];
  [profile_view addSubview:_profile_import_path_field];

  _profile_import_password_field =
      [[NSSecureTextField alloc] initWithFrame:NSMakeRect(std::max<CGFloat>(160.0, frame.size.width - 340.0) + 8.0, row5, 120.0, row_h)];
  [_profile_import_password_field setPlaceholderString:@"Password"];
  [_profile_import_password_field setAutoresizingMask:NSViewMinXMargin | NSViewMinYMargin];
  [profile_view addSubview:_profile_import_password_field];

  _profile_import_button =
      [[NSButton alloc] initWithFrame:NSMakeRect(std::max<CGFloat>(160.0, frame.size.width - 340.0) + 136.0, row5, 90.0, row_h)];
  [_profile_import_button setTitle:@"Import"];
  [_profile_import_button setButtonType:NSButtonTypeMomentaryPushIn];
  [_profile_import_button setBezelStyle:NSBezelStyleRounded];
  [_profile_import_button setTarget:self];
  [_profile_import_button setAction:@selector(onProfileImportKey:)];
  [_profile_import_button setAutoresizingMask:NSViewMinXMargin | NSViewMinYMargin];
  [profile_view addSubview:_profile_import_button];

  _profile_nuke_button =
      [[NSButton alloc] initWithFrame:NSMakeRect(std::max<CGFloat>(160.0, frame.size.width - 340.0) + 234.0, row5, 90.0, row_h)];
  [_profile_nuke_button setTitle:@"Nuke Key"];
  [_profile_nuke_button setButtonType:NSButtonTypeMomentaryPushIn];
  [_profile_nuke_button setBezelStyle:NSBezelStyleRounded];
  [_profile_nuke_button setTarget:self];
  [_profile_nuke_button setAction:@selector(onProfileNukeKey:)];
  [_profile_nuke_button setAutoresizingMask:NSViewMinXMargin | NSViewMinYMargin];
  [profile_view addSubview:_profile_nuke_button];

  const CGFloat text_h = std::max<CGFloat>(100.0, frame.size.height - ((row_h + gap) * 5.0) - 8.0);
  _profile_text = [self makeReadOnlyTextViewInFrame:NSMakeRect(0.0, 0.0, frame.size.width, text_h)
                                             parent:profile_view];
  [profile_item setView:profile_view];
  [_tab_view addTabViewItem:profile_item];

  NSTabViewItem* rewards_item = [[NSTabViewItem alloc] initWithIdentifier:@"rewards"];
  [rewards_item setLabel:@"Wallet"];
  NSView* rewards_view = [[NSView alloc] initWithFrame:[_tab_view bounds]];
  [rewards_view setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];

  const CGFloat panel_gap = 12.0;
  const CGFloat panel_w = std::max<CGFloat>(220.0, (frame.size.width - panel_gap) * 0.5);

  NSBox* balances_box =
      [[NSBox alloc] initWithFrame:NSMakeRect(0.0, frame.size.height - 260.0, panel_w, 250.0)];
  [balances_box setTitle:@"Balances"];
  [balances_box setBoxType:NSBoxPrimary];
  [balances_box setAutoresizingMask:NSViewWidthSizable | NSViewMinYMargin];
  NSView* balances_content = [balances_box contentView];

  NSTextField* available_label = [[NSTextField alloc] initWithFrame:NSMakeRect(12.0, 160.0, 120.0, 24.0)];
  [available_label setEditable:NO];
  [available_label setBezeled:NO];
  [available_label setDrawsBackground:NO];
  [available_label setSelectable:NO];
  [available_label setStringValue:@"Available:"];
  [balances_content addSubview:available_label];
  _wallet_available_value = [[NSTextField alloc] initWithFrame:NSMakeRect(132.0, 160.0, panel_w - 148.0, 24.0)];
  [_wallet_available_value setEditable:NO];
  [_wallet_available_value setBezeled:NO];
  [_wallet_available_value setDrawsBackground:NO];
  [_wallet_available_value setSelectable:NO];
  [_wallet_available_value setAlignment:NSTextAlignmentRight];
  [balances_content addSubview:_wallet_available_value];

  NSTextField* pending_label = [[NSTextField alloc] initWithFrame:NSMakeRect(12.0, 126.0, 120.0, 24.0)];
  [pending_label setEditable:NO];
  [pending_label setBezeled:NO];
  [pending_label setDrawsBackground:NO];
  [pending_label setSelectable:NO];
  [pending_label setStringValue:@"Pending:"];
  [balances_content addSubview:pending_label];
  _wallet_pending_value = [[NSTextField alloc] initWithFrame:NSMakeRect(132.0, 126.0, panel_w - 148.0, 24.0)];
  [_wallet_pending_value setEditable:NO];
  [_wallet_pending_value setBezeled:NO];
  [_wallet_pending_value setDrawsBackground:NO];
  [_wallet_pending_value setSelectable:NO];
  [_wallet_pending_value setAlignment:NSTextAlignmentRight];
  [balances_content addSubview:_wallet_pending_value];

  NSTextField* total_label = [[NSTextField alloc] initWithFrame:NSMakeRect(12.0, 60.0, 120.0, 24.0)];
  [total_label setEditable:NO];
  [total_label setBezeled:NO];
  [total_label setDrawsBackground:NO];
  [total_label setSelectable:NO];
  [total_label setStringValue:@"Total:"];
  [balances_content addSubview:total_label];
  _wallet_total_value = [[NSTextField alloc] initWithFrame:NSMakeRect(132.0, 60.0, panel_w - 148.0, 24.0)];
  [_wallet_total_value setEditable:NO];
  [_wallet_total_value setBezeled:NO];
  [_wallet_total_value setDrawsBackground:NO];
  [_wallet_total_value setSelectable:NO];
  [_wallet_total_value setAlignment:NSTextAlignmentRight];
  [balances_content addSubview:_wallet_total_value];
  [rewards_view addSubview:balances_box];

  NSBox* recent_box =
      [[NSBox alloc] initWithFrame:NSMakeRect(panel_w + panel_gap, 0.0, frame.size.width - panel_w - panel_gap, frame.size.height)];
  [recent_box setTitle:@"Recent transactions"];
  [recent_box setBoxType:NSBoxPrimary];
  [recent_box setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
  _wallet_recent_text = [self makeReadOnlyTextViewInFrame:[[recent_box contentView] bounds] parent:[recent_box contentView]];
  [_wallet_recent_text setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
  [rewards_view addSubview:recent_box];

  [rewards_item setView:rewards_view];
  [_tab_view addTabViewItem:rewards_item];

  NSTabViewItem* send_item = [[NSTabViewItem alloc] initWithIdentifier:@"send"];
  [send_item setLabel:@"Send"];
  NSView* send_view = [[NSView alloc] initWithFrame:[_tab_view bounds]];
  [send_view setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];

  _send_address_field =
      [[NSTextField alloc] initWithFrame:NSMakeRect(0.0, frame.size.height - 28.0, frame.size.width - 132.0, 24.0)];
  [_send_address_field setPlaceholderString:@"SoupNet Address (starts with S)"];
  [_send_address_field setAutoresizingMask:NSViewWidthSizable | NSViewMinYMargin];
  [send_view addSubview:_send_address_field];

  _send_amount_field = [[NSTextField alloc] initWithFrame:NSMakeRect(0.0, frame.size.height - 56.0, 180.0, 24.0)];
  [_send_amount_field setPlaceholderString:@"Amount (Basil units)"];
  [_send_amount_field setAutoresizingMask:NSViewMaxXMargin | NSViewMinYMargin];
  [send_view addSubview:_send_amount_field];

  _send_button = [[NSButton alloc] initWithFrame:NSMakeRect(188.0, frame.size.height - 56.0, 140.0, 24.0)];
  [_send_button setTitle:@"Send Basil/Leafs"];
  [_send_button setButtonType:NSButtonTypeMomentaryPushIn];
  [_send_button setBezelStyle:NSBezelStyleRounded];
  [_send_button setTarget:self];
  [_send_button setAction:@selector(onSendToAddress:)];
  [_send_button setAutoresizingMask:NSViewMaxXMargin | NSViewMinYMargin];
  [send_view addSubview:_send_button];

  _send_memo_field =
      [[NSTextField alloc] initWithFrame:NSMakeRect(0.0, frame.size.height - 84.0, frame.size.width, 24.0)];
  [_send_memo_field setPlaceholderString:@"Memo (optional)"];
  [_send_memo_field setAutoresizingMask:NSViewWidthSizable | NSViewMinYMargin];
  [send_view addSubview:_send_memo_field];

  _sign_message_field =
      [[NSTextField alloc] initWithFrame:NSMakeRect(0.0, frame.size.height - 112.0, frame.size.width - 132.0, 24.0)];
  [_sign_message_field setPlaceholderString:@"Message to sign"];
  [_sign_message_field setAutoresizingMask:NSViewWidthSizable | NSViewMinYMargin];
  [send_view addSubview:_sign_message_field];

  _sign_button = [[NSButton alloc] initWithFrame:NSMakeRect(frame.size.width - 124.0, frame.size.height - 112.0, 124.0, 24.0)];
  [_sign_button setTitle:@"Sign Message"];
  [_sign_button setButtonType:NSButtonTypeMomentaryPushIn];
  [_sign_button setBezelStyle:NSBezelStyleRounded];
  [_sign_button setTarget:self];
  [_sign_button setAction:@selector(onSignMessage:)];
  [_sign_button setAutoresizingMask:NSViewMinXMargin | NSViewMinYMargin];
  [send_view addSubview:_sign_button];

  _signature_text = [self makeReadOnlyTextViewInFrame:NSMakeRect(0.0, 0.0, frame.size.width, frame.size.height - 118.0)
                                                parent:send_view];
  [send_item setView:send_view];
  [_tab_view addTabViewItem:send_item];

  NSTabViewItem* receive_item = [[NSTabViewItem alloc] initWithIdentifier:@"receive"];
  [receive_item setLabel:@"Receive"];
  NSView* receive_view = [[NSView alloc] initWithFrame:[_tab_view bounds]];
  [receive_view setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
  _receive_text = [self makeReadOnlyTextViewInFrame:[receive_view bounds] parent:receive_view];
  [receive_item setView:receive_view];
  [_tab_view addTabViewItem:receive_item];

  NSTabViewItem* transactions_item = [[NSTabViewItem alloc] initWithIdentifier:@"transactions"];
  [transactions_item setLabel:@"Transactions"];
  NSView* transactions_view = [[NSView alloc] initWithFrame:[_tab_view bounds]];
  [transactions_view setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
  _transactions_text = [self makeReadOnlyTextViewInFrame:[transactions_view bounds] parent:transactions_view];
  [transactions_item setView:transactions_view];
  [_tab_view addTabViewItem:transactions_item];

  NSTabViewItem* hashspec_item = [[NSTabViewItem alloc] initWithIdentifier:@"hashspec"];
  [hashspec_item setLabel:@"HashSpec"];
  NSView* hashspec_view = [[NSView alloc] initWithFrame:[_tab_view bounds]];
  [hashspec_view setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
  _hashspec_text = [self makeReadOnlyTextViewInFrame:[hashspec_view bounds] parent:hashspec_view];
  [hashspec_item setView:hashspec_view];
  [_tab_view addTabViewItem:hashspec_item];

  NSTabViewItem* node_item = [[NSTabViewItem alloc] initWithIdentifier:@"node"];
  [node_item setLabel:@"Status"];
  NSView* node_view = [[NSView alloc] initWithFrame:[_tab_view bounds]];
  [node_view setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];

  _node_tor_toggle = [[NSButton alloc] initWithFrame:NSMakeRect(0.0, frame.size.height - 28.0, 80.0, 22.0)];
  [_node_tor_toggle setButtonType:NSButtonTypeSwitch];
  [_node_tor_toggle setTitle:@"Tor"];
  [_node_tor_toggle setAutoresizingMask:NSViewMaxXMargin | NSViewMinYMargin];
  [node_view addSubview:_node_tor_toggle];

  _node_i2p_toggle = [[NSButton alloc] initWithFrame:NSMakeRect(84.0, frame.size.height - 28.0, 80.0, 22.0)];
  [_node_i2p_toggle setButtonType:NSButtonTypeSwitch];
  [_node_i2p_toggle setTitle:@"I2P"];
  [_node_i2p_toggle setAutoresizingMask:NSViewMaxXMargin | NSViewMinYMargin];
  [node_view addSubview:_node_i2p_toggle];

  _node_localhost_toggle = [[NSButton alloc] initWithFrame:NSMakeRect(170.0, frame.size.height - 28.0, 190.0, 22.0)];
  [_node_localhost_toggle setButtonType:NSButtonTypeSwitch];
  [_node_localhost_toggle setTitle:@"Localhost 127.0.0.1"];
  [_node_localhost_toggle setAutoresizingMask:NSViewMaxXMargin | NSViewMinYMargin];
  [node_view addSubview:_node_localhost_toggle];

  _node_mode_menu = [[NSPopUpButton alloc] initWithFrame:NSMakeRect(370.0, frame.size.height - 30.0, 90.0, 26.0) pullsDown:NO];
  [_node_mode_menu addItemWithTitle:@"Tor"];
  [_node_mode_menu addItemWithTitle:@"I2P"];
  [_node_mode_menu setAutoresizingMask:NSViewMaxXMargin | NSViewMinYMargin];
  [node_view addSubview:_node_mode_menu];

  _node_apply_button = [[NSButton alloc] initWithFrame:NSMakeRect(466.0, frame.size.height - 30.0, 84.0, 26.0)];
  [_node_apply_button setTitle:@"Apply"];
  [_node_apply_button setButtonType:NSButtonTypeMomentaryPushIn];
  [_node_apply_button setBezelStyle:NSBezelStyleRounded];
  [_node_apply_button setTarget:self];
  [_node_apply_button setAction:@selector(onNodeApply:)];
  [_node_apply_button setAutoresizingMask:NSViewMaxXMargin | NSViewMinYMargin];
  [node_view addSubview:_node_apply_button];

  _node_refresh_button = [[NSButton alloc] initWithFrame:NSMakeRect(556.0, frame.size.height - 30.0, 84.0, 26.0)];
  [_node_refresh_button setTitle:@"Refresh"];
  [_node_refresh_button setButtonType:NSButtonTypeMomentaryPushIn];
  [_node_refresh_button setBezelStyle:NSBezelStyleRounded];
  [_node_refresh_button setTarget:self];
  [_node_refresh_button setAction:@selector(onNodeRefresh:)];
  [_node_refresh_button setAutoresizingMask:NSViewMaxXMargin | NSViewMinYMargin];
  [node_view addSubview:_node_refresh_button];

  _node_peer_field = [[NSTextField alloc] initWithFrame:NSMakeRect(0.0, frame.size.height - 58.0, frame.size.width - 102.0, 24.0)];
  [_node_peer_field setPlaceholderString:@"peer.host:port"];
  [_node_peer_field setAutoresizingMask:NSViewWidthSizable | NSViewMinYMargin];
  [node_view addSubview:_node_peer_field];

  _node_add_peer_button = [[NSButton alloc] initWithFrame:NSMakeRect(frame.size.width - 96.0, frame.size.height - 58.0, 96.0, 24.0)];
  [_node_add_peer_button setTitle:@"Add Peer"];
  [_node_add_peer_button setButtonType:NSButtonTypeMomentaryPushIn];
  [_node_add_peer_button setBezelStyle:NSBezelStyleRounded];
  [_node_add_peer_button setTarget:self];
  [_node_add_peer_button setAction:@selector(onNodeAddPeer:)];
  [_node_add_peer_button setAutoresizingMask:NSViewMinXMargin | NSViewMinYMargin];
  [node_view addSubview:_node_add_peer_button];

  _node_community_id_field = [[NSTextField alloc] initWithFrame:NSMakeRect(0.0, frame.size.height - 86.0, 180.0, 24.0)];
  [_node_community_id_field setPlaceholderString:@"community-id"];
  [_node_community_id_field setStringValue:@"recipes"];
  [_node_community_id_field setAutoresizingMask:NSViewMaxXMargin | NSViewMinYMargin];
  [node_view addSubview:_node_community_id_field];

  _node_community_name_field = [[NSTextField alloc] initWithFrame:NSMakeRect(186.0, frame.size.height - 86.0, frame.size.width - 286.0, 24.0)];
  [_node_community_name_field setPlaceholderString:@"Community name"];
  [_node_community_name_field setStringValue:@"Recipe Community"];
  [_node_community_name_field setAutoresizingMask:NSViewWidthSizable | NSViewMinYMargin];
  [node_view addSubview:_node_community_name_field];

  _node_community_apply_button = [[NSButton alloc] initWithFrame:NSMakeRect(frame.size.width - 96.0, frame.size.height - 86.0, 96.0, 24.0)];
  [_node_community_apply_button setTitle:@"Use Comm"];
  [_node_community_apply_button setButtonType:NSButtonTypeMomentaryPushIn];
  [_node_community_apply_button setBezelStyle:NSBezelStyleRounded];
  [_node_community_apply_button setTarget:self];
  [_node_community_apply_button setAction:@selector(onNodeUseCommunity:)];
  [_node_community_apply_button setAutoresizingMask:NSViewMinXMargin | NSViewMinYMargin];
  [node_view addSubview:_node_community_apply_button];

  _node_status_text = [self makeReadOnlyTextViewInFrame:NSMakeRect(0.0, 0.0, frame.size.width, frame.size.height - 92.0)
                                                 parent:node_view];

  [node_item setView:node_view];
  [_tab_view addTabViewItem:node_item];

  NSTabViewItem* settings_item = [[NSTabViewItem alloc] initWithIdentifier:@"settings"];
  [settings_item setLabel:@"Settings"];
  NSView* settings_view = [[NSView alloc] initWithFrame:[_tab_view bounds]];
  [settings_view setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];

  _settings_lock_wallet_button = [[NSButton alloc] initWithFrame:NSMakeRect(0.0, frame.size.height - 28.0, 120.0, 24.0)];
  [_settings_lock_wallet_button setTitle:@"Lock Wallet"];
  [_settings_lock_wallet_button setButtonType:NSButtonTypeMomentaryPushIn];
  [_settings_lock_wallet_button setBezelStyle:NSBezelStyleRounded];
  [_settings_lock_wallet_button setTarget:self];
  [_settings_lock_wallet_button setAction:@selector(onSettingsLockWallet:)];
  [_settings_lock_wallet_button setAutoresizingMask:NSViewMaxXMargin | NSViewMinYMargin];
  [settings_view addSubview:_settings_lock_wallet_button];

  _settings_unlock_password_field = [[NSSecureTextField alloc] initWithFrame:NSMakeRect(126.0, frame.size.height - 28.0, 180.0, 24.0)];
  [_settings_unlock_password_field setPlaceholderString:@"Unlock passphrase"];
  [_settings_unlock_password_field setAutoresizingMask:NSViewMaxXMargin | NSViewMinYMargin];
  [settings_view addSubview:_settings_unlock_password_field];

  _settings_unlock_wallet_button = [[NSButton alloc] initWithFrame:NSMakeRect(312.0, frame.size.height - 28.0, 120.0, 24.0)];
  [_settings_unlock_wallet_button setTitle:@"Unlock Wallet"];
  [_settings_unlock_wallet_button setButtonType:NSButtonTypeMomentaryPushIn];
  [_settings_unlock_wallet_button setBezelStyle:NSBezelStyleRounded];
  [_settings_unlock_wallet_button setTarget:self];
  [_settings_unlock_wallet_button setAction:@selector(onSettingsUnlockWallet:)];
  [_settings_unlock_wallet_button setAutoresizingMask:NSViewMaxXMargin | NSViewMinYMargin];
  [settings_view addSubview:_settings_unlock_wallet_button];

  _settings_validate_button = [[NSButton alloc] initWithFrame:NSMakeRect(438.0, frame.size.height - 28.0, 110.0, 24.0)];
  [_settings_validate_button setTitle:@"Validate Now"];
  [_settings_validate_button setButtonType:NSButtonTypeMomentaryPushIn];
  [_settings_validate_button setBezelStyle:NSBezelStyleRounded];
  [_settings_validate_button setTarget:self];
  [_settings_validate_button setAction:@selector(onSettingsValidateNow:)];
  [_settings_validate_button setAutoresizingMask:NSViewMaxXMargin | NSViewMinYMargin];
  [settings_view addSubview:_settings_validate_button];

  _settings_dev_mode_button = [[NSButton alloc] initWithFrame:NSMakeRect(554.0, frame.size.height - 28.0, 240.0, 24.0)];
  [_settings_dev_mode_button setTitle:@"Enable Developer Mode (Reboot)"];
  [_settings_dev_mode_button setButtonType:NSButtonTypeMomentaryPushIn];
  [_settings_dev_mode_button setBezelStyle:NSBezelStyleRounded];
  [_settings_dev_mode_button setTarget:self];
  [_settings_dev_mode_button setAction:@selector(onSettingsToggleDeveloperMode:)];
  [_settings_dev_mode_button setAutoresizingMask:NSViewMaxXMargin | NSViewMinYMargin];
  [settings_view addSubview:_settings_dev_mode_button];

  _settings_recover_path_field = [[NSTextField alloc] initWithFrame:NSMakeRect(0.0, frame.size.height - 56.0, frame.size.width - 420.0, 24.0)];
  [_settings_recover_path_field setStringValue:@"backup/identity-backup.dat"];
  [_settings_recover_path_field setAutoresizingMask:NSViewWidthSizable | NSViewMinYMargin];
  [settings_view addSubview:_settings_recover_path_field];

  _settings_recover_backup_password_field = [[NSSecureTextField alloc] initWithFrame:NSMakeRect(frame.size.width - 412.0, frame.size.height - 56.0, 120.0, 24.0)];
  [_settings_recover_backup_password_field setPlaceholderString:@"Backup pass"];
  [_settings_recover_backup_password_field setAutoresizingMask:NSViewMinXMargin | NSViewMinYMargin];
  [settings_view addSubview:_settings_recover_backup_password_field];

  _settings_recover_local_password_field = [[NSSecureTextField alloc] initWithFrame:NSMakeRect(frame.size.width - 286.0, frame.size.height - 56.0, 120.0, 24.0)];
  [_settings_recover_local_password_field setPlaceholderString:@"New local pass"];
  [_settings_recover_local_password_field setAutoresizingMask:NSViewMinXMargin | NSViewMinYMargin];
  [settings_view addSubview:_settings_recover_local_password_field];

  _settings_recover_wallet_button = [[NSButton alloc] initWithFrame:NSMakeRect(frame.size.width - 160.0, frame.size.height - 56.0, 160.0, 24.0)];
  [_settings_recover_wallet_button setTitle:@"Recover Wallet"];
  [_settings_recover_wallet_button setButtonType:NSButtonTypeMomentaryPushIn];
  [_settings_recover_wallet_button setBezelStyle:NSBezelStyleRounded];
  [_settings_recover_wallet_button setTarget:self];
  [_settings_recover_wallet_button setAction:@selector(onSettingsRecoverWallet:)];
  [_settings_recover_wallet_button setAutoresizingMask:NSViewMinXMargin | NSViewMinYMargin];
  [settings_view addSubview:_settings_recover_wallet_button];

  _settings_text = [self makeReadOnlyTextViewInFrame:NSMakeRect(0.0, 0.0, frame.size.width, frame.size.height - 62.0)
                                              parent:settings_view];
  [settings_item setView:settings_view];
  [_tab_view addTabViewItem:settings_item];

  NSTabViewItem* about_item = [[NSTabViewItem alloc] initWithIdentifier:@"about"];
  [about_item setLabel:@"About"];
  NSView* about_view = [[NSView alloc] initWithFrame:[_tab_view bounds]];
  [about_view setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
  [about_view setWantsLayer:YES];
  [[about_view layer] setBackgroundColor:[[NSColor clearColor] CGColor]];
  _about_image_view = [[NSImageView alloc] initWithFrame:NSMakeRect(0.0, frame.size.height - 210.0, 210.0, 210.0)];
  [_about_image_view setImageScaling:NSImageScaleProportionallyUpOrDown];
  [_about_image_view setAlphaValue:0.92];
  [_about_image_view setAutoresizingMask:NSViewMaxXMargin | NSViewMinYMargin];
  [about_view addSubview:_about_image_view];
  _about_leaf_glyph = [[NSTextField alloc] initWithFrame:NSMakeRect(172.0, frame.size.height - 52.0, 38.0, 38.0)];
  [_about_leaf_glyph setEditable:NO];
  [_about_leaf_glyph setBezeled:NO];
  [_about_leaf_glyph setDrawsBackground:NO];
  [_about_leaf_glyph setSelectable:NO];
  [_about_leaf_glyph setAlignment:NSTextAlignmentCenter];
  [_about_leaf_glyph setFont:[NSFont systemFontOfSize:30.0 weight:NSFontWeightBold]];
  [_about_leaf_glyph setTextColor:[NSColor systemGreenColor]];
  [_about_leaf_glyph setStringValue:@"❧"];
  [_about_leaf_glyph setAutoresizingMask:NSViewMaxXMargin | NSViewMinYMargin];
  [about_view addSubview:_about_leaf_glyph];
  _about_text = [self makeReadOnlyTextViewInFrame:NSMakeRect(216.0, 0.0, std::max<CGFloat>(120.0, frame.size.width - 216.0), frame.size.height)
                                           parent:about_view];
  [about_item setView:about_view];
  [_tab_view addTabViewItem:about_item];
}

- (void)buildWindow {
  _window = [[NSWindow alloc] initWithContentRect:NSMakeRect(0.0, 0.0, 1280.0, 780.0)
                                         styleMask:(NSWindowStyleMaskTitled |
                                                    NSWindowStyleMaskClosable |
                                                    NSWindowStyleMaskMiniaturizable |
                                                    NSWindowStyleMaskResizable)
                                           backing:NSBackingStoreBuffered
                                             defer:NO];

  [_window setTitle:@"SoupNet::P2P Tomato Soup - Recipe Forum"];
  [_window center];

  NSView* content = [_window contentView];
  NSRect bounds = [content bounds];

  constexpr CGFloat margin = 10.0;
  constexpr CGFloat top_h = 28.0;
  constexpr CGFloat close_w = 90.0;
  constexpr CGFloat combo_h = 28.0;
  constexpr CGFloat section_gap = 8.0;
  constexpr CGFloat left_w = 260.0;

  const CGFloat top_y = NSMaxY(bounds) - margin - top_h;
  const CGFloat search_w = NSWidth(bounds) - (margin * 3.0) - close_w;

  _search_field = [[NSSearchField alloc] initWithFrame:NSMakeRect(margin, top_y, search_w, top_h)];
  [_search_field setPlaceholderString:@"Search references and recipes..."];
  [_search_field setDelegate:self];
  [_search_field setAutoresizingMask:NSViewWidthSizable | NSViewMinYMargin];
  [content addSubview:_search_field];

  _close_button = [[NSButton alloc] initWithFrame:NSMakeRect(margin * 2.0 + search_w, top_y, close_w, top_h)];
  [_close_button setTitle:@"Close"];
  [_close_button setButtonType:NSButtonTypeMomentaryPushIn];
  [_close_button setBezelStyle:NSBezelStyleRounded];
  [_close_button setTarget:self];
  [_close_button setAction:@selector(onClose:)];
  [_close_button setAutoresizingMask:NSViewMinXMargin | NSViewMinYMargin];
  [content addSubview:_close_button];

  const CGFloat body_h = _developer_mode ? (NSHeight(bounds) - (margin * 3.0) - top_h)
                                         : (NSHeight(bounds) - (margin * 2.0));
  const CGFloat body_top = margin + body_h;

  _parent_menu = [[NSPopUpButton alloc] initWithFrame:NSMakeRect(margin, body_top - combo_h, left_w, combo_h)
                                            pullsDown:NO];
  [_parent_menu setTarget:self];
  [_parent_menu setAction:@selector(onParentMenuChanged:)];
  [_parent_menu setAutoresizingMask:NSViewMinYMargin];
  [content addSubview:_parent_menu];

  _secondary_menu = [[NSPopUpButton alloc] initWithFrame:NSMakeRect(margin, body_top - (combo_h * 2.0) - section_gap,
                                                                    left_w, combo_h)
                                                  pullsDown:NO];
  [_secondary_menu setTarget:self];
  [_secondary_menu setAction:@selector(onSecondaryMenuChanged:)];
  [_secondary_menu setAutoresizingMask:NSViewMinYMargin];
  [content addSubview:_secondary_menu];

  const CGFloat opening_h = std::max<CGFloat>(80.0, body_h - (combo_h * 2.0) - (section_gap * 2.0));
  NSScrollView* opening_scroll =
      [[NSScrollView alloc] initWithFrame:NSMakeRect(margin, margin, left_w, opening_h)];
  [opening_scroll setHasVerticalScroller:YES];
  [opening_scroll setAutoresizingMask:NSViewHeightSizable];

  _lookup_table = [[NSTableView alloc] initWithFrame:[opening_scroll bounds]];
  NSTableColumn* key_col = [[NSTableColumn alloc] initWithIdentifier:@"OpeningColumn"];
  [key_col setWidth:left_w - 24.0];
  [key_col setTitle:@"Opening"];
  [_lookup_table addTableColumn:key_col];
  [_lookup_table setHeaderView:nil];
  [_lookup_table setDelegate:self];
  [_lookup_table setDataSource:self];
  [_lookup_table setUsesAlternatingRowBackgroundColors:YES];
  [opening_scroll setDocumentView:_lookup_table];
  [content addSubview:opening_scroll];

  const CGFloat tab_x = _developer_mode ? ((margin * 2.0) + left_w) : margin;
  const CGFloat tab_w = NSWidth(bounds) - tab_x - margin;
  const CGFloat tab_h = body_h;
  [self buildTabsInFrame:NSMakeRect(tab_x, margin, tab_w, tab_h) parent:content];
}

- (void)showSplashScreen {
  const auto node = _api->node_status();

  NSWindow* splash = [[NSWindow alloc] initWithContentRect:NSMakeRect(0.0, 0.0, 520.0, 360.0)
                                                  styleMask:NSWindowStyleMaskBorderless
                                                    backing:NSBackingStoreBuffered
                                                      defer:NO];
  [splash setOpaque:NO];
  [splash setBackgroundColor:[NSColor clearColor]];
  [splash setHasShadow:YES];
  [splash center];

  NSView* content = [splash contentView];
  NSVisualEffectView* bg = [[NSVisualEffectView alloc] initWithFrame:[content bounds]];
  [bg setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
  [bg setBlendingMode:NSVisualEffectBlendingModeBehindWindow];
  [bg setMaterial:NSVisualEffectMaterialUnderWindowBackground];
  [bg setState:NSVisualEffectStateActive];
  [content addSubview:bg];

  NSImageView* image_view = [[NSImageView alloc] initWithFrame:NSMakeRect(154.0, 120.0, 212.0, 212.0)];
  [image_view setImageScaling:NSImageScaleProportionallyUpOrDown];
  NSString* splash_path = to_ns_string(node.data_dir + "/assets/tomato_soup.png");
  NSImage* splash_image = [[NSImage alloc] initWithContentsOfFile:splash_path];
  if (splash_image != nil) {
    [image_view setImage:splash_image];
  }
  [content addSubview:image_view];

  NSImageView* leaf_icon_view = [[NSImageView alloc] initWithFrame:NSMakeRect(410.0, 284.0, 56.0, 56.0)];
  [leaf_icon_view setImageScaling:NSImageScaleProportionallyUpOrDown];
  NSString* leaf_path = to_ns_string(node.data_dir + "/assets/leaf_icon.png");
  NSImage* leaf_image = [[NSImage alloc] initWithContentsOfFile:leaf_path];
  if (leaf_image != nil) {
    [leaf_icon_view setImage:leaf_image];
    [content addSubview:leaf_icon_view];
  } else {
    NSTextField* leaf_glyph = [[NSTextField alloc] initWithFrame:NSMakeRect(426.0, 286.0, 32.0, 50.0)];
    [leaf_glyph setEditable:NO];
    [leaf_glyph setBezeled:NO];
    [leaf_glyph setDrawsBackground:NO];
    [leaf_glyph setSelectable:NO];
    [leaf_glyph setAlignment:NSTextAlignmentCenter];
    [leaf_glyph setFont:[NSFont systemFontOfSize:38.0 weight:NSFontWeightBold]];
    [leaf_glyph setTextColor:[NSColor systemGreenColor]];
    [leaf_glyph setStringValue:@"❧"];
    [content addSubview:leaf_glyph];
  }

  NSTextField* title = [[NSTextField alloc] initWithFrame:NSMakeRect(20.0, 72.0, 480.0, 30.0)];
  [title setEditable:NO];
  [title setBezeled:NO];
  [title setDrawsBackground:NO];
  [title setSelectable:NO];
  [title setAlignment:NSTextAlignmentCenter];
  [title setFont:[NSFont boldSystemFontOfSize:20.0]];
  [title setStringValue:@"SoupNet::P2P Tomato Soup"];
  [content addSubview:title];

  NSTextField* meta = [[NSTextField alloc] initWithFrame:NSMakeRect(20.0, 42.0, 480.0, 24.0)];
  [meta setEditable:NO];
  [meta setBezeled:NO];
  [meta setDrawsBackground:NO];
  [meta setSelectable:NO];
  [meta setAlignment:NSTextAlignmentCenter];
  [meta setStringValue:to_ns_string(std::string{alpha::kAppVersion} + " • " +
                                    node.genesis.network_id)];
  [content addSubview:meta];

  [splash makeKeyAndOrderFront:nil];
  [NSApp activateIgnoringOtherApps:YES];
  [[NSRunLoop currentRunLoop] runUntilDate:[NSDate dateWithTimeIntervalSinceNow:1.2]];
  [splash orderOut:nil];
}

- (void)bootstrapDemoData {
  const auto recipes = _api->search({.text = {}, .category = {}});
  if (!recipes.empty()) {
    return;
  }

  _api->create_recipe({
      .category = "Core Topic",
      .title = "Tomato Soup Base",
      .markdown = "# Tomato Soup Base\n\nCore method for all tomato soup variations.",
      .core_topic = true,
      .menu_segment = "core-menu",
  });
  _api->create_recipe({
      .category = "Ingredient",
      .title = "Essential Ingredients",
      .markdown = "- Tomatoes\n- Olive oil\n- Garlic\n- Salt",
      .core_topic = true,
      .menu_segment = "core-ingredients",
  });
  _api->create_recipe({
      .category = "Community",
      .title = "Starter: P2P Tomato Soup",
      .markdown = "# Tomato Soup\n\n- 4 tomatoes\n- Olive oil\n- Salt\n\nSimmer 20 minutes.",
      .core_topic = false,
      .menu_segment = "community-post",
  });
}

- (void)applicationDidFinishLaunching:(NSNotification*)notification {
  (void)notification;

  _developer_mode = false;
  _live_timer = nil;
  _api = std::make_unique<alpha::CoreApi>();
  const std::string app_data_dir = default_app_data_dir();
  const alpha::Result init = _api->init({
      .app_data_dir = app_data_dir,
      .passphrase = "soupnet-dev-passphrase",
      .mode = alpha::AnonymityMode::Tor,
      .seed_peers = {"seed.got-soup.local:4001", "24.188.147.247:4001"},
      .seed_peers_mainnet = {"seed.got-soup.local:4001", "24.188.147.247:4001"},
      .seed_peers_testnet = {"seed.got-soup.local:14001"},
      .alpha_test_mode = false,
      .peers_dat_path = {},
      .community_profile_path = "tomato-soup",
      .production_swap = true,
      .block_interval_seconds = 150,
      .validation_interval_ticks = 10,
      .block_reward_units = 115,
      .minimum_post_value = 0,
      .genesis_psz_timestamp = "",
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
  _ui_mode_file_path = app_data_dir + "/ui_mode.cfg";
  _developer_mode = read_developer_mode_flag(_ui_mode_file_path);

  [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
  [self buildMenu];
  [self buildWindow];
  if (init.ok) {
    [self showSplashScreen];
  }

  if (!init.ok) {
    NSAlert* alert = [[NSAlert alloc] init];
    [alert setMessageText:@"Initialization Error"];
    const std::string details = init.message + "\n\nApp data dir:\n" + app_data_dir;
    [alert setInformativeText:to_ns_string(details)];
    [alert addButtonWithTitle:@"Exit"];
    [alert runModal];
    [NSApp terminate:nil];
    return;
  }

  [self bootstrapDemoData];
  [self rebuildParentMenu];
  [self rebuildOpeningList];
  [self refreshRecipesTab];
  [self refreshProfileAndAbout];
  [self refreshNodeStatusTab];
  [self applyUiModeVisibility];
  _live_timer = [NSTimer scheduledTimerWithTimeInterval:1.5
                                                  target:self
                                                selector:@selector(onLiveTick:)
                                                userInfo:nil
                                                 repeats:YES];

  [_window makeKeyAndOrderFront:nil];
  [NSApp activateIgnoringOtherApps:YES];
}

- (void)applicationWillTerminate:(NSNotification*)notification {
  (void)notification;
  if (_live_timer != nil) {
    [_live_timer invalidate];
    _live_timer = nil;
  }
}

- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication*)sender {
  (void)sender;
  return YES;
}

- (NSInteger)numberOfRowsInTableView:(NSTableView*)tableView {
  (void)tableView;
  return static_cast<NSInteger>(_opening_keys.size());
}

- (NSView*)tableView:(NSTableView*)tableView viewForTableColumn:(NSTableColumn*)tableColumn row:(NSInteger)row {
  (void)tableView;
  (void)tableColumn;

  NSTableCellView* cell = [tableView makeViewWithIdentifier:@"OpeningCell" owner:self];
  if (cell == nil) {
    cell = [[NSTableCellView alloc] initWithFrame:NSMakeRect(0.0, 0.0, 200.0, 20.0)];
    [cell setIdentifier:@"OpeningCell"];

    NSTextField* text_field = [[NSTextField alloc] initWithFrame:[cell bounds]];
    [text_field setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
    [text_field setBezeled:NO];
    [text_field setDrawsBackground:NO];
    [text_field setEditable:NO];
    [text_field setSelectable:NO];
    [cell addSubview:text_field];
    [cell setTextField:text_field];
  }

  if (row >= 0 && static_cast<size_t>(row) < _opening_keys.size()) {
    const auto entry = _api->reference_lookup(_opening_keys[static_cast<size_t>(row)]);
    if (entry.has_value()) {
      [[cell textField] setStringValue:to_ns_string(entry->title)];
    } else {
      [[cell textField] setStringValue:to_ns_string(_opening_keys[static_cast<size_t>(row)])];
    }
  } else {
    [[cell textField] setStringValue:@""];
  }

  return cell;
}

- (void)tableViewSelectionDidChange:(NSNotification*)notification {
  (void)notification;
  [self refreshRecipesTab];
}

- (void)controlTextDidChange:(NSNotification*)notification {
  (void)notification;
  [self rebuildOpeningList];
}

- (void)onParentMenuChanged:(id)sender {
  (void)sender;
  [self rebuildSecondaryMenu];
  [self rebuildOpeningList];
}

- (void)onSecondaryMenuChanged:(id)sender {
  (void)sender;
  [self rebuildOpeningList];
}

- (void)onUploadRecipe:(id)sender {
  (void)sender;

  const std::string title = from_ns_string([_upload_title stringValue]);
  const std::string category = from_ns_string([_upload_category stringValue]);
  const std::string body = from_ns_string([_upload_body string]);

  const alpha::Result result = _api->create_recipe({
      .category = category,
      .title = title,
      .markdown = body,
      .core_topic = false,
      .menu_segment = "community-post",
  });

  if (result.ok) {
    NSAlert* alert = [[NSAlert alloc] init];
    [alert setMessageText:@"Recipe Uploaded"];
    [alert setInformativeText:@"Recipe appended to local event log and sync queue."];
    [alert addButtonWithTitle:@"OK"];
    [alert runModal];

    [_tab_view selectTabViewItemAtIndex:0];
    [self refreshRecipesTab];
    [self refreshForumTab];
    [self refreshNodeStatusTab];
    return;
  }

  show_error_alert(_window, "Upload Failed", result.message);
}

- (void)onRecipesThumbUp:(id)sender {
  (void)sender;

  if (_recipes.empty()) {
    show_error_alert(_window, "Thumbs Up", "No recipe is currently selected by search/filter.");
    return;
  }

  const alpha::Result result = _api->add_thumb_up(_recipes.front().recipe_id);
  if (!result.ok) {
    show_error_alert(_window, "Thumbs Up", result.message);
    return;
  }

  [self refreshRecipesTab];
  [self refreshForumTab];
  [self refreshNodeStatusTab];
  [self rebuildOpeningList];
}

- (void)onRecipesRate:(id)sender {
  (void)sender;

  if (_recipes.empty()) {
    show_error_alert(_window, "Rate Recipe", "No recipe is currently selected by search/filter.");
    return;
  }

  const std::string rating_text = from_ns_string([_recipes_rate_menu titleOfSelectedItem]);
  int rating = 0;
  if (!rating_text.empty() && rating_text.size() == 1 && rating_text[0] >= '1' && rating_text[0] <= '5') {
    rating = rating_text[0] - '0';
  }
  if (rating < 1 || rating > 5) {
    show_error_alert(_window, "Rate Recipe", "Choose a rating between 1 and 5.");
    return;
  }

  const alpha::Result result = _api->add_review({
      .recipe_id = _recipes.front().recipe_id,
      .rating = rating,
      .markdown = "Rated via UI",
  });
  if (!result.ok) {
    show_error_alert(_window, "Rate Recipe", result.message);
    return;
  }

  [self refreshRecipesTab];
  [self refreshForumTab];
  [self refreshNodeStatusTab];
  [self rebuildOpeningList];
}

- (void)onCreateForumThread:(id)sender {
  (void)sender;

  if (_recipes.empty()) {
    show_error_alert(_window, "Create Thread", "No recipe selected. Create or search a recipe first.");
    return;
  }

  const auto& recipe = _recipes.front();
  const std::string title = from_ns_string([_forum_thread_title stringValue]);
  const std::string body = from_ns_string([_forum_thread_body string]);

  const alpha::Result result = _api->create_thread({
      .recipe_id = recipe.recipe_id,
      .title = title,
      .markdown = body,
  });
  if (!result.ok) {
    show_error_alert(_window, "Create Thread", result.message);
    return;
  }

  [_forum_thread_title setStringValue:@""];
  [_forum_thread_body setString:@""];
  [self refreshForumTab];
  [self refreshNodeStatusTab];
  [self rebuildOpeningList];
}

- (void)onCreateForumReply:(id)sender {
  (void)sender;

  if (_recipes.empty()) {
    show_error_alert(_window, "Create Reply", "No recipe selected. Create or search a recipe first.");
    return;
  }

  const auto& recipe = _recipes.front();
  const auto threads = _api->threads(recipe.recipe_id);
  if (threads.empty()) {
    show_error_alert(_window, "Create Reply", "No thread exists yet for this recipe.");
    return;
  }

  const std::string body = from_ns_string([_forum_reply_body string]);
  const alpha::Result result = _api->create_reply({
      .thread_id = threads.front().thread_id,
      .markdown = body,
  });
  if (!result.ok) {
    show_error_alert(_window, "Create Reply", result.message);
    return;
  }

  [_forum_reply_body setString:@""];
  [self refreshForumTab];
  [self refreshNodeStatusTab];
  [self rebuildOpeningList];
}

- (void)onSendToAddress:(id)sender {
  (void)sender;

  const std::string address = from_ns_string([_send_address_field stringValue]);
  const std::string amount_text = from_ns_string([_send_amount_field stringValue]);
  const std::string memo = from_ns_string([_send_memo_field stringValue]);
  if (address.empty() || amount_text.empty()) {
    show_error_alert(_window, "Send", "Address and amount are required.");
    return;
  }

  std::int64_t amount = 0;
  const auto parsed = std::from_chars(amount_text.data(), amount_text.data() + amount_text.size(), amount);
  if (parsed.ec != std::errc() || amount <= 0) {
    show_error_alert(_window, "Send", "Amount must be a positive integer.");
    return;
  }

  const alpha::Result result = _api->transfer_rewards_to_address({
      .to_address = address,
      .amount = amount,
      .memo = memo,
  });
  if (!result.ok) {
    show_error_alert(_window, "Send", result.message);
    return;
  }

  [_send_amount_field setStringValue:@""];
  [_send_memo_field setStringValue:@""];
  [self refreshNodeStatusTab];
  [self refreshProfileAndAbout];
  [self refreshSendReceiveTransactionsTabs];
}

- (void)onSignMessage:(id)sender {
  (void)sender;

  const std::string message = from_ns_string([_sign_message_field stringValue]);
  if (message.empty()) {
    show_error_alert(_window, "Sign Message", "Message is required.");
    return;
  }

  const auto signed_message = _api->sign_message(message);
  std::string text;
  text += "Message:\n" + signed_message.message + "\n\n";
  text += "Signature:\n" + signed_message.signature + "\n\n";
  text += "Address: " + signed_message.address + "\n";
  text += "CID: " + signed_message.cid + "\n";
  text += "PubKey:\n" + signed_message.public_key + "\n";
  text += "Wallet Locked: ";
  text += signed_message.wallet_locked ? "YES" : "NO";
  text += "\n";
  [_signature_text setString:to_ns_string(text)];
}

- (void)onNodeApply:(id)sender {
  (void)sender;

  const bool tor_enabled = ([_node_tor_toggle state] == NSControlStateValueOn);
  const bool i2p_enabled = ([_node_i2p_toggle state] == NSControlStateValueOn);
  const bool localhost_mode = ([_node_localhost_toggle state] == NSControlStateValueOn);

  alpha::Result result = _api->set_transport_enabled(alpha::AnonymityMode::Tor, tor_enabled);
  if (!result.ok) {
    show_error_alert(_window, "Node Controls", result.message);
    return;
  }

  result = _api->set_transport_enabled(alpha::AnonymityMode::I2P, i2p_enabled);
  if (!result.ok) {
    show_error_alert(_window, "Node Controls", result.message);
    return;
  }

  const std::string mode = from_ns_string([_node_mode_menu titleOfSelectedItem]);
  result = _api->set_active_transport(mode == "I2P" ? alpha::AnonymityMode::I2P : alpha::AnonymityMode::Tor);
  if (!result.ok) {
    show_error_alert(_window, "Node Controls", result.message);
    return;
  }

  result = _api->set_alpha_test_mode(localhost_mode);
  if (!result.ok) {
    show_error_alert(_window, "Node Controls", result.message);
    return;
  }

  [self refreshNodeStatusTab];
  [self refreshProfileAndAbout];
}

- (void)onNodeRefresh:(id)sender {
  (void)sender;

  const alpha::Result result = _api->reload_peers_dat();
  if (!result.ok) {
    show_error_alert(_window, "Node Status Refresh", result.message);
  }

  [self refreshNodeStatusTab];
  [self refreshProfileAndAbout];
}

- (void)onNodeAddPeer:(id)sender {
  (void)sender;

  const std::string peer = from_ns_string([_node_peer_field stringValue]);
  const alpha::Result result = _api->add_peer(peer);
  if (!result.ok) {
    show_error_alert(_window, "Add Peer", result.message);
    return;
  }

  [_node_peer_field setStringValue:@""];
  [self refreshNodeStatusTab];
}

- (void)onNodeUseCommunity:(id)sender {
  (void)sender;

  const std::string community_id = from_ns_string([_node_community_id_field stringValue]);
  const std::string community_name = from_ns_string([_node_community_name_field stringValue]);

  const alpha::Result result = _api->use_community_profile(community_id, community_name, "");
  if (!result.ok) {
    show_error_alert(_window, "Community Profile", result.message);
    return;
  }

  [self refreshRecipesTab];
  [self refreshForumTab];
  [self refreshNodeStatusTab];
  [self refreshProfileAndAbout];
}

- (void)onProfileSetName:(id)sender {
  (void)sender;

  const std::string display_name = from_ns_string([_profile_name_field stringValue]);
  const std::string cipher_password = from_ns_string([_profile_cipher_password_field stringValue]);
  const std::string cipher_salt = from_ns_string([_profile_cipher_salt_field stringValue]);
  const alpha::Result result =
      _api->set_immortal_name_with_cipher(display_name, cipher_password, cipher_salt);
  if (!result.ok) {
    show_error_alert(_window, "Set Immortal", result.message);
    return;
  }

  [_profile_cipher_password_field setStringValue:@""];
  [self refreshProfileAndAbout];
  [self refreshNodeStatusTab];
}

- (void)onProfileApplyPolicy:(id)sender {
  (void)sender;

  const bool reject_duplicates = ([_profile_duplicate_policy_toggle state] == NSControlStateValueOn);
  const alpha::Result result = _api->set_duplicate_name_policy(reject_duplicates);
  if (!result.ok) {
    show_error_alert(_window, "Duplicate Name Policy", result.message);
    return;
  }

  [self refreshProfileAndAbout];
}

- (void)onProfileApplyCipher:(id)sender {
  (void)sender;

  const std::string password = from_ns_string([_profile_cipher_password_field stringValue]);
  const std::string salt = from_ns_string([_profile_cipher_salt_field stringValue]);
  const alpha::Result result = _api->set_profile_cipher_password(password, salt);
  if (!result.ok) {
    show_error_alert(_window, "Cipher Key", result.message);
    return;
  }

  [_profile_cipher_password_field setStringValue:@""];
  [self refreshNodeStatusTab];
  [self refreshProfileAndAbout];
}

- (void)onProfileUpdateKey:(id)sender {
  (void)sender;

  const alpha::Result result = _api->update_key_to_peers();
  if (!result.ok) {
    show_error_alert(_window, "Update Key", result.message);
    return;
  }

  [self refreshNodeStatusTab];
  [self refreshProfileAndAbout];
}

- (void)onProfileExportKey:(id)sender {
  (void)sender;

  const std::string path = from_ns_string([_profile_export_path_field stringValue]);
  const std::string password = from_ns_string([_profile_export_password_field stringValue]);
  const std::string salt = from_ns_string([_profile_export_salt_field stringValue]);

  const alpha::Result result = _api->export_key_backup(path, password, salt);
  if (!result.ok) {
    show_error_alert(_window, "Export Key", result.message);
    return;
  }

  NSAlert* alert = [[NSAlert alloc] init];
  [alert setMessageText:@"Key Exported"];
  [alert setInformativeText:to_ns_string("Backup path: " + result.data)];
  [alert addButtonWithTitle:@"OK"];
  [alert beginSheetModalForWindow:_window completionHandler:nil];
}

- (void)onProfileImportKey:(id)sender {
  (void)sender;

  const std::string path = from_ns_string([_profile_import_path_field stringValue]);
  const std::string password = from_ns_string([_profile_import_password_field stringValue]);
  const alpha::Result result = _api->import_key_backup(path, password);
  if (!result.ok) {
    show_error_alert(_window, "Import Key", result.message);
    return;
  }

  [self refreshRecipesTab];
  [self refreshForumTab];
  [self refreshNodeStatusTab];
  [self refreshProfileAndAbout];
}

- (void)onProfileNukeKey:(id)sender {
  (void)sender;

  NSAlert* confirm = [[NSAlert alloc] init];
  [confirm setAlertStyle:NSAlertStyleWarning];
  [confirm setMessageText:@"Nuke key?"];
  [confirm setInformativeText:@"This permanently destroys the local identity key. Existing posts remain."];
  [confirm addButtonWithTitle:@"Nuke Key"];
  [confirm addButtonWithTitle:@"Cancel"];
  if ([confirm runModal] != NSAlertFirstButtonReturn) {
    return;
  }

  const alpha::Result result = _api->nuke_key("NUKE-KEY");
  if (!result.ok) {
    show_error_alert(_window, "Nuke Key", result.message);
    return;
  }

  [self refreshRecipesTab];
  [self refreshForumTab];
  [self refreshNodeStatusTab];
  [self refreshProfileAndAbout];
}

- (void)onSettingsLockWallet:(id)sender {
  (void)sender;
  const alpha::Result result = _api->lock_wallet();
  if (!result.ok) {
    show_error_alert(_window, "Lock Wallet", result.message);
    return;
  }
  [self refreshNodeStatusTab];
  [self refreshProfileAndAbout];
}

- (void)onSettingsUnlockWallet:(id)sender {
  (void)sender;
  const std::string pass = from_ns_string([_settings_unlock_password_field stringValue]);
  const alpha::Result result = _api->unlock_wallet(pass);
  if (!result.ok) {
    show_error_alert(_window, "Unlock Wallet", result.message);
    return;
  }
  [_settings_unlock_password_field setStringValue:@""];
  [self refreshNodeStatusTab];
  [self refreshProfileAndAbout];
}

- (void)onSettingsRecoverWallet:(id)sender {
  (void)sender;
  const std::string path = from_ns_string([_settings_recover_path_field stringValue]);
  const std::string backup_pass = from_ns_string([_settings_recover_backup_password_field stringValue]);
  const std::string local_pass = from_ns_string([_settings_recover_local_password_field stringValue]);
  const alpha::Result result = _api->recover_wallet(path, backup_pass, local_pass);
  if (!result.ok) {
    show_error_alert(_window, "Recover Wallet", result.message);
    return;
  }
  [_settings_recover_backup_password_field setStringValue:@""];
  [_settings_recover_local_password_field setStringValue:@""];
  [self refreshRecipesTab];
  [self refreshForumTab];
  [self refreshNodeStatusTab];
  [self refreshProfileAndAbout];
}

- (void)onSettingsValidateNow:(id)sender {
  (void)sender;
  const alpha::Result result = _api->run_backtest_validation();
  if (!result.ok) {
    show_error_alert(_window, "Validate State", result.message);
    return;
  }
  [self refreshNodeStatusTab];
}

- (void)onSettingsToggleDeveloperMode:(id)sender {
  (void)sender;
  const bool target_mode = !_developer_mode;
  if (!write_developer_mode_flag(_ui_mode_file_path, target_mode)) {
    show_error_alert(_window, "UI Mode", "Failed to persist UI mode file.");
    return;
  }
  std::string msg = target_mode ? "Developer Mode enabled. Reboot app to apply."
                                : "User Friendly Mode enabled. Reboot app to apply.";
  NSAlert* alert = [[NSAlert alloc] init];
  [alert setMessageText:@"UI Mode"];
  [alert setInformativeText:to_ns_string(msg)];
  [alert addButtonWithTitle:@"OK"];
  [alert beginSheetModalForWindow:_window completionHandler:nil];
}

- (void)onClose:(id)sender {
  (void)sender;
  [NSApp terminate:nil];
}

- (void)showAboutCredits:(id)sender {
  (void)sender;

  const auto node = _api->node_status();
  std::string about;
  about += std::string{alpha::kAppDisplayName} + "\n\n";
  about += "Current Tor Version: " + node.tor.version + "\n";
  about += "Current I2P Version: " + node.i2p.version + "\n";
  about += "Current P2P:Soup Version Build Release: " + std::string{alpha::kAppVersion} + " (" +
           std::string{alpha::kBuildRelease} + ")\n";
  about += "Authors: " + std::string{alpha::kAuthorList} + "\n\n";
  about += "Chain: " + node.genesis.chain_id + " (" + node.genesis.network_id + ")\n";
  about += "Genesis Merkle: " + node.genesis.merkle_root + "\n";
  about += "Reference: https://github.com/hendrayoga/smallchange\n";
  about += "About PNG: " + node.data_dir + "/assets/about.png\n";
  about += "Splash PNG: " + node.data_dir + "/assets/tomato_soup.png\n\n";
  about += "Core Phase 1:\n" + node.core_phase_status + "\n";

  NSAlert* alert = [[NSAlert alloc] init];
  [alert setMessageText:@"About SoupNet"];
  [alert setInformativeText:to_ns_string(about)];
  [alert addButtonWithTitle:@"OK"];
  [alert runModal];
}

@end

int main(int argc, const char* argv[]) {
  (void)argc;
  (void)argv;

  @autoreleasepool {
    NSApplication* app = [NSApplication sharedApplication];
    GotSoupMacController* controller = [[GotSoupMacController alloc] init];
    [app setDelegate:controller];
    [app run];
  }

  return 0;
}

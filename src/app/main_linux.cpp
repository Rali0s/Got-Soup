#include <iostream>

#include "core/api/core_api.hpp"
#include "core/model/app_meta.hpp"

#ifdef ALPHA_USE_GTK4
#include <gtk/gtk.h>

namespace {

struct AppContext {
  alpha::CoreApi api;
  GtkTextBuffer* recipes_buffer = nullptr;
  GtkTextBuffer* profile_buffer = nullptr;
  GtkTextBuffer* rewards_buffer = nullptr;
  GtkTextBuffer* about_buffer = nullptr;
  GtkTextBuffer* settings_buffer = nullptr;
};

void fill_texts(AppContext* ctx) {
  (void)ctx->api.sync_tick();
  const auto recipes = ctx->api.search({.text = {}, .category = {}});
  std::string recipes_text = "Recipes\n\n";
  for (const auto& recipe : recipes) {
    recipes_text += recipe.core_topic ? "[CORE] " : "[POST] ";
    recipes_text += recipe.title + " [" + recipe.category + "]";
    recipes_text += " 👍" + std::to_string(recipe.thumbs_up_count);
    recipes_text += " value=" + std::to_string(recipe.value_units);
    recipes_text += " conf=" + std::to_string(recipe.confirmation_count) + "\n";
  }
  if (recipes.empty()) {
    recipes_text += "No recipes yet. Use Upload tab in macOS/Windows shells.\n";
  }
  gtk_text_buffer_set_text(ctx->recipes_buffer, recipes_text.c_str(), -1);

  const auto profile = ctx->api.profile();
  const auto anonymity = ctx->api.anonymity_status();
  const auto node = ctx->api.node_status();
  std::string profile_text;
  profile_text += "CID: " + profile.cid.value + "\n";
  profile_text += "Display Name: " + profile.display_name + "\n";
  profile_text += "Display Name State: ";
  profile_text += (profile.display_name_immortalized ? "IMMORTALIZED" : "not set");
  profile_text += "\n";
  profile_text += "Duplicate Name Policy: ";
  profile_text += (profile.reject_duplicate_names ? "REJECT" : "ALLOW");
  profile_text += "\n";
  profile_text += "Duplicate State: ";
  profile_text += (profile.duplicate_name_detected ? "DUPLICATE DETECTED" : "UNIQUE");
  profile_text += " (count=" + std::to_string(profile.duplicate_name_count) + ")\n";
  profile_text += "Reward Balance: " + std::to_string(node.local_reward_balance) + "\n";
  profile_text += "Mode: " + anonymity.mode + "\n";
  profile_text += anonymity.details + "\n";
  profile_text += "\nProfile controls are available in the macOS and Windows native tabs.\n";
  gtk_text_buffer_set_text(ctx->profile_buffer, profile_text.c_str(), -1);

  std::string rewards_text;
  rewards_text += "Rewards (PoW)\n\n";
  rewards_text += "Network: " + node.p2p.network + "\n";
  rewards_text += "Max Supply: " + std::to_string(node.db.max_token_supply) + "\n";
  rewards_text += "Issued: " + std::to_string(node.db.issued_reward_total) + "\n";
  rewards_text += "Burned Fees: " + std::to_string(node.db.burned_fee_total) + "\n";
  rewards_text += "Circulating: " + std::to_string(node.db.reward_supply) + "\n";
  rewards_text += "Local Balance: " + std::to_string(node.local_reward_balance) + "\n";
  rewards_text += "Reward Claim Events: " + std::to_string(node.db.reward_claim_event_count) + "\n";
  rewards_text += "Transfer Events: " + std::to_string(node.db.reward_transfer_event_count) + "\n";
  rewards_text += "Invalid Economic Events: " + std::to_string(node.db.invalid_economic_event_count) + "\n";
  gtk_text_buffer_set_text(ctx->rewards_buffer, rewards_text.c_str(), -1);

  std::string about_text;
  about_text += std::string{alpha::kAppDisplayName} + "\n\n";
  about_text += "Current Tor Version: " + node.tor.version + "\n";
  about_text += "Current I2P Version: " + node.i2p.version + "\n";
  about_text += "Current P2P:Soup Version Build Release: " + std::string{alpha::kAppVersion} + " (" +
                std::string{alpha::kBuildRelease} + ")\n";
  about_text += "Authors: " + std::string{alpha::kAuthorList} + "\n\n";
  about_text += "Assets\n";
  about_text += "- About PNG: " + node.data_dir + "/assets/about.png\n";
  about_text += "- Splash PNG: " + node.data_dir + "/assets/tomato_soup.png\n";
  about_text += "Chain: " + node.genesis.chain_id + " (" + node.genesis.network_id + ")\n\n";
  about_text += "Credits\n";
  about_text += "- C++23 modular alpha_core\n";
  about_text += "- Native GTK4 shell\n";
  about_text += "- Planned deps: libp2p, libsodium, SQLCipher, libtor, i2pd\n";
  gtk_text_buffer_set_text(ctx->about_buffer, about_text.c_str(), -1);

  std::string settings_text;
  settings_text += "Settings\n\n";
  settings_text += "Data Dir: " + node.data_dir + "\n";
  settings_text += "Events: " + node.db.events_file + "\n";
  settings_text += "Blockdata: " + node.db.blockdata_file + "\n";
  settings_text += "Snapshot: " + node.db.snapshot_file + "\n";
  settings_text += "Wallet locked: ";
  settings_text += (node.wallet.locked ? "YES" : "NO");
  settings_text += "\nFinality threshold: " + std::to_string(node.chain_policy.confirmation_threshold) + "\n";
  settings_text += "Fork choice: " + node.chain_policy.fork_choice_rule + "\n";
  settings_text += "Max reorg depth: " + std::to_string(node.chain_policy.max_reorg_depth) + "\n";
  settings_text += "Max block events: " + std::to_string(node.validation_limits.max_block_events) + "\n";
  settings_text += "Max block bytes: " + std::to_string(node.validation_limits.max_block_bytes) + "\n";
  settings_text += "Max event bytes: " + std::to_string(node.validation_limits.max_event_bytes) + "\n";
  gtk_text_buffer_set_text(ctx->settings_buffer, settings_text.c_str(), -1);
}

void activate(GtkApplication* app, gpointer user_data) {
  auto* ctx = static_cast<AppContext*>(user_data);

  GtkWidget* window = gtk_application_window_new(app);
  gtk_window_set_title(GTK_WINDOW(window), "got-soup::P2P Tomato Soup - Recipe Forum");
  gtk_window_set_default_size(GTK_WINDOW(window), 1200, 760);

  GtkWidget* root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
  gtk_widget_set_margin_top(root, 10);
  gtk_widget_set_margin_bottom(root, 10);
  gtk_widget_set_margin_start(root, 10);
  gtk_widget_set_margin_end(root, 10);
  gtk_window_set_child(GTK_WINDOW(window), root);

  GtkWidget* top = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
  gtk_box_append(GTK_BOX(root), top);

  GtkWidget* search = gtk_search_entry_new();
  gtk_widget_set_hexpand(search, TRUE);
  gtk_box_append(GTK_BOX(top), search);

  GtkWidget* close = gtk_button_new_with_label("Close");
  g_signal_connect_swapped(close, "clicked", G_CALLBACK(gtk_window_destroy), window);
  gtk_box_append(GTK_BOX(top), close);

  GtkWidget* body = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
  gtk_widget_set_vexpand(body, TRUE);
  gtk_box_append(GTK_BOX(root), body);

  GtkWidget* left = gtk_list_box_new();
  gtk_widget_set_size_request(left, 260, -1);
  gtk_box_append(GTK_BOX(body), left);

  const auto parents = ctx->api.reference_parent_menus();
  for (const auto& item : parents) {
    GtkWidget* row = gtk_label_new(item.c_str());
    gtk_list_box_append(GTK_LIST_BOX(left), row);
  }

  GtkWidget* notebook = gtk_notebook_new();
  gtk_widget_set_hexpand(notebook, TRUE);
  gtk_widget_set_vexpand(notebook, TRUE);
  gtk_box_append(GTK_BOX(body), notebook);

  GtkWidget* recipes_view = gtk_text_view_new();
  gtk_text_view_set_editable(GTK_TEXT_VIEW(recipes_view), FALSE);
  ctx->recipes_buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(recipes_view));
  gtk_notebook_append_page(GTK_NOTEBOOK(notebook), recipes_view, gtk_label_new("Recipes"));

  GtkWidget* forum_view = gtk_text_view_new();
  gtk_text_view_set_editable(GTK_TEXT_VIEW(forum_view), FALSE);
  gtk_text_buffer_set_text(gtk_text_view_get_buffer(GTK_TEXT_VIEW(forum_view)),
                           "Forum tab placeholder for MVP shell.", -1);
  gtk_notebook_append_page(GTK_NOTEBOOK(notebook), forum_view, gtk_label_new("Forum"));

  GtkWidget* upload_view = gtk_text_view_new();
  gtk_text_buffer_set_text(gtk_text_view_get_buffer(GTK_TEXT_VIEW(upload_view)),
                           "Upload tab placeholder on Linux shell.", -1);
  gtk_notebook_append_page(GTK_NOTEBOOK(notebook), upload_view, gtk_label_new("Upload"));

  GtkWidget* profile_view = gtk_text_view_new();
  gtk_text_view_set_editable(GTK_TEXT_VIEW(profile_view), FALSE);
  ctx->profile_buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(profile_view));
  gtk_notebook_append_page(GTK_NOTEBOOK(notebook), profile_view, gtk_label_new("Profile"));

  GtkWidget* rewards_view = gtk_text_view_new();
  gtk_text_view_set_editable(GTK_TEXT_VIEW(rewards_view), FALSE);
  ctx->rewards_buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(rewards_view));
  gtk_notebook_append_page(GTK_NOTEBOOK(notebook), rewards_view, gtk_label_new("Rewards"));

  GtkWidget* about_view = gtk_text_view_new();
  gtk_text_view_set_editable(GTK_TEXT_VIEW(about_view), FALSE);
  ctx->about_buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(about_view));
  gtk_notebook_append_page(GTK_NOTEBOOK(notebook), about_view, gtk_label_new("About"));

  GtkWidget* settings_view = gtk_text_view_new();
  gtk_text_view_set_editable(GTK_TEXT_VIEW(settings_view), FALSE);
  ctx->settings_buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(settings_view));
  gtk_notebook_append_page(GTK_NOTEBOOK(notebook), settings_view, gtk_label_new("Settings"));

  fill_texts(ctx);
  gtk_window_present(GTK_WINDOW(window));
}

}  // namespace

int main(int argc, char** argv) {
  AppContext context;
  const alpha::Result init = context.api.init({
      .app_data_dir = "got-soup-data-linux",
      .passphrase = "got-soup-dev-passphrase",
      .mode = alpha::AnonymityMode::Tor,
      .seed_peers = {"seed.got-soup.local:4001"},
      .seed_peers_mainnet = {"seed.got-soup.local:4001"},
      .seed_peers_testnet = {"seed.got-soup.local:14001"},
      .production_swap = true,
      .block_interval_seconds = 25,
      .validation_interval_ticks = 10,
      .block_reward_units = 50,
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

  if (!init.ok) {
    std::cerr << "Init failed: " << init.message << '\n';
  }

  if (context.api.search({.text = {}, .category = {}}).empty()) {
    context.api.create_recipe({
        .category = "Core Topic",
        .title = "Tomato Soup Base",
        .markdown = "# Tomato Soup Base\n\nCore method for all tomato soup variations.",
        .core_topic = true,
        .menu_segment = "core-menu",
    });
    context.api.create_recipe({
        .category = "Ingredient",
        .title = "Essential Ingredients",
        .markdown = "- Tomatoes\n- Olive oil\n- Garlic\n- Salt",
        .core_topic = true,
        .menu_segment = "core-ingredients",
    });
    context.api.create_recipe({
        .category = "Community",
        .title = "Starter: P2P Tomato Soup",
        .markdown = "# Tomato Soup\n\n- 4 tomatoes\n- Olive oil\n- Salt\n\nSimmer 20 minutes.",
        .core_topic = false,
        .menu_segment = "community-post",
    });
  }

  GtkApplication* app = gtk_application_new("local.got-soup.desktop", G_APPLICATION_DEFAULT_FLAGS);
  g_signal_connect(app, "activate", G_CALLBACK(activate), &context);
  const int status = g_application_run(G_APPLICATION(app), argc, argv);
  g_object_unref(app);
  return status;
}

#else

int main() {
  alpha::CoreApi api;
  const alpha::Result init = api.init({
      .app_data_dir = "got-soup-data-linux",
      .passphrase = "got-soup-dev-passphrase",
      .mode = alpha::AnonymityMode::Tor,
      .seed_peers = {"seed.got-soup.local:4001"},
      .seed_peers_mainnet = {"seed.got-soup.local:4001"},
      .seed_peers_testnet = {"seed.got-soup.local:14001"},
      .production_swap = true,
      .block_interval_seconds = 25,
      .validation_interval_ticks = 10,
      .block_reward_units = 50,
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

  if (!init.ok) {
    std::cerr << "got-soup Linux shell init failed: " << init.message << '\n';
    return 1;
  }

  std::cout << "got-soup Linux target built without GTK4.\n";
  std::cout << "Install gtk4 development packages and rebuild for native GUI mode.\n";
  return 0;
}

#endif

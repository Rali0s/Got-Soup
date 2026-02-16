#include <cassert>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <ranges>
#include <string>
#include <thread>

#include "core/api/core_api.hpp"
#include "core/crypto/crypto.hpp"
#include "core/storage/store.hpp"
#include "core/util/canonical.hpp"

namespace {

std::filesystem::path temp_dir(const std::string& name) {
  const auto root = std::filesystem::temp_directory_path() / "got-soup-tests" / name;
  std::error_code ec;
  std::filesystem::remove_all(root, ec);
  std::filesystem::create_directories(root, ec);
  return root;
}

void test_crypto_signatures() {
  alpha::CryptoEngine crypto;
  const auto dir = temp_dir("crypto");

  const alpha::Result init = crypto.initialize(dir.string(), "test-passphrase", true);
  assert(init.ok);

  const std::string payload = "hello-alpha";
  const std::string hash_a = crypto.hash_bytes(payload);
  const std::string hash_b = crypto.hash_bytes(payload);
  assert(hash_a == hash_b);

  const std::string signature = crypto.sign(payload);
  assert(!signature.empty());
  assert(crypto.verify(payload, signature, crypto.identity().public_key));
  assert(!crypto.verify(payload + "-x", signature, crypto.identity().public_key));

  const std::string phase_status = crypto.core_phase_status();
  assert(!phase_status.empty());
}

void test_store_materialization() {
  alpha::Store store;
  const auto dir = temp_dir("store");

  alpha::Result open = store.open(dir.string(), "vault-key");
  assert(open.ok);

  alpha::EventEnvelope event;
  event.event_id = "evt-test-1";
  event.kind = alpha::EventKind::RecipeCreated;
  event.author_cid = "cid-test";
  event.unix_ts = alpha::util::unix_timestamp_now();
  event.payload = alpha::util::canonical_join({
      {"recipe_id", "rcp-1"},
      {"category", "Soup"},
      {"title", "Test Soup"},
      {"markdown", "Boil water"},
  });
  event.signature = "sig";

  alpha::Result append = store.append_event(event);
  assert(append.ok);
  assert(store.all_events().size() == 1);
  assert(std::filesystem::exists(dir / "blockdata.dat"));

  append = store.append_event(event);
  assert(append.ok);
  assert(store.all_events().size() == 1);

  auto recipes = store.query_recipes({.text = "soup", .category = {}});
  assert(recipes.size() == 1);
  assert(recipes.front().title == "Test Soup");

  const auto health = store.health_report();
  assert(health.healthy);
  assert(health.event_count == 1);
  assert(store.next_claim_reward(1) == 115);
  assert(store.next_claim_reward(24193) == 110);  // per-block exponential decay
}

void test_core_api_flow() {
  alpha::CoreApi api;
  const auto dir = temp_dir("core-api");

  const alpha::Result init = api.init({
      .app_data_dir = dir.string(),
      .passphrase = "integration-passphrase",
      .mode = alpha::AnonymityMode::Tor,
      .seed_peers = {"seed-a", "seed-b"},
      .alpha_test_mode = false,
      .peers_dat_path = {},
      .community_profile_path = "recipes",
      .production_swap = true,
      .p2p_mainnet_port = 4001,
      .p2p_testnet_port = 14001,
  });
  assert(init.ok);

  alpha::Result create_recipe = api.create_recipe({
      .category = "Dinner",
      .title = "Garlic Pasta",
      .markdown = "Cook pasta and add garlic butter.",
  });
  assert(create_recipe.ok);

  auto recipes = api.search({.text = "garlic", .category = {}});
  assert(!recipes.empty());
  assert(!recipes.front().core_topic);

  alpha::Result add_review = api.add_review({
      .recipe_id = recipes.front().recipe_id,
      .rating = 5,
      .markdown = "Great recipe",
  });
  assert(add_review.ok);

  alpha::Result thumb = api.add_thumb_up(recipes.front().recipe_id);
  assert(thumb.ok);

  recipes = api.search({.text = "garlic", .category = {}});
  assert(!recipes.empty());
  assert(recipes.front().review_count >= 1);
  assert(recipes.front().thumbs_up_count >= 1);

  const auto sync_events = api.sync_tick();
  assert(!sync_events.empty());
}

void test_forum_reference_sync() {
  alpha::CoreApi api;
  const auto dir = temp_dir("forum-reference-sync");

  const alpha::Result init = api.init({
      .app_data_dir = dir.string(),
      .passphrase = "integration-passphrase",
      .mode = alpha::AnonymityMode::Tor,
      .seed_peers = {"seed-a"},
      .alpha_test_mode = false,
      .peers_dat_path = {},
      .community_profile_path = "recipes",
      .production_swap = true,
      .p2p_mainnet_port = 4001,
      .p2p_testnet_port = 14001,
  });
  assert(init.ok);

  alpha::Result create_recipe = api.create_recipe({
      .category = "Lunch",
      .title = "Tomato Toast",
      .markdown = "Toast bread, add tomato and olive oil.",
      .core_topic = true,
      .menu_segment = "core-menu",
  });
  assert(create_recipe.ok);

  alpha::Result create_post = api.create_recipe({
      .category = "Community",
      .title = "Tomato Toast Remix",
      .markdown = "I add basil and black pepper.",
      .core_topic = false,
      .menu_segment = "community-post",
  });
  assert(create_post.ok);

  const auto recipes = api.search({.text = "Tomato Toast", .category = {}});
  assert(!recipes.empty());

  alpha::Result create_thread = api.create_thread({
      .recipe_id = recipes.front().recipe_id,
      .title = "Texture tips",
      .markdown = "How crisp should the toast be?",
  });
  assert(create_thread.ok);

  const auto threads = api.threads(recipes.front().recipe_id);
  assert(!threads.empty());

  alpha::Result create_reply = api.create_reply({
      .thread_id = threads.front().thread_id,
      .markdown = "I prefer medium-crisp for soaking juices.",
  });
  assert(create_reply.ok);

  const auto parents = api.reference_parent_menus();
  assert(std::ranges::find(parents, std::string{"Forum"}) != parents.end());

  const auto secondary = api.reference_secondary_menus("Forum");
  assert(std::ranges::find(secondary, std::string{"Core Menu"}) != secondary.end());
  assert(std::ranges::find(secondary, std::string{"Community Posts"}) != secondary.end());
  assert(std::ranges::find(secondary, std::string{"Recipes"}) != secondary.end());
  assert(std::ranges::find(secondary, std::string{"Threads"}) != secondary.end());
  assert(std::ranges::find(secondary, std::string{"Replies"}) != secondary.end());

  const auto core_openings = api.reference_openings("Forum", "Core Menu", "Tomato Toast");
  assert(!core_openings.empty());
  const auto core_lookup = api.reference_lookup(core_openings.front());
  assert(core_lookup.has_value());
  assert(core_lookup->title.find("[CORE]") != std::string::npos);

  const auto post_openings = api.reference_openings("Forum", "Community Posts", "Remix");
  assert(!post_openings.empty());
  const auto post_lookup = api.reference_lookup(post_openings.front());
  assert(post_lookup.has_value());
  assert(post_lookup->title.find("[COMMUNITY]") != std::string::npos);

  const auto recipe_openings = api.reference_openings("Forum", "Recipes", "Tomato Toast");
  assert(!recipe_openings.empty());
  assert(recipe_openings.front().rfind("forum::recipe::", 0) == 0);

  const auto recipe_lookup = api.reference_lookup(recipe_openings.front());
  assert(recipe_lookup.has_value());
  assert(recipe_lookup->title.find("Recipe:") != std::string::npos);
  assert(recipe_lookup->body.find("Universal Confirmation:") != std::string::npos);
  assert(recipe_lookup->body.find("Consensus Hash:") != std::string::npos);

  const auto thread_openings = api.reference_openings("Forum", "Threads", "Texture");
  assert(!thread_openings.empty());
  const auto thread_lookup = api.reference_lookup(thread_openings.front());
  assert(thread_lookup.has_value());
  assert(thread_lookup->title.find("Thread:") == 0);

  const auto reply_openings = api.reference_openings("Forum", "Replies", "medium-crisp");
  assert(!reply_openings.empty());
  const auto reply_lookup = api.reference_lookup(reply_openings.front());
  assert(reply_lookup.has_value());
  assert(reply_lookup->title.find("Reply:") == 0);
}

void test_node_status_toggles_and_alpha_mode() {
  alpha::CoreApi api;
  const auto dir = temp_dir("node-status");

  const alpha::Result init = api.init({
      .app_data_dir = dir.string(),
      .passphrase = "integration-passphrase",
      .mode = alpha::AnonymityMode::Tor,
      .seed_peers = {"seed-a"},
      .alpha_test_mode = false,
      .peers_dat_path = {},
      .community_profile_path = "recipes",
      .production_swap = true,
      .p2p_mainnet_port = 4001,
      .p2p_testnet_port = 14001,
  });
  assert(init.ok);

  auto status = api.node_status();
  assert(status.tor_enabled);
  assert(status.i2p_enabled);
  assert(status.db.healthy);
  assert(status.p2p.network == "mainnet");
  assert(status.p2p.bind_port == 4001);

  alpha::Result result = api.set_transport_enabled(alpha::AnonymityMode::I2P, false);
  assert(result.ok);
  status = api.node_status();
  assert(!status.i2p_enabled);

  result = api.set_alpha_test_mode(true);
  assert(result.ok);
  status = api.node_status();
  assert(status.alpha_test_mode);
  assert(status.p2p.bind_host == "127.0.0.1");
  assert(status.p2p.network == "testnet");
  assert(status.p2p.bind_port == 14001);
  assert(!status.db.consensus_hash.empty());
  assert(status.db.block_count >= 1);
}

void test_peers_dat_and_community_profiles() {
  alpha::CoreApi api;
  const auto dir = temp_dir("community-peers");

  const alpha::Result init = api.init({
      .app_data_dir = dir.string(),
      .passphrase = "integration-passphrase",
      .mode = alpha::AnonymityMode::Tor,
      .seed_peers = {"seed-a"},
      .alpha_test_mode = false,
      .peers_dat_path = {},
      .community_profile_path = "recipes",
      .production_swap = true,
      .p2p_mainnet_port = 4001,
      .p2p_testnet_port = 14001,
  });
  assert(init.ok);

  auto status = api.node_status();
  assert(!status.peers_dat_path.empty());
  assert(std::filesystem::exists(status.peers_dat_path));

  alpha::Result add_peer = api.add_peer("peer.alpha.local:4001");
  assert(add_peer.ok);

  std::ifstream peers_file(status.peers_dat_path);
  assert(peers_file.good());

  alpha::Result switch_community =
      api.use_community_profile("woodworking", "Woodworking Community", "Project-focused wood recipes");
  assert(switch_community.ok);

  const auto current = api.current_community();
  assert(current.community_id == "woodworking");
  assert(std::filesystem::exists(current.profile_path));

  const auto communities = api.community_profiles();
  assert(!communities.empty());

  alpha::Result create_recipe = api.create_recipe({
      .category = "Shop",
      .title = "Workbench Oil Finish",
      .markdown = "Apply two coats and cure for 24h.",
  });
  assert(create_recipe.ok);

  const auto recipes = api.search({.text = "Workbench", .category = {}});
  assert(!recipes.empty());
}

void test_profile_identity_controls() {
  alpha::CoreApi api;
  const auto dir = temp_dir("profile-controls");

  const alpha::Result init = api.init({
      .app_data_dir = dir.string(),
      .passphrase = "integration-passphrase",
      .mode = alpha::AnonymityMode::Tor,
      .seed_peers = {"seed-a"},
      .alpha_test_mode = false,
      .peers_dat_path = {},
      .community_profile_path = "recipes",
      .production_swap = true,
      .p2p_mainnet_port = 4001,
      .p2p_testnet_port = 14001,
  });
  assert(init.ok);

  const alpha::Result missing_password =
      api.set_immortal_name_with_cipher("Chef Tomato", "", "recipe-salt");
  assert(!missing_password.ok);

  const alpha::Result set_name =
      api.set_immortal_name_with_cipher("Chef Tomato", "cipher-pass", "recipe-salt");
  assert(set_name.ok);
  const auto named_profile = api.profile();
  assert(named_profile.display_name == "Chef Tomato");
  assert(named_profile.display_name_immortalized);

  const alpha::Result rename_attempt = api.set_profile_display_name("Chef Basil");
  assert(!rename_attempt.ok);

  const alpha::Result dup_policy = api.set_duplicate_name_policy(false);
  assert(dup_policy.ok);
  const auto updated_profile = api.profile();
  assert(!updated_profile.reject_duplicate_names);

  const alpha::Result cipher_update = api.set_profile_cipher_password("cipher-pass-2", "recipe-salt-2");
  assert(cipher_update.ok);

  const std::filesystem::path backup_path = dir / "backup" / "identity.dat";
  const alpha::Result export_key = api.export_key_backup(backup_path.string(), "backup-pass", "backup-salt");
  assert(export_key.ok);
  assert(std::filesystem::exists(backup_path));

  const std::string before_nuke_cid = api.profile().cid.value;
  const alpha::Result nuke = api.nuke_key("NUKE-KEY");
  assert(nuke.ok);
  const std::string after_nuke_cid = api.profile().cid.value;
  assert(!after_nuke_cid.empty());

  const alpha::Result import_key = api.import_key_backup(backup_path.string(), "backup-pass");
  assert(import_key.ok);
  const std::string after_import_cid = api.profile().cid.value;
  assert(after_import_cid == before_nuke_cid);

  const alpha::Result backtest = api.run_backtest_validation();
  assert(backtest.ok);
}

void test_wallet_lock_unlock_and_recovery() {
  alpha::CoreApi api;
  const auto dir = temp_dir("wallet-lifecycle");

  const alpha::Result init = api.init({
      .app_data_dir = dir.string(),
      .passphrase = "integration-passphrase",
      .mode = alpha::AnonymityMode::Tor,
      .seed_peers = {"seed-a"},
      .seed_peers_mainnet = {"seed-main"},
      .seed_peers_testnet = {"seed-test"},
      .alpha_test_mode = false,
      .peers_dat_path = {},
      .community_profile_path = "recipes",
      .production_swap = true,
      .p2p_mainnet_port = 4001,
      .p2p_testnet_port = 14001,
  });
  assert(init.ok);

  const alpha::Result set_name =
      api.set_immortal_name_with_cipher("Wallet Chef", "cipher-pass", "salt");
  assert(set_name.ok);

  const auto before = api.profile().cid.value;
  const auto backup_path = dir / "backup" / "wallet.dat";
  const alpha::Result backup = api.export_key_backup(backup_path.string(), "backup-pass", "salt");
  assert(backup.ok);
  assert(std::filesystem::exists(backup_path));

  const alpha::Result lock = api.lock_wallet();
  assert(lock.ok);
  const alpha::Result create_locked = api.create_recipe({
      .category = "Locked",
      .title = "Should Fail",
      .markdown = "wallet locked",
  });
  assert(!create_locked.ok);

  const alpha::Result unlock_bad = api.unlock_wallet("wrong-pass");
  assert(!unlock_bad.ok);
  const alpha::Result unlock = api.unlock_wallet("integration-passphrase");
  assert(unlock.ok);

  const alpha::Result create_unlocked = api.create_recipe({
      .category = "Unlocked",
      .title = "Should Pass",
      .markdown = "wallet unlocked",
  });
  assert(create_unlocked.ok);

  const alpha::Result nuke = api.nuke_key("NUKE-KEY");
  assert(nuke.ok);
  const auto nuked = api.profile().cid.value;
  assert(nuked != before);

  const alpha::Result recover =
      api.recover_wallet(backup_path.string(), "backup-pass", "integration-passphrase");
  assert(recover.ok);
  const auto recovered = api.profile().cid.value;
  assert(recovered == before);
}

void test_reward_claim_and_high_value_gating() {
  alpha::CoreApi api;
  const auto dir = temp_dir("reward-gating");

  const alpha::Result init = api.init({
      .app_data_dir = dir.string(),
      .passphrase = "integration-passphrase",
      .mode = alpha::AnonymityMode::Tor,
      .seed_peers = {"seed-a"},
      .alpha_test_mode = false,
      .peers_dat_path = {},
      .community_profile_path = "recipes",
      .production_swap = true,
      .p2p_mainnet_port = 4001,
      .p2p_testnet_port = 14001,
      .block_interval_seconds = 1,
      .block_reward_units = 6,
      .minimum_post_value = 3,
      .genesis_psz_timestamp = "Alpha-One genesis: got-soup reward ledger start",
  });
  assert(init.ok);

  alpha::Result create_core = api.create_recipe({
      .category = "Core Topic",
      .title = "Core Tomato Base",
      .markdown = "Core baseline recipe.",
      .core_topic = true,
      .menu_segment = "core-menu",
  });
  assert(create_core.ok);

  auto recipes = api.search({.text = "Core Tomato Base", .category = {}});
  assert(!recipes.empty());

  // Minimum post value is 3 in this community and there are no rewards yet.
  alpha::Result create_thread_fail = api.create_thread({
      .recipe_id = recipes.front().recipe_id,
      .title = "Needs rewards first",
      .markdown = "Should fail before mining rewards.",
      .value_units = 0,
  });
  assert(!create_thread_fail.ok);

  std::int64_t balance_after_claim = api.local_reward_balance();
  for (int i = 0; i < 20 && balance_after_claim < 6; ++i) {
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    (void)api.sync_tick();
    balance_after_claim = api.local_reward_balance();
  }
  assert(balance_after_claim >= 6);

  alpha::Result create_thread_ok = api.create_thread({
      .recipe_id = recipes.front().recipe_id,
      .title = "Now funded",
      .markdown = "Posting after reward claim.",
      .value_units = 0,
  });
  assert(create_thread_ok.ok);

  const std::int64_t balance_after_post = api.local_reward_balance();
  assert(balance_after_post <= balance_after_claim - 3);
}

void test_genesis_merkle_and_transfer_flow() {
  alpha::CoreApi api;
  const auto dir = temp_dir("genesis-merkle-transfer");

  const alpha::Result init = api.init({
      .app_data_dir = dir.string(),
      .passphrase = "integration-passphrase",
      .mode = alpha::AnonymityMode::Tor,
      .seed_peers = {"seed-a"},
      .alpha_test_mode = false,
      .peers_dat_path = {},
      .community_profile_path = "recipes",
      .production_swap = true,
      .p2p_mainnet_port = 4001,
      .p2p_testnet_port = 14001,
      .block_interval_seconds = 1,
      .block_reward_units = 4,
      .genesis_psz_timestamp = "The Times 14/Feb/2026 got-soup genesis",
  });
  assert(init.ok);

  const alpha::Result set_name = api.set_immortal_name_with_cipher("Genesis Chef", "cipher-pass", "salt");
  assert(set_name.ok);

  alpha::Result create_recipe = api.create_recipe({
      .category = "Dinner",
      .title = "Merkle Soup",
      .markdown = "Check confirmations and merkle roots.",
      .value_units = 0,
  });
  assert(create_recipe.ok);

  std::this_thread::sleep_for(std::chrono::seconds(2));
  (void)api.sync_tick();

  const auto status = api.node_status();
  assert(status.db.genesis_psz_timestamp.starts_with("Feb. 16 2026 - 07:18 - 1771244337"));
  assert(!status.db.latest_merkle_root.empty());
  assert(status.db.reward_claim_event_count >= 1);
  assert(status.local_reward_balance >= 4);

  const auto recipes = api.search({.text = "Merkle Soup", .category = {}});
  assert(!recipes.empty());
  assert(recipes.front().confirmation_age_seconds >= 0);

  const auto recipe_lookup_key = "forum::recipe::" + recipes.front().recipe_id;
  const auto recipe_lookup = api.reference_lookup(recipe_lookup_key);
  assert(recipe_lookup.has_value());
  assert(recipe_lookup->body.find("Confirmations:") != std::string::npos);
  assert(recipe_lookup->body.find("Post Value:") != std::string::npos);

  const std::int64_t before_transfer = api.local_reward_balance();
  const alpha::Result transfer = api.transfer_rewards({
      .to_display_name = "Genesis Chef",
      .amount = 1,
      .memo = "self-check",
  });
  assert(transfer.ok);
  const std::int64_t after_transfer = api.local_reward_balance();
  assert(after_transfer == before_transfer - 1);

  const auto balances = api.reward_balances();
  assert(!balances.empty());
}

void test_testnet_genesis_defaults_to_today() {
  alpha::CoreApi api;
  const auto dir = temp_dir("testnet-genesis-default");

  const alpha::Result init = api.init({
      .app_data_dir = dir.string(),
      .passphrase = "integration-passphrase",
      .mode = alpha::AnonymityMode::I2P,
      .seed_peers = {"seed-a"},
      .alpha_test_mode = false,
      .peers_dat_path = {},
      .community_profile_path = "recipes",
      .production_swap = true,
      .p2p_mainnet_port = 4001,
      .p2p_testnet_port = 14001,
  });
  assert(init.ok);

  const auto status = api.node_status();
  assert(status.p2p.network == "testnet");
  assert(status.p2p.bind_port == 14001);
  assert(status.db.genesis_psz_timestamp.starts_with("Got Soup::P2P Tomato Soup testnet genesis"));
  assert(std::filesystem::exists(dir / "db-recipes-testnet" / "blockdata.dat"));
  assert(status.genesis.chain_id == "got-soup-testnet-v1");
  assert(!status.genesis.merkle_root.empty());
  assert(status.chain_policy.confirmation_threshold >= 1);
  assert(status.validation_limits.max_block_events >= 1);
}

void test_moderation_controls() {
  alpha::CoreApi api;
  const auto dir = temp_dir("moderation-controls");

  const alpha::Result init = api.init({
      .app_data_dir = dir.string(),
      .passphrase = "integration-passphrase",
      .mode = alpha::AnonymityMode::Tor,
      .seed_peers = {"seed-a"},
      .alpha_test_mode = false,
      .community_profile_path = "recipes",
      .production_swap = true,
      .default_moderation_policy =
          {
              .moderation_enabled = true,
              .require_finality_for_actions = false,
              .min_confirmations_for_enforcement = 1,
              .max_flags_before_auto_hide = 2,
              .role_model = "single-signer",
              .moderator_cids = {},
          },
  });
  assert(init.ok);

  const auto local_cid = api.profile().cid.value;
  auto moderation = api.moderation_status();
  assert(moderation.enabled);
  assert(std::ranges::find(moderation.active_moderators, local_cid) != moderation.active_moderators.end());

  alpha::Result create_recipe = api.create_recipe({
      .category = "Moderation",
      .title = "Flaggable Soup",
      .markdown = "Needs moderation flow test.",
      .core_topic = false,
      .menu_segment = "community-post",
  });
  assert(create_recipe.ok);

  auto recipes = api.search({.text = "Flaggable Soup", .category = {}});
  assert(!recipes.empty());
  const std::string recipe_id = recipes.front().recipe_id;

  alpha::Result flag_once = api.flag_content(recipe_id, "test-flag-1");
  assert(flag_once.ok);
  recipes = api.search({.text = "Flaggable Soup", .category = {}});
  assert(!recipes.empty());

  alpha::Result flag_twice = api.flag_content(recipe_id, "test-flag-2");
  assert(flag_twice.ok);
  recipes = api.search({.text = "Flaggable Soup", .category = {}});
  assert(recipes.empty());

  alpha::Result unhide = api.set_content_hidden(recipe_id, false, "manual-restore");
  assert(unhide.ok);
  recipes = api.search({.text = "Flaggable Soup", .category = {}});
  assert(!recipes.empty());
  assert(!recipes.front().core_topic);

  alpha::Result pin = api.pin_core_topic(recipe_id, true);
  assert(pin.ok);
  recipes = api.search({.text = "Flaggable Soup", .category = {}});
  assert(!recipes.empty());
  assert(recipes.front().core_topic);

  alpha::Result unpin = api.pin_core_topic(recipe_id, false);
  assert(unpin.ok);
  recipes = api.search({.text = "Flaggable Soup", .category = {}});
  assert(!recipes.empty());
  assert(!recipes.front().core_topic);

  const alpha::Result add_moderator = api.add_moderator("cid-external-moderator");
  assert(add_moderator.ok);
  moderation = api.moderation_status();
  assert(std::ranges::find(moderation.active_moderators, std::string{"cid-external-moderator"}) !=
         moderation.active_moderators.end());

  const alpha::Result remove_moderator = api.remove_moderator("cid-external-moderator");
  assert(remove_moderator.ok);
  moderation = api.moderation_status();
  assert(std::ranges::find(moderation.active_moderators, std::string{"cid-external-moderator"}) ==
         moderation.active_moderators.end());

  const alpha::Result remove_last = api.remove_moderator(local_cid);
  assert(!remove_last.ok);
}

}  // namespace

int main() {
  test_crypto_signatures();
  test_store_materialization();
  test_core_api_flow();
  test_forum_reference_sync();
  test_node_status_toggles_and_alpha_mode();
  test_peers_dat_and_community_profiles();
  test_profile_identity_controls();
  test_wallet_lock_unlock_and_recovery();
  test_reward_claim_and_high_value_gating();
  test_genesis_merkle_and_transfer_flow();
  test_testnet_genesis_defaults_to_today();
  test_moderation_controls();

  std::cout << "got_soup_unit_tests passed\n";
  return 0;
}

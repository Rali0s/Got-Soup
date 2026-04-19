#include <arpa/inet.h>
#include <cctype>
#include <cstdint>
#include <csignal>
#include <cerrno>
#include <cstring>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <netinet/in.h>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <vector>

#include "core/api/core_api.hpp"
#include "core/model/app_meta.hpp"
#include "core/util/canonical.hpp"
#include "core/util/hash.hpp"

namespace {

using alpha::CoreApi;
using alpha::Result;

volatile std::sig_atomic_t g_running = 1;

void handle_signal(int) {
  g_running = 0;
}

std::string default_data_dir() {
  const char* home = std::getenv("HOME");
  if (home != nullptr && home[0] != '\0') {
    return std::string{home} + "/.local/share/soupnet";
  }
  return "got-soupd-data";
}

std::string trim_copy(std::string_view text) {
  return alpha::util::trim_copy(text);
}

std::string json_escape(std::string_view value) {
  std::string out;
  out.reserve(value.size() + 8U);
  for (char c : value) {
    switch (c) {
      case '\\': out += "\\\\"; break;
      case '"': out += "\\\""; break;
      case '\n': out += "\\n"; break;
      case '\r': out += "\\r"; break;
      case '\t': out += "\\t"; break;
      default:
        out.push_back(c);
        break;
    }
  }
  return out;
}

std::string json_string(std::string_view value) {
  return "\"" + json_escape(value) + "\"";
}

std::optional<std::string> extract_json_string(std::string_view body, std::string_view key) {
  const std::string needle = "\"" + std::string{key} + "\"";
  const std::size_t key_pos = body.find(needle);
  if (key_pos == std::string_view::npos) {
    return std::nullopt;
  }
  const std::size_t colon = body.find(':', key_pos + needle.size());
  if (colon == std::string_view::npos) {
    return std::nullopt;
  }
  std::size_t cursor = colon + 1U;
  while (cursor < body.size() && std::isspace(static_cast<unsigned char>(body[cursor])) != 0) {
    ++cursor;
  }
  if (cursor >= body.size() || body[cursor] != '"') {
    return std::nullopt;
  }
  ++cursor;
  std::string out;
  bool escape = false;
  for (; cursor < body.size(); ++cursor) {
    const char c = body[cursor];
    if (escape) {
      out.push_back(c == 'n' ? '\n' : c);
      escape = false;
      continue;
    }
    if (c == '\\') {
      escape = true;
      continue;
    }
    if (c == '"') {
      return out;
    }
    out.push_back(c);
  }
  return std::nullopt;
}

std::optional<long long> extract_json_int(std::string_view body, std::string_view key) {
  const std::string needle = "\"" + std::string{key} + "\"";
  const std::size_t key_pos = body.find(needle);
  if (key_pos == std::string_view::npos) {
    return std::nullopt;
  }
  const std::size_t colon = body.find(':', key_pos + needle.size());
  if (colon == std::string_view::npos) {
    return std::nullopt;
  }
  std::size_t cursor = colon + 1U;
  while (cursor < body.size() && std::isspace(static_cast<unsigned char>(body[cursor])) != 0) {
    ++cursor;
  }
  std::size_t end = cursor;
  while (end < body.size() &&
         (std::isdigit(static_cast<unsigned char>(body[end])) != 0 || body[end] == '-')) {
    ++end;
  }
  if (end == cursor) {
    return std::nullopt;
  }
  try {
    return std::stoll(std::string{body.substr(cursor, end - cursor)});
  } catch (...) {
    return std::nullopt;
  }
}

std::optional<bool> extract_json_bool(std::string_view body, std::string_view key) {
  const std::string needle = "\"" + std::string{key} + "\"";
  const std::size_t key_pos = body.find(needle);
  if (key_pos == std::string_view::npos) {
    return std::nullopt;
  }
  const std::size_t colon = body.find(':', key_pos + needle.size());
  if (colon == std::string_view::npos) {
    return std::nullopt;
  }
  std::size_t cursor = colon + 1U;
  while (cursor < body.size() && std::isspace(static_cast<unsigned char>(body[cursor])) != 0) {
    ++cursor;
  }
  if (body.substr(cursor, 4) == "true") {
    return true;
  }
  if (body.substr(cursor, 5) == "false") {
    return false;
  }
  return std::nullopt;
}

std::string extract_json_id(std::string_view body) {
  if (const auto string_id = extract_json_string(body, "id"); string_id.has_value()) {
    return json_string(*string_id);
  }
  if (const auto int_id = extract_json_int(body, "id"); int_id.has_value()) {
    return std::to_string(*int_id);
  }
  return "null";
}

std::string http_response(int status, std::string_view status_text, std::string_view body) {
  std::ostringstream out;
  out << "HTTP/1.1 " << status << " " << status_text << "\r\n";
  out << "Content-Type: application/json\r\n";
  out << "Content-Length: " << body.size() << "\r\n";
  out << "Connection: close\r\n\r\n";
  out << body;
  return out.str();
}

std::string json_rpc_result(std::string_view id, std::string_view result_json) {
  return "{\"jsonrpc\":\"2.0\",\"id\":" + std::string{id} + ",\"result\":" + std::string{result_json} + "}";
}

std::string json_rpc_error(std::string_view id, int code, std::string_view message) {
  return "{\"jsonrpc\":\"2.0\",\"id\":" + std::string{id} + ",\"error\":{\"code\":" + std::to_string(code) +
         ",\"message\":" + json_string(message) + "}}";
}

std::string ensure_token_file(const std::string& path) {
  std::filesystem::create_directories(std::filesystem::path(path).parent_path());
  std::ifstream in(path);
  std::string token;
  if (in.good()) {
    std::getline(in, token);
    token = trim_copy(token);
    if (!token.empty()) {
      return token;
    }
  }
  token = alpha::util::sha256_like_hex(std::to_string(alpha::util::unix_timestamp_now()) + "|" + path).substr(0, 48);
  std::ofstream out(path, std::ios::trunc);
  out << token << "\n";
  return token;
}

std::string node_status_json(const alpha::NodeStatusReport& status) {
  std::ostringstream out;
  out << "{"
      << "\"network\":" << json_string(status.p2p.network) << ","
      << "\"bind_host\":" << json_string(status.p2p.bind_host) << ","
      << "\"bind_port\":" << status.p2p.bind_port << ","
      << "\"peer_count\":" << status.p2p.peer_count << ","
      << "\"consensus_hash\":" << json_string(status.db.consensus_hash) << ","
      << "\"timeline_hash\":" << json_string(status.db.timeline_hash) << ","
      << "\"chain_id\":" << json_string(status.genesis.chain_id) << ","
      << "\"genesis_block_hash\":" << json_string(status.genesis.block_hash) << ","
      << "\"wallet_locked\":" << (status.wallet.locked ? "true" : "false") << ","
      << "\"backup_verified\":" << (status.wallet.backup_verified ? "true" : "false") << ","
      << "\"crypto_mode\":" << json_string(status.wallet.crypto_mode) << ","
      << "\"startup_recovery_summary\":" << json_string(status.startup_recovery_summary) << "}";
  return out.str();
}

std::string health_json(const alpha::NodeStatusReport& status) {
  std::ostringstream out;
  out << "{"
      << "\"healthy\":" << (status.db.healthy ? "true" : "false") << ","
      << "\"details\":" << json_string(status.db.details) << ","
      << "\"backtest_ok\":" << (status.db.backtest_ok ? "true" : "false") << ","
      << "\"recovered_from_corruption\":" << (status.db.recovered_from_corruption ? "true" : "false") << ","
      << "\"data_dir\":" << json_string(status.data_dir) << ","
      << "\"last_checkpoint_block_hash\":" << json_string(status.db.last_checkpoint_block_hash) << "}";
  return out.str();
}

std::string receive_info_json(const alpha::ReceiveAddressInfo& info) {
  return "{"
         "\"cid\":" + json_string(info.cid) + "," +
         "\"display_name\":" + json_string(info.display_name) + "," +
         "\"address\":" + json_string(info.address) + "," +
         "\"public_key\":" + json_string(info.public_key) + "}";
}

std::string mining_template_json(const alpha::MiningTemplate& tpl) {
  std::ostringstream out;
  out << "{"
      << "\"chain_id\":" << json_string(tpl.chain_id) << ","
      << "\"network_id\":" << json_string(tpl.network_id) << ","
      << "\"community_id\":" << json_string(tpl.community_id) << ","
      << "\"miner_cid\":" << json_string(tpl.miner_cid) << ","
      << "\"algorithm\":" << json_string(tpl.algorithm) << ","
      << "\"pool_protocol_hint\":" << json_string(tpl.pool_protocol_hint) << ","
      << "\"next_block_index\":" << tpl.next_block_index << ","
      << "\"next_open_unix\":" << tpl.next_open_unix << ","
      << "\"prev_hash\":" << json_string(tpl.prev_hash) << ","
      << "\"merkle_root\":" << json_string(tpl.merkle_root) << ","
      << "\"content_hash\":" << json_string(tpl.content_hash) << ","
      << "\"anticipated_block_hash\":" << json_string(tpl.anticipated_block_hash) << ","
      << "\"difficulty_nibbles\":" << tpl.difficulty_nibbles << ","
      << "\"pow_material\":" << json_string(tpl.pow_material) << ","
      << "\"sample_nonce_hash\":" << json_string(tpl.sample_nonce_hash) << "}";
  return out.str();
}

std::string recipes_json(const std::vector<alpha::RecipeSummary>& recipes) {
  std::ostringstream out;
  out << "[";
  for (std::size_t i = 0; i < recipes.size(); ++i) {
    if (i > 0) {
      out << ",";
    }
    const auto& recipe = recipes[i];
    out << "{"
        << "\"recipe_id\":" << json_string(recipe.recipe_id) << ","
        << "\"title\":" << json_string(recipe.title) << ","
        << "\"category\":" << json_string(recipe.category) << ","
        << "\"author_cid\":" << json_string(recipe.author_cid) << ","
        << "\"thumbs_up_count\":" << recipe.thumbs_up_count << ","
        << "\"review_count\":" << recipe.review_count << ","
        << "\"average_rating\":" << recipe.average_rating
        << "}";
  }
  out << "]";
  return out.str();
}

std::string require_auth_response(std::string_view id) {
  return http_response(401, "Unauthorized", json_rpc_error(id, -32001, "Missing or invalid bearer token."));
}

struct Args {
  std::string data_dir = default_data_dir();
  std::string bind_host = "127.0.0.1";
  std::string token_file = default_data_dir() + "/daemon.token";
  std::string community_profile = "tomato-soup";
  int port = 4888;
  bool alpha_test_mode = false;
};

Args parse_args(int argc, char** argv) {
  Args args;
  for (int i = 1; i < argc; ++i) {
    const std::string_view arg = argv[i];
    auto take = [&](std::string& target) {
      if (i + 1 < argc) {
        target = argv[++i];
      }
    };
    if (arg == "--data-dir") {
      take(args.data_dir);
    } else if (arg == "--bind-host") {
      take(args.bind_host);
    } else if (arg == "--token-file") {
      take(args.token_file);
    } else if (arg == "--community-profile") {
      take(args.community_profile);
    } else if (arg == "--port" && i + 1 < argc) {
      args.port = std::max(1, std::atoi(argv[++i]));
    } else if (arg == "--testnet") {
      args.alpha_test_mode = true;
    }
  }
  return args;
}

std::string method_result_json(CoreApi& api, std::string_view method, std::string_view body) {
  if (method == "node.status") {
    return node_status_json(api.node_status());
  }
  if (method == "system.health") {
    return health_json(api.node_status());
  }
  if (method == "wallet.lock") {
    const Result result = api.lock_wallet();
    return "{\"ok\":" + std::string{result.ok ? "true" : "false"} + ",\"message\":" + json_string(result.message) + "}";
  }
  if (method == "wallet.unlock") {
    const Result result = api.unlock_wallet(extract_json_string(body, "passphrase").value_or(""));
    return "{\"ok\":" + std::string{result.ok ? "true" : "false"} + ",\"message\":" + json_string(result.message) + "}";
  }
  if (method == "wallet.backup") {
    const Result result = api.export_key_backup(extract_json_string(body, "path").value_or(""),
                                                extract_json_string(body, "password").value_or(""),
                                                extract_json_string(body, "salt").value_or(""));
    return "{\"ok\":" + std::string{result.ok ? "true" : "false"} + ",\"message\":" + json_string(result.message) +
           ",\"data\":" + json_string(result.data) + "}";
  }
  if (method == "wallet.verify_backup") {
    const Result result = api.verify_key_backup(extract_json_string(body, "path").value_or(""),
                                                extract_json_string(body, "password").value_or(""));
    return "{\"ok\":" + std::string{result.ok ? "true" : "false"} + ",\"message\":" + json_string(result.message) +
           ",\"data\":" + json_string(result.data) + "}";
  }
  if (method == "wallet.import_backup") {
    const Result result = api.import_key_backup(extract_json_string(body, "path").value_or(""),
                                                extract_json_string(body, "password").value_or(""));
    return "{\"ok\":" + std::string{result.ok ? "true" : "false"} + ",\"message\":" + json_string(result.message) + "}";
  }
  if (method == "wallet.recover") {
    const Result result = api.recover_wallet(extract_json_string(body, "backup_path").value_or(""),
                                             extract_json_string(body, "backup_password").value_or(""),
                                             extract_json_string(body, "new_local_passphrase").value_or(""));
    return "{\"ok\":" + std::string{result.ok ? "true" : "false"} + ",\"message\":" + json_string(result.message) + "}";
  }
  if (method == "wallet.receive_info") {
    return receive_info_json(api.receive_info());
  }
  if (method == "wallet.sign") {
    const auto signed_message = api.sign_message(extract_json_string(body, "message").value_or(""));
    return "{"
           "\"message\":" + json_string(signed_message.message) + "," +
           "\"signature\":" + json_string(signed_message.signature) + "," +
           "\"public_key\":" + json_string(signed_message.public_key) + "," +
           "\"cid\":" + json_string(signed_message.cid) + "," +
           "\"address\":" + json_string(signed_message.address) + "," +
           "\"wallet_locked\":" + std::string{signed_message.wallet_locked ? "true" : "false"} + "}";
  }
  if (method == "wallet.transfer") {
    const Result result = api.transfer_rewards_to_address({
        .to_address = extract_json_string(body, "to_address").value_or(""),
        .amount = extract_json_int(body, "amount").value_or(0),
        .memo = extract_json_string(body, "memo").value_or(""),
    });
    return "{\"ok\":" + std::string{result.ok ? "true" : "false"} + ",\"message\":" + json_string(result.message) + "}";
  }
  if (method == "recipes.search") {
    return recipes_json(api.search({
        .text = extract_json_string(body, "text").value_or(""),
        .category = extract_json_string(body, "category").value_or(""),
    }));
  }
  if (method == "recipes.create") {
    const Result result = api.create_recipe({
        .category = extract_json_string(body, "category").value_or(""),
        .title = extract_json_string(body, "title").value_or(""),
        .markdown = extract_json_string(body, "markdown").value_or(""),
        .core_topic = extract_json_bool(body, "core_topic").value_or(false),
        .menu_segment = extract_json_string(body, "menu_segment").value_or("community-post"),
        .value_units = extract_json_int(body, "value_units").value_or(0),
    });
    return "{\"ok\":" + std::string{result.ok ? "true" : "false"} + ",\"message\":" + json_string(result.message) + "}";
  }
  if (method == "threads.create") {
    const Result result = api.create_thread({
        .recipe_id = extract_json_string(body, "recipe_id").value_or(""),
        .title = extract_json_string(body, "title").value_or(""),
        .markdown = extract_json_string(body, "markdown").value_or(""),
        .value_units = extract_json_int(body, "value_units").value_or(0),
    });
    return "{\"ok\":" + std::string{result.ok ? "true" : "false"} + ",\"message\":" + json_string(result.message) + "}";
  }
  if (method == "replies.create") {
    const Result result = api.create_reply({
        .thread_id = extract_json_string(body, "thread_id").value_or(""),
        .markdown = extract_json_string(body, "markdown").value_or(""),
        .value_units = extract_json_int(body, "value_units").value_or(0),
    });
    return "{\"ok\":" + std::string{result.ok ? "true" : "false"} + ",\"message\":" + json_string(result.message) + "}";
  }
  if (method == "forum.downvote_purge") {
    const Result result = api.downvote_and_purge_content(extract_json_string(body, "object_id").value_or(""),
                                                         extract_json_string(body, "reason").value_or(""));
    return "{\"ok\":" + std::string{result.ok ? "true" : "false"} + ",\"message\":" + json_string(result.message) + "}";
  }
  if (method == "mining.template") {
    return mining_template_json(api.mining_template());
  }
  if (method == "genesis.spec") {
    const auto node = api.node_status();
    return "{"
           "\"chain_id\":" + json_string(node.genesis.chain_id) + "," +
           "\"network_id\":" + json_string(node.genesis.network_id) + "," +
           "\"psz_timestamp\":" + json_string(node.genesis.psz_timestamp) + "," +
           "\"merkle_root\":" + json_string(node.genesis.merkle_root) + "," +
           "\"block_hash\":" + json_string(node.genesis.block_hash) + "}";
  }
  throw std::runtime_error("Unknown method");
}

std::string read_request_body(int fd) {
  std::string request;
  char buffer[4096];
  while (true) {
    const ssize_t n = ::read(fd, buffer, sizeof(buffer));
    if (n <= 0) {
      break;
    }
    request.append(buffer, static_cast<std::size_t>(n));
    const std::size_t header_end = request.find("\r\n\r\n");
    if (header_end == std::string::npos) {
      continue;
    }
    const std::size_t len_pos = request.find("Content-Length:");
    std::size_t content_length = 0;
    if (len_pos != std::string::npos) {
      const std::size_t line_end = request.find("\r\n", len_pos);
      const std::string value = trim_copy(request.substr(len_pos + 15, line_end - (len_pos + 15)));
      content_length = static_cast<std::size_t>(std::strtoull(value.c_str(), nullptr, 10));
    }
    if (request.size() >= header_end + 4U + content_length) {
      return request;
    }
  }
  return request;
}

}  // namespace

int main(int argc, char** argv) {
  std::signal(SIGINT, handle_signal);
  std::signal(SIGTERM, handle_signal);

  const Args args = parse_args(argc, argv);
  const std::string token = ensure_token_file(args.token_file);
  std::filesystem::create_directories(args.data_dir);

  CoreApi api;
  const Result init = api.init({
      .app_data_dir = args.data_dir,
      .passphrase = "soupnet-dev-passphrase",
      .mode = alpha::AnonymityMode::Tor,
      .seed_peers = {"24.188.147.247:4001"},
      .seed_peers_mainnet = {"24.188.147.247:4001"},
      .seed_peers_testnet = {"seed.got-soup.local:14001"},
      .alpha_test_mode = args.alpha_test_mode,
      .community_profile_path = args.community_profile,
      .production_swap = true,
      .p2p_mainnet_port = 4001,
      .p2p_testnet_port = 14001,
  });
  if (!init.ok) {
    std::cerr << "got-soupd init failed: " << init.message << "\n";
    return 1;
  }

  std::cout << "got-soupd " << alpha::kAppVersion << " listening on " << args.bind_host << ":" << args.port << "\n";
  std::cout << "token file: " << args.token_file << "\n";
  std::cout << "auth mode: bearer token required for all RPC methods\n";

  const int server_fd = ::socket(AF_INET, SOCK_STREAM, 0);
  if (server_fd < 0) {
    std::perror("socket");
    return 1;
  }
  int opt = 1;
  ::setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(static_cast<std::uint16_t>(args.port));
  if (::inet_pton(AF_INET, args.bind_host.c_str(), &addr.sin_addr) != 1) {
    std::cerr << "invalid bind host: " << args.bind_host << "\n";
    ::close(server_fd);
    return 1;
  }
  if (::bind(server_fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
    std::perror("bind");
    ::close(server_fd);
    return 1;
  }
  if (::listen(server_fd, 16) != 0) {
    std::perror("listen");
    ::close(server_fd);
    return 1;
  }

  while (g_running) {
    sockaddr_in client_addr{};
    socklen_t client_len = sizeof(client_addr);
    const int client_fd = ::accept(server_fd, reinterpret_cast<sockaddr*>(&client_addr), &client_len);
    if (client_fd < 0) {
      if (errno == EINTR) {
        continue;
      }
      std::perror("accept");
      break;
    }

    const std::string request = read_request_body(client_fd);
    const std::string id = extract_json_id(request);
    std::string response;
    if (request.find("Authorization: Bearer " + token) == std::string::npos) {
      response = require_auth_response(id);
    } else {
      const std::optional<std::string> method = extract_json_string(request, "method");
      if (!method.has_value()) {
        response = http_response(400, "Bad Request", json_rpc_error(id, -32600, "Missing JSON-RPC method."));
      } else {
        try {
          response = http_response(200, "OK", json_rpc_result(id, method_result_json(api, *method, request)));
        } catch (const std::exception& ex) {
          response = http_response(404, "Not Found", json_rpc_error(id, -32601, ex.what()));
        }
      }
    }

    (void)::write(client_fd, response.data(), response.size());
    ::close(client_fd);
  }

  ::close(server_fd);
  return 0;
}

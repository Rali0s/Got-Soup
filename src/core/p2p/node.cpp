#include "core/p2p/node.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>

#include "core/util/canonical.hpp"

namespace alpha {

Result P2PNode::start(const std::vector<std::string>& seed_peers, const ProxyEndpoint& endpoint,
                      std::string_view local_cid, bool alpha_test_mode, std::uint16_t p2p_port,
                      std::string_view network_name) {
  if (local_cid.empty()) {
    return Result::failure("P2P start failed: local CID is empty.");
  }

  running_ = true;
  alpha_test_mode_ = alpha_test_mode;
  network_name_ = network_name.empty() ? "mainnet" : std::string{network_name};
  p2p_port_ = p2p_port == 0 ? (alpha_test_mode_ ? 14001 : 4001) : p2p_port;
  local_cid_ = std::string{local_cid};
  endpoint_ = endpoint;
  if (endpoint_.port == 0) {
    endpoint_.port = alpha_test_mode_ ? 4444 : 9050;
  }

  for (const auto& peer : seed_peers) {
    if (!peer.empty()) {
      peers_.push_back(peer);
    }
  }

  std::ranges::sort(peers_);
  peers_.erase(std::unique(peers_.begin(), peers_.end()), peers_.end());

  return Result::success("P2P node started with seed peers.");
}

void P2PNode::stop() {
  running_ = false;
  outbound_queue_.clear();
}

std::vector<std::string> P2PNode::peers() const {
  return peers_;
}

Result P2PNode::load_peers_dat(std::string_view path) {
  peers_dat_path_ = std::string{path};

  std::ifstream in(std::string{path});
  if (!in) {
    return Result::success("Peers file not found yet; it will be created after first save.");
  }

  std::string line;
  while (std::getline(in, line)) {
    const std::string trimmed = trim(line);
    if (is_comment_or_empty(trimmed)) {
      continue;
    }

    peers_.push_back(trimmed);
  }

  std::ranges::sort(peers_);
  peers_.erase(std::unique(peers_.begin(), peers_.end()), peers_.end());
  return Result::success("Loaded peers.dat entries.");
}

Result P2PNode::save_peers_dat(std::string_view path) const {
  if (path.empty()) {
    return Result::failure("save_peers_dat failed: empty path.");
  }

  const std::filesystem::path file_path{std::string{path}};
  std::error_code ec;
  if (file_path.has_parent_path()) {
    std::filesystem::create_directories(file_path.parent_path(), ec);
    if (ec) {
      return Result::failure("Unable to create peers.dat directory: " + ec.message());
    }
  }

  std::ofstream out(file_path, std::ios::out | std::ios::trunc);
  if (!out) {
    return Result::failure("Unable to write peers.dat file.");
  }

  out << "# got-soup peers.dat\n";
  out << "# one peer per line\n";
  for (const auto& peer : peers_) {
    out << peer << '\n';
  }

  if (!out.good()) {
    return Result::failure("Failed writing peers.dat file.");
  }

  return Result::success("Saved peers.dat file.");
}

Result P2PNode::add_peer(std::string_view peer) {
  const std::string trimmed = trim(peer);
  if (trimmed.empty()) {
    return Result::failure("Peer is empty.");
  }

  if (!alpha_test_mode_ && trimmed.find("127.0.0.1") != std::string::npos) {
    // Allow loopback peers only in explicit alpha test mode.
    return Result::failure("127.0.0.1 peers require Alpha Test Mode.");
  }

  peers_.push_back(trimmed);
  std::ranges::sort(peers_);
  peers_.erase(std::unique(peers_.begin(), peers_.end()), peers_.end());
  return Result::success("Peer added.");
}

void P2PNode::queue_local_event(const EventEnvelope& event) {
  if (!running_ || event.event_id.empty()) {
    return;
  }

  if (seen_event_ids_.insert(event.event_id).second) {
    outbound_queue_.push_back(event);
  }
}

bool P2PNode::ingest_remote_event(const EventEnvelope& event) {
  if (!running_ || event.event_id.empty()) {
    return false;
  }

  return seen_event_ids_.insert(event.event_id).second;
}

std::vector<EventEnvelope> P2PNode::sync_tick() {
  if (!running_) {
    return {};
  }

  ++sync_tick_count_;

  // Stub transport: return local outbound events as if publish/flush occurred.
  std::vector<EventEnvelope> published = outbound_queue_;
  outbound_queue_.clear();
  return published;
}

NodeRuntimeStats P2PNode::runtime_status() const {
  return {
      .running = running_,
      .alpha_test_mode = alpha_test_mode_,
      .network = network_name_,
      .bind_host = alpha_test_mode_ ? "127.0.0.1" : "0.0.0.0",
      .bind_port = p2p_port_,
      .proxy_port = endpoint_.port,
      .peer_count = peers_.size(),
      .outbound_queue = outbound_queue_.size(),
      .seen_event_count = seen_event_ids_.size(),
      .sync_tick_count = sync_tick_count_,
  };
}

std::string P2PNode::trim(std::string_view text) {
  return util::trim_copy(text);
}

bool P2PNode::is_comment_or_empty(std::string_view line) {
  return line.empty() || line.front() == '#';
}

}  // namespace alpha

#pragma once

#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

#include "core/model/types.hpp"
#include "core/transport/anonymity_provider.hpp"

namespace alpha {

class P2PNode {
public:
  Result start(const std::vector<std::string>& seed_peers, const ProxyEndpoint& endpoint,
               std::string_view local_cid, bool alpha_test_mode, std::uint16_t p2p_port,
               std::string_view network_name);
  void stop();

  [[nodiscard]] bool running() const { return running_; }
  [[nodiscard]] std::vector<std::string> peers() const;

  Result load_peers_dat(std::string_view path);
  Result save_peers_dat(std::string_view path) const;
  Result add_peer(std::string_view peer);

  void queue_local_event(const EventEnvelope& event);
  bool ingest_remote_event(const EventEnvelope& event);
  std::vector<EventEnvelope> sync_tick();

  [[nodiscard]] NodeRuntimeStats runtime_status() const;
  [[nodiscard]] std::string peers_dat_path() const { return peers_dat_path_; }

private:
  static std::string trim(std::string_view text);
  static bool is_comment_or_empty(std::string_view line);

  bool running_ = false;
  bool alpha_test_mode_ = false;
  std::string network_name_ = "mainnet";
  std::uint16_t p2p_port_ = 0;
  std::string local_cid_;
  ProxyEndpoint endpoint_;
  std::vector<std::string> peers_;
  std::string peers_dat_path_;

  std::unordered_set<std::string> seen_event_ids_;
  std::vector<EventEnvelope> outbound_queue_;
  std::uint64_t sync_tick_count_ = 0;
};

}  // namespace alpha

#pragma once

#include <cstdint>
#include <memory>
#include <string>

#include "core/model/types.hpp"

namespace alpha {

struct ProxyEndpoint {
  std::string host;
  std::uint16_t port = 0;
};

struct AnonymityStatus {
  bool running = false;
  std::string mode;
  std::string version;
  std::string details;
  std::int64_t last_started_unix = 0;
  std::int64_t last_stopped_unix = 0;
  std::uint64_t update_count = 0;
  ProxyEndpoint endpoint;
};

class IAnonymityProvider {
public:
  virtual ~IAnonymityProvider() = default;

  virtual Result start() = 0;
  virtual void stop() = 0;
  virtual void set_alpha_test_mode(bool enabled) = 0;

  [[nodiscard]] virtual AnonymityStatus status() const = 0;
  [[nodiscard]] virtual ProxyEndpoint proxy_endpoint() const = 0;
};

class LibtorProvider final : public IAnonymityProvider {
public:
  Result start() override;
  void stop() override;
  void set_alpha_test_mode(bool enabled) override;

  [[nodiscard]] AnonymityStatus status() const override;
  [[nodiscard]] ProxyEndpoint proxy_endpoint() const override;

private:
  bool running_ = false;
  bool alpha_test_mode_ = false;
  std::int64_t last_started_unix_ = 0;
  std::int64_t last_stopped_unix_ = 0;
  std::uint64_t update_count_ = 0;
};

class I2pdProvider final : public IAnonymityProvider {
public:
  Result start() override;
  void stop() override;
  void set_alpha_test_mode(bool enabled) override;

  [[nodiscard]] AnonymityStatus status() const override;
  [[nodiscard]] ProxyEndpoint proxy_endpoint() const override;

private:
  bool running_ = false;
  bool alpha_test_mode_ = false;
  std::int64_t last_started_unix_ = 0;
  std::int64_t last_stopped_unix_ = 0;
  std::uint64_t update_count_ = 0;
};

std::unique_ptr<IAnonymityProvider> make_anonymity_provider(AnonymityMode mode);

}  // namespace alpha

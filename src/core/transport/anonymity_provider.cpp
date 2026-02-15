#include "core/transport/anonymity_provider.hpp"

#include <string_view>

#include "core/util/canonical.hpp"

namespace alpha {
namespace {

constexpr std::string_view kLibtorProviderVersion = "libtor-provider-scaffold-0.1";
constexpr std::string_view kI2pdProviderVersion = "i2pd-provider-scaffold-0.1";

}  // namespace

Result LibtorProvider::start() {
  running_ = true;
  last_started_unix_ = util::unix_timestamp_now();
  ++update_count_;
  return Result::success("Embedded libtor provider started.");
}

void LibtorProvider::stop() {
  running_ = false;
  last_stopped_unix_ = util::unix_timestamp_now();
  ++update_count_;
}

void LibtorProvider::set_alpha_test_mode(bool enabled) {
  alpha_test_mode_ = enabled;
  ++update_count_;
}

AnonymityStatus LibtorProvider::status() const {
  AnonymityStatus status;
  status.running = running_;
  status.mode = "Tor";
  status.version = std::string{kLibtorProviderVersion};
  status.details = running_
                       ? (alpha_test_mode_ ? "Tor provider running in localhost alpha test mode."
                                           : "Tor provider running in standard network mode.")
                       : "Tor provider stopped.";
  status.last_started_unix = last_started_unix_;
  status.last_stopped_unix = last_stopped_unix_;
  status.update_count = update_count_;
  status.endpoint = proxy_endpoint();
  return status;
}

ProxyEndpoint LibtorProvider::proxy_endpoint() const {
  if (alpha_test_mode_) {
    return {
        .host = "127.0.0.1",
        .port = 19050,
    };
  }

  return {
      .host = "127.0.0.1",
      .port = 9150,
  };
}

Result I2pdProvider::start() {
  running_ = true;
  last_started_unix_ = util::unix_timestamp_now();
  ++update_count_;
  return Result::success("Embedded i2pd provider started.");
}

void I2pdProvider::stop() {
  running_ = false;
  last_stopped_unix_ = util::unix_timestamp_now();
  ++update_count_;
}

void I2pdProvider::set_alpha_test_mode(bool enabled) {
  alpha_test_mode_ = enabled;
  ++update_count_;
}

AnonymityStatus I2pdProvider::status() const {
  AnonymityStatus status;
  status.running = running_;
  status.mode = "I2P";
  status.version = std::string{kI2pdProviderVersion};
  status.details = running_
                       ? (alpha_test_mode_ ? "I2P provider running in localhost alpha test mode."
                                           : "I2P provider running in standard network mode.")
                       : "I2P provider stopped.";
  status.last_started_unix = last_started_unix_;
  status.last_stopped_unix = last_stopped_unix_;
  status.update_count = update_count_;
  status.endpoint = proxy_endpoint();
  return status;
}

ProxyEndpoint I2pdProvider::proxy_endpoint() const {
  if (alpha_test_mode_) {
    return {
        .host = "127.0.0.1",
        .port = 14044,
    };
  }

  return {
      .host = "127.0.0.1",
      .port = 4444,
  };
}

std::unique_ptr<IAnonymityProvider> make_anonymity_provider(AnonymityMode mode) {
  if (mode == AnonymityMode::I2P) {
    return std::make_unique<I2pdProvider>();
  }

  return std::make_unique<LibtorProvider>();
}

}  // namespace alpha

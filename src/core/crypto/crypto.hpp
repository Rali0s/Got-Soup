#pragma once

#include <string>
#include <string_view>

#include "core/model/types.hpp"

namespace alpha {

struct IdentityKeyPair {
  std::string public_key;
  std::string private_key;
  Cid cid;
};

class CryptoEngine {
public:
  Result initialize(std::string_view app_data_dir, std::string_view passphrase,
                    bool production_swap_requested);

  [[nodiscard]] bool ready() const { return ready_; }
  [[nodiscard]] bool production_mode_active() const { return production_mode_active_; }
  [[nodiscard]] const IdentityKeyPair& identity() const { return identity_; }

  [[nodiscard]] std::string derive_vault_key(std::string_view passphrase,
                                             std::string_view salt) const;
  [[nodiscard]] std::string hash_bytes(std::string_view payload) const;
  [[nodiscard]] std::string content_id(std::string_view payload) const;

  [[nodiscard]] std::string sign(std::string_view payload) const;
  [[nodiscard]] bool verify(std::string_view payload, std::string_view signature,
                            std::string_view public_key) const;

  Result export_identity_backup(std::string_view backup_path, std::string_view password,
                                std::string_view salt) const;
  Result import_identity_backup(std::string_view backup_path, std::string_view password,
                                std::string_view local_passphrase);
  Result lock_identity();
  Result unlock_identity(std::string_view passphrase);
  Result nuke_identity(std::string_view local_passphrase, bool production_swap_requested);
  [[nodiscard]] std::string vault_path() const;
  [[nodiscard]] std::int64_t last_unlocked_unix() const { return last_unlocked_unix_; }
  [[nodiscard]] std::int64_t last_locked_unix() const { return last_locked_unix_; }

  [[nodiscard]] std::string core_phase_status() const;

private:
  Result persist_identity_vault(std::string_view passphrase);
  Result unlock_from_vault(std::string_view passphrase);
  Result generate_identity(bool prefer_production_keys);

  std::string app_data_dir_;
  IdentityKeyPair identity_;
  bool ready_ = false;
  bool production_swap_requested_ = false;
  bool production_mode_active_ = false;
  std::int64_t last_unlocked_unix_ = 0;
  std::int64_t last_locked_unix_ = 0;
};

}  // namespace alpha

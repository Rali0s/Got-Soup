#include "core/crypto/crypto.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <random>
#include <sstream>
#include <unordered_map>
#include <vector>

#include "core/util/canonical.hpp"

#ifdef GOT_SOUP_HAVE_SODIUM
#include <sodium.h>
#endif

namespace alpha {
namespace {

constexpr std::string_view kVaultFileName = "identity.vault";
constexpr std::string_view kProdVaultFormat = "prod-v1";
constexpr std::string_view kBackupFormat = "got-soup-key-backup-v1";

std::string to_hex(std::string_view bytes) {
  static constexpr char kHex[] = "0123456789abcdef";
  std::string out;
  out.reserve(bytes.size() * 2);
  for (unsigned char c : bytes) {
    out.push_back(kHex[(c >> 4U) & 0x0FU]);
    out.push_back(kHex[c & 0x0FU]);
  }
  return out;
}

int from_hex_digit(char c) {
  if (c >= '0' && c <= '9') {
    return c - '0';
  }
  if (c >= 'a' && c <= 'f') {
    return c - 'a' + 10;
  }
  if (c >= 'A' && c <= 'F') {
    return c - 'A' + 10;
  }
  return -1;
}

std::string from_hex(std::string_view hex) {
  if ((hex.size() % 2U) != 0U) {
    return {};
  }

  std::string out;
  out.reserve(hex.size() / 2U);
  for (std::size_t i = 0; i < hex.size(); i += 2U) {
    const int hi = from_hex_digit(hex[i]);
    const int lo = from_hex_digit(hex[i + 1U]);
    if (hi < 0 || lo < 0) {
      return {};
    }
    out.push_back(static_cast<char>((hi << 4U) | lo));
  }
  return out;
}

std::string xor_stream(std::string_view input, std::string_view key) {
  if (key.empty()) {
    return std::string{input};
  }

  std::string out(input.size(), '\0');
  for (std::size_t i = 0; i < input.size(); ++i) {
    out[i] = static_cast<char>(input[i] ^ key[i % key.size()]);
  }
  return out;
}

std::string random_bytes_raw(std::size_t bytes) {
  std::random_device rd;
  std::mt19937_64 gen(rd());
  std::uniform_int_distribution<unsigned int> dist(0, 255);

  std::string raw;
  raw.resize(bytes);
  for (std::size_t i = 0; i < bytes; ++i) {
    raw[i] = static_cast<char>(dist(gen));
  }
  return raw;
}

std::string random_hex(std::size_t bytes) {
  return to_hex(random_bytes_raw(bytes));
}

std::string read_file(const std::filesystem::path& path) {
  std::ifstream in(path, std::ios::in | std::ios::binary);
  if (!in) {
    return {};
  }

  std::ostringstream ss;
  ss << in.rdbuf();
  return ss.str();
}

bool write_file(const std::filesystem::path& path, std::string_view data) {
  std::ofstream out(path, std::ios::out | std::ios::binary | std::ios::trunc);
  if (!out) {
    return false;
  }

  out.write(data.data(), static_cast<std::streamsize>(data.size()));
  return static_cast<bool>(out);
}

std::unordered_map<std::string, std::string> parse_key_values(std::string_view text) {
  std::unordered_map<std::string, std::string> values;

  std::istringstream in(std::string{text});
  std::string line;
  while (std::getline(in, line)) {
    const auto split = line.find('=');
    if (split == std::string::npos) {
      continue;
    }

    const std::string key = line.substr(0, split);
    const std::string value = line.substr(split + 1U);
    values[key] = value;
  }

  return values;
}

IdentityKeyPair parse_identity(std::string_view plain) {
  IdentityKeyPair key_pair;
  const auto values = parse_key_values(plain);

  if (values.contains("public_key")) {
    key_pair.public_key = values.at("public_key");
  }
  if (values.contains("private_key")) {
    key_pair.private_key = values.at("private_key");
  }
  if (values.contains("cid")) {
    key_pair.cid.value = values.at("cid");
  }

  return key_pair;
}

std::string serialize_identity(const IdentityKeyPair& key_pair) {
  std::ostringstream out;
  out << "public_key=" << key_pair.public_key << "\n";
  out << "private_key=" << key_pair.private_key << "\n";
  out << "cid=" << key_pair.cid.value << "\n";
  return out.str();
}

#ifdef GOT_SOUP_HAVE_SODIUM

std::array<unsigned char, crypto_pwhash_SALTBYTES> salt_from_string(std::string_view salt_input) {
  std::array<unsigned char, crypto_pwhash_SALTBYTES> salt{};
  crypto_generichash(salt.data(), salt.size(),
                     reinterpret_cast<const unsigned char*>(salt_input.data()),
                     static_cast<unsigned long long>(salt_input.size()), nullptr, 0);
  return salt;
}

bool derive_argon2id_key(std::string_view passphrase,
                         const std::array<unsigned char, crypto_pwhash_SALTBYTES>& salt,
                         std::array<unsigned char, crypto_secretbox_KEYBYTES>& out_key) {
  return crypto_pwhash(out_key.data(), out_key.size(), passphrase.data(),
                       static_cast<unsigned long long>(passphrase.size()), salt.data(),
                       crypto_pwhash_OPSLIMIT_MODERATE, crypto_pwhash_MEMLIMIT_MODERATE,
                       crypto_pwhash_ALG_ARGON2ID13) == 0;
}

#endif

}  // namespace

Result CryptoEngine::initialize(std::string_view app_data_dir, std::string_view passphrase,
                                bool production_swap_requested) {
  app_data_dir_ = std::string{app_data_dir};
  production_swap_requested_ = production_swap_requested;
  production_mode_active_ = false;
  ready_ = false;

  if (passphrase.empty()) {
    return Result::failure("Passphrase is required to unlock the local identity vault.");
  }

#ifdef GOT_SOUP_HAVE_SODIUM
  if (sodium_init() < 0) {
    return Result::failure("libsodium initialization failed.");
  }
#endif

  std::error_code ec;
  const std::filesystem::path root{app_data_dir_};
  std::filesystem::create_directories(root, ec);
  if (ec) {
    return Result::failure("Failed to create app data directory: " + ec.message());
  }

  const std::filesystem::path vault = root / std::string{kVaultFileName};

  if (std::filesystem::exists(vault)) {
    return unlock_from_vault(passphrase);
  }

  const Result identity_result = generate_identity(production_swap_requested_);
  if (!identity_result.ok) {
    return identity_result;
  }

  const Result persist_result = persist_identity_vault(passphrase);
  if (!persist_result.ok) {
    return persist_result;
  }

  ready_ = true;
  last_unlocked_unix_ = util::unix_timestamp_now();
  if (production_mode_active_) {
    return Result::success("Identity vault created (production swap active).", "production");
  }
  if (production_swap_requested_) {
    return Result::success("Identity vault created in compatibility mode; production swap pending.",
                           "compatibility");
  }
  return Result::success("Identity vault created.", "compatibility");
}

Result CryptoEngine::unlock_from_vault(std::string_view passphrase) {
  const std::filesystem::path vault = std::filesystem::path{app_data_dir_} / std::string{kVaultFileName};
  const std::string vault_text = read_file(vault);
  if (vault_text.empty()) {
    return Result::failure("Identity vault exists but is empty.");
  }

  const auto values = parse_key_values(vault_text);

#ifdef GOT_SOUP_HAVE_SODIUM
  if (values.contains("format") && values.at("format") == kProdVaultFormat && values.contains("salt") &&
      values.contains("nonce") && values.contains("cipher")) {
    const std::string salt_bytes = from_hex(values.at("salt"));
    const std::string nonce = from_hex(values.at("nonce"));
    const std::string cipher = from_hex(values.at("cipher"));

    if (salt_bytes.size() != crypto_pwhash_SALTBYTES || nonce.size() != crypto_secretbox_NONCEBYTES ||
        cipher.size() < crypto_secretbox_MACBYTES) {
      return Result::failure("Production identity vault format is invalid.");
    }

    std::array<unsigned char, crypto_pwhash_SALTBYTES> salt{};
    std::copy(salt_bytes.begin(), salt_bytes.end(), salt.begin());

    std::array<unsigned char, crypto_secretbox_KEYBYTES> key{};
    if (!derive_argon2id_key(passphrase, salt, key)) {
      return Result::failure("Failed to derive production vault key (Argon2id).");
    }

    std::string plain(cipher.size() - crypto_secretbox_MACBYTES, '\0');
    if (crypto_secretbox_open_easy(
            reinterpret_cast<unsigned char*>(plain.data()),
            reinterpret_cast<const unsigned char*>(cipher.data()),
            static_cast<unsigned long long>(cipher.size()),
            reinterpret_cast<const unsigned char*>(nonce.data()), key.data()) != 0) {
      return Result::failure("Identity vault could not be decrypted. Wrong passphrase or corrupt file.");
    }

    identity_ = parse_identity(plain);
    if (identity_.private_key.empty() || identity_.public_key.empty() || identity_.cid.empty()) {
      return Result::failure("Production identity vault payload could not be parsed.");
    }

    production_mode_active_ = true;
    ready_ = true;
    last_unlocked_unix_ = util::unix_timestamp_now();
    return Result::success("Identity vault unlocked (production swap active).", "production");
  }
#endif

  // Compatibility vault path.
  const std::string vault_key = derive_vault_key(passphrase, std::filesystem::path{app_data_dir_}.string());
  const std::string encrypted = from_hex(vault_text);
  if (encrypted.empty()) {
    return Result::failure("Compatibility identity vault format is invalid.");
  }
  const std::string plain = xor_stream(encrypted, vault_key);
  identity_ = parse_identity(plain);

  if (identity_.private_key.empty() || identity_.public_key.empty() || identity_.cid.empty()) {
    return Result::failure("Identity vault could not be parsed. Wrong passphrase or corrupt file.");
  }

#ifdef GOT_SOUP_HAVE_SODIUM
  if (production_swap_requested_) {
    const std::string private_key = from_hex(identity_.private_key);
    const std::string public_key = from_hex(identity_.public_key);
    production_mode_active_ =
        private_key.size() == crypto_sign_SECRETKEYBYTES &&
        public_key.size() == crypto_sign_PUBLICKEYBYTES;
  }
#endif

  ready_ = true;
  last_unlocked_unix_ = util::unix_timestamp_now();
  if (production_swap_requested_) {
    return Result::success("Identity vault unlocked in compatibility mode; production swap pending.",
                           "compatibility");
  }
  return Result::success("Identity vault unlocked.", "compatibility");
}

Result CryptoEngine::persist_identity_vault(std::string_view passphrase) {
  if (app_data_dir_.empty()) {
    return Result::failure("Identity vault persistence failed: app_data_dir is not configured.");
  }

  const std::filesystem::path root{app_data_dir_};
  std::error_code ec;
  std::filesystem::create_directories(root, ec);
  if (ec) {
    return Result::failure("Failed to create app data directory: " + ec.message());
  }

  const std::filesystem::path vault = root / std::string{kVaultFileName};
  const std::string plain = serialize_identity(identity_);

#ifdef GOT_SOUP_HAVE_SODIUM
  if (production_mode_active_) {
    std::array<unsigned char, crypto_pwhash_SALTBYTES> salt{};
    randombytes_buf(salt.data(), salt.size());

    std::array<unsigned char, crypto_secretbox_KEYBYTES> key{};
    if (!derive_argon2id_key(passphrase, salt, key)) {
      return Result::failure("Failed to derive production vault key (Argon2id).");
    }

    std::array<unsigned char, crypto_secretbox_NONCEBYTES> nonce{};
    randombytes_buf(nonce.data(), nonce.size());

    std::string cipher(plain.size() + crypto_secretbox_MACBYTES, '\0');
    crypto_secretbox_easy(reinterpret_cast<unsigned char*>(cipher.data()),
                          reinterpret_cast<const unsigned char*>(plain.data()),
                          static_cast<unsigned long long>(plain.size()), nonce.data(), key.data());

    std::ostringstream out;
    out << "format=" << kProdVaultFormat << "\n";
    out << "mode=libsodium\n";
    out << "salt=" << to_hex(std::string_view{reinterpret_cast<const char*>(salt.data()), salt.size()})
        << "\n";
    out << "nonce=" << to_hex(std::string_view{reinterpret_cast<const char*>(nonce.data()), nonce.size()})
        << "\n";
    out << "cipher=" << to_hex(cipher) << "\n";

    if (!write_file(vault, out.str())) {
      return Result::failure("Failed to write production identity vault.");
    }

    return Result::success("Identity vault persisted (production).");
  }
#endif

  const std::string vault_key = derive_vault_key(passphrase, root.string());
  const std::string encrypted = xor_stream(plain, vault_key);
  if (!write_file(vault, to_hex(encrypted))) {
    return Result::failure("Failed to write compatibility identity vault.");
  }

  return Result::success("Identity vault persisted (compatibility).");
}

Result CryptoEngine::generate_identity(bool prefer_production_keys) {
(void)prefer_production_keys;
#ifdef GOT_SOUP_HAVE_SODIUM
  if (prefer_production_keys) {
    std::array<unsigned char, crypto_sign_PUBLICKEYBYTES> public_key{};
    std::array<unsigned char, crypto_sign_SECRETKEYBYTES> private_key{};
    crypto_sign_keypair(public_key.data(), private_key.data());

    production_mode_active_ = true;
    identity_.public_key = to_hex(std::string_view{reinterpret_cast<const char*>(public_key.data()),
                                                   public_key.size()});
    identity_.private_key = to_hex(std::string_view{reinterpret_cast<const char*>(private_key.data()),
                                                    private_key.size()});
    identity_.cid.value = "cid-" + hash_bytes(identity_.public_key).substr(0, 20);
    return Result::success("Generated production identity.");
  }
#endif

  production_mode_active_ = false;
  identity_.private_key = random_hex(32);
  identity_.public_key = hash_bytes(identity_.private_key + ":public");
  identity_.cid.value = "cid-" + hash_bytes(identity_.public_key).substr(0, 20);
  return Result::success("Generated compatibility identity.");
}

Result CryptoEngine::export_identity_backup(std::string_view backup_path, std::string_view password,
                                            std::string_view salt) const {
  if (!ready_) {
    return Result::failure("Key export failed: identity is not ready.");
  }
  if (backup_path.empty()) {
    return Result::failure("Key export failed: backup path is required.");
  }
  if (password.empty()) {
    return Result::failure("Key export failed: backup password is required.");
  }
  if (salt.empty()) {
    return Result::failure("Key export failed: salt is required.");
  }

  const std::filesystem::path path{std::string{backup_path}};
  std::error_code ec;
  if (path.has_parent_path()) {
    std::filesystem::create_directories(path.parent_path(), ec);
    if (ec) {
      return Result::failure("Key export failed: unable to create backup directory: " + ec.message());
    }
  }

  const std::string key = derive_vault_key(password, "backup:" + std::string{salt});
  const std::string plain = serialize_identity(identity_);
  const std::string cipher = xor_stream(plain, key);

  std::ostringstream out;
  out << "format=" << kBackupFormat << "\n";
  out << "salt=" << salt << "\n";
  out << "cid=" << identity_.cid.value << "\n";
  out << "public_key=" << identity_.public_key << "\n";
  out << "cipher=" << to_hex(cipher) << "\n";

  if (!write_file(path, out.str())) {
    return Result::failure("Key export failed: unable to write backup file.");
  }

  return Result::success("Key export completed.", path.string());
}

Result CryptoEngine::import_identity_backup(std::string_view backup_path, std::string_view password,
                                            std::string_view local_passphrase) {
  if (backup_path.empty()) {
    return Result::failure("Key import failed: backup path is required.");
  }
  if (password.empty()) {
    return Result::failure("Key import failed: backup password is required.");
  }
  if (local_passphrase.empty()) {
    return Result::failure("Key import failed: local passphrase is required.");
  }

  const std::string file_text = read_file(std::filesystem::path{std::string{backup_path}});
  if (file_text.empty()) {
    return Result::failure("Key import failed: backup file could not be read.");
  }

  const auto values = parse_key_values(file_text);
  if (!values.contains("format") || values.at("format") != kBackupFormat) {
    return Result::failure("Key import failed: unsupported backup format.");
  }
  if (!values.contains("salt") || !values.contains("cipher")) {
    return Result::failure("Key import failed: missing salt/cipher fields.");
  }

  const std::string key = derive_vault_key(password, "backup:" + values.at("salt"));
  const std::string cipher = from_hex(values.at("cipher"));
  if (cipher.empty()) {
    return Result::failure("Key import failed: cipher payload is invalid.");
  }

  const std::string plain = xor_stream(cipher, key);
  IdentityKeyPair imported = parse_identity(plain);
  if (imported.private_key.empty() || imported.public_key.empty() || imported.cid.empty()) {
    return Result::failure("Key import failed: wrong password or corrupt backup.");
  }

  identity_ = std::move(imported);

#ifdef GOT_SOUP_HAVE_SODIUM
  if (production_swap_requested_) {
    const std::string private_key = from_hex(identity_.private_key);
    const std::string public_key = from_hex(identity_.public_key);
    production_mode_active_ =
        private_key.size() == crypto_sign_SECRETKEYBYTES &&
        public_key.size() == crypto_sign_PUBLICKEYBYTES;
  } else {
    production_mode_active_ = false;
  }
#else
  production_mode_active_ = false;
#endif

  const Result persist = persist_identity_vault(local_passphrase);
  if (!persist.ok) {
    return persist;
  }

  ready_ = true;
  last_unlocked_unix_ = util::unix_timestamp_now();
  return Result::success("Key import completed.", identity_.cid.value);
}

Result CryptoEngine::lock_identity() {
  if (!ready_) {
    return Result::success("Wallet already locked.");
  }

  ready_ = false;
  identity_.private_key.clear();
  last_locked_unix_ = util::unix_timestamp_now();
  return Result::success("Wallet locked.");
}

Result CryptoEngine::unlock_identity(std::string_view passphrase) {
  if (passphrase.empty()) {
    return Result::failure("Wallet unlock failed: passphrase is required.");
  }
  return unlock_from_vault(passphrase);
}

Result CryptoEngine::nuke_identity(std::string_view local_passphrase, bool production_swap_requested) {
  if (local_passphrase.empty()) {
    return Result::failure("Nuke key failed: local passphrase is required.");
  }

  production_swap_requested_ = production_swap_requested;
  const Result generated = generate_identity(production_swap_requested_);
  if (!generated.ok) {
    return generated;
  }

  const Result persist = persist_identity_vault(local_passphrase);
  if (!persist.ok) {
    return persist;
  }

  ready_ = true;
  last_unlocked_unix_ = util::unix_timestamp_now();
  last_locked_unix_ = 0;
  return Result::success("Identity key nuked and replaced.", identity_.cid.value);
}

std::string CryptoEngine::vault_path() const {
  if (app_data_dir_.empty()) {
    return {};
  }
  return (std::filesystem::path{app_data_dir_} / std::string{kVaultFileName}).string();
}

std::string CryptoEngine::derive_vault_key(std::string_view passphrase, std::string_view salt) const {
#ifdef GOT_SOUP_HAVE_SODIUM
  if (production_mode_active_ || production_swap_requested_) {
    const auto salt_arr = salt_from_string(salt);
    std::array<unsigned char, crypto_secretbox_KEYBYTES> key{};
    if (derive_argon2id_key(passphrase, salt_arr, key)) {
      return to_hex(std::string_view{reinterpret_cast<const char*>(key.data()), key.size()});
    }
  }
#endif

  // Compatibility derivation fallback.
  return hash_bytes(std::string{passphrase} + "::" + std::string{salt} + "::argon2id-placeholder");
}

std::string CryptoEngine::hash_bytes(std::string_view payload) const {
#ifdef GOT_SOUP_HAVE_SODIUM
  if (production_mode_active_ || production_swap_requested_) {
    std::array<unsigned char, crypto_generichash_BYTES> digest{};
    crypto_generichash(digest.data(), digest.size(), reinterpret_cast<const unsigned char*>(payload.data()),
                       static_cast<unsigned long long>(payload.size()), nullptr, 0);
    return to_hex(std::string_view{reinterpret_cast<const char*>(digest.data()), digest.size()});
  }
#endif

  std::uint64_t hash = 1469598103934665603ULL;
  for (unsigned char c : payload) {
    hash ^= static_cast<std::uint64_t>(c);
    hash *= 1099511628211ULL;
  }

  std::ostringstream out;
  out << std::hex << hash;
  return out.str();
}

std::string CryptoEngine::content_id(std::string_view payload) const {
  return "evt-" + hash_bytes(payload);
}

std::string CryptoEngine::sign(std::string_view payload) const {
  if (!ready_) {
    return {};
  }

#ifdef GOT_SOUP_HAVE_SODIUM
  if (production_mode_active_) {
    const std::string private_key = from_hex(identity_.private_key);
    if (private_key.size() != crypto_sign_SECRETKEYBYTES) {
      return {};
    }

    std::array<unsigned char, crypto_sign_BYTES> signature{};
    crypto_sign_detached(signature.data(), nullptr,
                         reinterpret_cast<const unsigned char*>(payload.data()),
                         static_cast<unsigned long long>(payload.size()),
                         reinterpret_cast<const unsigned char*>(private_key.data()));
    return to_hex(std::string_view{reinterpret_cast<const char*>(signature.data()), signature.size()});
  }
#endif

  return hash_bytes(std::string{payload} + "::" + identity_.public_key);
}

bool CryptoEngine::verify(std::string_view payload, std::string_view signature,
                          std::string_view public_key) const {
#ifdef GOT_SOUP_HAVE_SODIUM
  if (production_mode_active_) {
    const std::string sig_bytes = from_hex(signature);
    const std::string public_key_bytes = from_hex(public_key);

    if (sig_bytes.size() != crypto_sign_BYTES || public_key_bytes.size() != crypto_sign_PUBLICKEYBYTES) {
      return false;
    }

    return crypto_sign_verify_detached(
               reinterpret_cast<const unsigned char*>(sig_bytes.data()),
               reinterpret_cast<const unsigned char*>(payload.data()),
               static_cast<unsigned long long>(payload.size()),
               reinterpret_cast<const unsigned char*>(public_key_bytes.data())) == 0;
  }
#endif

  const std::string expected = hash_bytes(std::string{payload} + "::" + std::string{public_key});
  return expected == signature;
}

std::string CryptoEngine::core_phase_status() const {
  if (!ready_) {
    return "Core Phase 1 pending: wallet is locked or crypto engine not initialized.";
  }

  if (production_mode_active_) {
    return "Core Phase 1 active: Production Swap enabled (Argon2id + Ed25519/libsodium).";
  }

  if (production_swap_requested_) {
    return "Core Phase 1 active: Production Swap requested, running compatibility scaffold until all production dependencies are linked.";
  }

  return "Core Phase 1 active: compatibility scaffold mode.";
}

}  // namespace alpha

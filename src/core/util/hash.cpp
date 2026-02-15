#include "core/util/hash.hpp"

#include <array>
#include <cstdint>
#include <string_view>
#include <vector>

#ifdef GOT_SOUP_HAVE_SODIUM
#include <sodium.h>
#endif

namespace alpha::util {
namespace {

#ifdef GOT_SOUP_HAVE_SODIUM
std::string to_hex(std::string_view bytes) {
  static constexpr char kHex[] = "0123456789abcdef";
  std::string out;
  out.reserve(bytes.size() * 2U);
  for (unsigned char c : bytes) {
    out.push_back(kHex[(c >> 4U) & 0x0FU]);
    out.push_back(kHex[c & 0x0FU]);
  }
  return out;
}
#endif

std::uint32_t rotr(std::uint32_t x, std::uint32_t n) {
  return (x >> n) | (x << (32U - n));
}

std::string sha256_fallback_hex(std::string_view payload) {
  static constexpr std::array<std::uint32_t, 64> k = {
      0x428a2f98U, 0x71374491U, 0xb5c0fbcfU, 0xe9b5dba5U, 0x3956c25bU, 0x59f111f1U,
      0x923f82a4U, 0xab1c5ed5U, 0xd807aa98U, 0x12835b01U, 0x243185beU, 0x550c7dc3U,
      0x72be5d74U, 0x80deb1feU, 0x9bdc06a7U, 0xc19bf174U, 0xe49b69c1U, 0xefbe4786U,
      0x0fc19dc6U, 0x240ca1ccU, 0x2de92c6fU, 0x4a7484aaU, 0x5cb0a9dcU, 0x76f988daU,
      0x983e5152U, 0xa831c66dU, 0xb00327c8U, 0xbf597fc7U, 0xc6e00bf3U, 0xd5a79147U,
      0x06ca6351U, 0x14292967U, 0x27b70a85U, 0x2e1b2138U, 0x4d2c6dfcU, 0x53380d13U,
      0x650a7354U, 0x766a0abbU, 0x81c2c92eU, 0x92722c85U, 0xa2bfe8a1U, 0xa81a664bU,
      0xc24b8b70U, 0xc76c51a3U, 0xd192e819U, 0xd6990624U, 0xf40e3585U, 0x106aa070U,
      0x19a4c116U, 0x1e376c08U, 0x2748774cU, 0x34b0bcb5U, 0x391c0cb3U, 0x4ed8aa4aU,
      0x5b9cca4fU, 0x682e6ff3U, 0x748f82eeU, 0x78a5636fU, 0x84c87814U, 0x8cc70208U,
      0x90befffaU, 0xa4506cebU, 0xbef9a3f7U, 0xc67178f2U,
  };

  std::array<std::uint32_t, 8> h = {
      0x6a09e667U, 0xbb67ae85U, 0x3c6ef372U, 0xa54ff53aU,
      0x510e527fU, 0x9b05688cU, 0x1f83d9abU, 0x5be0cd19U,
  };

  std::vector<unsigned char> msg(payload.begin(), payload.end());
  const std::uint64_t bit_len = static_cast<std::uint64_t>(msg.size()) * 8ULL;
  msg.push_back(0x80U);
  while ((msg.size() % 64U) != 56U) {
    msg.push_back(0x00U);
  }
  for (int i = 7; i >= 0; --i) {
    msg.push_back(static_cast<unsigned char>((bit_len >> (i * 8)) & 0xFFULL));
  }

  std::array<std::uint32_t, 64> w{};
  for (std::size_t chunk = 0; chunk < msg.size(); chunk += 64U) {
    for (std::size_t i = 0; i < 16U; ++i) {
      const std::size_t offset = chunk + (i * 4U);
      w[i] = (static_cast<std::uint32_t>(msg[offset]) << 24U) |
             (static_cast<std::uint32_t>(msg[offset + 1U]) << 16U) |
             (static_cast<std::uint32_t>(msg[offset + 2U]) << 8U) |
             static_cast<std::uint32_t>(msg[offset + 3U]);
    }
    for (std::size_t i = 16U; i < 64U; ++i) {
      const std::uint32_t s0 = rotr(w[i - 15U], 7U) ^ rotr(w[i - 15U], 18U) ^ (w[i - 15U] >> 3U);
      const std::uint32_t s1 = rotr(w[i - 2U], 17U) ^ rotr(w[i - 2U], 19U) ^ (w[i - 2U] >> 10U);
      w[i] = w[i - 16U] + s0 + w[i - 7U] + s1;
    }

    std::uint32_t a = h[0];
    std::uint32_t b = h[1];
    std::uint32_t c = h[2];
    std::uint32_t d = h[3];
    std::uint32_t e = h[4];
    std::uint32_t f = h[5];
    std::uint32_t g = h[6];
    std::uint32_t hh = h[7];

    for (std::size_t i = 0; i < 64U; ++i) {
      const std::uint32_t s1 = rotr(e, 6U) ^ rotr(e, 11U) ^ rotr(e, 25U);
      const std::uint32_t ch = (e & f) ^ ((~e) & g);
      const std::uint32_t temp1 = hh + s1 + ch + k[i] + w[i];
      const std::uint32_t s0 = rotr(a, 2U) ^ rotr(a, 13U) ^ rotr(a, 22U);
      const std::uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
      const std::uint32_t temp2 = s0 + maj;

      hh = g;
      g = f;
      f = e;
      e = d + temp1;
      d = c;
      c = b;
      b = a;
      a = temp1 + temp2;
    }

    h[0] += a;
    h[1] += b;
    h[2] += c;
    h[3] += d;
    h[4] += e;
    h[5] += f;
    h[6] += g;
    h[7] += hh;
  }

  std::string out;
  out.reserve(64U);
  static constexpr char kHex[] = "0123456789abcdef";
  for (const std::uint32_t word : h) {
    for (int shift = 28; shift >= 0; shift -= 4) {
      out.push_back(kHex[(word >> static_cast<std::uint32_t>(shift)) & 0x0FU]);
    }
  }
  return out;
}

}  // namespace

std::string sha256_like_hex(std::string_view payload) {
#ifdef GOT_SOUP_HAVE_SODIUM
  std::array<unsigned char, crypto_hash_sha256_BYTES> digest{};
  crypto_hash_sha256(digest.data(), reinterpret_cast<const unsigned char*>(payload.data()),
                     static_cast<unsigned long long>(payload.size()));
  return to_hex(std::string_view{reinterpret_cast<const char*>(digest.data()), digest.size()});
#endif

  return sha256_fallback_hex(payload);
}

bool has_leading_zero_nibbles(std::string_view hex_hash, int nibbles) {
  if (nibbles <= 0) {
    return true;
  }
  if (static_cast<std::size_t>(nibbles) > hex_hash.size()) {
    return false;
  }
  for (int i = 0; i < nibbles; ++i) {
    if (hex_hash[static_cast<std::size_t>(i)] != '0') {
      return false;
    }
  }
  return true;
}

}  // namespace alpha::util

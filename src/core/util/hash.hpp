#pragma once

#include <string>
#include <string_view>

namespace alpha::util {

std::string sha256_like_hex(std::string_view payload);
bool has_leading_zero_nibbles(std::string_view hex_hash, int nibbles);

}  // namespace alpha::util

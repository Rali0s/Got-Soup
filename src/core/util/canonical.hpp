#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace alpha::util {

std::int64_t unix_timestamp_now();

std::string lowercase_copy(std::string_view value);
std::string trim_copy(std::string_view value);

std::string canonical_join(std::vector<std::pair<std::string, std::string>> fields);
std::unordered_map<std::string, std::string> parse_canonical_map(std::string_view payload);

bool contains_case_insensitive(std::string_view haystack, std::string_view needle);

}  // namespace alpha::util

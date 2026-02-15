#include "core/util/canonical.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cctype>
#include <ranges>

namespace alpha::util {

std::int64_t unix_timestamp_now() {
  const auto now = std::chrono::system_clock::now();
  return std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
}

std::string lowercase_copy(std::string_view value) {
  std::string out;
  out.reserve(value.size());
  std::ranges::transform(value, std::back_inserter(out), [](unsigned char c) {
    return static_cast<char>(std::tolower(c));
  });
  return out;
}

std::string trim_copy(std::string_view value) {
  std::size_t begin = 0;
  while (begin < value.size() && std::isspace(static_cast<unsigned char>(value[begin])) != 0) {
    ++begin;
  }

  std::size_t end = value.size();
  while (end > begin && std::isspace(static_cast<unsigned char>(value[end - 1])) != 0) {
    --end;
  }

  return std::string{value.substr(begin, end - begin)};
}

std::string canonical_join(std::vector<std::pair<std::string, std::string>> fields) {
  std::ranges::sort(fields, [](const auto& lhs, const auto& rhs) {
    return lhs.first < rhs.first;
  });

  std::string payload;
  for (const auto& [key, value] : fields) {
    payload.append(key);
    payload.push_back('=');
    for (char c : value) {
      if (c == '\n') {
        payload.append("\\n");
      } else if (c == '\\') {
        payload.append("\\\\");
      } else {
        payload.push_back(c);
      }
    }
    payload.push_back('\n');
  }

  return payload;
}

std::unordered_map<std::string, std::string> parse_canonical_map(std::string_view payload) {
  std::unordered_map<std::string, std::string> parsed;

  std::string key;
  std::string value;
  key.reserve(64);
  value.reserve(payload.size());

  bool reading_key = true;
  bool escaping = false;
  for (char c : payload) {
    if (reading_key) {
      if (c == '=') {
        reading_key = false;
        continue;
      }
      if (c == '\n') {
        key.clear();
        continue;
      }
      key.push_back(c);
      continue;
    }

    if (escaping) {
      if (c == 'n') {
        value.push_back('\n');
      } else {
        value.push_back(c);
      }
      escaping = false;
      continue;
    }

    if (c == '\\') {
      escaping = true;
      continue;
    }

    if (c == '\n') {
      if (!key.empty()) {
        parsed.emplace(key, value);
      }
      key.clear();
      value.clear();
      reading_key = true;
      continue;
    }

    value.push_back(c);
  }

  if (!reading_key && !key.empty()) {
    parsed.emplace(key, value);
  }

  return parsed;
}

bool contains_case_insensitive(std::string_view haystack, std::string_view needle) {
  if (needle.empty()) {
    return true;
  }

  const std::string h = lowercase_copy(haystack);
  const std::string n = lowercase_copy(needle);
  return h.find(n) != std::string::npos;
}

}  // namespace alpha::util

#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace refpad {

struct WikiEntry {
  std::string parent_menu;
  std::string secondary_menu;
  std::string key;
  std::string title;
  std::string body;
};

class ReferenceEngine {
public:
  ReferenceEngine();

  std::vector<std::string> search(std::string_view query) const;
  std::vector<std::string> parent_menus() const;
  std::vector<std::string> secondary_menus(std::string_view parent) const;
  std::vector<std::string> openings(std::string_view parent, std::string_view secondary,
                                    std::string_view query) const;
  std::optional<WikiEntry> lookup(std::string_view key) const;

private:
  using SecondaryMap = std::unordered_map<std::string, std::vector<std::string>>;

  std::unordered_map<std::string, WikiEntry> entries_;
  std::unordered_map<std::string, SecondaryMap> openings_by_parent_;

  static std::string normalize(std::string_view text);
  void build_hierarchy();
};

}  // namespace refpad

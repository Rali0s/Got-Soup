#include "core/reference_engine.hpp"

#include <algorithm>
#include <cctype>
#include <functional>
#include <ranges>

namespace refpad {

ReferenceEngine::ReferenceEngine()
    : entries_({
          {"getting-started",
           {"General",
            "Reference",
            "getting-started",
            "Getting Started",
            "got-soup::P2P Tomato Soup is a searchable reference and recipe coordination pad.\n\n"
            "Use the search bar to filter keys in real time.\n"
            "Select a key from the left lookup column to load this internal wiki page."}},
          {"cpp-migration",
           {"General",
            "Reference",
            "cpp-migration",
            "C++11 to Modern C++",
            "Migration targets:\n"
            "- Profile 22: Stable C++23 mode\n"
            "- Profile 24: Try C++2c preview, fallback to C++23\n\n"
            "Recommended upgrades from C++11:\n"
            "1. Replace raw ownership with smart pointers\n"
            "2. Use string_view and optional for APIs\n"
            "3. Adopt ranges-based transformations\n"
            "4. Prefer constexpr and enum class where possible"}},
          {"smallchange-reference",
           {"General",
            "Reference",
            "smallchange-reference",
            "SmallChange Reference",
            "Reference implementation link:\n"
            "https://github.com/hendrayoga/smallchange"}},
          {"build-crosscompile",
           {"General",
            "Reference",
            "build-crosscompile",
            "Cross Compile on macOS -> Windows 11",
            "Toolchain: MinGW-w64/MSYS style cross toolchain\n"
            "Compilers: x86_64-w64-mingw32-g++, -gcc, -windres\n\n"
            "Build command:\n"
            "./scripts/build-win.sh 24\n"
            "Output:\n"
            "build-win/got-soup.exe"}},
          {"ui-map",
           {"General",
            "Reference",
            "ui-map",
            "UI Map",
            "Main UI requirements implemented:\n"
            "- Top area: Search bar + Close button\n"
            "- Left column: Parent Menu + Secondary Menu + Opening list\n"
            "- Right area: Tabs for Recipes, Forum, Upload, Profile, About\n"
            "- Internal resource wiki is integrated with recipe/forum workflow\n"
            "- Menu: File > Close, Help > About / Credits"}},
          {"vegetables",
           {"vegetables",
            "overview",
            "vegetables",
            "Vegetables",
            "Nested list path:\n"
            "Vegetables > Green Vegetables > Cellery, Asaparagus, Broccoli\n\n"
            "Open child pages:\n"
            "- vegetables > green vegetables > cellery\n"
            "- vegetables > green vegetables > asaparagus\n"
            "- vegetables > green vegetables > broccoli"}},
          {"vegetables > green vegetables",
           {"vegetables",
            "overview",
            "vegetables > green vegetables",
            "Vegetables > Green Vegetables",
            "Nested list members:\n"
            "- Cellery\n"
            "- Asaparagus\n"
            "- Broccoli\n\n"
            "Use the exact keys below to open pages:\n"
            "- vegetables > green vegetables > cellery\n"
            "- vegetables > green vegetables > asaparagus\n"
            "- vegetables > green vegetables > broccoli"}},
          {"vegetables > green vegetables > cellery",
           {"vegetables",
            "green vegetables",
            "vegetables > green vegetables > cellery",
            "Cellery Page",
            "Cellery Page\n\n"
            "Path:\n"
            "Vegetables > Green Vegetables > Cellery\n\n"
            "In-depth Reference Write-up\n\n"
            "Overview\n"
            "Cellery (commonly spelled celery) is a crunchy aromatic vegetable used in soup bases, stocks,\n"
            "fresh salads, and slow-cooked savory dishes. It is valued less for sweetness and more for\n"
            "clean vegetal notes, structure, and moisture.\n\n"
            "Flavor Profile\n"
            "- Fresh, grassy, and slightly bitter\n"
            "- Aromatic oils concentrate near leaves and outer ribs\n"
            "- Mildly salty perception that helps round savory recipes\n\n"
            "Kitchen Role in Soup Systems\n"
            "- Base aromatic with onion + carrot for foundational body\n"
            "- Texture carrier in chunky soups where ribs remain intact\n"
            "- Fresh garnish when sliced thin at service time\n\n"
            "Cutting Styles\n"
            "- Fine dice: best for broth and smooth texture\n"
            "- Medium dice: balanced texture in tomato and bean soups\n"
            "- Bias slices: visual presentation and firmer bite\n"
            "- Leaf mince: high aroma finish near end of cook\n\n"
            "Heat and Timing Guidance\n"
            "- Start of cook: softens bitterness and integrates flavor\n"
            "- Mid cook: preserves some bite for rustic soup styles\n"
            "- End cook: bright, green aromatic finish\n\n"
            "Stock and Broth Guidance\n"
            "- For clear stock, avoid over-browning\n"
            "- For dark stock, lightly caramelize before adding liquid\n"
            "- Long simmer can flatten aroma, so add leaf trim late if needed\n\n"
            "Nutrition Snapshot\n"
            "- High water content\n"
            "- Low calorie density\n"
            "- Useful for volume and satiety in low-fat soup plans\n\n"
            "Selection Checklist\n"
            "- Crisp ribs with minimal bend\n"
            "- Bright leaves with no heavy yellowing\n"
            "- Clean cut base without slime\n\n"
            "Storage and Shelf Life\n"
            "- Wrap in breathable material and chill\n"
            "- Keep away from ethylene-heavy fruit\n"
            "- Pre-cut pieces lose aroma faster than whole stalks\n\n"
            "Common Problems and Fixes\n"
            "- Stringy texture: peel outer ribs or cut smaller\n"
            "- Bitter edge: sweat gently with onion first\n"
            "- Flat flavor: add fresh leaf mince near finish\n\n"
            "Recipe Integration Notes\n"
            "For tomato soup families, cellery helps stabilize savory depth while balancing acidity.\n"
            "For creamy soups, use smaller cuts to avoid fibrous mouthfeel.\n"
            "For forum posts, include cut size and cook timing so ratings remain comparable.\n\n"
            "Reference Tag\n"
            "Segment: Core Ingredients\n"
            "Use this page as a baseline before posting community variations."}},
          {"vegetables > green vegetables > asaparagus",
           {"vegetables",
            "green vegetables",
            "vegetables > green vegetables > asaparagus",
            "Asaparagus Page",
            "Asaparagus Page\n\n"
            "Path:\n"
            "Vegetables > Green Vegetables > Asaparagus"}},
          {"vegetables > green vegetables > broccoli",
           {"vegetables",
            "green vegetables",
            "vegetables > green vegetables > broccoli",
            "Broccoli Page",
            "Broccoli Page\n\n"
            "Path:\n"
            "Vegetables > Green Vegetables > Broccoli"}},
      }) {
  build_hierarchy();
}

std::string ReferenceEngine::normalize(std::string_view text) {
  std::string normalized;
  normalized.reserve(text.size());

  std::ranges::transform(text, std::back_inserter(normalized), [](unsigned char c) {
    return static_cast<char>(std::tolower(c));
  });

  return normalized;
}

void ReferenceEngine::build_hierarchy() {
  openings_by_parent_.clear();

  for (const auto& [key, entry] : entries_) {
    const std::string parent = entry.parent_menu.empty() ? "General" : entry.parent_menu;
    const std::string secondary =
        entry.secondary_menu.empty() ? "Reference" : entry.secondary_menu;

    openings_by_parent_[parent][secondary].push_back(key);
  }

  for (auto& [parent, secondary_map] : openings_by_parent_) {
    (void)parent;
    for (auto& [secondary, keys] : secondary_map) {
      (void)secondary;
      std::ranges::sort(keys, std::less<>{}, [this](const std::string& key) {
        const auto it = entries_.find(key);
        if (it == entries_.end()) {
          return key;
        }
        return it->second.title;
      });
    }
  }
}

std::vector<std::string> ReferenceEngine::search(std::string_view query) const {
  const auto normalized_query = normalize(query);

  std::vector<std::string> keys;
  keys.reserve(entries_.size());

  for (const auto& [key, entry] : entries_) {
    const auto normalized_key = normalize(key);
    const auto normalized_title = normalize(entry.title);

    if (normalized_query.empty() || normalized_key.contains(normalized_query) ||
        normalized_title.contains(normalized_query)) {
      keys.push_back(key);
    }
  }

  std::ranges::sort(keys);
  return keys;
}

std::vector<std::string> ReferenceEngine::parent_menus() const {
  std::vector<std::string> parents;
  parents.reserve(openings_by_parent_.size());

  for (const auto& [parent, secondary_map] : openings_by_parent_) {
    (void)secondary_map;
    parents.push_back(parent);
  }

  std::ranges::sort(parents, [](const std::string& left, const std::string& right) {
    if (left == "General") {
      return true;
    }
    if (right == "General") {
      return false;
    }
    return left < right;
  });

  return parents;
}

std::vector<std::string> ReferenceEngine::secondary_menus(std::string_view parent) const {
  const auto parent_it = openings_by_parent_.find(std::string{parent});
  if (parent_it == openings_by_parent_.end()) {
    return {};
  }

  std::vector<std::string> secondary;
  secondary.reserve(parent_it->second.size());
  for (const auto& [name, openings] : parent_it->second) {
    (void)openings;
    secondary.push_back(name);
  }

  std::ranges::sort(secondary);
  return secondary;
}

std::vector<std::string> ReferenceEngine::openings(std::string_view parent,
                                                   std::string_view secondary,
                                                   std::string_view query) const {
  const auto parent_it = openings_by_parent_.find(std::string{parent});
  if (parent_it == openings_by_parent_.end()) {
    return {};
  }

  const auto secondary_it = parent_it->second.find(std::string{secondary});
  if (secondary_it == parent_it->second.end()) {
    return {};
  }

  const auto normalized_query = normalize(query);
  if (normalized_query.empty()) {
    return secondary_it->second;
  }

  std::vector<std::string> filtered;
  for (const auto& key : secondary_it->second) {
    const auto it = entries_.find(key);
    if (it == entries_.end()) {
      continue;
    }

    const auto normalized_key = normalize(key);
    const auto normalized_title = normalize(it->second.title);
    if (normalized_key.contains(normalized_query) || normalized_title.contains(normalized_query)) {
      filtered.push_back(key);
    }
  }

  return filtered;
}

std::optional<WikiEntry> ReferenceEngine::lookup(std::string_view key) const {
  const auto it = entries_.find(std::string{key});
  if (it == entries_.end()) {
    return std::nullopt;
  }

  return it->second;
}

}  // namespace refpad

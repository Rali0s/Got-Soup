#pragma once

#include <cstdint>
#include <string_view>

#ifndef GOT_SOUP_APP_VERSION
#define GOT_SOUP_APP_VERSION "0.2.1"
#endif

#ifndef GOT_SOUP_BUILD_RELEASE
#define GOT_SOUP_BUILD_RELEASE "Core Phase 1 MVP"
#endif

#ifndef GOT_SOUP_AUTHOR_LIST
#define GOT_SOUP_AUTHOR_LIST "bonjin, got-soup contributors"
#endif

namespace alpha {

inline constexpr std::string_view kAppDisplayName = "SoupNet::P2P Tomato Soup::Got-Soup";
inline constexpr std::string_view kNetworkDisplayName = "SoupNet";
inline constexpr std::string_view kCurrencyMajorName = "Basil";
inline constexpr std::string_view kCurrencyMinorName = "Leafs";
inline constexpr std::string_view kCurrencyMajorSymbol = "⌘";
inline constexpr std::string_view kCurrencyMinorSymbol = "❧";
inline constexpr std::string_view kAddressPrefix = "S";
inline constexpr std::int64_t kLeafsPerBasil = 10000000000LL;
inline constexpr std::string_view kAppVersion = GOT_SOUP_APP_VERSION;
inline constexpr std::string_view kBuildRelease = GOT_SOUP_BUILD_RELEASE;
inline constexpr std::string_view kAuthorList = GOT_SOUP_AUTHOR_LIST;

}  // namespace alpha

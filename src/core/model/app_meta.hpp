#pragma once

#include <string_view>

#ifndef GOT_SOUP_APP_VERSION
#define GOT_SOUP_APP_VERSION "0.1.0"
#endif

#ifndef GOT_SOUP_BUILD_RELEASE
#define GOT_SOUP_BUILD_RELEASE "Core Phase 1 MVP"
#endif

#ifndef GOT_SOUP_AUTHOR_LIST
#define GOT_SOUP_AUTHOR_LIST "bonjin, got-soup contributors"
#endif

namespace alpha {

inline constexpr std::string_view kAppDisplayName = "got-soup::P2P Tomato Soup";
inline constexpr std::string_view kAppVersion = GOT_SOUP_APP_VERSION;
inline constexpr std::string_view kBuildRelease = GOT_SOUP_BUILD_RELEASE;
inline constexpr std::string_view kAuthorList = GOT_SOUP_AUTHOR_LIST;

}  // namespace alpha


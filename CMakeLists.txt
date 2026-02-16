cmake_minimum_required(VERSION 3.24)

project(GotSoupP2PTomatoSoup VERSION 0.2.2 LANGUAGES CXX)

if(APPLE)
  enable_language(OBJCXX)
endif()

set(ALPHA_CPP_PROFILE "24" CACHE STRING "Language profile: 22 (stable C++23) or 24 (try C++2c, fallback to C++23)")
set_property(CACHE ALPHA_CPP_PROFILE PROPERTY STRINGS 22 24)

if(NOT ALPHA_CPP_PROFILE STREQUAL "22" AND NOT ALPHA_CPP_PROFILE STREQUAL "24")
  message(FATAL_ERROR "ALPHA_CPP_PROFILE must be 22 or 24")
endif()

include(CheckCXXCompilerFlag)

set(ALPHA_CXX_FLAG "-std=gnu++23")
set(ALPHA_EFFECTIVE_PROFILE "22")

if(ALPHA_CPP_PROFILE STREQUAL "24")
  check_cxx_compiler_flag("-std=gnu++2c" ALPHA_HAS_GNU2C)
  if(ALPHA_HAS_GNU2C)
    set(ALPHA_CXX_FLAG "-std=gnu++2c")
    set(ALPHA_EFFECTIVE_PROFILE "24")
  else()
    set(ALPHA_EFFECTIVE_PROFILE "24 requested, using stable C++23 fallback")
  endif()
endif()

message(STATUS "got-soup C++ profile request: ${ALPHA_CPP_PROFILE}")
message(STATUS "got-soup effective language mode: ${ALPHA_EFFECTIVE_PROFILE} via ${ALPHA_CXX_FLAG}")

option(GOT_SOUP_ENABLE_PRODUCTION_SWAP
  "Enable production crypto swap with libsodium when available on this build host"
  ON
)

find_package(PkgConfig QUIET)
if(GOT_SOUP_ENABLE_PRODUCTION_SWAP AND NOT CMAKE_CROSSCOMPILING AND PkgConfig_FOUND)
  pkg_check_modules(SODIUM QUIET IMPORTED_TARGET libsodium)
endif()

function(alpha_apply_compile_flags target_name)
  target_compile_options(${target_name} PRIVATE
    "$<$<COMPILE_LANGUAGE:CXX>:${ALPHA_CXX_FLAG}>"
    "$<$<COMPILE_LANGUAGE:CXX>:-Wall;-Wextra;-Wpedantic>"
    "$<$<COMPILE_LANGUAGE:OBJCXX>:-std=gnu++23;-Wall;-Wextra;-Wpedantic>"
  )
endfunction()

add_library(alpha_core STATIC
  src/core/api/core_api.cpp
  src/core/crypto/crypto.cpp
  src/core/model/types.hpp
  src/core/p2p/node.cpp
  src/core/reference_engine.cpp
  src/core/service/alpha_service.cpp
  src/core/storage/store.cpp
  src/core/transport/anonymity_provider.cpp
  src/core/util/canonical.cpp
  src/core/util/hash.cpp
)
target_include_directories(alpha_core PUBLIC src)
alpha_apply_compile_flags(alpha_core)
target_compile_definitions(alpha_core PUBLIC
  GOT_SOUP_APP_VERSION=\"${PROJECT_VERSION}\"
)
if(SODIUM_FOUND)
  target_compile_definitions(alpha_core PUBLIC GOT_SOUP_HAVE_SODIUM)
  target_link_libraries(alpha_core PUBLIC PkgConfig::SODIUM)
  message(STATUS "libsodium detected: Production Swap crypto path enabled")
elseif(GOT_SOUP_ENABLE_PRODUCTION_SWAP AND CMAKE_CROSSCOMPILING)
  message(STATUS "Cross-compiling: libsodium auto-detection skipped for target; compatibility crypto path will be used unless target libsodium is wired manually")
elseif(GOT_SOUP_ENABLE_PRODUCTION_SWAP)
  message(WARNING "libsodium not detected for this target; compatibility crypto path will be used")
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
  add_executable(alpha_win WIN32
    src/app/main_win.cpp
  )
  set_target_properties(alpha_win PROPERTIES OUTPUT_NAME "got-soup")
  target_include_directories(alpha_win PRIVATE src)
  alpha_apply_compile_flags(alpha_win)
  target_compile_definitions(alpha_win PRIVATE
    UNICODE
    _UNICODE
    NOMINMAX
    WIN32_LEAN_AND_MEAN
  )
  target_link_libraries(alpha_win PRIVATE
    alpha_core
    user32
    gdi32
    comctl32
  )
elseif(APPLE)
  add_executable(alpha_macos
    src/app/main_macos.mm
  )
  set_target_properties(alpha_macos PROPERTIES OUTPUT_NAME "got-soup")
  target_include_directories(alpha_macos PRIVATE src)
  alpha_apply_compile_flags(alpha_macos)
  target_compile_options(alpha_macos PRIVATE
    "$<$<COMPILE_LANGUAGE:OBJCXX>:-fobjc-arc>"
  )
  target_link_libraries(alpha_macos PRIVATE
    alpha_core
    "-framework Cocoa"
  )
elseif(UNIX)
  add_executable(alpha_linux
    src/app/main_linux.cpp
  )
  set_target_properties(alpha_linux PROPERTIES OUTPUT_NAME "got-soup")
  target_include_directories(alpha_linux PRIVATE src)
  alpha_apply_compile_flags(alpha_linux)
  target_link_libraries(alpha_linux PRIVATE alpha_core)

  if(PkgConfig_FOUND)
    pkg_check_modules(GTK4 QUIET gtk4)
  endif()

  if(GTK4_FOUND)
    target_compile_definitions(alpha_linux PRIVATE ALPHA_USE_GTK4)
    target_include_directories(alpha_linux PRIVATE ${GTK4_INCLUDE_DIRS})
    target_link_libraries(alpha_linux PRIVATE ${GTK4_LIBRARIES})
    target_link_options(alpha_linux PRIVATE ${GTK4_LDFLAGS_OTHER})
    message(STATUS "GTK4 detected: building alpha_linux (got-soup) with native GTK UI")
  else()
    message(WARNING "GTK4 not found: alpha_linux (got-soup) will build a non-GUI fallback shell")
  endif()
else()
  message(FATAL_ERROR "Unsupported platform: ${CMAKE_SYSTEM_NAME}")
endif()

option(ALPHA_BUILD_TESTS "Build got-soup tests" ON)
if(ALPHA_BUILD_TESTS)
  enable_testing()

  add_executable(alpha_unit_tests
    tests/test_alpha_core.cpp
  )
  target_include_directories(alpha_unit_tests PRIVATE src)
  alpha_apply_compile_flags(alpha_unit_tests)
  target_link_libraries(alpha_unit_tests PRIVATE alpha_core)

  add_test(NAME alpha_unit_tests COMMAND alpha_unit_tests)
endif()

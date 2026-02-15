set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

set(TOOLCHAIN_PREFIX x86_64-w64-mingw32 CACHE STRING "MinGW toolchain prefix")

set(_MINGW_HINTS
  /opt/homebrew/bin
  /usr/local/bin
  /opt/homebrew/opt/mingw-w64/bin
  /opt/homebrew/opt/mingw-w64/toolchain-x86_64/bin
  /usr/local/opt/mingw-w64/bin
  /usr/local/opt/mingw-w64/toolchain-x86_64/bin
)

file(GLOB _MINGW_CELLAR_HINTS
  "/opt/homebrew/Cellar/mingw-w64/*/toolchain-x86_64/bin"
  "/usr/local/Cellar/mingw-w64/*/toolchain-x86_64/bin"
)
list(APPEND _MINGW_HINTS ${_MINGW_CELLAR_HINTS})

find_program(MINGW_GCC
  NAMES ${TOOLCHAIN_PREFIX}-gcc
  HINTS ${_MINGW_HINTS}
)
find_program(MINGW_GXX
  NAMES ${TOOLCHAIN_PREFIX}-g++
  HINTS ${_MINGW_HINTS}
)
if(NOT MINGW_GCC OR NOT MINGW_GXX)
  message(FATAL_ERROR
    "Required MinGW tools were not found (${TOOLCHAIN_PREFIX}-gcc/g++).\n"
    "Install with: brew install mingw-w64\n"
    "Then ensure Homebrew is in PATH, e.g.:\n"
    "  eval \"$($(command -v brew) shellenv)\""
  )
endif()

set(CMAKE_C_COMPILER ${MINGW_GCC})
set(CMAKE_CXX_COMPILER ${MINGW_GXX})

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

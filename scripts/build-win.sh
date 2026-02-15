#!/usr/bin/env bash
set -euo pipefail

profile="${1:-24}"
build_dir="${2:-build-win}"
cmake_fresh_args=()

if cmake --help 2>/dev/null | grep -q -- "--fresh"; then
  cmake_fresh_args+=(--fresh)
fi

if [[ -d /opt/homebrew/bin ]]; then
  export PATH="/opt/homebrew/bin:$PATH"
elif [[ -d /usr/local/bin ]]; then
  export PATH="/usr/local/bin:$PATH"
fi

if [[ "$profile" != "22" && "$profile" != "24" ]]; then
  echo "Usage: $0 [22|24] [build-dir]" >&2
  exit 1
fi

ensure_mingw_in_path() {
  if command -v x86_64-w64-mingw32-g++ >/dev/null 2>&1; then
    return
  fi

  local candidates=()
  shopt -s nullglob
  candidates+=(
    /opt/homebrew/opt/mingw-w64/bin
    /opt/homebrew/opt/mingw-w64/toolchain-x86_64/bin
    /usr/local/opt/mingw-w64/bin
    /usr/local/opt/mingw-w64/toolchain-x86_64/bin
    /opt/homebrew/Cellar/mingw-w64/*/toolchain-x86_64/bin
    /usr/local/Cellar/mingw-w64/*/toolchain-x86_64/bin
  )
  shopt -u nullglob

  local candidate
  for candidate in "${candidates[@]}"; do
    if [[ -x "$candidate/x86_64-w64-mingw32-g++" ]]; then
      export PATH="$candidate:$PATH"
      return
    fi
  done
}

ensure_mingw_in_path
if ! command -v x86_64-w64-mingw32-g++ >/dev/null 2>&1; then
  echo "Missing tool: x86_64-w64-mingw32-g++" >&2
  echo "Install with: brew install mingw-w64" >&2
  echo "If brew exists but isn't loaded in this shell, run:" >&2
  echo "  eval \"\$(/opt/homebrew/bin/brew shellenv)\"" >&2
  exit 1
fi

cmake "${cmake_fresh_args[@]}" -S . -B "$build_dir" -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/mingw-w64-x86_64.cmake \
  -DALPHA_CPP_PROFILE="$profile"

cmake --build "$build_dir" --target alpha_win --parallel

echo "Built Windows binary: $build_dir/got-soup.exe"

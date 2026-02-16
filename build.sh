#!/usr/bin/env bash
set -euo pipefail

profile="${1:-24}"
win_build_dir="${2:-build-win}"
mac_build_dir="${3:-build-macos}"
linux_build_dir="${4:-build-linux}"
dist_dir="${5:-dist}"
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
  echo "Usage: $0 [22|24] [win-build-dir] [mac-build-dir] [linux-build-dir] [dist-dir]" >&2
  exit 1
fi

release_version="$(sed -nE 's/^project\(.* VERSION ([0-9]+\.[0-9]+\.[0-9]+).*/\1/p' CMakeLists.txt | head -n1)"
if [[ -z "$release_version" ]]; then
  echo "Unable to detect release version from CMakeLists.txt" >&2
  exit 1
fi
echo "Building SoupNet Mainnet release v$release_version"

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

resolve_about_png() {
  local root="$1"
  if [[ -f "$root/about.png" ]]; then
    echo "$root/about.png"
    return 0
  fi

  local candidate
  shopt -s nullglob
  for candidate in $root/*.png; do
    local name
    name="$(basename "$candidate")"
    if [[ "$name" == "tomato_soup.png" ]]; then
      continue
    fi
    echo "$candidate"
    shopt -u nullglob
    return 0
  done
  shopt -u nullglob

  return 1
}

copy_assets_to_data_dir() {
  local root="$1"
  local data_dir="$2"
  local assets_dir="$data_dir/assets"
  mkdir -p "$assets_dir"

  if [[ -f "$root/tomato_soup.png" ]]; then
    cp "$root/tomato_soup.png" "$assets_dir/tomato_soup.png"
  fi

  local about_src=""
  if about_src="$(resolve_about_png "$root")"; then
    cp "$about_src" "$assets_dir/about.png"
  fi

  if [[ ! -f "$assets_dir/about.png" ]]; then
    local candidate
    shopt -s nullglob
    for candidate in $assets_dir/*.png; do
      local name
      name="$(basename "$candidate")"
      if [[ "$name" == "about.png" || "$name" == "tomato_soup.png" ]]; then
        continue
      fi
      cp "$candidate" "$assets_dir/about.png"
      break
    done
    shopt -u nullglob
  fi
}

if ! command -v cmake >/dev/null 2>&1; then
  echo "Missing tool: cmake" >&2
  exit 1
fi

if ! command -v ninja >/dev/null 2>&1; then
  echo "Missing tool: ninja" >&2
  exit 1
fi

mkdir -p "$dist_dir"

uname_s="$(uname -s)"

if [[ "$uname_s" == "Darwin" ]]; then
  ensure_mingw_in_path
  if ! command -v x86_64-w64-mingw32-g++ >/dev/null 2>&1; then
    echo "Missing tool: x86_64-w64-mingw32-g++" >&2
    echo "Install with: brew install mingw-w64" >&2
    exit 1
  fi

  echo "[1/6] Configuring Windows cross-build ($profile)"
  cmake "${cmake_fresh_args[@]}" -S . -B "$win_build_dir" -G Ninja \
    -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/mingw-w64-x86_64.cmake \
    -DALPHA_CPP_PROFILE="$profile"

  echo "[2/6] Building Windows target alpha_win"
  cmake --build "$win_build_dir" --target alpha_win --parallel

  echo "[3/6] Running core tests on host build"
  cmake "${cmake_fresh_args[@]}" -S . -B "$mac_build_dir" -G Ninja \
    -DALPHA_CPP_PROFILE="$profile"

  echo "[4/6] Building macOS target alpha_macos"
  cmake --build "$mac_build_dir" --target alpha_macos --parallel

  echo "[5/6] Building alpha_unit_tests"
  cmake --build "$mac_build_dir" --target alpha_unit_tests --parallel

  echo "[6/6] Running alpha_unit_tests"
  ctest --test-dir "$mac_build_dir" --output-on-failure

  cp "$win_build_dir/got-soup.exe" "$dist_dir/got-soup.exe"
  cp "$mac_build_dir/got-soup" "$dist_dir/got-soup-macos"
  chmod +x "$dist_dir/got-soup-macos"
  mkdir -p "$dist_dir/got-soup-data-macos" "$dist_dir/got-soup-data-win"
  copy_assets_to_data_dir "$PWD" "$dist_dir/got-soup-data-macos"
  copy_assets_to_data_dir "$PWD" "$dist_dir/got-soup-data-win"

  echo "Built outputs:"
  echo "- $dist_dir/got-soup.exe"
  echo "- $dist_dir/got-soup-macos"
  echo "Seeded assets:"
  echo "- $dist_dir/got-soup-data-macos/assets"
  echo "- $dist_dir/got-soup-data-win/assets"
  exit 0
fi

if [[ "$uname_s" == "Linux" ]]; then
  echo "[1/4] Configuring Linux build ($profile)"
  cmake "${cmake_fresh_args[@]}" -S . -B "$linux_build_dir" -G Ninja \
    -DALPHA_CPP_PROFILE="$profile"

  echo "[2/4] Building alpha_linux"
  cmake --build "$linux_build_dir" --target alpha_linux --parallel

  echo "[3/4] Building alpha_unit_tests"
  cmake --build "$linux_build_dir" --target alpha_unit_tests --parallel

  echo "[4/4] Running alpha_unit_tests"
  ctest --test-dir "$linux_build_dir" --output-on-failure

  cp "$linux_build_dir/got-soup" "$dist_dir/got-soup-linux"
  chmod +x "$dist_dir/got-soup-linux"
  mkdir -p "$dist_dir/got-soup-data-linux"
  copy_assets_to_data_dir "$PWD" "$dist_dir/got-soup-data-linux"

  echo "Built outputs:"
  echo "- $dist_dir/got-soup-linux"
  echo "Seeded assets:"
  echo "- $dist_dir/got-soup-data-linux/assets"
  exit 0
fi

echo "Unsupported host for build.sh: $uname_s" >&2
exit 1

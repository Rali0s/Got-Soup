#!/usr/bin/env bash
set -euo pipefail

profile="${1:-24}"
build_dir="${2:-build-linux}"
cmake_fresh_args=()

if cmake --help 2>/dev/null | grep -q -- "--fresh"; then
  cmake_fresh_args+=(--fresh)
fi

if [[ "$profile" != "22" && "$profile" != "24" ]]; then
  echo "Usage: $0 [22|24] [build-dir]" >&2
  exit 1
fi

cmake "${cmake_fresh_args[@]}" -S . -B "$build_dir" -G Ninja \
  -DALPHA_CPP_PROFILE="$profile"

cmake --build "$build_dir" --target alpha_linux --parallel

echo "Built Linux binary: $build_dir/got-soup"

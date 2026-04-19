#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${BUILD_DIR:-$ROOT_DIR/build}"
OUT_DIR="${OUT_DIR:-$ROOT_DIR/dist/aws-release}"

rm -rf "$OUT_DIR"
mkdir -p "$OUT_DIR/bin" "$OUT_DIR/scripts"

cmake -S "$ROOT_DIR" -B "$BUILD_DIR" -G Ninja -DALPHA_CPP_PROFILE=24
cmake --build "$BUILD_DIR" --target alpha_daemon

install -m 0755 "$BUILD_DIR/got-soupd" "$OUT_DIR/bin/got-soupd"
install -m 0755 "$ROOT_DIR/scripts/install_got_soupd.sh" "$OUT_DIR/scripts/install_got_soupd.sh"
install -m 0755 "$ROOT_DIR/scripts/genesis_hasher.py" "$OUT_DIR/scripts/genesis_hasher.py"

cat > "$OUT_DIR/README.txt" <<'EOF'
AWS got-soupd bundle

1. Copy this directory to an Ubuntu EC2 instance.
2. Run sudo ./scripts/install_got_soupd.sh
3. Read the generated token file path from the install output.
4. Add the suggested public IP:port line to peers.dat on your desktop node.
5. Query the daemon locally with:

curl -s \
  -H "Authorization: Bearer $(cat /var/lib/got-soup/daemon.token)" \
  -H "Content-Type: application/json" \
  -d '{"jsonrpc":"2.0","id":1,"method":"node.status","params":{}}' \
  http://127.0.0.1:4888/rpc
EOF

tar -C "$OUT_DIR/.." -czf "$OUT_DIR.tar.gz" "$(basename "$OUT_DIR")"
echo "Created bundle: $OUT_DIR.tar.gz"

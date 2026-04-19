#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${BUILD_DIR:-$ROOT_DIR/build}"
PREFIX="${PREFIX:-/opt/got-soup}"
USER_NAME="${GOT_SOUP_USER:-${USER:-ubuntu}}"
GROUP_NAME="${GOT_SOUP_GROUP:-$USER_NAME}"
RPC_PORT="${GOT_SOUPD_RPC_PORT:-4888}"
P2P_PORT="${GOT_SOUPD_P2P_PORT:-4001}"
BIND_HOST="${GOT_SOUPD_BIND_HOST:-127.0.0.1}"
COMMUNITY_PROFILE="${GOT_SOUPD_COMMUNITY_PROFILE:-tomato-soup}"
DATA_DIR="${GOT_SOUPD_DATA_DIR:-/var/lib/got-soup}"
TOKEN_FILE="$DATA_DIR/daemon.token"
UNIT_PATH="/etc/systemd/system/got-soupd.service"
ENV_PATH="$PREFIX/got-soupd.env"

mkdir -p "$BUILD_DIR"
cmake -S "$ROOT_DIR" -B "$BUILD_DIR" -G Ninja -DALPHA_CPP_PROFILE=24
cmake --build "$BUILD_DIR" --target alpha_daemon

install -d "$PREFIX/bin" "$PREFIX/scripts" "$DATA_DIR"
install -m 0755 "$BUILD_DIR/got-soupd" "$PREFIX/bin/got-soupd"
install -m 0755 "$ROOT_DIR/scripts/genesis_hasher.py" "$PREFIX/scripts/genesis_hasher.py"

if [[ ! -f "$TOKEN_FILE" ]]; then
  python3 - <<'PY' > "$TOKEN_FILE"
import hashlib, time
print(hashlib.sha256(f"{time.time()}|got-soupd".encode()).hexdigest()[:48])
PY
  chmod 600 "$TOKEN_FILE"
fi

cat > "$ENV_PATH" <<EOF
GOT_SOUPD_DATA_DIR=$DATA_DIR
GOT_SOUPD_BIND_HOST=$BIND_HOST
GOT_SOUPD_RPC_PORT=$RPC_PORT
GOT_SOUPD_P2P_PORT=$P2P_PORT
GOT_SOUPD_COMMUNITY_PROFILE=$COMMUNITY_PROFILE
GOT_SOUPD_TOKEN_FILE=$TOKEN_FILE
EOF

cat > "$UNIT_PATH" <<EOF
[Unit]
Description=Got Soup daemon
After=network-online.target
Wants=network-online.target

[Service]
Type=simple
User=$USER_NAME
Group=$GROUP_NAME
EnvironmentFile=$ENV_PATH
ExecStart=$PREFIX/bin/got-soupd --data-dir \${GOT_SOUPD_DATA_DIR} --bind-host \${GOT_SOUPD_BIND_HOST} --port \${GOT_SOUPD_RPC_PORT} --token-file \${GOT_SOUPD_TOKEN_FILE} --community-profile \${GOT_SOUPD_COMMUNITY_PROFILE}
Restart=on-failure
RestartSec=3
WorkingDirectory=$PREFIX
NoNewPrivileges=true
PrivateTmp=true

[Install]
WantedBy=multi-user.target
EOF

systemctl daemon-reload
systemctl enable got-soupd.service

PUBLIC_IP="$(curl -fsS https://checkip.amazonaws.com || true)"
echo "Installed got-soupd to $PREFIX"
echo "Token file: $TOKEN_FILE"
if [[ -n "$PUBLIC_IP" ]]; then
  echo "Suggested peers.dat entry: ${PUBLIC_IP}:${P2P_PORT}"
fi
echo "Start with: sudo systemctl start got-soupd"

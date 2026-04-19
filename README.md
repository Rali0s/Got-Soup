# Got-Soup

Native C++ desktop software for `SoupNet::P2P Tomato Soup`, a privacy-minded recipe forum and wallet experiment with event-sourced storage, deterministic replay, moderation controls, and a Basil/Leaf reward economy.

## Overview

Got-Soup is built around a shared `alpha_core` and thin native shells for macOS, Windows, and Linux.

Core ideas:

- event-sourced local storage with replay and backtest validation
- native desktop UI instead of a web shell
- wallet identity, signing, backup, recovery, and lock/unlock flows
- forum-style recipes, threads, replies, ratings, and moderation
- deterministic block and hash reporting
- optional headless daemon for remote and cloud-node testing

Current branding/runtime labels:

- App: `SoupNet::P2P Tomato Soup`
- Network label: `SoupNet`
- Major unit: `Basil`
- Minor unit: `Leafs`
- Address prefix: `S`

Reference inspiration:

- [smallchange](https://github.com/hendrayoga/smallchange)

## What’s In The Repo

- [src/core](/Users/premise/Documents/github/Got-Soup/src/core)
  Shared domain model, crypto, storage, transport, service orchestration, and API surface.
- [src/app](/Users/premise/Documents/github/Got-Soup/src/app)
  Native desktop entry points for macOS, Windows, and Linux.
- [src/daemon](/Users/premise/Documents/github/Got-Soup/src/daemon)
  `got-soupd`, a headless authenticated JSON-RPC daemon for lite and cloud-node operations.
- [tests](/Users/premise/Documents/github/Got-Soup/tests)
  Core integration and regression coverage.
- [scripts](/Users/premise/Documents/github/Got-Soup/scripts)
  Build helpers, genesis hashing, and AWS/systemd install tooling.

## Implemented Features

- recipe publishing and search
- forum threads and replies
- ratings and thumbs up
- reward transfers and transaction history
- wallet receive info, message signing, and address derivation
- key backup, backup verification, import, recovery, and key nuking
- moderation flag/hide flows
- startup backtest and corrupted-store recovery
- fresh genesis reset support
- mining template output for future pool/Stratum adapters
- authenticated local daemon methods for node, wallet, forum, and health operations

## Build

### Prerequisites

- `cmake` 3.24+
- `ninja`
- a C++23/C++2c-capable compiler

Optional:

- `libsodium` for the production crypto path
- `mingw-w64` for Windows cross-builds from macOS

### Configure

```bash
cmake -S . -B build -G Ninja -DALPHA_CPP_PROFILE=24
```

### Build Desktop App

```bash
cmake --build build
```

Primary outputs:

- `build/got-soup`
- `build/got-soupd`
- `build/alpha_unit_tests`

### Run Tests

```bash
ctest --test-dir build --output-on-failure
```

### Helper Scripts

- `./build.sh 24`
- `./scripts/build-linux.sh 24`
- `./scripts/build-win.sh 24`

## Run

### Desktop App

```bash
./build/got-soup
```

### Daemon

```bash
./build/got-soupd --data-dir /tmp/got-soupd --port 4888 --token-file /tmp/got-soupd/token
```

Example JSON-RPC call:

```bash
TOKEN=$(cat /tmp/got-soupd/token)
curl -s \
  -H "Authorization: Bearer $TOKEN" \
  -H "Content-Type: application/json" \
  -d '{"jsonrpc":"2.0","id":1,"method":"node.status","params":{}}' \
  http://127.0.0.1:4888/rpc
```

## Genesis And Network

Current default mainnet genesis:

- Chain ID: `got-soup-mainnet-v3`
- Merkle Root: `3841f540d53967b51a1eab6bfd50be5feb0cab2323fd7c1bda44795c400352fd`
- Block Hash: `20d14caaebca64e012c09fdacbbe2fcfb1def9fe083512591879dce30f090cbf`
- Seed peer: `24.188.147.247:4001`

Genesis helper:

```bash
python3 scripts/genesis_hasher.py --pretty
```

Custom spec example:

```bash
python3 scripts/genesis_hasher.py \
  --chain-id got-soup-mainnet-v4 \
  --network-id mainnet \
  --psz-timestamp "Custom genesis timestamp" \
  --seed-peer 1.2.3.4:4001 \
  --pretty
```

## AWS / Cloud Node

Plain binary + `systemd` deployment tooling is included for Ubuntu-style hosts.

Create a release bundle:

```bash
./scripts/aws_release_bundle.sh
```

Install on the target machine:

```bash
sudo ./scripts/install_got_soupd.sh
```

That flow:

- builds and installs `got-soupd`
- creates a token file
- writes a `systemd` service
- enables the service
- prints a suggested public peer entry for `peers.dat`

## Runtime Notes

- app/runtime data is intentionally local and mutable
- the repo ignores generated stores, backups, recovery state, build outputs, and daemon tokens
- if the fresh genesis release tag changes, existing runtime state may be quarantined and recreated on next launch

## Status

This is still an active experimental codebase. The core is farther along than the desktop polish, and the daemon/cloud-node path is intentionally simple so it can be tested and iterated quickly.

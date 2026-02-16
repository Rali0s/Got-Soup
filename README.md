# SoupNet::P2P Tomato Soup::Got-Soup

Privacy-focused, native desktop, quasi-P2P recipe forum MVP with event-sourced storage, deterministic validation, and Basil/Leaf economy.

## About

SoupNet is a C++23-first application with a shared `alpha_core` and native shells for Windows, macOS, and Linux.

- App name: `SoupNet::P2P Tomato Soup::Got-Soup`
- Network label: `SoupNet`
- Major unit: `Basil` (`1.0 ⌘`)
- Minor unit: `Leafs` (`0.0000000001 ❧`)
- Address prefix: `S`
- Hash/address style: SHA256-like addressing over CID-derived identity

Reference inspiration:

- [smallchange](https://github.com/hendrayoga/smallchange)

## Implemented Modules

- `src/core/model/`
  - Domain/event/status/community types.
- `src/core/crypto/`
  - Identity vault, sign/verify, backup/import, lock/unlock, nuke.
- `src/core/storage/`
  - Append-only event log, blockdata, deterministic materialized views, health/backtest reports.
- `src/core/transport/`
  - Tor/I2P provider interface and runtime status adapters.
- `src/core/p2p/`
  - Seed-peer + `peers.dat` handling and sync queue.
- `src/core/service/`
  - Orchestration for profile, wallet, moderation, rewards, community switching, node controls.
- `src/core/api/`
  - Public application API surface for all shells.
- `src/core/reference_engine.*`
  - Internal wiki/reference pane integration.

## Implemented UI Tabs

Shared IA across shells:

- Recipes
- Forum
- Upload
- Profile
- Rewards
- Send
- Receive
- Transactions
- Node Status
- Settings
- About / Credits

Windows and macOS have active send/sign actions. Linux shell currently exposes Send in read-only preview mode and fully exposes Receive/Transactions state views.

## Getting Started

### 1) Prerequisites

- `cmake` (3.24+ recommended)
- `ninja`
- C++23-capable compiler
- On macOS for Windows cross-build:
  - `x86_64-w64-mingw32-g++` from `mingw-w64`

Install MinGW toolchain on macOS:

```bash
brew install mingw-w64
```

If brew binaries are not in your shell:

```bash
eval "$(/opt/homebrew/bin/brew shellenv)"
```

### 2) Full build (macOS host: Windows + macOS + tests)

```bash
./build.sh 24
```

Outputs:

- `dist/got-soup.exe`
- `dist/got-soup-macos`

### 3) Windows-only cross-build

```bash
./scripts/build-win.sh 24
```

Output:

- `build-win/got-soup.exe`

### 4) Linux build

```bash
./scripts/build-linux.sh 24
```

Output:

- `build-linux/got-soup`

### 5) Run unit tests

```bash
cmake -S . -B build -G Ninja -DALPHA_CPP_PROFILE=24
cmake --build build --target alpha_unit_tests
ctest --test-dir build --output-on-failure
```

## Runtime Data and Assets

App data directories are auto-created by each shell, with an `assets/` subfolder.

Asset files used:

- `tomato_soup.png` (splash)
- `about.png` (about panel art)
- `leaf_icon.png` (currency icon)

Asset seeding logic scans project root and `Art/` for these files and copies into runtime `assets/`.

## Send / Receive / Transactions

### Send

- Send to SoupNet address (`S...`) with amount and memo.
- Amount is integer Basil units.
- Transfer burn fee is applied by policy.
- Transfer events are signed and appended to event log.

### Receive

- Shows:
  - Display name
  - CID
  - SoupNet address
  - Public key
  - Private key

### Transactions

- Event-backed history of reward transfers.
- Includes:
  - transfer ID / event ID
  - from/to addresses
  - amount
  - burned fee
  - timestamp
  - confirmations

## Message Signatures

Implemented in core API and desktop shells:

- Sign arbitrary message payloads with current local key.
- Output includes:
  - message
  - signature
  - CID/address
  - public key
- Verification API available by message + signature + public key.

## Node Status and Control

- Tor and I2P toggles
- Active transport selector
- Alpha test mode (`127.0.0.1`) toggle
- Peer management (`peers.dat`, add/reload)
- DB health and consensus/timeline hashes
- Block, finality, checkpoint, and validation limits visibility
- Community profile switching (`*.dat`)

## Community and Profile Notes

- Core topics and community posts are separated in model and UI.
- Immortal display-name flow is enforced through profile APIs.
- Duplicate-name policy is configurable.
- Key backup/import/recovery/nuke flows are implemented.

## Known Build Names

Current binary filenames remain:

- `got-soup` (macOS/Linux)
- `got-soup.exe` (Windows)

Branding/runtime labels are SoupNet-branded in-app.

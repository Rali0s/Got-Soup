# got-soup::P2P Tomato Soup

got-soup is a C++23 desktop MVP scaffold for a privacy-focused, quasi-P2P recipe forum.

## Implemented in this repository

- `alpha_core` modular library:
  - `src/core/model/` domain/event/status/community types.
  - `src/core/crypto/` identity vault + deterministic hash/signature hooks.
  - `src/core/storage/` append-only event log with materialized recipe/forum views + DB health report.
  - `src/core/transport/` anonymity provider interface + Tor/I2P providers (runtime controls + stats).
  - `src/core/p2p/` seed-peer sync queue + `peers.dat` load/save + node runtime status.
  - `src/core/service/` orchestration for node toggles, localhost alpha mode, and community profiles.
  - `src/core/api/` stable `CoreApi` surface.
- Existing `src/core/reference_engine.*` retained for internal wiki/reference charts.
- Native app targets:
  - Windows: `alpha_win` (`src/app/main_win.cpp`, pure Win32 C++) with Node Status tab.
  - macOS: `alpha_macos` (`src/app/main_macos.mm`, Cocoa Objective-C++) with Node Status tab.
  - Linux: `alpha_linux` (`src/app/main_linux.cpp`, GTK4 path when available).
- Unit tests: `alpha_unit_tests`.

## New core features

- Node Status page data model:
  - Tor/I2P runtime update stats.
  - Active transport mode.
  - P2P runtime counters.
  - DB health checks.
  - Current and known community profile metadata.
- Forum/reference synchronization:
  - Dynamic `Forum` parent menu in reference navigation.
  - Live `Recipes`, `Threads`, and `Replies` openings sourced from event-backed forum data.
  - `reference_lookup` renders forum entries as internal wiki pages.
- Tor and I2P control toggles through `CoreApi`:
  - `set_transport_enabled`
  - `set_active_transport`
- Alpha Test Mode (`127.0.0.1`) through `CoreApi::set_alpha_test_mode`.
- External peers file support:
  - `peers.dat` load/save/reload.
- Modular community profiles (`*.dat`):
  - Per-community profile, peers file, store path, and cipher-key field.
  - Create/switch community by ID or profile path.

## Core Phase 1 note

Core Phase 1 architecture is implemented in the code paths above (status/toggles/peers/community wiring). `InitConfig.production_swap` defaults to `true` and activates the libsodium-backed production path when available, with compatibility fallback when unavailable.

## Build on macOS (Windows `.exe` + macOS binary)

```bash
./build.sh 24
```

Outputs:

- `dist/got-soup.exe`
- `dist/got-soup-macos`

## Build Windows only (cross-compile)

```bash
./scripts/build-win.sh 24
```

Output:

- `build-win/got-soup.exe`

## Build Linux

```bash
./scripts/build-linux.sh 24
```

Output:

- `build-linux/got-soup`

## Run tests

```bash
cmake -S . -B build -G Ninja -DALPHA_CPP_PROFILE=24
cmake --build build --target alpha_unit_tests
ctest --test-dir build --output-on-failure
```

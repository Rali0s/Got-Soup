# Moderation Reintroduction Plan (got-soup::P2P Tomato Soup)

## 1. Moderation Model
1. Policy-level moderation per community profile (`community .dat`): defines who can curate core topics/menu.
2. Content-level moderation is event-sourced: moderation actions are signed events, not destructive deletes.
3. Visibility is deterministic after finality threshold so all peers converge.
4. No hard delete: posts remain in block/event history; moderation changes display state only.

## 2. Data Model Changes
1. Extend `EventKind` in `src/core/model/types.hpp`:
   - `ModeratorAdded`
   - `ModeratorRemoved`
   - `ContentFlagged`
   - `ContentHidden`
   - `ContentUnhidden`
   - `CoreTopicPinned`
   - `CoreTopicUnpinned`
   - `PolicyUpdated`
2. Add moderation policy structure:
   - `moderation_enabled`
   - `require_finality_for_actions`
   - `min_confirmations_for_enforcement`
   - `max_flags_before_auto_hide`
   - `role_model` (`single-signer` / `threshold`)
   - `moderator_cids` / moderator keys
3. Add materialized moderation view in `Store`:
   - `content_state[object_id]` = `visible|hidden|flagged`
   - `core_menu_state[recipe_id]` = `pinned|unpinned`
   - active moderator set at chain tip

## 3. Community `.dat` Format
Add fields in community profile parsing/writing in `src/core/service/alpha_service.cpp`:
1. `moderation_enabled=1`
2. `moderation_require_finality=1`
3. `moderation_min_confirmations=6`
4. `moderation_auto_hide_flags=3`
5. `moderators=<cid1,cid2,...>`

This keeps moderation modular per community (recipes, woodworking, etc.).

## 4. API Additions
Add to `src/core/api/core_api.hpp` (+cpp/service wiring):
1. `Result add_moderator(std::string_view cid)`
2. `Result remove_moderator(std::string_view cid)`
3. `Result flag_content(std::string_view object_id, std::string_view reason)`
4. `Result set_content_hidden(std::string_view object_id, bool hidden, std::string_view reason)`
5. `Result pin_core_topic(std::string_view recipe_id, bool pinned)`
6. `ModerationStatus moderation_status() const`

All operations become signed events via existing flow:
`make_event -> append_event -> backtest_validate`.

## 5. Validation & Consensus Rules
Implement in `Store::backtest_validate` (`src/core/storage/store.cpp`):
1. Only active moderators can emit moderation-enforcement events.
2. `ModeratorAdded/Removed` must be signed by current policy authority.
3. Moderation effects activate only when `confirmation_count >= min_confirmations_for_enforcement`.
4. Conflict resolution is deterministic:
   - higher block height first
   - then lexicographically smaller `event_id`
5. One terminal visibility state per object at a given effective height.

## 6. UI Changes (Native Shells)
Add a `Moderation` tab in:
1. `src/app/main_win.cpp`
2. `src/app/main_macos.mm`
3. `src/app/main_linux.cpp`

Tab includes:
1. Moderator list
2. Add/remove moderator controls
3. Flag queue
4. Hide/unhide actions
5. Core topic pin/unpin controls

List rendering adds badges:
1. `[HIDDEN]`
2. `[FLAGGED]`
3. `[PINNED CORE]`

Existing core/community visual distinction remains.

## 7. Delivery Phases
1. Phase A: event kinds + policy structs + `.dat` schema parse/write.
2. Phase B: Store materialization + deterministic moderation enforcement.
3. Phase C: CoreApi/AlphaService moderation methods.
4. Phase D: UI moderation tab + status badges.
5. Phase E: integration + consensus tests.

## 8. Test Matrix
1. Valid moderator action accepted.
2. Non-moderator action rejected.
3. Finality-gated enforcement works.
4. Replay/idempotency stable across peers.
5. Fork/reorg conflict resolution deterministic.
6. Core pin/unpin convergence across two peers.

## 9. Recommended MVP Defaults
1. `moderation_enabled=1`
2. `moderation_require_finality=1`
3. `moderation_min_confirmations=6`
4. `moderation_auto_hide_flags=3`
5. Initial policy authority = community profile owner CID.

This restores moderation while preserving immutable history and deterministic peer convergence.

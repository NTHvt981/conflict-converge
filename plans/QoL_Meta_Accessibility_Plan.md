# Plan: Meta & Accessibility QoL

## Implementation status: FULLY SHIPPED (corrected 2026-09-17)

- Snapshot replays (`c887ede`, 2s recording + ReplayViewer) and map editor
  (`56d57be`, WriteMapFile + painter + warnings) — shipped, as previously noted.
- **Correction**: this status block previously said hotkey remap and color-blind
  mode were NOT shipped — that was stale. Both are implemented and live:
  - Hotkey remap: a full HotkeyRemap screen in `Menu.cpp` (list UI ~1021-1075,
    Paused/Settings entry point ~1000-1075 and 2305), backed by `Hotkeys.cpp`.
  - Color-blind mode: `Art::SetColorBlindMode`/`Art::TeamTint` swap to the
    Okabe-Ito orange/sky-blue pair (`Art.cpp:476-486`), toggled via a Settings
    checkbox (`Menu.cpp:985`, `Menu.cpp:2302`) — matching Path A's design below
    essentially as written.
  - Note: the doc's claim that the rectangle fallback "ignores teamID" is
    stale too — `Game.cpp` draws it with `TeamTint`, so the `TeamTint`-level
    color-blind palette covers that path for free, no separate work needed.

Covers, from `missing_features.md` § "Meta & accessibility":
- Fully remappable hotkeys
- Color-blind mode
- Replays / spectator
- Map editor / modding support

These four are largely independent of each other and of the other five plans. Hotkey remapping and
color-blind mode are cheap and should ship quickly; replays and map editor are substantially larger
efforts and are scoped down deliberately below — read the "scope decision" in each section before
starting, since both have a much larger "ideal" version than what's recommended here.

---

## 1. Fully remappable hotkeys

### Current data model
`ShortcutRegistry` (`src/game/public/app/Shortcuts.h:11-43`,
`src/game/private/app/Shortcuts.cpp`): `unordered_map<int, KeyBindings> bindings_`, keyed by raw
raylib key code (`KEY_F1`, etc.), where `KeyBindings { Action plain; Action chord; bool hasPlain;
bool hasChord; }`. This is a **key-to-callback** map — there is no logical-action identity
independent of the key code. `PollAndFire()` (`Shortcuts.cpp:51-73`) iterates the map each frame,
checks `IsKeyPressed(pair.first)`, fires `chord` under Shift if bound, else `plain`.

All actual bindings live in `Game::BindShortcuts()` (`src/game/private/core/Game.cpp:192` through
~350), each a `[&]` lambda capturing `this`, bound against a literal `KEY_*` constant:
`KEY_F1` (hints toggle), `KEY_P` (pause), `KEY_F5`/`KEY_F9` (quicksave/quickload), `KEY_F6`/`F7`/
`F8` + their Shift chords (named save slots), `KEY_A`/`KEY_H`/`KEY_G`/`KEY_V`/`KEY_R` (gameplay
orders/stances), `KEY_ESCAPE`, `KEY_SPACE`. **Grep the live file for the complete, current list
before adding or remapping anything** — new features from the other 5 plans in this roadmap will
have added more bindings by the time you implement this.

### Key insight: the callbacks don't need to change, only what key they're bound to
Since every binding is a reusable lambda already, "remappable" doesn't require restructuring
`UpdateUnit`/order logic at all — it only requires:
1. A way to know "what logical action does key X currently perform" (doesn't exist today — the
   key code *is* the only identity).
2. A way to change that mapping and have it take effect (mechanically possible today via
   `Unbind(oldKey); Bind(newKey, sameLambda)`, but `Game::BindShortcuts()` needs restructuring to
   go through a lookup instead of literal constants).
3. Persistence across sessions.

### Design
1. **Give every binding a stable string name.** Change `Game::BindShortcuts()` from:
   ```cpp
   input.shortcuts.Bind(KEY_F1, [&] { showHints = !showHints; });
   ```
   to:
   ```cpp
   input.shortcuts.Bind(hotkeys.KeyFor("ToggleHints", KEY_F1), [&] { showHints = !showHints; });
   ```
   for every single binding, where `hotkeys.KeyFor(name, defaultKey)` returns the currently
   configured key for `name`, falling back to `defaultKey` if unconfigured. This makes the diff to
   `BindShortcuts()` itself purely mechanical (wrap every literal `KEY_*` argument), with zero
   change to any lambda body.
2. **New `HotkeyMap` class** (new file, e.g. `src/game/public/app/Hotkeys.h`/`.cpp`, fits alongside
   `Shortcuts`/`Menu` in the `app` layer):
   ```cpp
   class HotkeyMap
   {
   public:
       int KeyFor(const std::string &action, int defaultKey) const;
       void Rebind(const std::string &action, int newKey);
       // For a settings UI: what's currently bound to `action` (or the
       // default if never explicitly rebound), and reverse lookup (is
       // `key` already claimed by some other action, to warn on conflict).
       std::optional<std::string> ActionForKey(int key) const;
   private:
       std::unordered_map<std::string, int> overrides_;
   };
   ```
3. **Rebinding flow**: a new settings-UI screen (or a simple "press a key to rebind" prompt state
   in `Menu.cpp`) lists every known action name (hardcode the list of action names matching
   `BindShortcuts()`'s bindings) with its current key; selecting one and pressing a new key calls
   `hotkeys.Rebind(action, newKey)`. **After rebinding, `Game::BindShortcuts()` needs to re-run**
   (unbind everything, rebuild from `hotkeys.KeyFor(...)` again) since bindings are resolved once
   at bind-time, not looked up live on every `PollAndFire()` call — this is the one structurally
   non-trivial part: make sure `BindShortcuts()` is idempotent/re-callable (check whether
   `ShortcutRegistry` needs a `ClearAll()` before rebinding, or whether re-`Bind`ing the same key
   twice just overwrites cleanly — check `Shortcuts.cpp`'s `Bind` implementation for whether it
   silently clobbers the map entry, which is very likely, matching the `unordered_map[key]=...`
   idiom apparent from Shortcuts.h's shape) — if it does clobber cleanly, `BindShortcuts()` is
   already safe to re-call after changing `hotkeys`' overrides, just re-invoke it in full.
4. **Conflict handling**: if the player tries to rebind action A to a key already used by action B,
   decide explicitly (recommend: warn and require confirmation, then unbind B implicitly) —
   `ActionForKey` above supports detecting this before committing the rebind.

### Persistence: extend `settings.cfg`'s existing flat format
`MenuSettings` (`src/game/public/app/Menu.h:29-38`) has no hotkey fields today. `SaveSettings`/
`LoadSettings` (`src/game/private/app/Menu.cpp`) use a strictly flat `key=value` text format with
an `if (key == "cameraSpeed") ... else if (...)` parser chain — confirmed to already tolerate
unknown keys gracefully (`menu_tests.cpp` explicitly tests that `unknownKey=9` is silently ignored)
and to reject malformed values without crashing (leaves the field untouched). This means **adding
new keys is additively safe** — old settings files without hotkey lines will just fall back to
defaults, and old code reading a settings file with hotkey lines it doesn't understand (e.g. after
a rollback) won't crash either, per the existing tolerance.

Recommended line format, one per bound action: `hotkey.ToggleHints=290` (raylib int key code) —
simplest to parse/write, matches the existing flat convention exactly. A symbolic name
(`hotkey.ToggleHints=F1`) would be more human-readable in a hand-edited file but needs a
name↔`KEY_*` codec table; only bother with this if hand-editability matters to the project (check
if `settings.cfg` is expected to be manually edited by players — if there's already an in-game
remap UI per this plan, raw ints are fine since players never touch the file directly).

Extend `SaveSettings` to iterate `HotkeyMap`'s overrides and emit one `hotkey.<action>=<key>` line
per entry (only overrides, not every action — unconfigured actions stay at their code-level
default, keeping the file small). Extend `LoadSettings`'s `if/else if` chain to recognize the
`hotkey.` prefix and call `hotkeys.Rebind(actionName, key)` for each.

### Tests
New `tests/unit-tests/hotkey_tests.cpp` (`RunHotkeyTests`): `KeyFor` returns default when
unconfigured, returns override after `Rebind`, `ActionForKey` reverse lookup works, conflict
detection identifies a clash. Extend `menu_tests.cpp`'s save/load roundtrip tests with hotkey
override lines, following its existing garbage-value-rejection test pattern for a malformed
`hotkey.` line.

---

## 2. Color-blind mode

### Two independent rendering pipelines with very different retrofit cost — confirm which is live
`src/game/public/app/Art.h` / `.cpp` implements **two separate team-color mechanisms** depending on
which asset pipeline is active (`Art::UseAtlas()` vs. legacy per-team textures):

**Path A — atlas/sprite (current primary pipeline, cheap to retrofit):**
`Art::TeamTint(int teamID)` (`Art.cpp`, currently `static`, hardcoded `teamID == 0 ? BLUE : RED`)
is applied as a genuine runtime color multiply in `DrawAtlasFrame`/`DrawOneAtlasSprite` via
`DrawTexturePro(..., tint)`, per the base+mask sprite design already documented in the project's
project's architecture notes (base sprite = true colors, mask sprite = armor pixels baked pure white,
tinted per-team at draw time). **Swapping the BLUE/RED pair for a colorblind-safe pair (e.g.
orange/sky-blue, a standard Deuteranopia-safe substitute) is a pure logic change** — no new
textures needed for anything drawn through this path.

**Path B — legacy per-team baked textures (fallback pipeline, NOT cheaply tintable):**
`Art::DrawUnit`/`DrawBuilding` load fully pre-colored PNGs per team (filenames like
`units/<type>_blue_idle.png` / `units/<type>_red_idle.png`) and draw with a hardcoded `WHITE` tint
(no modulation at all) — the color is baked into the pixels, not multiplied at draw time. A
colorblind mode here needs either a third pre-baked PNG variant per unit/building×team (asset
work, not just code) or re-authoring these as tintable masks (a real code + pipeline change,
matching what Path A already does).

**Also confirmed: a third, even-more-fallback rectangle-drawing path exists** (`Game.cpp`'s
`art.UseRectangles() && !art.UseAtlas()` branch) which **ignores `teamID` entirely** for unit
bodies (always draws BLUE, or ORANGE while attacking, regardless of team) — this is a pre-existing
gap unrelated to colorblind mode (there's no team signal to make colorblind-safe here at all, since
it's not team-colored in the first place) but worth flagging since it means colorblind mode can't
"just work" uniformly across every render path — it needs to explicitly target Path A.

### Scope decision: implement for Path A only
Given the project's architecture notes describe the base+mask atlas pipeline as the intended/current
approach (not the legacy fallback), **implement colorblind mode for Path A only** in this pass.
Document Paths B/C as known-unsupported for colorblind mode (they're fallback/legacy tiers, not
the shipping rendering path) rather than trying to solve all three at once.

### Implementation
1. Make `Art::TeamTint` an instance method (or take an explicit mode parameter) instead of
   `static`, reading a new `bool colorBlindMode` field on `Art` (or wherever team-rendering config
   already lives — check `Art.h`'s existing member layout):
   ```cpp
   Color TeamTint(int teamID) const
   {
       if (colorBlindMode_)
       {
           return teamID == 0 ? Color{230, 159, 0, 255} /*orange*/
                               : Color{86, 180, 233, 255} /*sky blue*/;
       }
       return teamID == 0 ? BLUE : RED;
   }
   ```
   (Exact colorblind-safe RGB values above are the standard Okabe-Ito palette orange/sky-blue pair
   — reasonable defaults; a designer may want to tune them, keep them as named constants.)
2. Update the one call site (`Game.cpp:974`-ish: `const Color tint = Art::TeamTint(unit.teamID);`)
   to call the now-instance method (`art.TeamTint(unit.teamID)`).
3. Add a `MenuSettings` bool (`colorBlindMode = false`), wire a settings-UI toggle, persist via
   `SaveSettings`/`LoadSettings` following the exact same flat-format extension pattern as the
   hotkey settings above (`colorBlindMode=1`), and set `art.colorBlindMode_` from it at startup and
   whenever the setting changes.
4. **Preserve the existing test**: `art_tests.cpp` asserts raw BLUE/RED values from `TeamTint` —
   make sure that test explicitly constructs/uses `Art` with `colorBlindMode_ = false` (the
   default) so it keeps passing unchanged; add a new test case for `colorBlindMode_ = true`
   asserting the alternate palette.

### Tests
`art_tests.cpp`: extend as above. If a settings toggle is added, extend `menu_tests.cpp`'s
save/load roundtrip the same way as the hotkey settings.

---

## 3. Replays / spectator

### Scope decision, made explicit up front: snapshot-based, not input-recording
Two fundamentally different replay architectures exist in the genre; only one is feasible without
a much larger rewrite:
- **Input-recording / lockstep** (log player commands + a fixed RNG seed, re-simulate
  deterministically from the same commands): **not feasible today**. Confirmed: every system
  integrates on raylib's variable `GetFrameTime()` — there is no fixed-timestep accumulator
  anywhere in `Game.cpp`. Re-running the same input sequence on a different machine/framerate would
  not reproduce bit-identical results, which this approach fundamentally requires. Implementing it
  would mean migrating the entire simulation loop to a fixed tick first — a large, separate,
  higher-risk project, **out of scope for this plan**.
- **Periodic full-state snapshots** (recommended): reuses the existing save/load pipeline
  (`SaveWorld`/`LoadWorld`, `src/game/public/app/SaveGame.h:39-40`) essentially verbatim. This is
  the only approach directly compatible with the current architecture with zero determinism risk.

### Design
1. **Recording**: during a match, call `SaveWorld(...)` (the same function used for quicksave, per
   `SaveGame.h:22-40`) on a fixed wall-clock interval (e.g. every 1-2 seconds — tune based on file
   size vs. playback smoothness) into an in-memory or on-disk sequence of snapshots, rather than a
   single named slot. Reuse `SaveSlotPath`-style naming but for a numbered sequence
   (`replay_0001.sav`, `replay_0002.sav`, ...) in a dedicated replay directory, or accumulate
   snapshots into one growing file with a small index header — pick whichever is simpler to
   implement first; a directory of numbered files is the lower-effort option and matches the
   existing `SaveWorld`-per-call usage pattern most directly.
2. **Playback**: a "replay viewer" mode that loads snapshot N, renders it statically for the
   interval duration, then loads N+1 and (optionally) interpolates positions between the two
   snapshots for smoother-looking motion (since there's no continuous state between saves — decide
   whether interpolation is worth the complexity for v1; a simple "snap to each snapshot" playback
   is a reasonable first cut and avoids writing new interpolation code).
3. **Spectator (live)**: a live match could push the same periodic `SaveWorld` snapshots to a
   second `Game` instance (or a stripped-down viewer) running in parallel, effectively "replay with
   a live, growing snapshot list" — same underlying mechanism as recorded replay, just consumed as
   it's produced instead of after the fact. Treat spectator as "replay viewer pointed at a live,
   still-growing snapshot sequence" rather than a separate system.

### Known limitations to document explicitly (don't let these surprise a future implementer)
- **File size scales with unit count × snapshot count.** Every `Unit` message in
  `savegame.proto:46-74`-ish carries 15+ scalar fields plus a variable-length `path` list — a
  2-second snapshot interval in a battle with hundreds of units will produce large files quickly.
  Tune the interval based on real playtesting, and consider whether older snapshots in a long
  replay could be pruned/thinned (keep every snapshot for the first minute, every-other after,
  etc.) if size becomes a problem.
- **Production queue state is explicitly NOT saved** by the existing save format
  (`SaveGame.h:13-19`'s own header comment) — a replay reconstructed purely from snapshots will
  show units appearing "from nowhere" at each snapshot boundary rather than mid-build, and won't
  show what was queued. This is an accepted limitation of reusing the existing save format
  as-is; extending the proto schema to include production state is a separate, optional follow-up.
- **This is explicitly NOT deterministic lockstep replay** — don't market/design it as "watch the
  exact same match bit-for-bit," since it's fundamentally a slideshow of full-state snapshots, not
  a re-simulation. Set expectations accordingly in any player-facing UI copy.

### Tests
Extend `save_tests.cpp`'s existing roundtrip pattern: record a short sequence of `SaveWorld` calls
across simulated frames with unit movement in between, reload each snapshot in order, assert
positions match what was saved at each point (this is really just `SaveWorld`/`LoadWorld`'s
existing correctness, repeated N times — the new thing to test is the snapshot *sequencing*/naming
logic, not save/load itself, which is already covered).

---

## 4. Map editor / modding support

### The map format is already a plain, documented, hand-editable text format
`src/game/public/world/MapFile.h:13-24` fully documents it: a fixed 5-line header (`# comment`,
`name=`, `author=`, `width=`, `height=`), then exactly `height` rows of exactly `width` legend
characters (`.` grass, `~` water, `T` forest, `^` rock, `I`/`O` iron/oil node — must sit on grass,
`1`/`2` player/AI spawn markers — must sit on grass, `B` pre-placed building tile). Any
unrecognized character or a ragged row **rejects the whole file** — `ParseMapFile`
(`src/game/private/world/MapFile.cpp:50-134`) is strict.

### Confirmed gap: there is no writer, only a reader
`ParseMapFile` (read) exists; **there is no `WriteMapFile`/serializer at all**. An editor needs to
add one that emits this exact format (5-line header + grid), respecting the same hard caps already
enforced on read (`kMaxDim = 1024`, `kMaxTiles = 1024*1024`, `MapFile.cpp:16-17`) so
editor-produced maps never fail to load.

### Confirmed format limitation: no per-map resource tuning
`ApplyMapData` (`MapFile.cpp:136-167`) spawns every `I`/`O` marker with **hardcoded default**
amounts (`kDefaultIron=200`, `kDefaultOil=150`, `kDefaultRespawn=10`) — the text format has no way
to specify custom node amounts/respawn rates per map. If the editor is expected to support balance
tuning per map (not just terrain layout), the `.map` format itself needs extending first (e.g. an
optional `node_amount=<x>,<y>,<iron>,<respawn>` override line per node, parsed additively so
old maps without any such lines keep using the current defaults) — decide whether this is in scope
before starting; if the editor is "layout only," skip this and just use defaults like the game
already does.

### Confirmed format limitation: only two spawn categories
`playerSpawns`/`aiSpawns` are flat lists with no per-slot distinction beyond "player" vs. "AI" —
the format has no native concept of more than one human spawn set. Check `Skirmish.cpp` to see
whether multiplayer/2v2 support (if planned) needs more spawn categories than the format
currently encodes; if so, that's a format extension the editor would need to write correctly, not
purely a UI concern.

### Recommended editor scope for v1
Given the format's actual expressiveness (confirmed above): a terrain + node + spawn +
building-footprint tile painter, **not** a scenario/mission editor — there is no trigger/scripting
layer, no per-map win-condition data, and no unit placement (only building `B` tiles and spawn
points) in the current schema. Build to exactly this scope; don't imply the editor can do more than
the format supports.

1. **`WriteMapFile(const MapData&, const std::string &path) -> bool`** in `MapFile.h`/`.cpp`,
   inverse of `ParseMapFile`, emitting the documented format exactly (including respecting
   `kMaxDim`/`kMaxTiles` — return `false` rather than writing an unloadable file if exceeded).
2. **Editor UI**: a new mode (likely its own `Menu.h` state, following whatever pattern
   `MenuState` already uses for `Playing`/setup screens) with a tile-grid canvas, a legend-character
   palette to paint with, and a save button calling the new `WriteMapFile`. Reuse existing
   `TileMap`/`ResourceNodes` rendering code where possible (the game already draws a `TileMap`
   during play — an editor canvas is largely the same rendering, just with mouse-paint input
   instead of unit movement).
3. **Validation before allowing export**: `ParseMapFile` itself does not check reachability
   (whether spawns can actually reach each other/resources), only legend/dimension validity.
   `tests/unit-tests/mapfile_tests.cpp` already has a `Reachable`-style flood-fill helper used to
   validate shipped maps — reuse that exact helper in the editor's "export" flow to warn (not
   necessarily block) if the produced map has unreachable regions, matching the same quality bar
   already applied to hand-authored maps in the test suite.
4. **Discovery**: `ListMaps("data/maps")` (`MapFile.h:62-71`) already enumerates `*.map` files from
   that directory for the in-game map-select screen (`Game.cpp:407`) — the editor's save target is
   simply that same directory; no new discovery mechanism needed, new maps show up automatically
   once written there.

### Tests
Extend `mapfile_tests.cpp`: `WriteMapFile` → `ParseMapFile` round-trip (write a `MapData`, read it
back, assert equality field-by-field), and a rejection test (attempt to write dimensions exceeding
`kMaxDim`/`kMaxTiles`, assert `WriteMapFile` returns `false` and writes nothing). If per-map
resource tuning is added to the format, extend the round-trip test to cover the new optional lines
and confirm old-format maps (without them) still parse with the existing defaults unchanged.

# Plan: Color-Blind Mode

## Implementation status: SHIPPED (2026-09-17, commit `42c3556`)

- As designed: `TeamTint` is an instance method (Okabe-Ito pair, default
  off) + setter; minimap dots folded in; raw-rect fallback took the
  recommended option (a) — team base + ORANGE attack outline, which also
  fixes the pre-existing no-team-distinction gap in that tier.
- Persisted `colorBlindMode` (save/load, both checkboxes, live apply).
- Tests: `art_tests` default + colorblind palettes, `menu_tests`
  roundtrip + garbage rejection. No deviations.

The last unshipped item (of two) from `plans/QoL_Meta_Accessibility_Plan.md` §2. Re-grounded
against the current code, which turned up **two extra team-color hardcodes** beyond the one the
original plan scoped for — both cheap to fold in, one of them arguably a pre-existing bug
independent of colorblindness.

Read [QoL_Roadmap_Plan.md](QoL_Roadmap_Plan.md) before starting.

---

## Current state (verified fresh)

- `Art::TeamTint` is still `static`, unchanged:
  ```cpp
  // Art.h:118
  static Color TeamTint(int teamID);
  // Art.cpp:453-456
  Color Art::TeamTint(int teamID) { return teamID == 0 ? BLUE : RED; }
  ```
- **Exactly one real call site**: `Game.cpp:1693` — `const Color tint = Art::TeamTint(unit.teamID);`
  inside the per-unit render loop (`registry.Each<Unit>`, starting `Game.cpp:1668`), feeding both
  the atlas-sprite tint (`Game.cpp:1703`, `art.DrawAtlasFrame(sprite, corner, tint, slotScale)`)
  and a nested small-rect fallback (`Game.cpp:1713-1714`).
- **Two additional hardcoded team-color spots that bypass `TeamTint` entirely**, found during this
  research pass (not in the original plan's scope):
  1. **The raw-rectangle ultra-fallback** (`art.UseRectangles() && !art.UseAtlas()`),
     `Game.cpp:1677-1679`:
     ```cpp
     if (art.UseRectangles() && !art.UseAtlas())
     {
         DrawRectangleRec(body, unit.state == UnitState::Attacking ? ORANGE : BLUE);
     }
     ```
     This colors by **attack state**, not team — every unit of every team renders `BLUE` (or
     `ORANGE` while attacking) regardless of which side it's on. This is a real gap **independent
     of colorblindness**: in this fallback tier, you can't tell teams apart at all today, for
     anyone. Flagged explicitly below as a decision point, not silently folded into "just route it
     through `TeamTint`" (which would need to also decide how to still show attack-state, since
     `TeamTint` only knows team, not combat state).
  2. **Minimap unit dots**, `Game.cpp:1517`: `unit.teamID == 0 ? SKYBLUE : RED` — a second,
     independent BLUE/RED-family hardcode, entirely bypassing `TeamTint`. This one **is** a clean
     team-only color choice (no competing "attack state" concern), so folding it through
     `TeamTint` is a trivial one-line fix with no design decision attached.
- Settings-UI template to copy (same one `HotkeyRemap_Plan.md` uses) — `rightDragPan`'s full
  wiring: field `Menu.h:40`, save line `Menu.cpp:116`, load block `Menu.cpp:165-170`, checkbox in
  both the Settings screen (`Game.cpp:742-743`) and the Paused overlay (`Game.cpp:1905-1906`).
  (The building auto-repair panel, `Hud.cpp:173-178`, is a *different* pattern — an always-visible
  in-match HUD panel bound to plain `Game` fields, not a persisted `MenuSettings` toggle — that's
  **not** the right template here; `rightDragPan` is.)

---

## Scope: fix all three spots, but treat the raw-rect fallback's design decision separately

The original plan scoped colorblind mode to the atlas path only, reasoning that the two other
paths were legacy/fallback tiers not worth the trouble. Now that the fix is known to be a one-line
change per call site (once `TeamTint` takes an instance), there's no real cost to covering the
minimap too — do it. The raw-rectangle fallback is different: fixing it changes what "attacking"
looks like in that tier, which is a small design call, not just a palette swap. Make that call
explicitly (see §3) rather than leaving the fallback un-colorblind-safe by default while
everything else is handled.

## 1. Make `TeamTint` an instance method with a mode flag
```cpp
// Art.h
Color TeamTint(int teamID) const; // no longer static
private:
    bool colorBlindMode_ = false;
```
```cpp
// Art.cpp
Color Art::TeamTint(int teamID) const
{
    if (colorBlindMode_)
    {
        // Okabe-Ito colorblind-safe pair: orange / sky blue.
        return teamID == 0 ? Color{230, 159, 0, 255} : Color{86, 180, 233, 255};
    }
    return teamID == 0 ? BLUE : RED;
}
```
Add a setter (`void SetColorBlindMode(bool)`) and update the one real call site,
`Game.cpp:1693`, from `Art::TeamTint(unit.teamID)` (static) to `art.TeamTint(unit.teamID)`
(instance).

**Preserve the existing test**: `art_tests.cpp` asserts raw `BLUE`/`RED` from `TeamTint` — make
sure that test explicitly constructs/uses `Art` with `colorBlindMode_` at its default (`false`) so
it keeps passing unchanged, and add a new case for `colorBlindMode_ = true` asserting the
alternate palette.

## 2. Fold the minimap dots through `TeamTint`
`Game.cpp:1517`: replace `unit.teamID == 0 ? SKYBLUE : RED` with `art.TeamTint(unit.teamID)` (or a
locally-cached `tint` if one's already computed nearby in that loop — check before adding a
redundant call). Trivial, no design decision, no behavior change except under colorblind mode.

## 3. Raw-rectangle fallback — decide, then fix
`Game.cpp:1677-1679` currently encodes attack-state (not team) via `BLUE`/`ORANGE`. Options,
pick one explicitly and document the choice in the PR:
- **(a) Team color as base, attack state as a secondary cue**: `DrawRectangleRec(body,
  art.TeamTint(unit.teamID))`, then layer a separate visual for "attacking" (e.g. a brief
  brightness pulse via `Fade`, or a thin outline) so both signals survive. More correct, slightly
  more code.
  - Recommended: this fixes the pre-existing "can't tell teams apart in raw-rect mode at all"
    bug as a side effect, which is worth doing regardless of the colorblind-mode task, since it's
    a real usability gap for every player in this rendering tier, not just colorblind ones.
- **(b) Team color only, drop the attack-state distinction in this fallback tier**: simplest,
  but a step backward for anyone currently relying on the ORANGE cue in raw-rect mode (likely a
  small audience — raw-rect is the last-resort tier when actual sprite assets are missing/broken,
  not a mode anyone chooses deliberately).

Recommend (a). Either way, this fallback's fix is a real behavior change beyond "add a toggle" —
call it out explicitly in the implementation PR rather than bundling it silently into the
colorblind-mode commit's description.

## 4. Persisted setting + UI
Add `bool colorBlindMode = false;` to `MenuSettings` (`Menu.h`), following `rightDragPan`'s exact
template: one `SaveSettings` line, one `LoadSettings` block, one `GuiCheckBox` in the Settings
screen and one in the Paused overlay, both bound to `&menu.settings.colorBlindMode`. Wire
`art.SetColorBlindMode(menu.settings.colorBlindMode)` at startup (after `LoadSettings`) and
whenever the checkbox changes (immediately, so the effect is visible without needing to close and
reopen the settings screen — check how `rightDragPan`'s toggle takes effect, whether it's live or
requires re-entering `MenuState::Playing`, and match that same immediacy).

---

## Tests
- `art_tests.cpp`: extend as described in §1 (default-mode assertion preserved, new colorblind-mode
  case added).
- `menu_tests.cpp`: extend the save/load roundtrip test with `colorBlindMode` following the
  `rightDragPan` precedent exactly (including a garbage-value-rejection case).
- No test needed for §2/§3 (draw-call-only changes) beyond visual verification when the game is
  actually run with the mode toggled on, per this project's established pure-logic-vs-draw-call
  testing split.

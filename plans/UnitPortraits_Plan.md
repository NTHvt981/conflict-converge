# Plan: Unit Portraits in the Selection Panel

## Implementation status: SHIPPED (2026-09-17, commit `af51828`)

- As designed: single-selection portrait + `"%d units selected"` count
  (`SelectedUnitCount`, tested), gated so types without atlas art keep
  the text-only panel.
- Two deviations: the gate uses `Art::UnitSprite()` returning non-empty
  instead of a raw `FindSpriteByName` call (no new `Art` API needed);
  portrait scale is a fixed 1.25x tuned for the current 16x32 cells.
- Note: this plan's "Current state" prose predates the `f50a9f0`
  namespace split (`PrototypeInfantry` now resolves
  `prototypeinfantry_*`, rifle sheets wired) — the design is unaffected
  since it builds names through `UnitFile`/`UnitSprite`.
One of several standalone UI/UX polish plans (see the others alongside this one in
`plans/` — each independent, no shared prerequisites).

---

## Current state (verified fresh)

**No portraits or images of any kind in the selection panel today.**
`DrawSelectionPanel(Registry&)` (`src/game/private/app/Hud.cpp:136-176`) draws a
`GuiPanel` titled "Selection" (143) and a single `GuiLabel` (175) with plain text —
no icon/image/sprite draw call anywhere in the function.

Single-unit text: `SelectionSummary(const Unit&)` (`Hud.cpp:59-66`) —
`"%s  HP %.0f/%.0f  Team %d"`, e.g. `"Infantry  HP 100/100  Team 0"`.
Building-only selection shows `"%d %s selected"` (count + `BuildingTypeName`,
`Hud.cpp:166-173`).

**Multi-unit selection gap, relevant to portrait layout**: `SelectedUnit(Registry&)`
(`src/game/private/app/Selection.cpp:35-45`) returns only the *first* entity found
via `registry.Each<Unit>` with `isSelected == true` (iteration-order "first," not
lowest-health or most-recently-selected). `DrawSelectionPanel` renders only that
one unit's summary and never reports a total selected count. Selecting 5 units shows
the exact same single-line text as selecting 1 — no aggregate, unlike the building
path which does count. **Any portrait design must decide how to handle N>1 selected
units, since there is no existing multi-select data path to build on.**

### Sprite reuse feasibility — mechanically supported, but current art isn't portrait-quality

`Art::DrawAtlasFrame(const std::string &spriteName, Vector2 tileCorner, Color tint,
float scale = 1.0f)` (`src/game/public/app/Art.h:96-97`, impl
`src/game/private/app/Art.cpp:496-520`) already takes an arbitrary `scale`
multiplier and blits from an atlas rect, drawing base (`WHITE`) + team-tinted mask
layer if present. Calling `DrawAtlasFrame("infantry_idle_0_4", portraitPos,
teamTint, 4.0f)` to blow up an idle frame is a one-line-callable capability already
— no new engine plumbing needed.

`Facing::Bottom = 4` (`src/game/public/units/Unit.h:109-120`, comment: "0 top ... 4
bottom") is the octant facing straight down/toward the camera — the natural
front-facing idle pose for a portrait, i.e. sprite name `"<prefix>_idle_0_4"`.

**But only Infantry/PrototypeInfantry have real directional atlas frames.**
`data/configs/textures.json`/`sprites.json` define grid-sliced sprites only for
`prototype_infantry_idle`/`prototype_infantry_run` (8 directions x 16x32px cells).
`UnitFile()` (`Art.cpp:166-188`) maps both `UnitType::Infantry` and
`UnitType::PrototypeInfantry` to prefix `"infantry"` — every other type
(`AntiArmorInfantry`, `Engineer`, `IFV`, `Artillery`, `LightTank`, `HeavyTank`) has
zero atlas entries and falls through to legacy per-team single-frame textures
(`data/sprites/units/<type>_<blue|red>_idle.png`, verified 32x32px, 150-320 bytes —
essentially flat-color placeholder shapes) or the flat-rectangle fallback. Blowing
these up to portrait scale would look like a scaled-up colored blob, not a
recognizable portrait.

**Unwired asset worth flagging before starting**: `data/sprites/units/
infantry_idle_base.png`/`infantry_idle_mask.png` and `infantry_walk_base.png`/
`infantry_walk_mask.png` (256x256, newer than the other placeholder sprites) exist
on disk but are **not referenced anywhere** in `textures.json`/`sprites.json`/
`animations.json` — confirmed via grep, the only hit is a doc-comment example string
in `src/game/public/app/SpriteData.h:29`, not an actual data reference. If these are
meant to be real base+mask infantry art, they need atlas-JSON entries before `Art`
can load/use them at all. **Check with whoever owns the art pipeline before assuming
portraits can reuse them** — don't silently wire them in as a side effect of this
plan without confirming that's their intended purpose.

## Scope decision: single-selection portrait only for v1; multi-selection gets a count, not N portraits

Given no existing multi-unit summary path exists for units (unlike buildings, which
already count), building full per-unit portrait rows for an arbitrary-size selection
is a larger, separate UI layout problem. For v1:
- **Single unit selected**: show one portrait (see Design §1).
- **Multiple units selected**: extend `DrawSelectionPanel` to report a count the
  same way the building path already does (`"%d units selected"`), no portraits —
  this alone closes the existing text-only asymmetry between unit and building
  multi-select without requiring new portrait-layout design.
- A follow-up plan can add a scrollable row of small per-unit-type icons for
  multi-select once the single-portrait path is proven and real (non-placeholder)
  art exists for more than one unit type.

## Design

1. **Single-selection portrait**: in `DrawSelectionPanel`, when exactly one unit is
   selected, call `art.DrawAtlasFrame(UnitFile(unit.type) + "_idle_0_4", portraitPos,
   art.TeamTint(unit.teamID), portraitScale)` inside the panel bounds, next to the
   existing text summary. Gate this on the sprite actually existing (check
   `FindSpriteByName` returns non-null before drawing) so unit types without atlas
   art (everything except Infantry/PrototypeInfantry today) fall back to the current
   text-only panel instead of drawing a missing/blank frame.
2. **Multi-selection count**: add a `SelectedUnitCount(Registry&)` helper (mirroring
   whatever the building path already uses for its count) and switch
   `DrawSelectionPanel` to show `"%d units selected"` when count > 1, falling back
   to today's single-unit `SelectionSummary` text when count == 1.
3. Do not block this plan on new art — ship the gated single-portrait path now so it
   activates automatically (Infantry/PrototypeInfantry get portraits, everything
   else keeps today's text-only look) as more unit types gain atlas frames later,
   with zero further code changes needed per new type.

## Tests

`SelectedUnitCount` is a pure registry query — straightforward unit test (0 units
selected → 0, N selected → N, mix of selected/unselected → correct count only).
Extend `selection_tests.cpp`. The portrait draw call itself is draw-call-only per
this project's established testing split; verify manually (confirm Infantry shows a
portrait, confirm a type without atlas art still shows correct fallback text with no
missing-texture artifact).

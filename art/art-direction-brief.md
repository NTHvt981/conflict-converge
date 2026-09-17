# Art direction brief — Conflict Converge (prototype phase)

Per `create-game-assets` (project skill): inspect → lock frame → name the
system → manifest (`asset-manifest.json`) → approve target → families.

## Game frame

- Player fantasy: command RTS squads; prototype validates infantry feel.
- Core verbs: select, move, attack-move, patrol (facing matters for all).
- Engine and renderer: custom C++20 + raylib (nearest-neighbor NOT set —
  textures load with raylib's default bilinear filter; see open decision).
- Target platforms: Windows desktop.
- Camera/view/facing: top-down, zoomable; 8 facing columns (top,
  top-left, left, bottom-left, bottom, bottom-right, right, top-right).
- Native viewport and common display scale: 1200x675 window (min 800x450).
- Typical asset size on screen: 16x32 soldier cell at 2x = 32x64 px.

## Visual system (locked from the approved prototype infantry)

- Shape language: compact humanoid, readable at 16px wide; limbs posed
  per-frame, torso stable across the cycle.
- Silhouette priorities: head + gun + stride pose first; backpack/kit
  secondary. Must read against grass/water/forest at 2x.
- Value structure: mid-blue body, darker outline, lighter kit accents.
- Palette roles and exact swatches: mid-blue body sampled from
  `test_sandbox/prototype_infantry_idle.png`, team color via FULL-SPRITE
  tint (no mask sheets in prototype: BLUE/RED, Okabe-Ito pair in
  color-blind mode). Red-team readability is an open check.
- Materials and surface cues: flat cloth + hard kit edges, no gradients.
- Edge/line treatment: 1px darker outline at source resolution.
- Lighting direction and contrast: top-lit, contrast carried by outline
  rather than shading (reads at 16px).
- Detail density and focal hierarchy: face/gun > limbs > kit; no below-2px
  detail (dies at native scale).
- Motion character: 4-frame run @120ms, snappy march, no anticipation
  frames at this size; idle is a single directional pose per facing.
- Explicit exclusions: no text/labels in sheets, no baked backgrounds
  (RGBA transparency required), no scenery or multi-figure compositions
  inside unit sheets (the squad cluster is engine-composed, not baked —
  see `test_sandbox/prototype_infantry_squad.png`, which is a TARGET
  mockup, not source art).

## Technical contract

- Asset dimensions/aspect: idle 128x32 (8 cols x 1 row), run 128x128
  (8 cols x 4 rows); cells 16x32. One direction per column, frames run
  DOWN rows (walk anim = single column, 4 frames).
- Alpha/background: RGBA, transparent background (colortype 6 verified).
- Grid/tile/frame size: game tiles stay 64px; soldier cells 16x32.
- Anchor/pivot/baseline: origin (8,16) = cell center; engine aligns it to
  the tile center (see `DrawOneAtlasSprite`). Feet are NOT the pivot —
  do not author bottom-anchored sheets.
- Filtering/mipmaps/compression: UNDECIDED — raylib bilinear default is
  active; point filtering proposed for crisp 2x (one-line change in
  `Art::Init`/`LoadAtlas`, needs eye check, not yet done).
- Color space: sRGB PNG, no embedded profiles needed.
- Texture/poly/material budgets: trivial (sub-3KB sheets); no budget
  pressure. No texture packing yet (one PNG per sheet).
- Naming and folders:
- Source pipeline: AI voxel base model → user voxel edits (.vox kept
  beside the PNGs) → tools/convert_vox_to_sprite.py (8 directions,
  recentered, multiples of 16px) → test_sandbox PNGs → normalized into
  data/sprites/units/ + data/configs atlas JSON. Provenance per asset in
  asset-manifest.json. source `test_sandbox/<type>_{idle,run}.png` →
  runtime `data/sprites/units/<type>_{idle,run}.png`; atlas entries in
  `data/configs/{textures,sprites,animations}.json`; animation names
  `<prefix>_walk_<0-7>`, idle cells `<prefix>_idle_0_<0-7>`.

## Visual target

- Approved seed/reference paths:
  `test_sandbox/prototype_infantry_idle.png`,
  `test_sandbox/prototype_infantry_run.png` (wired as the infantry
  atlas; `prototype_infantry_squad.png` is the cluster target mockup).
- Required do/don't examples: DO match stride phase across the 4 rows
  of a column (row 0 = the wired walk cycle); DON'T reorder columns
  (engine maps column index to compass facing, no remap table).
- Native-scale gameplay capture: pending (user screenshots from the
  prototype level; border/bar fixes landed after the last capture).
- Approval owner/date: user, 2026-09-17 (prototype infantry only; the
  other 7 types are planned, not approved).

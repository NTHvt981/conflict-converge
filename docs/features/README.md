# Feature Docs — Index

As-built behavior descriptions, one per shipped feature. Each doc names
its key files, owning test suites, and the decisions that shaped it
(including what was deliberately left out).

## Selection & orders

- [selection-and-control](selection-and-control.md) — click/drag-box,
  control groups, double-click select-type, line-draw formations,
  slowest-speed toggle.
- [orders-and-queueing](orders-and-queueing.md) — shift-queued chains,
  attack-ground, patrol semantics, repair, area-build/repair. Reclaim
  deferred (no wreck system).
- [unit-automation](unit-automation.md) — overkill protection, target
  priority, auto-retreat, building auto-repair toggle. AI auto-repair
  an explicit non-goal.

## Economy & production

- [production-and-economy](production-and-economy.md) — repeat queues,
  idle finders, select-all hotkeys. Per-unit build hotkeys absent.

## Camera, minimap & info

- [camera-minimap-info](camera-minimap-info.md) — pings + jump,
  right-drag pan, selection range rings, viewport. Hover/placement
  rings absent.
- [screen-shake](screen-shake.md) — trauma-pattern shake on a render
  copy.

## Units & combat presentation

- [facing](facing.md) — 8-way facing + directional anims.
- [context-cursors](context-cursors.md) — per-frame intent cursor.
- [floating-damage-numbers](floating-damage-numbers.md) — pooled hit
  numbers from the sim edge trigger.
- [building-construction](building-construction.md) — sites ramp to
  operational over per-type times.
- [unit-portraits](unit-portraits.md) — single-select portrait,
  multi-select count.

## Shell, menus & content

- [accessibility](accessibility.md) — remappable Tier-1 hotkeys,
  color-blind palette, UI scale, remap-aware hints. Tier-2 inputs raw
  by design.
- [accessibility-scaling](accessibility-scaling.md) — text-scale setting
  (Phase 1). Raw `DrawText` sites + high-contrast open.
- [menu-transitions](menu-transitions.md) — fade-through-black clock.
- [hover-tooltips](hover-tooltips.md) — HUD/settings tips + debounced
  unit tooltip, fog-gated.
- [hint-overlay](hint-overlay.md) — live F1 overlay from the remap
  table.
- [map-editor](map-editor.md) — in-menu tile editor behind raygui
  chrome, custom canvas.
- [widget-toolkit](widget-toolkit.md) — raygui inventory reference
  (nothing to build; check here before hand-rolling a control).

## Architecture & systems

- [game-update-decomposition](game-update-decomposition.md) —
  `Simulation` / `PlayingInput` / `MenuScreens` split.
- [order-flags-retreat-bugfix](order-flags-retreat-bugfix.md) — stale
  order flags + centroid retreat home.
- [serialization](serialization.md) — cereal saves (`CCB2`), JSON
  atlas/configs. Old `CCSV`/`CCPB` saves rejected.
- [planned/grid-movement](planned/grid-movement.md) — multi-tile grid
  design: mostly superseded, arbitrary masks open.

## History

- [history/milestones](../history/milestones.md) — M1–M16 build record.
- [history/decisions](../history/decisions.md) — planning Q&A decision log.

## Standing rules (learned building all of the above)

- Sim-side changes (economy/combat timing) each get an isolated
  full-suite run — the Medium-vs-Hard soak margin is ~2% and one
  attempted change flipped it and was reverted.
- Pure logic stays unit-testable without a window; raylib draw calls
  live in render sections or thin wrappers and are verified visually.
- Save-format enums append-only (decode validates `< Count`); the
  format itself is versioned (`CCB2`, v2).

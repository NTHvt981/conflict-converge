# Accessibility Scaling

Shipped: Phase 1 only, 2026-09-17 (commit `ff8ea6a`). Phase 2 and
high-contrast mode are open (see below).

Persisted UI text scale applied live to all raygui chrome.

## Behavior

- `MenuSettings.uiScale` (default 1.0, clamped 0.75–2.0, garbage
  rejected) persisted in `settings.cfg`.
- Applied via `GuiSetStyle(TEXT_SIZE, ...)` at startup and live from the
  Settings slider.

## Key files

- `src/game/public/app/Menu.h` + `private/app/Menu.cpp` — setting,
  persist, clamp.
- `src/game/private/core/MenuScreens.cpp` — slider.
- Tests: `Menu` suite (round-trip, garbage rejection, clamps).

## Decisions

- No deviations from the Phase 1 design.

## Open

- Phase 2: ~20 raw `DrawText` sites are still hardcoded and ignore
  `TEXT_SIZE`.
- High-contrast mode: no chrome/color sweep exists.

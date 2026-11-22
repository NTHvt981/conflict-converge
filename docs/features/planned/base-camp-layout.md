# Base camp layout in data (planned)

Status: proposed. The two camp layouts are hand-written offset lists today.

## Where the lists live

| Owner | File | Entries per type |
| --- | --- | --- |
| Player camp | `src/game/private/app/match/Skirmish.cpp:213`, `:228`, `:243` | 3–4 spots |
| AI camp | `src/game/private/units/AICommanderEconomy.cpp:383`, `:392`, `:403` | 4–8 spots |

Both read offsets from the tile where the base landed. Each type tries its
spots in order and falls back to a ring search, so the lists are hints.

## Option A: one code helper

`BaseCampSpots(BuildingType)` returns the offset list. Both call sites use it.
The AI takes the longer list. No IO, no fallback path, no validation.

## Option B: data file

`data/configs/base_layout.json` parsed like `ai.json` through `AIConfig.h`
(`ParseAIDifficultyJson`, `ActiveAIDifficultyParams`,
`RefreshAIDifficultyParamsFromSearch`), with a compiled default.

```json
{
  "baseLayout": {
    "depot": [[3, 0], [0, 3], [-1, 0], [0, -2]],
    "bootcamp": [[0, 3], [3, 0], [-2, 3], [0, -3]],
    "workshop": [[3, 3], [-3, 3], [0, 5], [-4, 0]]
  }
}
```

## Trade-offs

- Data removes a rebuild for tuning, and gives maps or mods a layout knob.
- Data adds validation: each offset must clear the base footprint, fit the
  map and not repeat. `ai.json` carries only scalars today.
- The lists are hints with a ring fallback, so a bad offset degrades the camp
  and does not stop the match.
- Neither option settles whether the player and AI share one list. Option A
  makes that decision explicit.

## Order

1. Option A, which removes the duplication.
2. Option B over the single list, if tuning without a rebuild matters.

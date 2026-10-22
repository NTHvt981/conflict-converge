# Damage Model vs EA Warheads (planned)

Status: one open item. EA reference is the archived
`CnC_Remastered_Collection/TIBERIANDAWN` tree under `ea-ref-prjs/`.

## Remaining open item

- `TECHNO.CPP:4683` — the AI values weapons per target class as
  `Attack x Modifier[armor] x Range / ROF`. Ours (`TargetPriorityWeight`)
  is a flat vehicle bonus; route it through `Effectiveness` so shooters
  prefer targets their damage type actually hurts, gated on a green
  difficulty-ladder soak.

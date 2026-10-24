# ASCII diagrams

Plain-text diagrams explain a flow, a nesting, or a set of records in issues, PR bodies,
`docs/`, and review comments. They survive copy-paste and diffs, and they make the author
name the concrete pieces involved.

## Symbols

```
symbol       use
-----------  -------------------------------------------------
─ │ ┌ ┐ └ ┘  box edges and corners, one box per actor or layer
├ └ │        tree branches, one level per nesting
→ ▼ ◀ ▲      direction of flow, labelled with what crosses it
·            a step inside a box
[x] [ ]      done / not done
```

Box edges and tree branches carry the structure; arrows carry the direction. Add a symbol
to this table before using it.

## Size

- at most 70 columns per line; wider lines wrap in chat panes and break the edges
- one box width per diagram: pad every box to the same width so the right edges line up
- one space of padding inside each edge: `│ text │`
- one column per table field, with the dash row matching each column width
- about 25 lines per diagram at most; split a longer one and caption each part
- monospace only: no tabs, no proportional-font glyphs, no emoji

## Rules

- fence with a bare triple backtick, no language tag, so nothing re-renders the glyphs
- put a legend under any symbol that is not a box edge
- annotate outside the diagram, in one aligned column, with real values
- show before and after as two diagrams plus one line naming the difference
- mark an estimate as an estimate, never as a measured value
- no trailing spaces; LF endings

## Examples

### Boxes and arrows

```
┌─ client ────────────────┐
│ POST /jobs              │
│ body: {name: "build"}   │
└─────────────────────────┘
             │
             ▼
┌─ queue ─────────────────┐
│ jobs.build              │
└─────────────────────────┘
             │
             ▼
┌─ worker ────────────────┐
│ run(name) -> status     │
└─────────────────────────┘
```

Each box is one actor or layer, named in its top border. Every arrow is labelled with what
crosses it.

### Tree

```
main
├─ parse()
│  ├─ tokenize()
│  └─ build_tree()
└─ run()
   └─ write()
```

One level per nesting step. The last child of a level uses `└─`, every other child uses
`├─`, and the branch column stays under its parent.

### Table

```
field   type  null  default
------  ----  ----  -------
id      uuid  no
name    text  no    'build'
status  text  yes   null
```

Column widths follow the widest cell, and the dash row matches the header.

## Checklist

- [ ] every line fits in 70 columns
- [ ] boxes share one width, and arrows sit on the centre column
- [ ] every non-edge symbol appears in a legend
- [ ] annotations sit in one column and use real values
- [ ] a before/after difference is stated in one line of prose
- [ ] the block uses a bare fence and renders in monospace

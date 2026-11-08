# Add a map

## Goal

Ship a new skirmish map: a text `.map` file under `data/maps/` that
loads through the shipped-map sanity checks and appears in the
skirmish setup list (also editable in the in-menu map editor). Layout
rules come from the same playability checks the tests enforce, so a
map that fails them fails a test, not a match in someone's hands.

## Prerequisites

A Debug build that compiles and passes the test suite.

## Steps

1. **Author the file.** Create `data/maps/<name>.map` against the
   format spec below.

2. **Register it in the shipped sanity loop.** In
   `tests/unit-tests/mapfile_tests.cpp`, add the filename to the
   `shipped[]` list inside `RunMapFileTests`. The loop then parses it
   and enforces the standard 1v1 profile — dimensions `24x18`, exactly
   one `1` and one `2` marker, at least three nodes, every node on
   open reachable ground, bases connected, and terrain point- mirrored
   across the map center — so build the layout to fit or extend the
   loop deliberately. Wider formats (2v2) keep their own named block.

3. **Ship.** The skirmish setup screen and the map editor list
   whatever `data/maps/` contains; nothing is registered twice. The
   editor saves with the same name guard the loader accepts.

## Reference

The file is text with a five-line header, then exactly `height` rows
of exactly `width` legend characters, one per tile:

```
# Conflict Converge map v1      <- first line, comment, exactly this
name=<display name>
author=<byline>
width=<W>
height=<H>
<W><W>...<H rows>               <- one legend char per tile
```

Legend:

| Char | Meaning |
|---|---|
| `.` | grass, pathable |
| `~` | water, blocks |
| `T` | forest, slow, passable |
| `^` | rock, blocks |
| `B` | building tile, blocks |
| `I` | iron resource node, standing on grass |
| `O` | oil resource node, standing on grass |
| `1` | player spawn (repeatable for more player bases) |
| `2` | AI spawn (repeatable; 2v2 maps carry two each) |

Marker tiles (`I`, `O`, `1`, `2`) stand on grass; every other
character selects terrain directly.

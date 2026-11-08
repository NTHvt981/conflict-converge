# Extend the save format

## Goal

Persist new state in saved games without breaking existing saves. The
format is cereal-JSON with tolerant field loads: appended fields with
member defaults make old saves load unchanged, and the only forbidden
moves are renaming a field, removing one, or reinterpreting one.

## Prerequisites

A Debug build that compiles and passes the test suite.

## Steps

1. **Pick the wire struct.** `src/game/public/app/save/SaveWire.h`
   defines one struct per saved object (`SaveUnit`, `SaveBuilding`,
   …) plus the root `SaveGameData`. Add the field to the struct that
   mirrors what you are saving — a unit field to `SaveUnit`, world data
   to `SaveGameData`.

2. **Write it tolerantly.** Append the field with a member default,
   then extend both archive templates:

   ```cpp
   // save():
   cereal::make_nvp("<name>", <field>)
   // load():
   save_detail::TryLoadValue(ar, "<name>", <field>);
   ```

   The NVP **name is the on-disk contract** — never rename one, and
   use the same name in both templates. `TryLoadValue` is what makes
   saves written before the field existed still load: absent means
   default, present-but-malformed still fails the load loudly.

3. **New enums validate at decode.** Save the enum as its `int`, then
   range-check it on decode (`src/game/private/app/save/SaveDecode.cpp`
   rejects values outside `0..< Count>`). Appending to an enum keeps
   every existing value's meaning, which is the only compatible move;
   count tripwire `static_assert`s sit inside `SaveDecode.cpp` and
   must land in the same commit, raised deliberately once the append
   order is confirmed.

4. **Bump the version only for meaning changes.** Additive fields keep
   `kSaveVersion` untouched, because tolerant loads make the version
   question irrelevant to them. Raise `kSaveVersion`, and
   `kMinSupportedVersion` with it, when existing fields are
   reinterpreted or dropped — the load then refuses every older file
   rather than inventing semantics for it.

5. **Prove both directions.** Run the `SaveGame` suite: its roundtrip
   and old-save cases cover a fresh field and a file missing the
   field. New well-formed values get roundtrip-checked inside the
   suite, not on paper.

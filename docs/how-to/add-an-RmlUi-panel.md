# Add an RmlUi menu dialog

## Goal

Ship a new menu-branch document: an `data/ui/*.rml` screen owned by the
RML shell, shown for one `MenuState`, with wired clicks and populated
values. The raygui `MenuScreens` fallback already renders every state,
so a document that fails to load degrades instead of breaking boot.

HUD-side documents (in-match panels) are a different owner and a
different recipe; this one covers the shell and its dialogs. Mapping
keys stay with the game (the shell deliberately never handles Escape).

## Prerequisites

A Debug build that compiles and passes the test suite.

## Steps

1. **Author the document.** Create `data/ui/<name>.rml`. Style uses the
   existing classes from `data/ui/theme.rcss` and `hud.rcss`; add a
   class only when nothing composes. Repositor indent rule: one tab
   per nesting level, single-text elements on one line, `<style>`
   opening at two tabs with its body plain and the closing tag at
   column zero.

2. **Name every interactive element.** Add each id to
   `src/game/public/app/ui/UiIds.h` as a `string_view` constant. The
   refresh path and the click/change handlers share these constants,
   so a wrong id breaks the build rather than no-opping at runtime.

3. **Load and own it.** In `src/game/private/app/ui/RmlUiMenus.cpp`,
   `Init` loads each document with `Load("<name>.rml")` into a member
   pointer; add yours to the call list, the null-check guard that
   decides readiness (each missing document drives the raygui
   fallback), and the `Shutdown` release list.

4. **Show it for a state.** For a new screen state: add the
   `MenuState` in `src/game/public/app/match/Menu.h`, return your
   document from `DocForState`, and clear it in `ShowState`'s hide
   list so switching states collapses leftovers. For a modal inside an
   existing state, follow the confirm document's show/hide pair
   instead.

5. **Fill it live.** Add a `Refresh<Yours>()` step to `RefreshCurrent`
   that pushes values through the `Set*In` helpers before the document
   is displayed; never cache RML element pointers across frames.

6. **Wire the events.** Register listeners with
   `Listen(doc, "click", { UiIds::... })` for every interactive id
   (plus `"change"` on inputs), then handle them by id in `OnClick` /
   `OnChange`. State changes go through the shared `MenuFlow`, keeping
   raygui and RML screens in lockstep.

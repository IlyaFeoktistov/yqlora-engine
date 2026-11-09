# Game interface

[Русская версия](ui.ru.md)

Step 8 of the MVP: what the player sees over the game and clicks: health
and score, menus, buttons, settings screens. The editor interface (Dear
ImGui) is not this: a game never draws through ImGui.

Status marks: see [README](README.md).

## Decisions [planned, step 8]

The design is the established one for a retained game interface: controls
are nodes with anchors, containers lay them out, a theme styles them, text
is shaped and drawn from distance fields; nothing here is new.

- **Controls are engine nodes** (see [scene](scene.md)): `Control` and the
  nodes derived from it. A control has a rectangle given by anchors (four
  fractions of the parent rectangle) and offsets in pixels from them, a
  pivot, rotation and scale, a minimum size and size flags (fill, expand,
  shrink) for containers. Anchors cover every case of placing: a corner,
  an edge, the centre, stretched over the parent.
- **`UiCanvas`** is the root of an interface: its controls draw over the
  game in canvas `order`; `scaleMode` makes the interface the same on any
  screen: `ReferenceResolution` (designed for 1920 x 1080, scaled to the
  window by width, height or the smaller of them) or `ConstantPixelSize`.
  A canvas may sit anywhere in the tree, so a prefab of an enemy can hold
  its own interface. Drawn in world space (a health bar over an enemy, a
  screen inside the level) is a mode of the canvas, after the MVP.
- **Layout is a pass of `UiSystem`** in `systems.lateUpdate`, after scripts
  changed values: sizes from the leaves up (minimum sizes, text), then
  rectangles from the canvas down. A control changed in code is laid out in
  the same frame. Containers place their children and own their
  rectangles: `VBox`, `HBox`, `Grid`, `Margin`, `Center`, `Scroll`.
- **Text**: FreeType reads fonts (`.ttf`, `.otf` as resources), HarfBuzz
  shapes them (ligatures, kerning, scripts other than Latin), glyphs go into
  an atlas of multi-channel signed distance fields (msdfgen), so text is
  sharp at any size and scale from one atlas. A font has fallbacks
  (Cyrillic, symbols, emoji) tried in order for a missing glyph. Strings
  may be keys of [localization](localization.md) and change with the
  language.
- **Drawing**: `UiPass` after the output pass, over the final picture in
  sRGB, so tone mapping and exposure never touch the interface. Controls
  become textured quads and nine-slice panels batched by texture, clipped
  by rectangles (`Scroll`, `clipChildren`) with the scissor of the RHI.
- **Input**: the canvas with the highest order under the pointer gets it
  first; a control that handles a click or a key marks it used, and the
  game input actions do not see it (`Input::consumedByUi`). Keyboard and
  gamepad move the focus (`UiUp`, `UiDown`, `UiLeft`, `UiRight`,
  `UiAccept`, `UiCancel` join the default actions); the neighbour in a
  direction is found by geometry unless set by hand. Text input goes
  through SDL text input, so IME and layouts work.
- **Themes**: a `Theme` resource (`.ytheme`) holds fonts, sizes, colours
  and styles (a colour, a nine-slice texture, borders and corner radius)
  per control type and state (normal, hovered, pressed, focused,
  disabled); a control takes the theme of the nearest canvas or ancestor
  and may override any entry. The engine ships a default theme, so a
  button is usable without a theme.
- **Events** (see [events](events.md)): `Button.onPressed`,
  `CheckBox.onToggled`, `Slider.onValueChanged`, `LineEdit.onSubmitted`,
  `Control.onFocusEntered` and others, from code and wired in the
  inspector.

## Controls [planned, step 8]

`Control` (base), `Panel`, `Label` (wrapping, alignment, autosize, rich
text with colour and size tags), `Image` (a texture, keep aspect, nine
slice), `Button` (text, icon, toggle mode), `CheckBox`, `Slider`,
`ProgressBar`, `LineEdit`, `Dropdown` (a choice from a list: resolution,
language, quality), `Tabs`, `Dialog` (modal, with buttons), `Scroll`, and
the containers above. Every
property is set in the inspector and from code with the same setter.

## Menus [planned, step 8]

Main, pause and settings menus need more than controls; what every game
needs is in the engine, what each game wants is in its prefabs.

- **Screens**: `UiScreens` on a canvas keeps a stack of screens (a screen
  is a prefab of controls). `push` shows a screen over the others and moves
  the focus into it, `pop` (also on `UiCancel`) closes it and gives the
  focus back to where it was; a screen marked `modal` takes all input below
  it. Opening and closing are events, so a game animates them.
- **Pause**: a screen marked `pausesGame` sets the time scale to 0 while it
  is open and restores it when the last such screen closes; interfaces run
  on real time (`unscaled` timers, `unscaledDelta`), so a pause menu
  animates while the game stands still.
- **Settings at run time**, each a script API kept in the user data folder
  of the platform, so a settings menu only shows and sets values:
  - display: resolution, window mode, vsync (`display()`, see
    [building](build.md));
  - graphics: shadows on or off, shadow quality, the render scale;
  - input: rebinding an action (`inputBindings().rebind(action, index)`
    waits for the next key or button and refuses one taken by another
    action of the same group), reset to the project defaults;
  - language (see [localization](localization.md));
  - audio: volumes of the buses (step 7).
- **Templates**: Create in Files offers "Main menu", "Pause menu" and
  "Settings menu": prefabs from `assets/templates/ui/` wired to the APIs
  above, ready to restyle with a theme. They are game content made with
  the engine, not code of the engine.

## Where the engine ends and the game begins

The engine gives controls that lay out, draw, take input and focus,
style through themes and speak every language. The game decides which
screens exist and when they show, what a button does, where the numbers of
the HUD come from and how menus animate.

## Editor [planned, step 8]

- The viewport switches to a 2D view of the canvas of the selected control
  (the size of the reference resolution, or the Game window size): the
  rectangles with their anchors, handles to move and resize, presets of
  anchors (corners, edges, centre, full) in a menu, snapping to the pixel
  grid and to the edges of siblings.
- "Add node" offers the controls; a texture dropped onto the canvas makes
  an `Image`, a font onto a `Label` sets its font.
- The Game window shows the interface as the player sees it, at the
  chosen resolution.

## Phases (a commit per phase)

### UI.1 Controls and layout

- `Control` with anchors, offsets, pivot, size flags; `UiCanvas` with its
  scale modes; `UiSystem` with the layout pass; the containers.
- Tests: anchors at each preset over three window sizes; a `VBox` of three
  children with expand flags; minimum sizes go up the tree; a control
  changed in `onUpdate` is laid out the same frame; the reference
  resolution scales 1920 x 1080 to 1280 x 720 by height.

### UI.2 Drawing

- `UiPass` after the output pass, batching by texture, nine-slice,
  clipping by scissor; draw order by canvas order and tree order.
- Tests on the test RHI: a canvas of 100 panels with one texture is one
  draw; a clipped child is cut by the scissor; the interface is not
  affected by the exposure of the camera.

### UI.3 Text

- FreeType, HarfBuzz and msdfgen in the build; the font loader; the glyph
  atlas with fallbacks; `Label` with wrapping, alignment and rich text
  tags; localization keys.
- Tests: a line in Russian and in Latin shapes with kerning; a missing
  glyph comes from the fallback font; wrapping at a width; the atlas grows
  without breaking existing glyphs; a key changes with the language.

### UI.4 Input and focus

- Hit testing, hover and press states, consumed input, focus and its
  navigation, the UI default actions, text input.
- Tests: a click on a button over the game does not reach the `Fire`
  action; the higher canvas gets the click; arrow keys move the focus to
  the geometric neighbour; text typed into a `LineEdit`.

### UI.5 Controls and events

- `Panel`, `Image`, `Button`, `CheckBox`, `Slider`, `ProgressBar`,
  `LineEdit`, `Scroll`; their events.
- Tests: `onPressed` once per click and on `UiAccept` with focus; a
  slider dragged and moved by keys reports its value; scrolling clips and
  reports its offset.

### UI.6 Themes

- The `Theme` resource and loader, styles by type and state, inheritance
  and overrides, the default theme.
- Tests: a hovered button takes the hovered style; a child canvas inherits
  the theme; an override wins over the theme.

### UI.7 Menus

- `UiScreens` with push, pop, focus return and modal screens; pausing
  screens; `Dropdown`, `Tabs`, `Dialog`; run-time settings for graphics
  and input rebinding with their storage; the three templates.
- Tests: `UiCancel` pops the top screen and the focus returns to the
  button that opened it; a pausing screen stops game time and timers of
  the game, not `unscaled` ones; a rebound action fires on the new key
  after a restart of the player; a key taken by another action is refused;
  the settings template changes the resolution through `display()`.

### UI.8 Editor

- The 2D view of a canvas, handles, anchor presets, snapping, drops of
  textures and fonts; the Game window resolution.
- Tests (logic without the interface): an anchor preset keeps the visible
  rectangle; a resize by handle is one undo step.

### UI.9 Documents

This file, scene (control nodes), rendering (`UiPass`), input (UI actions,
consumed input, rebinding), resources (fonts, themes), the guide "Game
interface and menus", README.

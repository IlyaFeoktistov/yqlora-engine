# Documentation and the release

[Русская версия](documentation.ru.md)

Step 10 of the MVP: what a person needs to make a game without reading the
source of the engine, and the release 0.0.2 that closes the MVP.

Status marks: see [README](README.md).

## Decisions [planned, step 10]

The design is the established one for an open engine: an API reference
generated from the headers and checked by CI, a site built from the
`docs/` folder in two languages, screenshots made by a script, a tutorial
whose project is built by CI, a release built by CI with a changelog.

- **API reference** from the public headers (`engine/include/`) with
  Doxygen, in English and in Russian: the description of every public
  class, function and property holds both languages next to each other
  (the multilingual comments of Doxygen, `\~english` and `\~russian`), and
  Doxygen builds one reference per language. Both parts stand together, so
  a change of a method shows both to update. A public symbol without a
  description, or without one of its languages, fails CI
  (`WARN_IF_UNDOCUMENTED` as an error and a check of the two parts).
  Internal headers (`internal/`) and sources are not in the reference and
  keep their comments as they are.
- **The site**: the documents of `docs/` (guides, architecture) and the
  reference, built with MkDocs Material (MIT) with an English and a
  Russian version side by side, published to GitHub Pages by CI. The links
  and anchors of every document are checked by CI (`tools/check_links.py`),
  as the translations already are.
- **The editor guide**: every panel, menu, window and key, one page each,
  with screenshots. Screenshots are made by a script: the editor opens a
  demo project and saves pictures of its windows (`yqlora --screenshots`,
  through `rhi::Device::readPixels`), so they stay true after every change.
- **The tutorial "Your first game"**: from an empty project to a built
  game: a character that walks and jumps, things to collect, a score on the
  HUD, a main and a pause menu, sounds and music, a build. Its project
  lies in `examples/first-game/` and is built and run headless by CI, so
  the tutorial never breaks silently.
- **Release 0.0.2**: the version in `CMakeLists.txt`, `CHANGELOG.md` (the
  Keep a Changelog form, in English, with the Russian version next to it),
  a CI workflow that builds the editor with the players for Windows and
  Linux, runs the tests and attaches the archives to a GitHub release on
  the tag `v0.0.2`.

## Phases (a commit per phase)

### D.1 API reference

- Doxygen configuration (`make docs`) for both languages, descriptions of
  every public symbol in English and Russian, the CI check.
- Tests: CI fails on a public function without a description and on one
  with only one language; both references build without warnings.

### D.2 The site and link checks

- MkDocs Material with both languages, the navigation, `check_links.py`
  in CI, publishing to GitHub Pages.
- Tests: a broken anchor fails CI; every English page has its Russian
  pair (as `check_translations.py`).

### D.3 The editor guide

- The pages of the guide, the screenshot script and its demo project.
- Tests: the screenshot run makes every picture the guide refers to.

### D.4 The tutorial

- `examples/first-game/`, the tutorial pages, the CI job that builds the
  project and runs it headless for a few seconds.
- Tests: the job builds the game folder and the player exits with code 0.

### D.5 Release 0.0.2

- The version, `CHANGELOG.md`, the release workflow, the tag, README.
- Tests: the release workflow on a test tag produces both archives, and
  the editor of each archive starts and opens the tutorial project.

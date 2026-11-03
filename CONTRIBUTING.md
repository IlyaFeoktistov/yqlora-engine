# Contributing

[Русская версия](CONTRIBUTING.ru.md)

How the code of Yqlora is written. The architecture itself is in
[docs/architecture](docs/architecture/README.md).

## Before a commit

```bash
make hooks        # once: formats staged C++ files and runs the tests before every commit
make format       # clang-format 23.1.3 over all sources
make test         # build and run the tests
make sanitize     # the tests with AddressSanitizer and UBSan (Linux)
make docs-check   # the English and Russian documents match
```

CI runs the format check, the documents check, the tests on Linux (also with
the sanitizers) and on Windows (MSYS2 UCRT64). A change is checked on both
systems before it is merged.

## Layout

- Public headers in `include/`, headers for the engine and the editor only in
  `internal/`, `src/` holds only `.cpp` files. The same in every part:
  `engine/`, `editor/` (logic), `editor/ui/` (interface),
  `backends/opengl/`.
- Headers are included as `"yqlora/<part>/X.hpp"`, never by a relative path.
- Each subsystem is a folder of its own and registers its own types.
- Editor logic never includes ImGui; the interface only calls the logic.
  Colours and sizes of the interface come from `assets/themes`, icons only
  from the Lucide icon font.

## Design

- **One responsibility.** A class, service or function does one thing and is
  put in its subsystem and folder at once, not "here for now". The engine
  grows by adding parts, not by rewriting them.
- **One source of knowledge.** A list of types, a name, a rule is written in
  one place and everything else is derived from it (the type name from
  `YQLORA_TYPE`, the property type from `std::variant`, record fields from
  `YQLORA_RECORD`). The same for documents: a decision lives in one document.
  Common chores have one function each: a whole file is
  `platform::readFile`, an atomic write `platform::writeFileAtomically`,
  ASCII case `toLowerAscii` and `containsIgnoringCase` (`core/Text.hpp`);
  test helpers are in `tests/TestFiles.hpp`.
- **No magic values.** Numbers and strings that mean something (sizes,
  limits, JSON keys, file names, action IDs) are named constants, one set per
  format or subsystem, shared by reading and writing.
- **Free functions or methods.** A method when it needs private data or the
  invariants of a class; otherwise a free function in a namespace. No hidden
  global state behind free functions; the log and `tr()` are the only engine
  services with a global access point.
- **Namespaces.** Classes live in `yqlora` (the editor in `yqlora::editor`,
  the OpenGL backend in `yqlora::opengl`); a nested namespace is for a group
  of free functions and constants without an owner (`math`, `log`).
- **Formats.** A change of a file format comes with a migration and a test
  (see [project and file formats](docs/architecture/project.md)).

## Code style

- `.clang-format` decides the formatting. On top of it: a function body is
  never written on one line, not even a getter or an empty `{}`; a blank line
  before `return` unless it is the first line of a block.
- Comments only where they add something: a non-obvious reason, a limit, a
  trap, a contract that does not follow from the name. Do not retell the code.
- Comments, documents and commit messages use only characters from a plain
  keyboard: straight quotes, `-` instead of dashes, `->` instead of arrows.
  Other symbols in code only as escape codes (`"\uE1C3"`).
- Examples in comments and tests are plausible for the engine (`Health` is a
  future script, not a built-in component).

## Documents

- Every document is a pair: `X.md` in English and `X.ru.md` in Russian with
  the same headings, code blocks and status marks; both change in the same
  commit.
- A document changes together with the code it describes; status marks
  (**[done]**, **[next]**, **[planned, step N]**) say what is implemented.

## Commits

- One short line, in Russian, without trailers.
- One logical change per commit; formatting and moves go apart from
  behaviour.

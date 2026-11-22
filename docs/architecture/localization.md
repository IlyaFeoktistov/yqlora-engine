# Localization

[Русская версия](localization.ru.md)

Texts of the editor and, later, of games in several languages.

Status marks: see [README](README.md).

## Files and keys [done]

- One file per language: `assets/locales/<code>.json` (`en.json`,
  `ru.json`). Nested objects become dotted keys:
  `{"editor": {"menu": {"save": "Save scene"}}}` gives `editor.menu.save`.
- `language.name` is the name of the language in itself ("Русский"); the
  language menu is built from the files found, nothing else lists languages.
- Key groups:

| Keys | What |
|---|---|
| `editor.*` | texts of the editor: menus, windows, panels, dialogs |
| `projects.*` | the project manager |
| `type.<Type>.description` | tooltip of a registered type |
| `property.<name>` | label of a property (and of a field of a record) in the inspector and the settings window, `Property::labelKey` |
| `enum.<Value>` | label of an enum value in a list, `enumValueLabelKey`; one name means one thing in every enum |
| `group.<name>` | heading of a group of properties (`group("group.ambientLight")`) |
| `editor.settings.<Section>` | name of a settings section in the settings window |

- Names of nodes, types and properties in scene files are not translated:
  they are identifiers, always in English.

## Lookup [done]

- `tr(key)` gives the text in the current language. A key missing in it is
  taken from English; a key missing everywhere comes back as the key itself,
  so a forgotten translation is seen, not an empty string.
- `trOr(key, fallback)` gives `fallback` instead of the key: for labels that
  have a sensible untranslated name (a type, an enum value, a debug channel
  of the game). `Localization::find(key)` says whether a translation exists.
- `trFormat(key, values...)` puts values into the translation at its `{}`
  (`std::format`): the translation decides where a value stands, so word
  order can differ between languages; texts are never glued from pieces. A
  translation with broken braces is shown as it is. A test checks that a
  translation has as many `{` as its English text.
- Types are shown by their type name everywhere; the type picker adds the
  name in the person's language, `type.<Type>.name` (a test requires it for
  every creatable node and component type).
- `Language::english()` keeps the English texts whatever the language, so
  searches in the editor also find English words.
- A property without a `property.<name>` key is shown by its name with a
  capital letter, so a game type works before anyone translates it.
- The language is chosen by the user (View -> Language), at first the first
  system language that has a file; the choice is kept in the user settings,
  not in a project.
- `tr` is one of the two services of the engine with a global access point
  (the other is the log): the application sets the active `Localization`.

## Rules [done]

- Every registered engine type has a description in every language, and
  every shown property of it (fields of records too) and every enum value a
  label in every language; tests check it, so a new type, property or value
  without its key in `en.json` and `ru.json` fails the tests. A raw
  variable name never reaches the inspector.
- Every shown property of an engine type (fields of records too) and every
  field of a built-in shader has a description, `help.<Type>.<property>`,
  in every language, the Russian text its own; see
  [editor](editor.md#descriptions-of-properties-done).
- A script names its fields with `displayName("key or text")`, a shader its
  material fields with `//@ label("key or text")`: a key is translated, a
  text is shown as it is.
- Texts are added to all language files in the same change.

## Planned

- Translations for games (texts of the HUD, a language setting of the game)
  come with the player interface **[planned, step 8]**.
- **The log in the user's language** **[planned, step 8]**: engine and
  editor messages are written as a translation key with arguments
  (`log::warn("log.physics.meshHull", node)`), and the text in the current
  language is made once, when the message is output. Lines already output
  do not change when the language changes: the log buffer and the file
  hold finished text. A key without a translation is output in English,
  like any `tr`.

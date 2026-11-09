# Audio

[Русская версия](audio.ru.md)

Step 7 of the MVP: sounds and music of a game, in space and on the
interface, mixed through buses with their volumes and effects, on
[miniaudio](https://miniaud.io) (MIT or public domain).

Status marks: see [README](README.md).

## Decisions [planned, step 7]

The design is the established one for game audio: clips as resources,
sources and a listener as components, a tree of buses with effects, voices
limited by priority; miniaudio is wired through its high-level engine as
its documentation recommends. Nothing here is new.

- **One audio engine per program** (`ma_engine` in `EngineServices`, on the
  default output device); each scene has an `AudioWorld` system that keeps
  its sources and its listener. Only the Play copy and a game make sound;
  the edited scene is silent, and the editor has a mute button for Play.
- **`AudioClip`**, a resource: WAV, FLAC and MP3 (decoders of miniaudio),
  Ogg Vorbis (stb_vorbis). Import settings: `loadMode` (`Decompress`: the
  whole sound decoded in memory, for short effects; `Stream`: decoded while
  playing, for music and long ambience), `forceMono`. In a built game clips
  stay compressed in the package and are read from it through memory.
- **`AudioSource`** (a component on a node): `clip`, `volume` (0..1),
  `pitch`, `loop`, `playOnStart`, `bus`, `priority`, `spatial` (0 a flat
  sound, 1 a sound in space, between for a blend), `minDistance` and
  `maxDistance`, `rolloff` (inverse, linear, exponential as miniaudio
  gives), `dopplerFactor`, a cone (inner and outer angle, outer volume).
  Its position and velocity come from its node every frame, in
  `systems.lateUpdate` after scripts moved things.
- **`AudioListener`** (on a node, usually the main camera): the first
  active one is used; without one the main camera is the listener. A new
  scene has one on its camera.
- **One-shot sounds**: `audio().playOneShot(clip, position, bus, volume)`
  for shots and footsteps without a component; the voice lives until the
  sound ends.
- **Voices**: at most `maxVoices` sound at once (Audio section); above it
  the voices of the lowest priority and the quietest at the listener are
  made virtual: they keep their position in time and come back when there
  is room, so a long ambience does not restart.
- **Buses**: the Audio section of the project settings holds a tree of
  buses with `Master` at the root and `Music`, `Effects`, `Voice`,
  `Interface` under it by default (miniaudio sound groups). A bus has a
  volume (0..1, shown also in decibels), mute, `pausedWithGame` (on for
  game buses, off for `Interface`, so menu sounds play during a pause) and
  a chain of effects: low-pass, high-pass, delay, reverb (Freeverb, public
  domain), and a limiter on `Master`, as nodes of the miniaudio node graph.
- **The player's volumes** (the settings menu, see [game interface](ui.md))
  are set through `audio().setBusVolume(bus, value)` and kept in the user
  data folder; the project values are the defaults.
- **A changed output device** (headphones plugged in or out) moves the
  output to the new default device; sounds keep playing.
- **Events**: `AudioSource.onFinished` (see [events](events.md)).

## From code

Every property is set in the inspector and from code with one setter; on
top of it: `play()`, `stop()`, `pause()`, `resume()`, `isPlaying()`,
`time()` and `seek(seconds)` of a source; `playOneShot`; bus volumes,
mute and effect parameters by name (a low-pass under water, a reverb in a
cave), all through `audio()` of a script.

## Where the engine ends and the game begins

The engine plays, places in space, mixes, limits voices and keeps the
player's volumes. The game decides which sound plays when, the music of
each place and its transitions, footsteps by surface, and which buses its
settings menu shows.

## Editor [planned, step 7]

- The inspector of an `AudioClip` shows its length, channels, rate and a
  waveform, with a play button.
- The selected source draws its `minDistance` and `maxDistance` spheres
  and its cone.
- The Audio section shows the bus tree with volumes and effect chains;
  fields of clips and buses take drops from Files and from the tree.

## Phases (a commit per phase)

### AU.1 The audio engine

- miniaudio and stb_vorbis in the build; the engine in `EngineServices`
  with a null device for tests and headless runs (frames are read by
  hand); `AudioWorld`; the mute of the editor.
- Tests: an engine on the null device reads silence; the edited scene
  never plays; a scene destroyed while playing frees its sounds (asan).

### AU.2 Clips

- The `AudioClip` resource and loader for the four formats, `loadMode`,
  `forceMono`, cooking for the package.
- Tests: each format decodes to the expected length and channel count; a
  streamed clip and a decompressed one give the same samples; a broken
  file is a resource error.

### AU.3 Sources, the listener, one-shots

- `AudioSource`, `AudioListener`, spatial parameters from nodes, one-shots,
  voice limits with virtual voices, `onFinished`.
- Tests (reading frames from the null device): a source to the right of
  the listener is louder in the right channel; past `maxDistance` it is
  silent with linear rolloff; `pitch` 2 halves the length; the
  lowest-priority voice goes virtual over the limit and resumes at the
  right time; `onFinished` once.

### AU.4 Buses and effects

- The Audio section with the bus tree, volumes, mute, `pausedWithGame`,
  effect chains, the limiter; `setBusVolume` with the user storage.
- Tests: a bus at 0.5 halves the amplitude of its sounds; muting `Master`
  silences all; a pause stops `Effects` and not `Interface`; a low-pass
  cuts a high tone; the player's volume survives a restart.

### AU.5 Editor

- The clip inspector with the waveform and preview, gizmos of sources, the
  bus tree in the settings.
- Tests (logic without the interface): the waveform summary of a known
  clip; a change of a bus volume is one edit of the settings file.

### AU.6 Documents

This file, scene (audio components), project settings (Audio), scripting
(`audio()`), the guide "Sound and music", README.

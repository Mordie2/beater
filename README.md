# beater

A small two-deck, loop-based DJ app that sits next to Resolume. Qt 6 / QML / C++, macOS first.

## Build

Open `CMakeLists.txt` in Qt Creator with the Qt 6.11 macOS kit and hit Run. Or from a terminal:

```sh
git submodule update --init   # the UI kit, external/highlighter-ui
cmake -S . -B build -G Ninja -DCMAKE_PREFIX_PATH=~/Qt/6.11.2/macos
cmake --build build
open build/beater.app
```

The first configure clones miniaudio and Rubber Band from GitHub (FetchContent), so it needs internet once.

If `cmake` / `ninja` aren't on your PATH, use the ones Qt ships:
`export PATH=~/Qt/Tools/CMake/CMake.app/Contents/bin:~/Qt/Tools/Ninja:$PATH`.

Headless engine checks: `./build/engine_test`.

Files or folders passed on the command line are added to the playlist. Debug hooks
(environment variables, see `src/main.cpp`): `BEATER_TEST=1` uses its own settings and
Qt's test data folders instead of the real library, `BEATER_AUTOPLAY=1` loads rows 1 and 2 into
decks A and B, plays them and logs deck state, and `BEATER_SNAPSHOT=out.png` saves
a grab of the window after a few seconds and quits, `BEATER_MODE=perform|edit` starts in
that mode.

## How it works

- **Edit and Perform modes** (switch at the top, or ⌘E). Edit has everything: waveforms,
  loops, grid / BPM / key editing, the mixer, the full playlist. Perform is a narrow column
  to keep open next to Resolume: master tempo, per deck the overview, play / sync / loop,
  key, BPM and volume, then the master fader and the playlist. **Pin** keeps it above other
  windows. Each mode remembers its own window size.

- **Master tempo** is a fixed BPM you set (drag the number, scroll, ±, or double-click to type). Everything follows it.
- **Sync** is on by default for each deck. A synced deck is time-stretched to the master tempo and phase-locked to the master grid (Rubber Band R3 engine, so pitch is untouched). Pressing play starts the deck immediately, already in phase: it jumps to the nearest position whose beat (or bar, with PHASE = BAR) matches the master.
- **Key shift** in semitones, tempo stays locked. The key readout follows.
- **Moving around**: click or drag on the timeline (the overview under the big waveform) and
  the playhead follows; the deck jumps there when you let go, on the beat, or in phase with
  the master when it's synced and playing. The big waveform can be grabbed and dragged the
  same way.
- **Loading**: drag a track from the playlist onto a deck (in either mode), double-click it,
  or press A / B. Over a playing deck the drop target turns red: it replaces that track.
- **Loops**: every track has a loop region. By default it is the last N bars (END), so a track that runs out keeps looping its ending until you bring the next one in. Shift-drag across the timeline to set your own region (snaps to bars, add Option to snap to beats), or use HERE to loop N bars from the current bar. LOOP toggles looping on the deck.
- **Volume**: a fader per deck plus master. No automatic transitions.
- **Analysis**: BPM, beat grid and key are detected on import (tracks analyzed by an older
  build are analyzed again once). Everything is editable: click the BPM to type it, ×2 / ÷2,
  TAP, nudge the grid, SET 1 puts the downbeat at the playhead, pick the key from the list.
  **Auto** (next to the grid, and next to the key) drops your edit and detects it again. Edits are stored per track (keyed by file content, so renames don't lose them) in `~/Library/Application Support/BLOCK SYSTEM/beater/library.json` and are never overwritten by re-analysis.

## Layout

```
src/core       small shared types (beat grid, keys, lock-free queue, track buffer)
src/audio      decoder (ExtAudioFile on macOS, miniaudio fallback), deck engine, audio engine
src/analysis   tempo / grid estimator, key estimator
src/app        Qt side: library model, deck + app controllers, waveform item
qml/           UI, built on HighlighterUI (external/highlighter-ui, `import HighlighterUI`).
               Look.qml holds the deck colours (A orange, B violet; orange is the app accent).
               A panel that declares `property color accent` tints the kit controls inside it.
tests/         headless engine tests
```

Audio thread rules: no locks, no allocation. The UI talks to it through a lock-free command queue and atomics; decoded buffers are released back to the UI thread for freeing.

See `docs/ROADMAP.md` for what's next.

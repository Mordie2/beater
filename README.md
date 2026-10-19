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
Qt's test data folders instead of the real library (and the window ignores real clicks), `BEATER_AUTOPLAY=1` loads rows 1 and 2 into
decks A and B, plays them and logs deck state, and `BEATER_SNAPSHOT=out.png` saves
a grab of the window after a few seconds and quits, `BEATER_MODE=perform|edit` starts in
that mode, `BEATER_FPS=1` logs frames per second and the time spent building waveform frames.

## How it works

- **Edit and Perform modes** (switch at the top, or ⌘E). Edit has everything: the decks
  stacked full width (big waveforms), loops, grid / BPM / key editing, and beside them the
  mixer and the playlist. Perform is a narrow column
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
- **Loops**: every track has three, and the deck loops one at a time:
  - **Intro**: its first N bars (8 by default). A loaded track starts with INTRO armed, so
    play loops the intro until you release it.
  - **In / Out**: a loop anywhere in between. IN marks the playhead (nearest beat); OUT
    marks the end and the loop starts at once, jumping back to the In in phase. LOOP turns it
    off and back on (on again with the playhead past it jumps back in). HERE makes one of
    the chosen length from the current bar.
  - **End**: its last N bars. Armed by default, so a track that runs out keeps looping its
    ending until you bring in the next one.

  In use: In / Out if on, else the intro if armed, else the end. A loop the playhead has
  left behind (seeked past it) releases itself, so the end loop always catches the track.
  The bar buttons set the length of the loop in use. The **loop lane** under the timeline
  shows all three (in use solid, armed half lit, off dim): drag an end to resize, the middle
  to move, the empty lane to draw an In / Out loop (Shift-drag on the timeline does too);
  double-click a loop to reset it. Snaps to bars, hold Option for beats.
- **Output**: OUTPUT at the top right (Edit mode) picks the audio device: System default or
  any output by name; remembered. Switching is live (~60 ms gap, decks keep playing; the
  engine keeps its rate and miniaudio converts). If the device goes away the app plays
  through the default meanwhile and goes back when it returns; a device whose clock stalls
  is reopened.
- **Keys** show as names (F#m) by default; KEYS at the top switches to Camelot (11A). LOOP toggles looping on the deck.
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

# Roadmap

## Stage 1 (this build)
- Two decks, fixed master tempo, per-deck sync with phase lock (beat or bar)
- Rubber Band R3 real-time stretch + independent key shift
- Loop regions per track (auto last-N-bars end loop, drag to set, loop here)
- Per-deck volume + master, level meters
- Playlist with drag-and-drop, persistent library keyed by content hash
- Built-in BPM / grid estimator (spectral flux + autocorrelation + whole-track phase fold, kick-band downbeat)
- Built-in key estimator (peak chromagram + Temperley profiles), a stopgap until stage 2
- Manual BPM / grid / key editing, Auto to go back to the detected values
- Edit mode (everything) and Perform mode (compact column next to Resolume, pin on top)

## Stage 2: analysis quality
- **Beat This** (CPJKU, MIT) via ONNX Runtime for beats + downbeats. Needs: ONNX export of the
  checkpoint, the exact log-mel front end (22.05 kHz, 50 fps, 128 mels), then fit a constant-tempo
  grid to its beats. Replaces `estimateTempo`, same `TempoResult` shape.
- **Key**: libkeyfinder and Essentia `Key` with the `edma` / `bgate` profiles, run both, compare
  against manual corrections on the real library (and the built-in estimator), keep the better
  one (or use agreement as confidence). Bump `kAnalysisVersion` so the library is redone.
- Re-analyze button per track (already in the model: `Library::reanalyze`).

## Stage 3: control
- MIDI input from the BitchBoy, opened in parallel with Resolume (CoreMIDI is multi-client), read-only.
- Shift layer on MIDI channel 2 (firmware), so Resolume templates (channel 1) are untouched.
- Optional "both" mappings: selected channel-1 CCs also drive deck parameters.
- Mapping file in the app data folder.

## Later
- Ableton Link so Resolume follows the master tempo
- Headphone cue on a second output pair (the output picker is in)
- EQ / filter per deck

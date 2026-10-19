#pragma once
#include <cmath>

// Constant-tempo beat grid. Beat 0 is a downbeat (start of a bar) at firstBeatSec.
// Beats before it are negative. Bars are 4 beats.
struct BeatGrid {
    double bpm = 0.0;
    double firstBeatSec = 0.0;

    bool valid() const { return bpm > 1.0; }
    double secPerBeat() const { return 60.0 / bpm; }
    double beatAtSec(double sec) const { return (sec - firstBeatSec) * bpm / 60.0; }
    double secAtBeat(double beat) const { return firstBeatSec + beat * 60.0 / bpm; }
    double beatAtFrame(double frame, double sr) const { return beatAtSec(frame / sr); }
    double frameAtBeat(double beat, double sr) const { return secAtBeat(beat) * sr; }
};

inline double fracPos(double x) { return x - std::floor(x); }

// Wrap x into [-period/2, period/2).
inline double wrapCentered(double x, double period)
{
    return x - period * std::floor(x / period + 0.5);
}

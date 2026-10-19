#pragma once
#include <cstdint>
#include <vector>

// Built-in tempo + beat-grid estimator for constant-tempo electronic music.
// Spectral-flux onset envelope -> autocorrelation tempo candidates (with an
// EDM range prior) -> whole-track phase folding to refine BPM to ~0.01 and find
// the grid phase. Downbeat = first beat at or after the first strong onset.
//
// This is the stage-1 analyzer. Stage 2 replaces it with Beat This (neural
// beat + downbeat tracking); the result shape stays the same.
struct TempoResult {
    bool ok = false;
    double bpm = 0.0;
    double firstBeatSec = 0.0; // a downbeat
    double confidence = 0.0;   // 0..1, rough
};

TempoResult estimateTempo(const std::vector<float> &mono, uint32_t sampleRate,
                          double minBpm = 85.0, double maxBpm = 175.0);

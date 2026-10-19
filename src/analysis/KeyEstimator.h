#pragma once
#include <cstdint>
#include <vector>

// Built-in key estimator: a whole-track chromagram (spectral peaks folded onto
// pitch classes, A = 440 Hz) correlated with the 24 rotated Temperley
// (Kostka-Payne) key profiles, plus a bonus for the bass's root and a
// preference for minor (most of this music is minor).
//
// Stage-1 stopgap. Stage 2 runs libkeyfinder and Essentia Key (edma / bgate)
// and keeps whichever agrees with manual corrections; the result shape stays.
struct KeyResult {
    bool ok = false;
    int key = -1;             // 0..23, see core/Key.h
    double confidence = 0.0;  // 0..1, gap between the best and the runner-up
};

KeyResult estimateKey(const std::vector<float> &mono, uint32_t sampleRate);

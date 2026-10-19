#include "analysis/KeyEstimator.h"
#include "analysis/Fft.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <numeric>

namespace {

constexpr int kFft = 8192;
constexpr int kHop = 4096;
// Pitch range folded into the chroma: E2 .. C7. Below E2 the bins are too
// coarse at this FFT size to tell neighbouring semitones apart.
constexpr double kMinMidi = 40.0;
constexpr double kMaxMidi = 96.0;

// In this music the bass names the key, and most tracks are minor: on 85 of
// my edits labelled with their key (all but one minor), these two raised exact
// hits from 46 to 58 (fifths are most of what's left).
constexpr double kBassTonic = 0.45; // * the bass's share of the tonic (vs its strongest note)
constexpr double kMinorBias = 0.15; // added to every minor key's score
constexpr double kBassTopMidi = 52.5; // the bass: E2 .. E3

// Temperley's Kostka-Payne key profiles, tonic first. Krumhansl-Kessler
// confused major keys with their mediant minor (Eb -> Gm) on harmonic-rich pads.
constexpr std::array<double, 12> kMajor = {0.748, 0.060, 0.488, 0.082, 0.670, 0.460, 0.096, 0.715, 0.104, 0.366, 0.057, 0.400};
constexpr std::array<double, 12> kMinor = {0.712, 0.084, 0.474, 0.618, 0.049, 0.460, 0.105, 0.747, 0.404, 0.067, 0.133, 0.330};

double correlate(const std::array<double, 12> &chroma, const std::array<double, 12> &profile, int tonic)
{
    double mc = 0, mp = 0;
    for (int i = 0; i < 12; ++i) {
        mc += chroma[size_t(i)];
        mp += profile[size_t(i)];
    }
    mc /= 12.0;
    mp /= 12.0;
    double num = 0, dc = 0, dp = 0;
    for (int i = 0; i < 12; ++i) {
        const double c = chroma[size_t((i + tonic) % 12)] - mc;
        const double p = profile[size_t(i)] - mp;
        num += c * p;
        dc += c * c;
        dp += p * p;
    }
    return dc > 0 && dp > 0 ? num / std::sqrt(dc * dp) : 0.0;
}

} // namespace

KeyResult estimateKey(const std::vector<float> &x, uint32_t sr)
{
    KeyResult res;
    if (sr == 0 || x.size() < size_t(kFft) * 4)
        return res;

    // Which pitch class each bin feeds, and how much (1 at the semitone's
    // centre, falling to 0 half a semitone away).
    const int bins = kFft / 2;
    std::vector<int> binPc(size_t(bins), -1);
    std::vector<float> binW(size_t(bins), 0.0f);
    for (int b = 1; b < bins; ++b) {
        const double f = double(b) * sr / kFft;
        const double midi = 69.0 + 12.0 * std::log2(f / 440.0);
        if (midi < kMinMidi - 0.5 || midi > kMaxMidi + 0.5)
            continue;
        const double nearest = std::round(midi);
        binPc[size_t(b)] = int(nearest) % 12;
        binW[size_t(b)] = float(1.0 - 2.0 * std::abs(midi - nearest));
    }

    std::vector<float> win(kFft);
    for (int i = 0; i < kFft; ++i)
        win[size_t(i)] = 0.5f - 0.5f * std::cos(2.0f * float(M_PI) * float(i) / float(kFft));

    std::array<double, 12> chroma{}, bass{};
    std::vector<bool> binBass(size_t(bins), false);
    for (int b = 1; b < bins; ++b)
        binBass[size_t(b)] = 69.0 + 12.0 * std::log2(double(b) * sr / kFft / 440.0) < kBassTopMidi;
    std::vector<std::complex<float>> buf(kFft);
    std::vector<float> mag(static_cast<size_t>(bins));
    const size_t frames = (x.size() - kFft) / kHop + 1;
    // Frames quieter than this (relative to the loudest) are breaks or silence.
    std::vector<double> frameEnergy(frames, 0.0);
    std::vector<std::array<double, 12>> frameChroma(frames), frameBass(frames);
    double loudest = 0.0;
    for (size_t t = 0; t < frames; ++t) {
        const size_t off = t * kHop;
        for (int i = 0; i < kFft; ++i)
            buf[size_t(i)] = {x[off + size_t(i)] * win[size_t(i)], 0.0f};
        fftInPlace(buf);
        for (int b = 0; b < bins; ++b)
            mag[size_t(b)] = std::abs(buf[size_t(b)]);
        std::array<double, 12> c{}, cb{};
        double e = 0.0;
        // Spectral peaks only: tones, not the broadband noise of drums.
        for (int b = 2; b < bins - 1; ++b) {
            if (binPc[size_t(b)] < 0)
                continue;
            const float m = mag[size_t(b)];
            if (m <= mag[size_t(b - 1)] || m < mag[size_t(b + 1)])
                continue;
            // sqrt: softer than linear, but keeps upper partials below the fundamentals
            const double v = std::sqrt(double(m)) * binW[size_t(b)];
            c[size_t(binPc[size_t(b)])] += v;
            if (binBass[size_t(b)])
                cb[size_t(binPc[size_t(b)])] += v;
            e += v;
        }
        frameEnergy[t] = e;
        frameChroma[t] = c;
        frameBass[t] = cb;
        loudest = std::max(loudest, e);
    }
    if (loudest <= 0.0)
        return res;
    // Each usable frame counts equally, so a loud drop doesn't outvote the rest.
    for (size_t t = 0; t < frames; ++t) {
        if (frameEnergy[t] < 0.2 * loudest)
            continue;
        for (int i = 0; i < 12; ++i) {
            chroma[size_t(i)] += frameChroma[t][size_t(i)] / frameEnergy[t];
            bass[size_t(i)] += frameBass[t][size_t(i)] / frameEnergy[t];
        }
    }

    const double bassTop = *std::max_element(bass.begin(), bass.end());
    double best = -2.0, second = -2.0;
    int bestKey = -1;
    for (int k = 0; k < 24; ++k) {
        double r = correlate(chroma, k < 12 ? kMajor : kMinor, k % 12);
        if (k >= 12)
            r += kMinorBias;
        if (bassTop > 0.0)
            r += kBassTonic * bass[size_t(k % 12)] / bassTop;
        if (r > best) {
            second = best;
            best = r;
            bestKey = k;
        } else if (r > second) {
            second = r;
        }
    }
    if (bestKey < 0 || best <= 0.0)
        return res;
    res.ok = true;
    res.key = bestKey;
    res.confidence = std::clamp((best - second) * 5.0, 0.0, 1.0);
    return res;
}

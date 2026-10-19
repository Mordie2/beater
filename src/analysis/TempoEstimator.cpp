#include "analysis/TempoEstimator.h"
#include "analysis/Fft.h"
#include "core/BeatGrid.h"

#include <algorithm>
#include <cmath>
#include <complex>
#include <numeric>

namespace {

constexpr int kFft = 1024;
constexpr int kHop = 256;

std::vector<float> detrend(const std::vector<float> &odf)
{
    // Remove the slowly varying part, keep positive peaks.
    const int half = 16;
    std::vector<double> csum(odf.size() + 1, 0.0);
    for (size_t i = 0; i < odf.size(); ++i)
        csum[i + 1] = csum[i] + odf[i];
    std::vector<float> out(odf.size());
    for (int i = 0; i < int(odf.size()); ++i) {
        const int a = std::max(0, i - half), b = std::min(int(odf.size()), i + half + 1);
        const double mean = (csum[size_t(b)] - csum[size_t(a)]) / double(b - a);
        out[size_t(i)] = std::max(0.0f, float(odf[size_t(i)] - mean));
    }
    return out;
}

// Full-band spectral flux, plus low-band (kick, < ~150 Hz) flux used to pick
// the on-beat phase over the off-beat hats.
void onsetEnvelopes(const std::vector<float> &x, uint32_t sr, std::vector<float> &full, std::vector<float> &low)
{
    const int frames = x.size() > size_t(kFft) ? int((x.size() - kFft) / kHop) + 1 : 0;
    std::vector<float> odf(size_t(std::max(frames, 0)), 0.0f);
    std::vector<float> lowOdf(odf.size(), 0.0f);
    full.clear();
    low.clear();
    if (frames < 2)
        return;
    const int lowBins = std::max(2, int(150.0 * kFft / double(sr)));

    std::vector<float> win(kFft);
    for (int i = 0; i < kFft; ++i)
        win[size_t(i)] = 0.5f - 0.5f * std::cos(2.0f * float(M_PI) * float(i) / float(kFft));

    const int bins = kFft / 2;
    std::vector<float> prev(size_t(bins), 0.0f), cur(size_t(bins), 0.0f);
    std::vector<std::complex<float>> buf(kFft);
    for (int t = 0; t < frames; ++t) {
        const size_t off = size_t(t) * kHop;
        for (int i = 0; i < kFft; ++i)
            buf[size_t(i)] = {x[off + size_t(i)] * win[size_t(i)], 0.0f};
        fftInPlace(buf);
        float flux = 0.0f, lowFlux = 0.0f;
        for (int b = 1; b < bins; ++b) {
            cur[size_t(b)] = std::log1p(100.0f * std::abs(buf[size_t(b)]));
            const float d = cur[size_t(b)] - prev[size_t(b)];
            if (d > 0) {
                flux += d;
                if (b <= lowBins)
                    lowFlux += d;
            }
        }
        odf[size_t(t)] = t == 0 ? 0.0f : flux;
        lowOdf[size_t(t)] = t == 0 ? 0.0f : lowFlux;
        std::swap(prev, cur);
    }
    full = detrend(odf);
    low = detrend(lowOdf);
}

double interp(const std::vector<double> &v, double idx)
{
    if (idx < 0 || idx >= double(v.size() - 1))
        return 0.0;
    const size_t i = size_t(idx);
    const double f = idx - double(i);
    return v[i] * (1.0 - f) + v[i + 1] * f;
}

// Fold the envelope at period P (frames). Returns peak strength and the phase (frames).
struct Fold {
    double strength = 0.0;
    double phase = 0.0;
};

Fold foldAt(const std::vector<float> &odf, double period, int B = 48)
{
    std::vector<double> hist(size_t(B), 0.0);
    for (size_t t = 0; t < odf.size(); ++t) {
        if (odf[t] <= 0.0f)
            continue;
        const double ph = std::fmod(double(t), period) / period * B;
        const int b0 = int(ph) % B;
        const double f = ph - std::floor(ph);
        hist[size_t(b0)] += odf[t] * (1.0 - f);
        hist[size_t((b0 + 1) % B)] += odf[t] * f;
    }
    double mean = 0.0;
    for (double h : hist)
        mean += h;
    mean /= B;
    Fold best;
    auto at = [&](int b) { return hist[size_t(((b % B) + B) % B)]; };
    for (int b = 0; b < B; ++b) {
        const double s = at(b - 1) * 0.5 + at(b) + at(b + 1) * 0.5;
        if (s > best.strength) {
            best.strength = s;
            // centroid of the three bins for sub-bin phase
            const double w0 = at(b - 1), w1 = at(b), w2 = at(b + 1);
            const double c = (w2 - w0) / std::max(1e-9, w0 + w1 + w2);
            best.phase = std::fmod((b + c + B) / B * period, period);
        }
    }
    best.strength = mean > 0 ? best.strength / (2.0 * mean) : 0.0;
    return best;
}

// ---- fine timing ----------------------------------------------------------
// The STFT envelopes above find the tempo well but place beats ~15-60 ms
// late on real kicks (46 ms windows, softer attacks than the calibration
// clicks). The beat phase is refined on a 1.5 ms envelope instead.
constexpr int kFineHop = 32;   // 1.45 ms at 22.05 kHz
constexpr int kFineWin = 128;  // 5.8 ms

// Energy rise envelope at kFineHop: positive change of log energy over a few
// frames. low = kick band (two one-pole low-passes at ~150 Hz), else full band.
std::vector<float> fineRise(const std::vector<float> &x, uint32_t sr, bool low, std::vector<float> *energyOut = nullptr)
{
    std::vector<float> y(x);
    if (low) {
        const float a = float(1.0 - std::exp(-2.0 * M_PI * 150.0 / sr));
        for (int pass = 0; pass < 2; ++pass) {
            float z = 0.0f;
            for (float &v : y) {
                z += a * (v - z);
                v = z;
            }
        }
    }
    const size_t frames = y.size() > size_t(kFineWin) ? (y.size() - kFineWin) / kFineHop + 1 : 0;
    std::vector<float> e(frames, 0.0f);
    for (size_t t = 0; t < frames; ++t) {
        double s = 0.0;
        const float *p = y.data() + t * kFineHop;
        for (int i = 0; i < kFineWin; ++i)
            s += double(p[i]) * p[i];
        e[t] = float(s / kFineWin);
    }
    if (energyOut)
        *energyOut = e;
    std::vector<float> rise(frames, 0.0f);
    constexpr int lag = 3;
    for (size_t t = lag; t < frames; ++t) {
        const float d = std::log(1e-9f + e[t]) - std::log(1e-9f + e[t - lag]);
        rise[t] = std::max(0.0f, d);
    }
    return rise;
}

// Frame index of the fine envelope -> seconds at the onset (calibrated on the
// synthetic kicks in engine_test: the rise peaks as the attack fills the window).
double fineFrameToSec(double t, uint32_t sr) { return (t * kFineHop + 0.5 * kFineWin) / double(sr); }

} // namespace

TempoResult estimateTempo(const std::vector<float> &mono, uint32_t sr, double minBpm, double maxBpm)
{
    TempoResult res;
    std::vector<float> odf, lowOdf;
    onsetEnvelopes(mono, sr, odf, lowOdf);
    if (odf.size() < 400)
        return res;
    const double fps = double(sr) / kHop;

    // Autocorrelation up to 4x the longest beat period.
    const int maxLag = int(std::ceil(fps * 60.0 / minBpm * 4.0)) + 2;
    std::vector<double> ac(size_t(maxLag) + 1, 0.0);
    const size_t N = odf.size();
    for (int L = 1; L <= maxLag && size_t(L) < N; ++L) {
        double s = 0.0;
        for (size_t t = 0; t + size_t(L) < N; ++t)
            s += double(odf[t]) * odf[t + size_t(L)];
        ac[size_t(L)] = s / double(N - size_t(L));
    }

    // Coarse tempo: harmonic sum + soft prior around 125 BPM.
    double bestBpm = 0.0, bestScore = -1.0;
    for (double bpm = minBpm; bpm <= maxBpm; bpm += 0.1) {
        const double lag = fps * 60.0 / bpm;
        const double s = interp(ac, lag) + 0.5 * interp(ac, 2 * lag) + 0.25 * interp(ac, 4 * lag);
        const double z = std::log2(bpm / 125.0) / 0.7;
        const double score = s * std::exp(-0.5 * z * z);
        if (score > bestScore) {
            bestScore = score;
            bestBpm = bpm;
        }
    }
    if (bestBpm <= 0.0)
        return res;

    // Fine tempo: fold the whole track, the sharpest fold wins.
    auto foldBpm = [&](double bpm) { return foldAt(odf, fps * 60.0 / bpm); };
    double fineBpm = bestBpm;
    Fold fineFold = foldBpm(bestBpm);
    for (double bpm = bestBpm - 0.6; bpm <= bestBpm + 0.6; bpm += 0.01) {
        const Fold f = foldBpm(bpm);
        if (f.strength > fineFold.strength) {
            fineFold = f;
            fineBpm = bpm;
        }
    }
    // Electronic tracks are almost always produced at integer (or .5) tempos.
    for (double snap : {std::round(fineBpm), std::round(fineBpm * 2.0) / 2.0}) {
        if (std::abs(snap - fineBpm) < 0.08) {
            const Fold f = foldBpm(snap);
            if (f.strength >= fineFold.strength * 0.92) {
                fineBpm = snap;
                fineFold = f;
                break;
            }
        }
    }

    // Frame index -> seconds. With log compression the flux peaks as soon as the
    // onset enters the tail of the window, ~80% into the frame (calibrated on
    // synthetic kicks).
    auto frameToSec = [&](double t) { return (t * kHop + 0.795 * kFft) / double(sr); };

    const double P = fps * 60.0 / fineBpm;
    const double spb = 60.0 / fineBpm;
    // Prefer the kick phase when there is a clear low-end pulse.
    const Fold lowFold = foldAt(lowOdf, P);
    const bool kick = lowFold.strength > 1.5;
    const double coarseSec = std::fmod(frameToSec(kick ? lowFold.phase : fineFold.phase), spb);

    // Refine the phase on the fine envelope, near the coarse one (so it can't
    // hop to the off-beat hats), and early: the coarse one runs late.
    std::vector<float> energy;
    const std::vector<float> rise = fineRise(mono, sr, kick, &energy);
    double phaseSec = coarseSec;
    if (!rise.empty()) {
        const double fineP = double(sr) / kFineHop * spb;
        constexpr int B = 256;
        std::vector<double> hist(B, 0.0);
        for (size_t t = 0; t < rise.size(); ++t) {
            if (rise[t] <= 0.0f)
                continue;
            hist[size_t(std::fmod(double(t), fineP) / fineP * B) % B] += rise[t];
        }
        double best = -1.0;
        for (int b = 0; b < B; ++b) {
            const double sec = std::fmod(fineFrameToSec(double(b) / B * fineP, sr), spb);
            const double d = wrapCentered(sec - coarseSec, spb);   // + = later than coarse
            if (d < -0.090 || d > 0.030)
                continue;
            double s = 0.0;
            for (int k = -2; k <= 2; ++k)
                s += hist[size_t((b + k + B) % B)] * (3 - std::abs(k));
            if (s > best) {
                best = s;
                phaseSec = sec;
            }
        }
    }

    // Where the music starts: the first fine frame with real energy. MP3s
    // often begin with a few ms of encoder padding (digital silence).
    double startSec = 0.0;
    bool hardStart = false;
    if (!energy.empty()) {
        std::vector<float> sortedE(energy);
        std::nth_element(sortedE.begin(), sortedE.begin() + long(sortedE.size() * 9 / 10), sortedE.end());
        const float loud = sortedE[sortedE.size() * 9 / 10];
        size_t t0 = 0;
        while (t0 < energy.size() && energy[t0] < loud * 1e-4f)
            ++t0;
        // the exact first sample, 40 dB under the loud level
        const float amp = 0.01f * std::sqrt(loud);
        size_t i = t0 * kFineHop;
        while (i < mono.size() && i < (t0 + 1) * kFineHop + kFineWin && std::abs(mono[i]) < amp)
            ++i;
        startSec = double(i) / sr;
        // A hit, not a fade-in: loud within 40 ms of the first sound.
        const size_t t1 = std::min(energy.size(), t0 + size_t(0.040 * sr / kFineHop));
        for (size_t t = t0; t < t1; ++t)
            hardStart |= energy[t] >= 0.1f * loud;
    }

    // Most tracks (and every edit exported from a DAW) start on a downbeat:
    // when the music starts on the grid, the grid starts there. A hard hit
    // may sit a little further off the refined phase than a soft start.
    double firstBeat;
    const double startOff = wrapCentered(startSec - phaseSec, spb);
    if (std::abs(startOff) < (hardStart ? 0.035 : 0.020)) {
        firstBeat = startSec;
    } else {
        // Otherwise the downbeat is the first grid beat at/after the first strong onset.
        std::vector<float> sorted(odf);
        std::nth_element(sorted.begin(), sorted.begin() + long(sorted.size() * 95 / 100), sorted.end());
        const float strong = 0.3f * sorted[sorted.size() * 95 / 100];
        size_t firstOnset = 0;
        while (firstOnset < N && odf[firstOnset] < strong)
            ++firstOnset;
        firstBeat = phaseSec;
        while (firstBeat < frameToSec(double(firstOnset)) - 0.25 * spb)
            firstBeat += spb;
    }

    res.ok = true;
    res.bpm = fineBpm;
    res.firstBeatSec = std::max(0.0, firstBeat);
    res.confidence = std::clamp((fineFold.strength - 1.0) / 3.0, 0.0, 1.0);
    return res;
}

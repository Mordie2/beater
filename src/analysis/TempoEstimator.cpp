#include "analysis/TempoEstimator.h"
#include "analysis/Fft.h"

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

Fold foldAt(const std::vector<float> &odf, double period)
{
    constexpr int B = 48;
    double hist[B] = {};
    for (size_t t = 0; t < odf.size(); ++t) {
        if (odf[t] <= 0.0f)
            continue;
        const double ph = std::fmod(double(t), period) / period * B;
        const int b0 = int(ph) % B;
        const double f = ph - std::floor(ph);
        hist[b0] += odf[t] * (1.0 - f);
        hist[(b0 + 1) % B] += odf[t] * f;
    }
    double mean = 0.0;
    for (double h : hist)
        mean += h;
    mean /= B;
    Fold best;
    for (int b = 0; b < B; ++b) {
        const double s = hist[(b + B - 1) % B] * 0.5 + hist[b] + hist[(b + 1) % B] * 0.5;
        if (s > best.strength) {
            best.strength = s;
            // centroid of the three bins for sub-bin phase
            const double w0 = hist[(b + B - 1) % B], w1 = hist[b], w2 = hist[(b + 1) % B];
            const double c = (w2 - w0) / std::max(1e-9, w0 + w1 + w2);
            best.phase = std::fmod((b + c + B) / B * period, period);
        }
    }
    best.strength = mean > 0 ? best.strength / (2.0 * mean) : 0.0;
    return best;
}

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

    // Downbeat: first grid beat at/after the first strong onset.
    std::vector<float> sorted(odf);
    std::nth_element(sorted.begin(), sorted.begin() + long(sorted.size() * 95 / 100), sorted.end());
    const float strong = 0.3f * sorted[sorted.size() * 95 / 100];
    size_t firstOnset = 0;
    while (firstOnset < N && odf[firstOnset] < strong)
        ++firstOnset;
    const double P = fps * 60.0 / fineBpm;
    // Prefer the kick phase when there is a clear low-end pulse.
    const Fold lowFold = foldAt(lowOdf, P);
    double beatFrame = lowFold.strength > 1.5 ? lowFold.phase : fineFold.phase;
    while (beatFrame < double(firstOnset) - 0.25 * P)
        beatFrame += P;

    res.ok = true;
    res.bpm = fineBpm;
    res.firstBeatSec = std::max(0.0, frameToSec(beatFrame));
    res.confidence = std::clamp((fineFold.strength - 1.0) / 3.0, 0.0, 1.0);
    return res;
}

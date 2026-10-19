#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

// Fully decoded track, stereo, at the engine sample rate.
// Planar layout (one vector per channel) because Rubber Band wants planar input.
struct TrackBuffer {
    std::vector<float> left;
    std::vector<float> right;
    uint32_t sampleRate = 0;

    // Waveform overview: one value per bucket (abs peak and RMS), 0..1.
    std::vector<float> peaks;
    std::vector<float> rms;
    int64_t framesPerBucket = 0;
    // Fine peaks for the zoomed waveform.
    std::vector<float> detail;
    static constexpr int64_t kDetailFrames = 256;

    int64_t frames() const { return int64_t(left.size()); }
    double durationSec() const { return sampleRate ? double(frames()) / sampleRate : 0.0; }

    void computeOverview(int buckets = 4096)
    {
        const int64_t n = frames();
        peaks.assign(buckets, 0.f);
        rms.assign(buckets, 0.f);
        if (n == 0 || buckets <= 0)
            return;
        framesPerBucket = (n + buckets - 1) / buckets;
        for (int b = 0; b < buckets; ++b) {
            const int64_t s = int64_t(b) * framesPerBucket;
            const int64_t e = std::min<int64_t>(n, s + framesPerBucket);
            float pk = 0.f;
            double sum = 0.0;
            for (int64_t i = s; i < e; ++i) {
                const float m = 0.5f * (left[i] + right[i]);
                const float a = m < 0 ? -m : m;
                if (a > pk)
                    pk = a;
                sum += double(m) * m;
            }
            peaks[b] = std::min(1.f, pk);
            rms[b] = e > s ? std::min(1.f, float(std::sqrt(sum / double(e - s)))) : 0.f;
        }
        detail.assign(size_t((n + kDetailFrames - 1) / kDetailFrames), 0.f);
        for (int64_t i = 0; i < n; ++i) {
            const float m = 0.5f * (left[i] + right[i]);
            float &d = detail[size_t(i / kDetailFrames)];
            d = std::max(d, m < 0 ? -m : m);
        }
    }
};

using TrackBufferPtr = std::shared_ptr<const TrackBuffer>;

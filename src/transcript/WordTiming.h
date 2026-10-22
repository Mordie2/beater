#pragma once
#include <cmath>
#include <vector>

struct WordTiming { double start = -1.0, end = -1.0; };

// Source-audio seconds, never elapsed wall time. Exclusive end avoids lighting
// two adjacent words, and gaps deliberately have no active word.
inline int activeWordAt(const std::vector<WordTiming> &words, double position)
{
    if (!std::isfinite(position) || position < 0) return -1;
    int active = -1;
    for (size_t i = 0; i < words.size(); ++i) {
        const auto &w = words[i];
        if (std::isfinite(w.start) && std::isfinite(w.end) && w.start >= 0
            && w.end > w.start && w.start <= position && position < w.end)
            active = int(i);
    }
    return active;
}

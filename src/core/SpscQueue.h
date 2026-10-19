#pragma once
#include <array>
#include <atomic>
#include <cstddef>

// Lock-free single-producer / single-consumer ring buffer.
// Used to pass commands UI -> audio thread and garbage audio -> UI thread
// without locking inside the audio callback.
template <typename T, size_t N>
class SpscQueue {
    static_assert((N & (N - 1)) == 0, "N must be a power of two");

public:
    bool push(const T &v)
    {
        const size_t w = m_write.load(std::memory_order_relaxed);
        const size_t r = m_read.load(std::memory_order_acquire);
        if (w - r >= N)
            return false;
        m_buf[w & (N - 1)] = v;
        m_write.store(w + 1, std::memory_order_release);
        return true;
    }

    bool pop(T &out)
    {
        const size_t r = m_read.load(std::memory_order_relaxed);
        const size_t w = m_write.load(std::memory_order_acquire);
        if (r == w)
            return false;
        out = m_buf[r & (N - 1)];
        m_read.store(r + 1, std::memory_order_release);
        return true;
    }

private:
    std::array<T, N> m_buf{};
    alignas(64) std::atomic<size_t> m_write{0};
    alignas(64) std::atomic<size_t> m_read{0};
};

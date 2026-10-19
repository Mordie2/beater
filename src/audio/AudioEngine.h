#pragma once
#include "audio/DeckEngine.h"
#include "core/SpscQueue.h"

#include <array>
#include <atomic>
#include <memory>
#include <string>

struct ma_device;

// Owns the output device, the master clock and the decks.
// UI thread: post() commands, set atomics, read published state.
// Audio thread: render().
class AudioEngine {
public:
    static constexpr int kDecks = 2;

    AudioEngine();
    ~AudioEngine();

    // Opens the default output device at its native rate and starts it.
    bool start(std::string *error = nullptr);
    void stop();
    // For tests: set up decks without a device; drive with renderOffline().
    void initOffline(uint32_t sampleRate);
    void renderOffline(float *interleaved, int frames) { render(interleaved, frames); }

    uint32_t sampleRate() const { return m_sr; }
    std::string deviceName() const { return m_deviceName; }

    // ---- UI thread ----
    void post(int deck, const DeckCommand &cmd);
    void setMasterBpm(double bpm) { m_masterBpm.store(bpm); }
    double masterBpm() const { return m_masterBpm.load(); }
    void setMasterVolume(float v) { m_masterVolume.store(v); }
    void setPhaseMode(PhaseMode p) { m_phase.store(int(p)); }
    double masterBeat() const { return m_pubMasterBeat.load(std::memory_order_relaxed); }
    float masterLevel() const { return m_pubMasterLevel.load(std::memory_order_relaxed); }
    DeckEngine &deck(int i) { return *m_decks[size_t(i)]; }
    // Frees track buffers released by the audio thread. Call regularly from the UI thread.
    void collectGarbage();

private:
    struct Msg {
        int deck = 0;
        DeckCommand cmd;
    };
    static void dataCallback(ma_device *dev, void *out, const void *in, unsigned int frames);
    void render(float *interleaved, int frames);
    void createDecks();

    uint32_t m_sr = 48000;
    std::string m_deviceName;
    std::unique_ptr<ma_device> m_device;
    bool m_running = false;
    std::array<std::unique_ptr<DeckEngine>, kDecks> m_decks;

    SpscQueue<Msg, 256> m_commands;
    SpscQueue<TrackBufferPtr *, 64> m_garbage;

    std::atomic<double> m_masterBpm{125.0};
    std::atomic<float> m_masterVolume{0.8f};
    std::atomic<int> m_phase{int(PhaseMode::Beat)};

    double m_masterBeat = 0.0; // audio thread
    float m_lastMasterGain = 0.8f;
    float m_masterLevel = 0.0f;
    std::atomic<double> m_pubMasterBeat{0.0};
    std::atomic<float> m_pubMasterLevel{0.0f};

    std::array<float, DeckEngine::kMaxBlock> m_mixL{}, m_mixR{};
};

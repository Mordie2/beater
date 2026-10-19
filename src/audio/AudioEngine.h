#pragma once
#include "audio/DeckEngine.h"
#include "core/SpscQueue.h"

#include <array>
#include <atomic>
#include <memory>
#include <string>
#include <vector>

struct ma_context;
struct ma_device;

// Owns the output device, the master clock and the decks.
// UI thread: post() commands, set atomics, read published state.
// Audio thread: render().
class AudioEngine {
public:
    static constexpr int kDecks = 2;

    AudioEngine();
    ~AudioEngine();

    // Opens an output device by name ("" = the system default) and starts it.
    // The first open runs at the device's native rate; later ones keep that
    // rate (miniaudio converts), so loaded tracks stay valid.
    bool start(const std::string &deviceName = {}, std::string *error = nullptr);
    void stop();
    // Close the current device and open another; playback carries on.
    bool switchDevice(const std::string &deviceName, std::string *error = nullptr);
    // The output devices there are now (names), the system default's first.
    std::vector<std::string> outputDevices();
    // The device stopped by itself (unplugged...). Cleared by the next start().
    bool deviceLost() const { return m_lost.load(); }
    // From miniaudio's notification callback: the device stopped.
    void deviceStopped() { if (!m_stopping.load()) m_lost = true; }
    // For tests: set up decks without a device; drive with renderOffline().
    void initOffline(uint32_t sampleRate);
    void renderOffline(float *interleaved, int frames) { render(interleaved, frames); }

    uint32_t sampleRate() const { return m_sr; }
    bool ready() const { return bool(m_decks[0]); } // a device (or initOffline) has set the rate
    bool running() const { return m_running; }      // a device is open and started
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
    bool ensureContext();
    void render(float *interleaved, int frames);
    void createDecks();

    uint32_t m_sr = 48000;
    std::string m_deviceName;
    std::unique_ptr<ma_context> m_context;
    std::unique_ptr<ma_device> m_device;
    bool m_running = false;
    std::atomic<bool> m_stopping{false};
    std::atomic<bool> m_lost{false};
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

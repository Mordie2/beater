#include "audio/AudioEngine.h"

#include <miniaudio.h>

#include <algorithm>
#include <cmath>

AudioEngine::AudioEngine() = default;

AudioEngine::~AudioEngine()
{
    stop();
    collectGarbage();
}

void AudioEngine::createDecks()
{
    for (auto &d : m_decks)
        d = std::make_unique<DeckEngine>(m_sr);
}

void AudioEngine::initOffline(uint32_t sampleRate)
{
    m_sr = sampleRate;
    createDecks();
}

bool AudioEngine::start(std::string *error)
{
    if (m_running)
        return true;
    m_device = std::make_unique<ma_device>();
    ma_device_config cfg = ma_device_config_init(ma_device_type_playback);
    cfg.playback.format = ma_format_f32;
    cfg.playback.channels = 2;
    cfg.sampleRate = 0; // device native
    cfg.periodSizeInFrames = 512;
    cfg.dataCallback = &AudioEngine::dataCallback;
    cfg.pUserData = this;
    cfg.noPreSilencedOutputBuffer = MA_FALSE;
    cfg.noClip = MA_FALSE;

    if (ma_device_init(nullptr, &cfg, m_device.get()) != MA_SUCCESS) {
        if (error)
            *error = "Could not open the audio output device";
        m_device.reset();
        return false;
    }
    m_sr = m_device->sampleRate;
    char name[256] = {};
    if (ma_device_get_name(m_device.get(), ma_device_type_playback, name, sizeof(name), nullptr) == MA_SUCCESS)
        m_deviceName = name;
    createDecks();
    if (ma_device_start(m_device.get()) != MA_SUCCESS) {
        if (error)
            *error = "Could not start the audio output device";
        ma_device_uninit(m_device.get());
        m_device.reset();
        return false;
    }
    m_running = true;
    return true;
}

void AudioEngine::stop()
{
    if (m_device) {
        ma_device_uninit(m_device.get());
        m_device.reset();
    }
    m_running = false;
}

void AudioEngine::post(int deck, const DeckCommand &cmd)
{
    if (!m_commands.push(Msg{deck, cmd})) {
        // Queue full (should never happen at UI rates); don't leak a load.
        delete cmd.track;
    }
}

void AudioEngine::collectGarbage()
{
    TrackBufferPtr *p = nullptr;
    while (m_garbage.pop(p))
        delete p;
}

void AudioEngine::dataCallback(ma_device *dev, void *out, const void *, unsigned int frames)
{
    static_cast<AudioEngine *>(dev->pUserData)->render(static_cast<float *>(out), int(frames));
}

void AudioEngine::render(float *interleaved, int frames)
{
    int done = 0;
    while (done < frames) {
        const int n = std::min(frames - done, DeckEngine::kMaxBlock);

        MasterState m;
        m.bpm = std::clamp(m_masterBpm.load(std::memory_order_relaxed), 20.0, 400.0);
        m.beat = m_masterBeat;
        m.sampleRate = double(m_sr);
        m.phase = PhaseMode(m_phase.load(std::memory_order_relaxed));

        Msg msg;
        while (m_commands.pop(msg)) {
            if (msg.deck < 0 || msg.deck >= kDecks)
                continue;
            if (TrackBufferPtr *g = m_decks[size_t(msg.deck)]->handle(msg.cmd, m))
                m_garbage.push(g); // if this fails we leak rather than free on the audio thread
        }

        std::fill(m_mixL.begin(), m_mixL.begin() + n, 0.0f);
        std::fill(m_mixR.begin(), m_mixR.begin() + n, 0.0f);
        for (auto &d : m_decks)
            d->render(m_mixL.data(), m_mixR.data(), n, m);

        const float g0 = m_lastMasterGain;
        const float g1 = std::clamp(m_masterVolume.load(std::memory_order_relaxed), 0.0f, 1.0f);
        float peak = 0.0f;
        float *o = interleaved + size_t(done) * 2;
        for (int i = 0; i < n; ++i) {
            const float g = g0 + (g1 - g0) * float(i + 1) / float(n);
            const float l = std::clamp(m_mixL[size_t(i)] * g, -1.0f, 1.0f);
            const float r = std::clamp(m_mixR[size_t(i)] * g, -1.0f, 1.0f);
            o[2 * i] = l;
            o[2 * i + 1] = r;
            peak = std::max(peak, std::max(std::abs(l), std::abs(r)));
        }
        m_lastMasterGain = g1;
        m_masterLevel = std::max(peak, m_masterLevel * 0.9f);

        m_masterBeat += double(n) * m.bpm / 60.0 / double(m_sr);
        done += n;
    }
    m_pubMasterBeat.store(m_masterBeat, std::memory_order_relaxed);
    m_pubMasterLevel.store(m_masterLevel, std::memory_order_relaxed);
}

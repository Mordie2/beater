#include "audio/AudioEngine.h"

#include <miniaudio.h>

#include <algorithm>
#include <cmath>

namespace {
// stopped, and not by us: the device went away
void deviceNotification(const ma_device_notification *n)
{
    if (n->type == ma_device_notification_type_stopped)
        static_cast<AudioEngine *>(n->pDevice->pUserData)->deviceStopped();
}
} // namespace

AudioEngine::AudioEngine() = default;

AudioEngine::~AudioEngine()
{
    stop();
    if (m_context)
        ma_context_uninit(m_context.get());
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

bool AudioEngine::ensureContext()
{
    if (m_context)
        return true;
    m_context = std::make_unique<ma_context>();
    if (ma_context_init(nullptr, 0, nullptr, m_context.get()) != MA_SUCCESS) {
        m_context.reset();
        return false;
    }
    return true;
}

std::vector<std::string> AudioEngine::outputDevices()
{
    std::vector<std::string> names;
    ma_device_info *infos = nullptr;
    ma_uint32 count = 0;
    if (!ensureContext() || ma_context_get_devices(m_context.get(), &infos, &count, nullptr, nullptr) != MA_SUCCESS)
        return names;
    for (ma_uint32 i = 0; i < count; ++i) {
        if (infos[i].isDefault)
            names.insert(names.begin(), infos[i].name);
        else
            names.emplace_back(infos[i].name);
    }
    return names;
}

bool AudioEngine::start(const std::string &deviceName, std::string *error)
{
    if (m_running)
        return true;
    if (!ensureContext()) {
        if (error)
            *error = "Could not start Core Audio";
        return false;
    }
    // find the device by name (the ids are opaque)
    ma_device_id id{};
    bool haveId = false;
    if (!deviceName.empty()) {
        ma_device_info *infos = nullptr;
        ma_uint32 count = 0;
        if (ma_context_get_devices(m_context.get(), &infos, &count, nullptr, nullptr) == MA_SUCCESS) {
            for (ma_uint32 i = 0; i < count && !haveId; ++i) {
                if (deviceName == infos[i].name) {
                    id = infos[i].id;
                    haveId = true;
                }
            }
        }
        if (!haveId) {
            if (error)
                *error = "\"" + deviceName + "\" is not connected";
            return false;
        }
    }

    const bool firstOpen = !m_decks[0];
    m_device = std::make_unique<ma_device>();
    ma_device_config cfg = ma_device_config_init(ma_device_type_playback);
    cfg.playback.pDeviceID = haveId ? &id : nullptr;
    cfg.playback.format = ma_format_f32;
    cfg.playback.channels = 2;
    cfg.sampleRate = firstOpen ? 0 : m_sr; // native at first, then keep the engine's rate
    cfg.periodSizeInFrames = 512;
    cfg.dataCallback = &AudioEngine::dataCallback;
    cfg.notificationCallback = &deviceNotification;
    cfg.pUserData = this;
    cfg.noPreSilencedOutputBuffer = MA_FALSE;
    cfg.noClip = MA_FALSE;

    if (ma_device_init(m_context.get(), &cfg, m_device.get()) != MA_SUCCESS) {
        if (error)
            *error = "Could not open the audio output device";
        m_device.reset();
        return false;
    }
    if (firstOpen) {
        m_sr = m_device->sampleRate;
        createDecks();
    }
    char name[256] = {};
    if (ma_device_get_name(m_device.get(), ma_device_type_playback, name, sizeof(name), nullptr) == MA_SUCCESS)
        m_deviceName = name;
    m_lost = false;
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
        m_stopping = true;
        ma_device_uninit(m_device.get());
        m_device.reset();
        m_stopping = false;
    }
    m_running = false;
}

bool AudioEngine::switchDevice(const std::string &deviceName, std::string *error)
{
    stop();
    return start(deviceName, error);
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

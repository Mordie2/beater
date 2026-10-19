#include "app/DeckController.h"

#include "app/Library.h"
#include "audio/AudioEngine.h"
#include "audio/Decoder.h"
#include "core/Key.h"

#include <QDateTime>
#include <QtConcurrent/QtConcurrent>

#include <cmath>

DeckController::DeckController(int index, AudioEngine *engine, Library *library, QObject *parent)
    : QObject(parent)
    , m_index(index)
    , m_engine(engine)
    , m_library(library)
{
    m_engine->deck(m_index).volume.store(float(m_volume * m_volume));
    m_tapClock.start();
}

double DeckController::beat() const
{
    return m_rec.grid().valid() ? m_rec.grid().beatAtSec(m_position) : 0.0;
}

QString DeckController::keyText() const
{
    if (!keys::valid(m_rec.key))
        return QString();
    return QString::fromStdString(keys::camelot(m_rec.key) + " · " + keys::standardName(m_rec.key));
}

QString DeckController::shiftedKeyText() const
{
    const int k = keys::transpose(m_rec.key, m_keyShift);
    if (!keys::valid(k))
        return QString();
    return QString::fromStdString(keys::camelot(k) + " · " + keys::standardName(k));
}

QStringList DeckController::keyNames() const
{
    QStringList l;
    for (int k = 0; k < 24; ++k)
        l << QString::fromStdString(keys::camelot(k) + "  " + keys::standardName(k));
    return l;
}

bool DeckController::inLoop() const
{
    if (!m_loopEnabled || m_loopEnd <= m_loopStart || !m_rec.grid().valid())
        return false;
    const double b = beat();
    return b >= m_loopStart && b < m_loopEnd;
}

// ---------------------------------------------------------------- loading

void DeckController::loadRecord(const TrackRecord &rec)
{
    const int serial = ++m_loadSerial;
    m_loading = true;
    m_error.clear();
    emit loadingChanged();
    emit errorChanged();
    const QString path = rec.path;
    const QString id = rec.id;
    const uint32_t sr = m_engine->sampleRate();
    QtConcurrent::run([path, sr]() -> std::shared_ptr<TrackBuffer> {
        std::string err;
        return decoder::decodeStereo(path.toStdString(), sr, &err);
    }).then(this, [this, serial, id](std::shared_ptr<TrackBuffer> tb) {
        if (serial != m_loadSerial)
            return; // a newer load superseded this one
        m_loading = false;
        emit loadingChanged();
        if (!tb) {
            m_error = QStringLiteral("Could not decode this file");
            emit errorChanged();
            return;
        }
        m_buffer = tb;
        m_rec = m_library->record(id);
        if (m_rec.durationSec <= 0.0) {
            m_rec.durationSec = tb->durationSec();
            m_library->update(m_rec);
        }
        m_keyShift = 0;
        m_engine->deck(m_index).semitones.store(0);
        m_position = 0.0;
        ++m_waveRev;

        m_engine->post(m_index, {DeckCommand::Load, 0.0, 0.0, new TrackBufferPtr(m_buffer)});
        sendGrid();
        computeLoop();
        sendLoop();
        m_engine->post(m_index, {DeckCommand::SetLoopEnabled, m_loopEnabled ? 1.0 : 0.0});
        m_engine->post(m_index, {DeckCommand::SetSync, m_sync ? 1.0 : 0.0});

        emit trackChanged();
        emit gridChanged();
        emit analyzingChanged();
        emit keyChanged();
        emit loopChanged();
        emit positionChanged();
        updatePlayingBpm();
    });
}

void DeckController::refreshFromLibrary(const QString &id)
{
    if (id != m_rec.id || !m_buffer)
        return;
    const TrackRecord r = m_library->record(id);
    const bool gridDiff = r.bpm != m_rec.bpm || r.firstBeatSec != m_rec.firstBeatSec;
    const bool manualDiff = r.gridManual != m_rec.gridManual;
    const bool keyDiff = r.key != m_rec.key || r.keyManual != m_rec.keyManual;
    const bool statusDiff = r.status != m_rec.status;
    const bool loopDiff = r.loopStartBeat != m_rec.loopStartBeat || r.loopEndBeat != m_rec.loopEndBeat;
    const bool titleDiff = r.title != m_rec.title;
    m_rec = r;
    if (gridDiff) {
        sendGrid();
        emit gridChanged();
        updatePlayingBpm();
    }
    if (gridDiff || loopDiff) {
        computeLoop();
        sendLoop();
        emit loopChanged();
    }
    if (manualDiff && !gridDiff)
        emit gridChanged();
    if (keyDiff)
        emit keyChanged();
    if (statusDiff)
        emit analyzingChanged();
    if (titleDiff)
        emit trackChanged();
}

void DeckController::commitRecord()
{
    if (!m_rec.id.isEmpty())
        m_library->update(m_rec);
}

// ---------------------------------------------------------------- engine sync

void DeckController::sendGrid()
{
    m_engine->post(m_index, {DeckCommand::SetGrid, m_rec.bpm, m_rec.firstBeatSec});
}

void DeckController::computeLoop()
{
    if (m_rec.hasManualLoop()) {
        m_loopStart = m_rec.loopStartBeat;
        m_loopEnd = m_rec.loopEndBeat;
        return;
    }
    const BeatGrid g = m_rec.grid();
    if (!g.valid() || !m_buffer) {
        m_loopStart = m_loopEnd = 0.0;
        return;
    }
    // Last whole bars of the track.
    const double lastBeat = g.beatAtSec(m_buffer->durationSec());
    m_loopEnd = std::floor(lastBeat / 4.0) * 4.0;
    m_loopStart = std::max(std::ceil(g.beatAtSec(0.0) / 4.0) * 4.0, m_loopEnd - 4.0 * m_loopBars);
    if (m_loopEnd <= m_loopStart)
        m_loopStart = m_loopEnd = 0.0;
}

void DeckController::sendLoop()
{
    m_engine->post(m_index, {DeckCommand::SetLoop, m_loopStart, m_loopEnd});
}

void DeckController::updatePlayingBpm()
{
    double b = m_rec.bpm;
    if (m_sync && m_rec.bpm > 0.0)
        b = m_engine->masterBpm();
    if (b != m_playingBpm) {
        m_playingBpm = b;
        emit playingBpmChanged();
    }
}

void DeckController::poll()
{
    const DeckEngine &d = m_engine->deck(m_index);
    const uint32_t sr = m_engine->sampleRate();
    const double pos = d.pubPosFrame.load(std::memory_order_relaxed) / double(sr);
    const bool playing = d.pubPlaying.load(std::memory_order_relaxed);
    const double level = d.pubLevel.load(std::memory_order_relaxed);
    const double err = d.pubPhaseError.load(std::memory_order_relaxed);
    if (std::abs(pos - m_position) > 0.0005 || err != m_phaseError) {
        m_position = pos;
        m_phaseError = err;
        emit positionChanged();
    }
    if (playing != m_playing) {
        m_playing = playing;
        emit playingChanged();
    }
    if (std::abs(level - m_level) > 0.002) {
        m_level = level;
        emit levelChanged();
    }
}

// ---------------------------------------------------------------- transport

void DeckController::togglePlay()
{
    m_playing ? pause() : play();
}

void DeckController::play()
{
    if (m_buffer)
        m_engine->post(m_index, {DeckCommand::Play});
}

void DeckController::pause()
{
    m_engine->post(m_index, {DeckCommand::Pause});
}

void DeckController::cue()
{
    if (!m_buffer)
        return;
    if (m_playing)
        pause();
    m_engine->post(m_index, {DeckCommand::Seek, 0.0});
    m_position = 0.0;
    emit positionChanged();
}

double DeckController::snapToBeat(double sec) const
{
    const BeatGrid g = m_rec.grid();
    if (!g.valid())
        return sec;
    return g.secAtBeat(std::round(g.beatAtSec(sec)));
}

void DeckController::seek(double sec)
{
    if (!m_buffer)
        return;
    sec = std::clamp(sec, 0.0, duration());
    const double t = snapToBeat(sec);
    m_engine->post(m_index, {DeckCommand::Seek, t * m_engine->sampleRate()});
    if (!m_playing) {
        m_position = t;
        emit positionChanged();
    }
}

void DeckController::jumpBeats(double beats)
{
    if (!m_buffer || !m_rec.grid().valid())
        return;
    const double t = std::clamp(m_position + beats * m_rec.grid().secPerBeat(), 0.0, duration());
    m_engine->post(m_index, {DeckCommand::Seek, t * m_engine->sampleRate()});
    if (!m_playing) {
        m_position = t;
        emit positionChanged();
    }
}

// ---------------------------------------------------------------- grid

void DeckController::setBpm(double bpm)
{
    if (m_rec.id.isEmpty() || !(bpm > 0))
        return;
    bpm = std::clamp(std::round(bpm * 100.0) / 100.0, 40.0, 250.0);
    if (bpm == m_rec.bpm)
        return;
    m_rec.bpm = bpm;
    m_rec.gridManual = true;
    sendGrid();
    computeLoop();
    sendLoop();
    commitRecord();
    emit gridChanged();
    emit loopChanged();
    updatePlayingBpm();
}

void DeckController::tap()
{
    const qint64 now = m_tapClock.elapsed();
    if (!m_taps.isEmpty() && now - m_taps.last() > 2000)
        m_taps.clear();
    m_taps.append(now);
    while (m_taps.size() > 16)
        m_taps.removeFirst();
    if (m_taps.size() >= 4) {
        const double ms = double(m_taps.last() - m_taps.first()) / double(m_taps.size() - 1);
        if (ms > 0)
            setBpm(std::round(60000.0 / ms * 10.0) / 10.0);
    }
}

void DeckController::setDownbeatHere()
{
    if (m_rec.id.isEmpty() || !m_rec.grid().valid())
        return;
    const double bar = 4.0 * m_rec.grid().secPerBeat();
    m_rec.firstBeatSec = m_position - bar * std::floor(m_position / bar);
    m_rec.gridManual = true;
    sendGrid();
    computeLoop();
    sendLoop();
    commitRecord();
    emit gridChanged();
    emit loopChanged();
}

void DeckController::nudgeGrid(double ms)
{
    if (m_rec.id.isEmpty() || !m_rec.grid().valid())
        return;
    m_rec.firstBeatSec += ms / 1000.0;
    m_rec.gridManual = true;
    sendGrid();
    computeLoop();
    sendLoop();
    commitRecord();
    emit gridChanged();
    emit loopChanged();
}

void DeckController::autoGrid()
{
    if (!m_rec.id.isEmpty())
        m_library->autoDetect(m_rec.id, true, false);
}

// ---------------------------------------------------------------- key

void DeckController::autoKey()
{
    if (!m_rec.id.isEmpty())
        m_library->autoDetect(m_rec.id, false, true);
}

void DeckController::setKey(int key)
{
    if (m_rec.id.isEmpty())
        return;
    m_rec.key = keys::valid(key) ? key : -1;
    m_rec.keyManual = true;
    commitRecord();
    emit keyChanged();
}

void DeckController::setKeyShift(int st)
{
    st = std::clamp(st, -12, 12);
    if (st == m_keyShift)
        return;
    m_keyShift = st;
    m_engine->deck(m_index).semitones.store(st);
    emit keyChanged();
}

// ---------------------------------------------------------------- loops

void DeckController::setSync(bool on)
{
    if (on == m_sync)
        return;
    m_sync = on;
    m_engine->post(m_index, {DeckCommand::SetSync, on ? 1.0 : 0.0});
    emit syncChanged();
    updatePlayingBpm();
}

void DeckController::setLoopEnabled(bool on)
{
    if (on == m_loopEnabled)
        return;
    m_loopEnabled = on;
    m_engine->post(m_index, {DeckCommand::SetLoopEnabled, on ? 1.0 : 0.0});
    emit loopChanged();
}

void DeckController::setLoopBars(int bars)
{
    bars = std::clamp(bars, 1, 64);
    if (bars == m_loopBars)
        return;
    m_loopBars = bars;
    emit loopBarsChanged();
    if (m_rec.hasManualLoop()) {
        m_rec.loopEndBeat = m_rec.loopStartBeat + 4.0 * bars;
        commitRecord();
    }
    computeLoop();
    sendLoop();
    emit loopChanged();
}

void DeckController::setLoopBeats(double startBeat, double endBeat)
{
    if (m_rec.id.isEmpty() || !m_rec.grid().valid())
        return;
    if (endBeat < startBeat)
        std::swap(startBeat, endBeat);
    if (endBeat - startBeat < 1.0)
        return;
    m_rec.loopStartBeat = startBeat;
    m_rec.loopEndBeat = endBeat;
    commitRecord();
    computeLoop();
    sendLoop();
    if (!m_loopEnabled)
        setLoopEnabled(true);
    emit loopChanged();
}

void DeckController::loopHere()
{
    if (!m_rec.grid().valid())
        return;
    const double start = std::floor(beat() / 4.0) * 4.0;
    setLoopBeats(start, start + 4.0 * m_loopBars);
}

void DeckController::endLoop()
{
    if (m_rec.id.isEmpty())
        return;
    m_rec.loopStartBeat = m_rec.loopEndBeat = 0.0;
    commitRecord();
    computeLoop();
    sendLoop();
    emit loopChanged();
}

void DeckController::setVolume(double v)
{
    v = std::clamp(v, 0.0, 1.0);
    if (v == m_volume)
        return;
    m_volume = v;
    m_engine->deck(m_index).volume.store(float(v * v)); // simple fader taper
    emit volumeChanged();
}

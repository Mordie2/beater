#include "app/AppController.h"

#include <QSettings>

#include <cmath>

AppController::AppController(QObject *parent)
    : QObject(parent)
    , m_engine(std::make_unique<AudioEngine>())
{
    QSettings s;
    m_masterBpm = s.value("masterBpm", 125.0).toDouble();
    m_masterVolume = s.value("masterVolume", 0.85).toDouble();
    m_phaseMode = s.value("phaseMode", 0).toInt();

    std::string err;
    if (m_engine->start(&err)) {
        m_audioOk = true;
        m_audioStatus = QStringLiteral("%1 · %2 kHz")
                            .arg(QString::fromStdString(m_engine->deviceName()))
                            .arg(m_engine->sampleRate() / 1000.0, 0, 'f', 1);
    } else {
        // Keep the UI usable without a device (decks won't make sound).
        m_engine->initOffline(48000);
        m_audioStatus = QString::fromStdString(err);
    }
    m_engine->setMasterBpm(m_masterBpm);
    m_engine->setMasterVolume(float(m_masterVolume * m_masterVolume));
    m_engine->setPhaseMode(PhaseMode(m_phaseMode));

    m_library = std::make_unique<Library>();
    for (int i = 0; i < 2; ++i)
        m_decks[i] = std::make_unique<DeckController>(i, m_engine.get(), m_library.get());
    connect(m_library.get(), &Library::recordChanged, this, [this](const QString &id) {
        for (auto &d : m_decks)
            d->refreshFromLibrary(id);
    });

    m_timer.setInterval(16);
    m_timer.setTimerType(Qt::PreciseTimer);
    connect(&m_timer, &QTimer::timeout, this, &AppController::tick);
    m_timer.start();
}

AppController::~AppController()
{
    m_timer.stop();
    QSettings s;
    s.setValue("masterBpm", m_masterBpm);
    s.setValue("masterVolume", m_masterVolume);
    s.setValue("phaseMode", m_phaseMode);
    m_engine->stop();
}

void AppController::tick()
{
    m_engine->collectGarbage();
    for (auto &d : m_decks)
        d->poll();
    m_masterBeat = m_engine->masterBeat();
    m_masterLevel = m_engine->masterLevel();
    emit masterBeatChanged();
}

void AppController::setMasterBpm(double bpm)
{
    bpm = std::clamp(std::round(bpm * 100.0) / 100.0, 60.0, 200.0);
    if (bpm == m_masterBpm)
        return;
    m_masterBpm = bpm;
    m_engine->setMasterBpm(bpm);
    for (auto &d : m_decks)
        d->masterTempoChanged();
    emit masterBpmChanged();
}

void AppController::setMasterVolume(double v)
{
    v = std::clamp(v, 0.0, 1.0);
    if (v == m_masterVolume)
        return;
    m_masterVolume = v;
    m_engine->setMasterVolume(float(v * v));
    emit masterVolumeChanged();
}

void AppController::setPhaseMode(int m)
{
    m = m ? 1 : 0;
    if (m == m_phaseMode)
        return;
    m_phaseMode = m;
    m_engine->setPhaseMode(PhaseMode(m));
    emit phaseModeChanged();
}

void AppController::loadToDeck(int row, int deck)
{
    const QString id = m_library->idAt(row);
    if (id.isEmpty() || deck < 0 || deck > 1)
        return;
    m_decks[deck]->loadRecord(m_library->record(id));
}

void AppController::loadToFreeDeck(int row)
{
    // Prefer an empty deck, then one that isn't playing, then the quieter one.
    int target = 0;
    if (!m_decks[0]->loaded())
        target = 0;
    else if (!m_decks[1]->loaded())
        target = 1;
    else if (!m_decks[0]->playing())
        target = 0;
    else if (!m_decks[1]->playing())
        target = 1;
    else
        target = m_decks[0]->volume() <= m_decks[1]->volume() ? 0 : 1;
    loadToDeck(row, target);
}

void AppController::matchMasterTo(int deck)
{
    if (deck >= 0 && deck < 2 && m_decks[deck]->bpm() > 0)
        setMasterBpm(m_decks[deck]->bpm());
}

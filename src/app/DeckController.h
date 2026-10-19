#pragma once
#include "app/TrackRecord.h"
#include "core/TrackBuffer.h"

#include <QElapsedTimer>
#include <QObject>
#include <QtQml/qqmlregistration.h>

class AudioEngine;
class Library;

// UI-side view of one deck. Sends commands to the engine, polls its published
// state, and writes edits (grid, key, loop) back to the library.
class DeckController : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Owned by AppController")

    Q_PROPERTY(int index READ index CONSTANT)
    Q_PROPERTY(QString name READ name CONSTANT)
    Q_PROPERTY(bool loaded READ loaded NOTIFY trackChanged)
    Q_PROPERTY(bool loading READ loading NOTIFY loadingChanged)
    Q_PROPERTY(QString title READ title NOTIFY trackChanged)
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)
    Q_PROPERTY(double duration READ duration NOTIFY trackChanged)

    Q_PROPERTY(double position READ position NOTIFY positionChanged)
    Q_PROPERTY(double beat READ beat NOTIFY positionChanged)
    Q_PROPERTY(bool playing READ playing NOTIFY playingChanged)
    Q_PROPERTY(double level READ level NOTIFY levelChanged)
    Q_PROPERTY(double phaseError READ phaseError NOTIFY positionChanged)

    Q_PROPERTY(double bpm READ bpm NOTIFY gridChanged)
    Q_PROPERTY(double firstBeat READ firstBeat NOTIFY gridChanged)
    Q_PROPERTY(bool gridManual READ gridManual NOTIFY gridChanged)
    Q_PROPERTY(bool analyzing READ analyzing NOTIFY analyzingChanged)
    Q_PROPERTY(double playingBpm READ playingBpm NOTIFY playingBpmChanged)

    Q_PROPERTY(int key READ key NOTIFY keyChanged)
    Q_PROPERTY(bool keyManual READ keyManual NOTIFY keyChanged)
    Q_PROPERTY(int keyShift READ keyShift WRITE setKeyShift NOTIFY keyChanged)
    Q_PROPERTY(QString keyText READ keyText NOTIFY keyChanged)
    Q_PROPERTY(QString shiftedKeyText READ shiftedKeyText NOTIFY keyChanged)
    Q_PROPERTY(QStringList keyNames READ keyNames NOTIFY keyChanged) // 24 entries, index = key

    Q_PROPERTY(bool sync READ sync WRITE setSync NOTIFY syncChanged)
    Q_PROPERTY(bool loopEnabled READ loopEnabled WRITE setLoopEnabled NOTIFY loopChanged)
    Q_PROPERTY(double loopStartBeat READ loopStartBeat NOTIFY loopChanged)
    Q_PROPERTY(double loopEndBeat READ loopEndBeat NOTIFY loopChanged)
    Q_PROPERTY(bool loopAuto READ loopAuto NOTIFY loopChanged)
    Q_PROPERTY(int loopBars READ loopBars WRITE setLoopBars NOTIFY loopBarsChanged)
    Q_PROPERTY(bool inLoop READ inLoop NOTIFY positionChanged)
    // The intro loop: while armed, the deck loops the intro (the first N bars,
    // or a drawn one) instead of its main loop. Armed on every load; release it
    // to play on. It releases itself once the playhead is past it.
    Q_PROPERTY(bool introArmed READ introArmed WRITE setIntroArmed NOTIFY loopChanged)
    Q_PROPERTY(bool introActive READ introActive NOTIFY loopChanged)
    Q_PROPERTY(double introStartBeat READ introStartBeat NOTIFY loopChanged)
    Q_PROPERTY(double introEndBeat READ introEndBeat NOTIFY loopChanged)
    Q_PROPERTY(bool introAuto READ introAuto NOTIFY loopChanged)
    Q_PROPERTY(int introBars READ introBars WRITE setIntroBars NOTIFY loopBarsChanged)
    // The region the engine loops right now (the intro or the main loop).
    Q_PROPERTY(double activeLoopStartBeat READ activeLoopStartBeat NOTIFY loopChanged)
    Q_PROPERTY(double activeLoopEndBeat READ activeLoopEndBeat NOTIFY loopChanged)

    Q_PROPERTY(double volume READ volume WRITE setVolume NOTIFY volumeChanged)
    Q_PROPERTY(int waveformRevision READ waveformRevision NOTIFY trackChanged)

public:
    DeckController(int index, AudioEngine *engine, Library *library, QObject *parent = nullptr);

    int index() const { return m_index; }
    QString name() const { return m_index == 0 ? QStringLiteral("A") : QStringLiteral("B"); }
    bool loaded() const { return bool(m_buffer); }
    bool loading() const { return m_loading; }
    QString title() const { return m_rec.title; }
    QString error() const { return m_error; }
    double duration() const { return m_buffer ? m_buffer->durationSec() : 0.0; }

    double position() const { return m_position; }
    double beat() const;
    bool playing() const { return m_playing; }
    double level() const { return m_level; }
    double phaseError() const { return m_phaseError; }

    double bpm() const { return m_rec.bpm; }
    double firstBeat() const { return m_rec.firstBeatSec; }
    bool gridManual() const { return m_rec.gridManual; }
    bool analyzing() const { return m_rec.status == QLatin1String("analyzing"); }
    double playingBpm() const { return m_playingBpm; }

    int key() const { return m_rec.key; }
    bool keyManual() const { return m_rec.keyManual; }
    int keyShift() const { return m_keyShift; }
    void setKeyShift(int st);
    QString keyText() const;
    QString shiftedKeyText() const;

    bool sync() const { return m_sync; }
    void setSync(bool on);
    bool loopEnabled() const { return m_loopEnabled; }
    void setLoopEnabled(bool on);
    double loopStartBeat() const { return m_loopStart; }
    double loopEndBeat() const { return m_loopEnd; }
    bool loopAuto() const { return !m_rec.hasManualLoop(); }
    int loopBars() const { return m_loopBars; }
    void setLoopBars(int bars);
    bool inLoop() const;
    bool introArmed() const { return m_introArmed; }
    void setIntroArmed(bool on);
    bool introActive() const { return m_introArmed && m_introEnd > m_introStart; }
    double introStartBeat() const { return m_introStart; }
    double introEndBeat() const { return m_introEnd; }
    bool introAuto() const { return !m_rec.hasManualIntro(); }
    int introBars() const { return m_introBars; }
    void setIntroBars(int bars);
    double activeLoopStartBeat() const { return introActive() ? m_introStart : m_loopStart; }
    double activeLoopEndBeat() const { return introActive() ? m_introEnd : m_loopEnd; }

    double volume() const { return m_volume; }
    void setVolume(double v);
    int waveformRevision() const { return m_waveRev; }

    const TrackBuffer *buffer() const { return m_buffer.get(); }
    BeatGrid grid() const { return m_rec.grid(); }
    QString trackId() const { return m_rec.id; }

    // transport
    Q_INVOKABLE void togglePlay();
    Q_INVOKABLE void play();
    Q_INVOKABLE void pause();
    Q_INVOKABLE void cue();
    Q_INVOKABLE void seek(double sec);
    Q_INVOKABLE void jumpBeats(double beats);

    // While the user drags through the track: where it will land. Waveforms
    // draw shownPosition(); commitPreview() jumps there (on the beat, or in
    // phase with the master when synced and playing).
    void setPreview(double sec);
    void commitPreview();
    double shownPosition() const;

    // grid
    Q_INVOKABLE void setBpm(double bpm);
    Q_INVOKABLE void doubleBpm() { setBpm(m_rec.bpm * 2.0); }
    Q_INVOKABLE void halveBpm() { setBpm(m_rec.bpm / 2.0); }
    Q_INVOKABLE void tap();
    Q_INVOKABLE void setDownbeatHere();
    Q_INVOKABLE void nudgeGrid(double ms);
    Q_INVOKABLE void autoGrid(); // drop the manual grid, detect it again

    // key
    Q_INVOKABLE void setKey(int key);
    Q_INVOKABLE void autoKey(); // drop the manual key, detect it again
    QStringList keyNames() const;

    // loops (beats on the track grid)
    Q_INVOKABLE void setLoopBeats(double startBeat, double endBeat);
    Q_INVOKABLE void loopHere();
    Q_INVOKABLE void endLoop(); // back to automatic last-N-bars loop
    Q_INVOKABLE void setIntroBeats(double startBeat, double endBeat);
    Q_INVOKABLE void autoIntro(); // back to the automatic first-N-bars intro

    void loadRecord(const TrackRecord &rec);
    void poll(); // called by AppController at display rate
    void refreshFromLibrary(const QString &id);
    void masterTempoChanged() { updatePlayingBpm(); }

signals:
    void trackChanged();
    void loadingChanged();
    void errorChanged();
    void positionChanged();
    void playingChanged();
    void levelChanged();
    void gridChanged();
    void analyzingChanged();
    void playingBpmChanged();
    void keyChanged();
    void syncChanged();
    void loopChanged();
    void loopBarsChanged();
    void volumeChanged();

private:
    void sendGrid();
    void sendLoop();
    void computeLoop();
    void commitRecord(); // write m_rec to the library
    void updatePlayingBpm();
    double snapToBeat(double sec) const;

    int m_index;
    AudioEngine *m_engine;
    Library *m_library;

    TrackRecord m_rec;
    TrackBufferPtr m_buffer;
    bool m_loading = false;
    int m_loadSerial = 0;
    QString m_error;
    int m_waveRev = 0;

    double m_position = 0.0;
    bool m_playing = false;
    double m_level = 0.0;
    double m_phaseError = 0.0;
    double m_playingBpm = 0.0;

    int m_keyShift = 0;
    bool m_sync = true;
    bool m_loopEnabled = true;
    int m_loopBars = 8;
    double m_loopStart = 0.0, m_loopEnd = 0.0;
    bool m_introArmed = true;
    int m_introBars = 8;
    double m_introStart = 0.0, m_introEnd = 0.0;
    double m_volume = 0.8;

    double m_preview = -1.0;     // < 0: not dragging
    QElapsedTimer m_previewHold; // after commit, until the seek lands
    QElapsedTimer m_loadedAt;

    QList<qint64> m_taps;
    QElapsedTimer m_tapClock;
};

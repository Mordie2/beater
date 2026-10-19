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
    // ---- loops. Three per track; the engine loops one at a time:
    //  intro   the first N bars (or drawn). Armed on every load, so play loops
    //          it until it's released.
    //  custom  In / Out markers anywhere (or drawn). Out starts it.
    //  end     the last N bars (or drawn). Armed by default, so a track that
    //          runs out keeps looping its ending.
    // In use: the custom loop if on, else the intro if armed, else the end.
    // Switching the intro or the custom loop on with the playhead past it
    // jumps back in (in phase when playing); one the playhead has left behind
    // releases itself, so the end loop still catches the track.
    Q_PROPERTY(QString activeLoop READ activeLoop NOTIFY loopChanged) // "intro", "custom", "end" or ""
    Q_PROPERTY(double activeLoopStartBeat READ activeLoopStartBeat NOTIFY loopChanged)
    Q_PROPERTY(double activeLoopEndBeat READ activeLoopEndBeat NOTIFY loopChanged)
    Q_PROPERTY(bool inLoop READ inLoop NOTIFY positionChanged)

    Q_PROPERTY(bool introArmed READ introArmed WRITE setIntroArmed NOTIFY loopChanged)
    Q_PROPERTY(double introStartBeat READ introStartBeat NOTIFY loopChanged)
    Q_PROPERTY(double introEndBeat READ introEndBeat NOTIFY loopChanged)
    Q_PROPERTY(bool introAuto READ introAuto NOTIFY loopChanged)

    Q_PROPERTY(bool customOn READ customOn WRITE setCustomOn NOTIFY loopChanged)
    Q_PROPERTY(bool hasCustom READ hasCustom NOTIFY loopChanged)        // both markers placed
    Q_PROPERTY(bool customPending READ customPending NOTIFY loopChanged) // In placed, waiting for Out
    Q_PROPERTY(double customStartBeat READ customStartBeat NOTIFY loopChanged)
    Q_PROPERTY(double customEndBeat READ customEndBeat NOTIFY loopChanged)

    Q_PROPERTY(bool endArmed READ endArmed WRITE setEndArmed NOTIFY loopChanged)
    Q_PROPERTY(double endStartBeat READ endStartBeat NOTIFY loopChanged)
    Q_PROPERTY(double endEndBeat READ endEndBeat NOTIFY loopChanged)
    Q_PROPERTY(bool endAuto READ endAuto NOTIFY loopChanged)

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
    QString activeLoop() const;
    double activeLoopStartBeat() const;
    double activeLoopEndBeat() const;
    bool inLoop() const;

    bool introArmed() const { return m_introArmed; }
    void setIntroArmed(bool on);
    double introStartBeat() const { return m_introStart; }
    double introEndBeat() const { return m_introEnd; }
    bool introAuto() const { return !m_rec.hasManualIntro(); }

    bool customOn() const { return m_customOn; }
    void setCustomOn(bool on);
    bool hasCustom() const { return m_customEnd > m_customStart; }
    bool customPending() const { return m_customPending; }
    double customStartBeat() const { return m_customStart; }
    double customEndBeat() const { return m_customEnd; }

    bool endArmed() const { return m_endArmed; }
    void setEndArmed(bool on);
    double endStartBeat() const { return m_endStart; }
    double endEndBeat() const { return m_endEnd; }
    bool endAuto() const { return !m_rec.hasManualLoop(); }

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
    Q_INVOKABLE void setDownbeatAt(double sec); // a bar line there, the grid kept
    Q_INVOKABLE void autoGrid(); // drop the manual grid, detect it again

    // key
    Q_INVOKABLE void setKey(int key);
    Q_INVOKABLE void autoKey(); // drop the manual key, detect it again
    QStringList keyNames() const;

    // loops (beats on the track grid)
    Q_INVOKABLE void loopIn();   // In marker at the playhead (nearest beat)
    Q_INVOKABLE void loopOut();  // Out marker at the playhead: the In / Out loop starts
    Q_INVOKABLE void loopHere(); // an In / Out loop of the last chosen length from this bar
    Q_INVOKABLE void setLoopLength(int bars); // the loop in use (or the next Here)
    Q_INVOKABLE void setIntroBeats(double startBeat, double endBeat);
    Q_INVOKABLE void autoIntro(); // back to the automatic first-N-bars intro
    Q_INVOKABLE void setCustomBeats(double startBeat, double endBeat);
    Q_INVOKABLE void clearCustom();
    Q_INVOKABLE void setEndBeats(double startBeat, double endBeat);
    Q_INVOKABLE void autoEnd(); // back to the automatic last-N-bars loop

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
    void volumeChanged();

private:
    void sendGrid();
    void sendLoop();
    void computeLoop();
    void jumpInto(double startBeat, double endBeat); // into a loop the playhead is past
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
    int m_introBars = 8, m_endBars = 8, m_customBars = 4; // auto lengths, Here's length
    bool m_introArmed = true;
    double m_introStart = 0.0, m_introEnd = 0.0;
    bool m_customOn = false;
    bool m_customPending = false; // In placed, no Out yet (start == end)
    double m_customStart = 0.0, m_customEnd = 0.0;
    bool m_endArmed = true;
    double m_endStart = 0.0, m_endEnd = 0.0;
    QElapsedTimer m_loopSwitchedAt; // the auto-release waits for a jump in to land
    double m_volume = 0.8;

    double m_preview = -1.0;     // < 0: not dragging
    QElapsedTimer m_previewHold; // after commit, until the seek lands
    QElapsedTimer m_loadedAt;

    QList<qint64> m_taps;
    QElapsedTimer m_tapClock;
};

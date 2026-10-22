#pragma once
#include <QObject>
#include <QProcess>
#include <QTimer>
#include <QLocalSocket>
#include <QElapsedTimer>
#include <QVariantList>
#include <QJsonArray>
#include "WordTiming.h"

class TranscriptController : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString title READ title NOTIFY changed)
    Q_PROPERTY(QString deck READ deck NOTIFY changed)
    Q_PROPERTY(QString text READ text NOTIFY changed)
    Q_PROPERTY(QString status READ status NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(bool canRetry READ canRetry NOTIFY changed)
    Q_PROPERTY(QVariantList words READ words NOTIFY wordsChanged)
    Q_PROPERTY(int activeWord READ activeWord NOTIFY activeWordChanged)
    Q_PROPERTY(double position READ position NOTIFY playbackChanged)
    Q_PROPERTY(bool playing READ playing NOTIFY playbackChanged)
    Q_PROPERTY(bool playbackConnected READ playbackConnected NOTIFY playbackChanged)
    Q_PROPERTY(int elapsedSeconds READ elapsedSeconds NOTIFY changed)
    Q_PROPERTY(QString language READ language NOTIFY changed)
    Q_PROPERTY(QString backend READ backend NOTIFY changed)
public:
    explicit TranscriptController(const QString &session, QObject *parent = nullptr);
    ~TranscriptController() override;
    QString title() const { return m_title; }
    QString deck() const { return m_deck; }
    QString text() const { return m_text; }
    QString status() const { return m_status; }
    bool busy() const { return m_busy; }
    bool canRetry() const { return !m_busy && !m_vocals.isEmpty(); }
    QVariantList words() const { return m_words; }
    int activeWord() const { return m_activeWord; }
    double position() const { return m_position; }
    bool playing() const { return m_playing; }
    bool playbackConnected() const { return m_playbackConnected; }
    int elapsedSeconds() const { return m_workClock.isValid() ? int(m_workClock.elapsed() / 1000) : 0; }
    QString language() const { return m_language; }
    QString backend() const { return m_backend; }
    Q_INVOKABLE void setLanguage(const QString &language);
    Q_INVOKABLE void retry();
    Q_INVOKABLE void copyText();
    Q_INVOKABLE void openFolder();
signals:
    void changed();
    void trackChanged();
    void revealRequested();
    void wordsChanged();
    void activeWordChanged();
    void playbackChanged();
private:
    void poll();
    void start(bool force = false, bool cpuRetry = false);
    void stop();
    void readOutput();
    void finish(int code, QProcess::ExitStatus state);
    void readPlayback();
    void updateHighlight();
    void appendWords(const QJsonArray &words);
    void clearWords();
    QString m_session, m_title, m_deck, m_text, m_status = "Waiting for a track…", m_vocals;
    QString m_language = "auto", m_backend;
    bool m_attemptedGpu = false, m_cpuRetry = false, m_forceRetry = false;
    int m_generation = -1;
    int m_showRevision = -1;
    bool m_busy = true, m_done = false, m_stopping = false;
    QTimer m_timer;
    QProcess m_process;
    QByteArray m_output, m_errors;
    QLocalSocket m_socket;
    QByteArray m_playbackBuffer;
    QElapsedTimer m_lastPlayback;
    QElapsedTimer m_workClock;
    double m_duration = 0;
    QVariantList m_words;
    std::vector<WordTiming> m_wordTimes;
    int m_activeWord = -1;
    double m_position = 0.0;
    bool m_playing = false, m_playbackConnected = false;
};

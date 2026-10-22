#pragma once
#include <QProcess>
#include <QTemporaryDir>
#include <QJsonObject>
#include <QLocalServer>
#include <QLocalSocket>
#include <QPointer>

// One independent preview process per deck; only a small status file is shared.
class TranscriptPreview {
public:
    TranscriptPreview();
    ~TranscriptPreview();
    void load(const QString &deck, const QString &title, int generation);
    void update(const QString &status, const QString &vocals, bool busy);
    void playback(double position, bool playing);
    void show();
private:
    void write();
    void start();
    QTemporaryDir m_directory;
    QProcess m_process;
    QJsonObject m_state;
    QLocalServer m_server;
    QPointer<QLocalSocket> m_socket;
    QByteArray m_playback;
    int m_showRevision = 0, m_restartAttempts = 0;
    bool m_pendingShow = false;
};

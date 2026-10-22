#include "TranscriptPreview.h"
#include <QCoreApplication>
#include <QJsonDocument>
#include <QSaveFile>
#include <QDebug>
#include <QUuid>
#include <QTimer>

TranscriptPreview::TranscriptPreview()
{
    m_server.setSocketOptions(QLocalServer::UserAccessOption);
    m_server.listen("beater-words-" + QUuid::createUuid().toString(QUuid::WithoutBraces));
    QObject::connect(&m_server, &QLocalServer::newConnection, &m_server, [this] {
        while (m_server.hasPendingConnections()) {
            if (m_socket) { m_socket->abort(); m_socket->deleteLater(); }
            m_socket = m_server.nextPendingConnection();
            auto *socket = m_socket.data();
            QObject::connect(socket, &QLocalSocket::readyRead, &m_server, [this, socket, buffer = QByteArray()]() mutable {
                buffer += socket->readAll();
                while (buffer.contains('\n')) {
                    const auto end = buffer.indexOf('\n');
                    const auto ack = QJsonDocument::fromJson(buffer.left(end)).object();
                    buffer.remove(0, end + 1);
                    if (ack["shown"].toInt(-1) == m_showRevision) m_pendingShow = false;
                }
                if (buffer.size() > 4096) buffer.clear();
            });
            if (!m_playback.isEmpty()) m_socket->write(m_playback);
        }
    });
    QObject::connect(&m_process, &QProcess::finished, &m_process, [this] {
        // A load can race with the old window closing. Retry only an unacknowledged
        // show request, once; an intentional close after showing stays closed.
        if (m_pendingShow && m_restartAttempts++ == 0)
            QTimer::singleShot(0, &m_process, [this] { if (m_pendingShow) start(); });
    });
    QObject::connect(&m_process, &QProcess::errorOccurred, &m_process, [this] {
        qWarning() << "Lyrics window:" << m_process.errorString();
    });
}

TranscriptPreview::~TranscriptPreview()
{
    m_pendingShow = false;
    m_process.disconnect();
    if (m_process.state() != QProcess::NotRunning) {
        m_state["stage"] = "closed";
        write();
        if (!m_process.waitForFinished(2000)) {
            m_process.terminate();
            if (!m_process.waitForFinished(1000)) {
                m_process.kill();
                m_process.waitForFinished(2000);
            }
        }
    }
}

void TranscriptPreview::write()
{
    QSaveFile f(m_directory.filePath("session.json"));
    if (f.open(QIODevice::WriteOnly)) {
        f.write(QJsonDocument(m_state).toJson());
        f.commit();
    }
}

void TranscriptPreview::load(const QString &deck, const QString &title, int generation)
{
    m_state = {{"deck", deck}, {"title", title}, {"generation", generation},
               {"stage", "waiting"}, {"status", "Loading track…"}, {"vocals", ""},
               {"playbackSocket", m_server.serverName()}};
    playback(0.0, false);
    show();
}

void TranscriptPreview::show()
{
    if (m_state.isEmpty()) return;
    m_state["showRevision"] = ++m_showRevision;
    m_pendingShow = true;
    m_restartAttempts = 0;
    write();
    start();
}

void TranscriptPreview::start()
{
    if (m_process.state() == QProcess::NotRunning) {
        QString executable = QCoreApplication::applicationDirPath() + "/beater-transcript";
#ifdef Q_OS_WIN
        executable += ".exe";
#endif
        m_process.setStandardOutputFile(QProcess::nullDevice());
        m_process.setStandardErrorFile(m_directory.filePath("preview.log"));
        m_process.start(executable, {"--track-session", m_directory.filePath("session.json")});
    }
}

void TranscriptPreview::playback(double position, bool playing)
{
    if (m_state.isEmpty()) return;
    m_playback = QJsonDocument(QJsonObject{{"generation", m_state["generation"]},
        {"position", position}, {"playing", playing}}).toJson(QJsonDocument::Compact) + '\n';
    // Only the UI thread writes to the local pipe. Avoid accumulating stale frames.
    if (m_socket && m_socket->state() == QLocalSocket::ConnectedState && m_socket->bytesToWrite() < 4096)
        m_socket->write(m_playback);
}

void TranscriptPreview::update(const QString &status, const QString &vocals, bool busy)
{
    if (m_state.isEmpty()) return;
    m_state["stage"] = busy ? "waiting" : vocals.isEmpty() ? "error" : "ready";
    m_state["status"] = status;
    m_state["vocals"] = busy ? QString() : vocals;
    write();
}

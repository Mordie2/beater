#include "TranscriptController.h"
#include <QClipboard>
#include <QCoreApplication>
#include <QDesktopServices>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcessEnvironment>
#include <QUrl>

TranscriptController::TranscriptController(const QString &session, QObject *parent)
    : QObject(parent), m_session(session)
{
    connect(&m_socket, &QLocalSocket::readyRead, this, &TranscriptController::readPlayback);
    connect(&m_socket, &QLocalSocket::disconnected, this, [this] {
        m_playbackConnected = false;
        m_playing = false;
        m_playbackBuffer.clear();
        updateHighlight();
        emit playbackChanged();
    });
    connect(&m_timer, &QTimer::timeout, this, &TranscriptController::poll);
    connect(&m_process, &QProcess::readyReadStandardOutput, this, &TranscriptController::readOutput);
    connect(&m_process, &QProcess::readyReadStandardError, this, [this] {
        m_errors = (m_errors + m_process.readAllStandardError()).right(8192);
    });
    connect(&m_process, &QProcess::finished, this, &TranscriptController::finish);
    connect(&m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError e) {
        if (!m_stopping && e == QProcess::FailedToStart) {
            m_status = "Could not start speech recognition. Check the bundled runtime.";
            m_busy = false;
            emit changed();
        }
    });
    m_timer.start(250);
    poll();
}

TranscriptController::~TranscriptController() { stop(); }
void TranscriptController::stop()
{
    m_stopping = true;
    if (m_process.state() != QProcess::NotRunning) {
        m_process.kill();
        m_process.waitForFinished(3000);
    }
    m_stopping = false;
}

void TranscriptController::poll()
{
    QFile f(m_session);
    if (!f.open(QIODevice::ReadOnly)) {
        // The parent removes its private session directory on exit.
        if (m_generation >= 0) QCoreApplication::quit();
        return;
    }
    const auto data = QJsonDocument::fromJson(f.readAll()).object();
    if (data.isEmpty()) return;
    if (data["stage"] == "closed") { QCoreApplication::quit(); return; }
    const int generation = data["generation"].toInt();
    if (generation != m_generation) {
        stop();
        m_generation = generation;
        m_workClock.start();
        m_title = data["title"].toString();
        m_deck = data["deck"].toString();
        m_vocals.clear();
        m_backend.clear();
        m_text.clear();
        clearWords();
        m_position = 0.0;
        m_playing = false;
        m_playbackConnected = false;
        m_lastPlayback.invalidate();
        emit playbackChanged();
        m_done = false;
        m_busy = true;
        emit trackChanged();
    }
    const QString socketName = data["playbackSocket"].toString();
    if (!socketName.isEmpty() && m_socket.state() == QLocalSocket::UnconnectedState) {
        m_socket.connectToServer(socketName, QIODevice::ReadWrite);
    }
    const int showRevision = data["showRevision"].toInt();
    if (showRevision != m_showRevision) {
        m_showRevision = showRevision;
        emit revealRequested();
    }
    if (m_socket.state() == QLocalSocket::ConnectedState)
        m_socket.write(QJsonDocument(QJsonObject{{"shown", m_showRevision}}).toJson(QJsonDocument::Compact) + '\n');
    if (m_playbackConnected && m_lastPlayback.isValid() && m_lastPlayback.elapsed() > 1000) {
        m_playbackConnected = false;
        m_playing = false;
        updateHighlight();
        emit playbackChanged();
    }
    const QString stage = data["stage"].toString();
    if (stage == "ready") {
        const QString path = data["vocals"].toString();
        if (!path.isEmpty() && path != m_vocals) {
            m_vocals = path;
            start();
        }
    } else if (m_process.state() == QProcess::NotRunning && m_vocals.isEmpty()) {
        m_status = stage == "error" ? data["status"].toString() + " · retry vocals in beater"
                                    : data["status"].toString() + " · waiting for vocals";
        m_busy = stage != "error";
    }
    emit changed();
}

void TranscriptController::start(bool force, bool cpuRetry)
{
    stop();
    m_text.clear();
    clearWords();
    m_done = false;
    m_output.clear();
    m_errors.clear();
    m_backend.clear();
    m_attemptedGpu = false;
    m_cpuRetry = cpuRetry;
    m_forceRetry = force;
    m_busy = true;
    m_status = "Starting local transcription…";
    m_duration = 0;
    m_workClock.start();
    const QString dir = QCoreApplication::applicationDirPath();
    const QString python = dir + "/stem-runtime/python.exe";
    const QString worker = dir + "/transcript-worker.py";
    if (!QFileInfo::exists(python) || !QFileInfo::exists(worker)) {
        m_status = "Speech recognition is missing from this build.";
        m_busy = false;
        emit changed();
        return;
    }
    auto env = QProcessEnvironment::systemEnvironment();
    env.insert("PYTHONUTF8", "1");
    env.insert("PYTHONUNBUFFERED", "1");
    env.insert("HF_HUB_OFFLINE", "1");
    m_process.setProcessEnvironment(env);
    QStringList args{worker, "--input", m_vocals, "--model", dir + "/stem-runtime/models/whisper-large-v3",
                     "--language", m_language};
    if (force) args << "--force";
    if (cpuRetry) args << "--device" << "cpu";
    m_process.start(python, args);
    emit changed();
}

void TranscriptController::readOutput()
{
    m_output += m_process.readAllStandardOutput();
    while (m_output.contains('\n')) {
        const auto end = m_output.indexOf('\n');
        const auto o = QJsonDocument::fromJson(m_output.left(end)).object();
        m_output.remove(0, end + 1);
        if (m_stopping) continue;
        const QString state = o["state"].toString();
        if (state == "loading") {
            m_attemptedGpu = o["device"] == "cuda";
            m_backend = o["model"].toString() + (m_attemptedGpu ? " · NVIDIA GPU" : " · CPU");
            m_status = m_cpuRetry ? "GPU unavailable · loading the same model on CPU…" : "Loading speech recognition…";
        }
        else if (state == "queued") m_status = "Waiting for the other deck's recognition…";
        else if (state == "reset") {
            m_text.clear(); clearWords();
            m_status = o["message"].toString();
        }
        else if (state == "aligning") m_status = "Refining word timing…";
        else if (state == "notice") m_status = o["message"].toString();
        else if (state == "transcribing") {
            m_duration = o["duration"].toDouble();
            m_status = "Transcribing vocals · " + o["language"].toString();
        }
        else if (state == "segment") {
            if (!m_text.isEmpty()) m_text += "\n\n";
            m_text += o["text"].toString();
            appendWords(o["words"].toArray());
            m_status = QStringLiteral("Transcribing · %1 / %2 seconds processed")
                .arg(o["end"].toDouble(), 0, 'f', 0).arg(m_duration, 0, 'f', 0);
        } else if (state == "ready") {
            m_text = o["text"].toString();
            clearWords();
            for (const auto &segment : o["segments"].toArray())
                appendWords(segment.toObject()["words"].toArray());
            m_done = true;
            m_backend = o["model"].toString() + (o["device"] == "cuda" ? " · NVIDIA GPU" : " · CPU");
            if (o["cached"].toBool()) m_backend += " · saved result";
            const auto alignment = o["alignment"].toObject();
            if (alignment["refined_words"].toInt() > 0) m_backend += " · timing refined";
            else m_backend += " · Whisper timing";
            m_status = m_text.isEmpty() ? "No clear speech detected in this vocal stem."
                : (o["cached"].toBool() ? "Transcript ready · cached · " : "Transcript ready · ") + o["language"].toString();
        } else if (state == "error") {
            m_status = "Transcription failed: " + o["message"].toString().left(300);
        }
        emit changed();
    }
}

void TranscriptController::finish(int code, QProcess::ExitStatus state)
{
    if (m_stopping) return;
    readOutput();
    // Native driver failures can terminate Python before it can fall back itself.
    if (!m_done && m_attemptedGpu && !m_cpuRetry) {
        start(m_forceRetry, true);
        return;
    }
    m_busy = false;
    if ((code != 0 || state != QProcess::NormalExit || !m_done) && !m_status.startsWith("Transcription failed:"))
        m_status = "Transcription stopped before completion. Try again.";
    if (!m_done) {
        QFile log(QFileInfo(m_vocals).absolutePath() + "/transcript-error.log");
        if (log.open(QIODevice::WriteOnly)) log.write(m_status.toUtf8() + '\n' + m_errors);
    }
    emit changed();
}

void TranscriptController::retry() { if (canRetry()) start(true); }
void TranscriptController::setLanguage(const QString &language)
{
    if (language == m_language || !QStringList{"auto", "en", "nl", "de", "fr", "es", "it", "pt", "ja", "ko", "zh"}.contains(language)) return;
    m_language = language;
    if (!m_vocals.isEmpty()) start();
    emit changed();
}

void TranscriptController::clearWords()
{
    m_words.clear();
    m_wordTimes.clear();
    emit wordsChanged();
    updateHighlight();
}

void TranscriptController::appendWords(const QJsonArray &words)
{
    for (const auto &value : words) {
        const auto word = value.toObject();
        const QString text = word["word"].toString().trimmed();
        if (text.isEmpty()) continue;
        const double start = word["start"].toDouble(-1.0), end = word["end"].toDouble(-1.0);
        m_words.append(QVariantMap{{"text", text}, {"start", start}, {"end", end}});
        m_wordTimes.push_back({start, end});
    }
    emit wordsChanged();
    updateHighlight();
}

void TranscriptController::readPlayback()
{
    m_playbackBuffer += m_socket.readAll();
    QJsonObject latest;
    while (m_playbackBuffer.contains('\n')) {
        const auto end = m_playbackBuffer.indexOf('\n');
        const auto sample = QJsonDocument::fromJson(m_playbackBuffer.left(end)).object();
        m_playbackBuffer.remove(0, end + 1);
        if (sample["generation"].toInt(-1) == m_generation && sample["position"].isDouble())
            latest = sample;
    }
    if (m_playbackBuffer.size() > 8192) m_playbackBuffer.clear();
    if (latest.isEmpty()) return;
    const double position = latest["position"].toDouble();
    if (!std::isfinite(position) || position < 0) return;
    m_position = position;
    m_playing = latest["playing"].toBool();
    m_playbackConnected = true;
    m_lastPlayback.start();
    updateHighlight();
    emit playbackChanged();
}

void TranscriptController::updateHighlight()
{
    // Hold the current word when paused. During gaps or lost playback, clear it.
    const int active = m_playbackConnected ? activeWordAt(m_wordTimes, m_position) : -1;
    if (active != m_activeWord) {
        m_activeWord = active;
        emit activeWordChanged();
    }
}

void TranscriptController::copyText() { QGuiApplication::clipboard()->setText(m_text); }
void TranscriptController::openFolder()
{
    if (!m_vocals.isEmpty()) QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(m_vocals).absolutePath()));
}

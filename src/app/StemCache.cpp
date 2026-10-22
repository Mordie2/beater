#include "StemCache.h"
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcessEnvironment>
#include <QStandardPaths>
#include <QTimer>
#include <QtConcurrent/QtConcurrent>

StemCache::StemCache(QObject *parent) : QObject(parent)
{
    m_root = qEnvironmentVariable("BEATER_STEM_CACHE");
    if (m_root.isEmpty())
        m_root = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + "/stems/v1";
    QDir().mkpath(m_root);
    connect(&m_process, &QProcess::readyReadStandardOutput, this, &StemCache::readOutput);
    connect(&m_process, &QProcess::readyReadStandardError, this, [this] {
        m_errors = (m_errors + m_process.readAllStandardError()).right(8192);
    });
    connect(&m_process, &QProcess::finished, this, [this](int code, QProcess::ExitStatus state) {
        readOutput();
        finish(code == 0 && state == QProcess::NormalExit);
    });
    connect(&m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError e) {
        if (e == QProcess::FailedToStart) finish(false);
    });
}

StemCache::~StemCache()
{
    m_process.disconnect(this);
    if (m_process.state() != QProcess::NotRunning) {
        m_process.kill();
        m_process.waitForFinished(5000);
    }
}

QString StemCache::folder(const QString &id) const { return m_root + '/' + id; }
QString StemCache::status(const QString &id) const { return m_items.value(id).status; }
QString StemCache::vocals(const QString &id) const
{
    const QString path = m_items.value(id).vocals;
    return QFileInfo::exists(path) ? path : QString();
}
bool StemCache::busy(const QString &id) const { return m_items.value(id).busy; }

QString StemCache::cachedSource(const QString &id, const QString &source) const
{
    QFile f(folder(id) + "/manifest.json");
    if (!f.open(QIODevice::ReadOnly)) return source;
    const auto manifest = QJsonDocument::fromJson(f.readAll()).object();
    const QFileInfo input(source);
    const QString cached = manifest["original"].toString();
    const bool matches = !input.exists() || (input.size() == manifest["size"].toInteger()
        && input.lastModified().toMSecsSinceEpoch() == manifest["modified_ms"].toInteger());
    return matches && QFileInfo::exists(cached) ? cached : source;
}

void StemCache::request(const QString &id, const QString &source)
{
    if (id.isEmpty() || m_items.value(id).busy) return;
    auto &item = m_items[id];
    if (QFileInfo::exists(folder(id) + "/manifest.json")) {
        item = {source, "Checking cached vocals…", {}, true};
        emit changed(id);
        const QString appDir = QCoreApplication::applicationDirPath();
        const QString python = qEnvironmentVariable("BEATER_STEM_PYTHON", appDir + "/stem-runtime/python.exe");
        const QString worker = qEnvironmentVariable("BEATER_STEM_WORKER", appDir + "/stem-worker.py");
        const QString cache = folder(id);
        // Cached tracks must not wait behind another deck's lengthy separation.
        QtConcurrent::run([python, worker, source, cache] {
            QProcess probe;
            auto env = QProcessEnvironment::systemEnvironment();
            env.insert("PYTHONUTF8", "1");
            probe.setProcessEnvironment(env);
            probe.start(python, {worker, "--input", source, "--cache", cache, "--probe"});
            if (!probe.waitForFinished(30000)) {
                probe.kill();
                probe.waitForFinished(3000);
                return QString();
            }
            const auto result = QJsonDocument::fromJson(probe.readAllStandardOutput()).object();
            return result["state"] == "ready" ? result["vocals"].toString() : QString();
        }).then(this, [this, id](const QString &vocals) {
            auto &item = m_items[id];
            if (!vocals.isEmpty() && QFileInfo::exists(vocals)) {
                item.vocals = vocals;
                item.status = "Vocals ready · cached";
                item.busy = false;
                emit changed(id);
            } else {
                item.status = "Vocals queued";
                m_queue.enqueue(id);
                emit changed(id);
                startNext();
            }
        });
        return;
    }
    item = {source, "Vocals queued", {}, true};
    m_queue.enqueue(id);
    emit changed(id);
    startNext();
}

void StemCache::startNext()
{
    if (!m_active.isEmpty() || m_queue.isEmpty()) return;
    // Avoid duplicate work if another instance of beater is extracting stems.
    m_lock = std::make_unique<QLockFile>(m_root + "/worker.lock");
    m_lock->setStaleLockTime(0);
    if (!m_lock->tryLock()) {
        m_lock.reset();
        QTimer::singleShot(1000, this, &StemCache::startNext);
        return;
    }
    m_active = m_queue.dequeue();
    m_output.clear();
    m_errors.clear();
    const QString appDir = QCoreApplication::applicationDirPath();
    QString python = qEnvironmentVariable("BEATER_STEM_PYTHON");
    if (python.isEmpty()) python = appDir + "/stem-runtime/python.exe";
    QString worker = qEnvironmentVariable("BEATER_STEM_WORKER");
    if (worker.isEmpty()) worker = appDir + "/stem-worker.py";
    if (!QFileInfo::exists(python) || !QFileInfo::exists(worker)) {
        m_items[m_active].status = "Vocal separator missing from this build";
        finish(false);
        return;
    }
    auto env = QProcessEnvironment::systemEnvironment();
    env.insert("TORCH_HOME", appDir + "/stem-runtime/models");
    env.insert("PYTHONUTF8", "1");
    env.insert("PYTHONUNBUFFERED", "1");
    m_process.setProcessEnvironment(env);
    m_process.start(python, {worker, "--input", m_items[m_active].source, "--cache", folder(m_active)});
}

void StemCache::readOutput()
{
    m_output += m_process.readAllStandardOutput();
    while (m_output.contains('\n')) {
        const auto end = m_output.indexOf('\n');
        const auto object = QJsonDocument::fromJson(m_output.left(end)).object();
        m_output.remove(0, end + 1);
        if (m_active.isEmpty()) continue;
        auto &item = m_items[m_active];
        const QString state = object["state"].toString();
        if (state == "caching") item.status = "Caching track…";
        else if (state == "cached" || state == "separating") item.status = "Separating vocals…";
        else if (state == "ready") {
            item.vocals = object["vocals"].toString();
            item.status = object["cached"].toBool() ? "Vocals ready · cached" : "Vocals ready";
        } else if (state == "error") item.status = "Vocals: " + object["message"].toString().left(240);
        emit changed(m_active);
    }
}

void StemCache::finish(bool success)
{
    if (m_active.isEmpty()) return;
    auto &item = m_items[m_active];
    item.busy = false;
    if (!success || item.vocals.isEmpty()) {
        item.vocals.clear();
        if (!item.status.startsWith("Vocals:") && !item.status.contains("missing"))
            item.status = "Vocal separation failed · retry";
        QFile log(folder(m_active) + "/last-error.log");
        if (log.open(QIODevice::WriteOnly)) log.write(m_errors);
    }
    emit changed(m_active);
    m_active.clear();
    m_lock.reset();
    QTimer::singleShot(0, this, &StemCache::startNext);
}

#include "app/Library.h"

#include "analysis/KeyEstimator.h"
#include "analysis/TempoEstimator.h"
#include "audio/Decoder.h"
#include "core/Key.h"

#include <QCryptographicHash>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSaveFile>
#include <QStandardPaths>

namespace {

QString contentId(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return {};
    QCryptographicHash h(QCryptographicHash::Sha1);
    const qint64 size = f.size();
    h.addData(QByteArray::number(size));
    const qint64 chunk = 256 * 1024;
    h.addData(f.read(chunk));
    if (size > 2 * chunk) {
        f.seek(size - chunk);
        h.addData(f.read(chunk));
    }
    return QString::fromLatin1(h.result().toHex().left(20));
}

QString fmtTime(double s)
{
    if (s <= 0)
        return QString();
    const int t = int(s + 0.5);
    return QStringLiteral("%1:%2").arg(t / 60).arg(t % 60, 2, 10, QLatin1Char('0'));
}

} // namespace

Library::Library(QObject *parent)
    : QAbstractListModel(parent)
{
    m_pool.setMaxThreadCount(2);
    m_saveTimer.setSingleShot(true);
    m_saveTimer.setInterval(400);
    connect(&m_saveTimer, &QTimer::timeout, this, &Library::save);
    load();
    for (const QString &id : std::as_const(m_playlist))
        if (m_tracks[id].needsAnalysis())
            analyze(id);
}

Library::~Library()
{
    m_pool.clear();
    m_pool.waitForDone();
    if (m_saveTimer.isActive())
        save();
}

QStringList Library::audioExtensions()
{
    return {"mp3", "wav", "aif", "aiff", "flac", "m4a", "aac", "alac", "ogg"};
}

int Library::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : int(m_playlist.size());
}

QVariant Library::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_playlist.size())
        return {};
    const TrackRecord &r = m_tracks[m_playlist[index.row()]];
    switch (role) {
    case IdRole: return r.id;
    case TitleRole: return r.title;
    case PathRole: return r.path;
    case BpmRole: return r.bpm;
    case BpmTextRole: return r.bpm > 0 ? QString::number(r.bpm, 'f', r.bpm == std::round(r.bpm) ? 0 : 2) : QString();
    case KeyTextRole:
        return keys::valid(r.key)
            ? QString::fromStdString(keys::camelot(r.key) + " " + keys::standardName(r.key))
            : QString();
    case DurationTextRole: return fmtTime(r.durationSec);
    case StatusRole: return r.status;
    case GridManualRole: return r.gridManual;
    }
    return {};
}

QHash<int, QByteArray> Library::roleNames() const
{
    return {
        {IdRole, "trackId"},
        {TitleRole, "title"},
        {PathRole, "path"},
        {BpmRole, "bpm"},
        {BpmTextRole, "bpmText"},
        {KeyTextRole, "keyText"},
        {DurationTextRole, "durationText"},
        {StatusRole, "status"},
        {GridManualRole, "gridManual"},
    };
}

void Library::addUrls(const QList<QUrl> &urls)
{
    const QStringList exts = audioExtensions();
    for (const QUrl &u : urls) {
        const QString p = u.isLocalFile() ? u.toLocalFile() : u.toString();
        QFileInfo fi(p);
        if (fi.isDir()) {
            QStringList files;
            QDirIterator it(p, QDir::Files, QDirIterator::Subdirectories);
            while (it.hasNext()) {
                const QString f = it.next();
                if (exts.contains(QFileInfo(f).suffix().toLower()))
                    files << f;
            }
            files.sort(Qt::CaseInsensitive);
            for (const QString &f : files)
                addFile(f);
        } else if (exts.contains(fi.suffix().toLower())) {
            addFile(fi.absoluteFilePath());
        }
    }
}

void Library::addFile(const QString &path)
{
    const QString id = contentId(path);
    if (id.isEmpty() || m_playlist.contains(id))
        return;
    TrackRecord &r = m_tracks[id];
    if (r.id.isEmpty()) {
        r.id = id;
        r.title = QFileInfo(path).completeBaseName();
    }
    r.path = path; // follows renames/moves
    beginInsertRows({}, int(m_playlist.size()), int(m_playlist.size()));
    m_playlist.append(id);
    endInsertRows();
    emit countChanged();
    if (r.needsAnalysis())
        analyze(id);
    scheduleSave();
}

void Library::removeAt(int row)
{
    if (row < 0 || row >= m_playlist.size())
        return;
    beginRemoveRows({}, row, row);
    m_playlist.removeAt(row);
    endRemoveRows();
    emit countChanged();
    scheduleSave();
}

void Library::move(int from, int to)
{
    if (from == to || from < 0 || to < 0 || from >= m_playlist.size() || to >= m_playlist.size())
        return;
    beginMoveRows({}, from, from, {}, to > from ? to + 1 : to);
    m_playlist.move(from, to);
    endMoveRows();
    scheduleSave();
}

void Library::reanalyze(int row)
{
    const QString id = idAt(row);
    if (id.isEmpty())
        return;
    analyze(id);
}

void Library::autoDetect(const QString &id, bool grid, bool key)
{
    if (!m_tracks.contains(id))
        return;
    TrackRecord r = m_tracks[id];
    if (grid)
        r.gridManual = false;
    if (key)
        r.keyManual = false;
    update(r);
    analyze(id);
}

QString Library::idAt(int row) const
{
    return row >= 0 && row < m_playlist.size() ? m_playlist[row] : QString();
}

int Library::rowOf(const QString &id) const
{
    return int(m_playlist.indexOf(id));
}

void Library::update(const TrackRecord &rec)
{
    if (!m_tracks.contains(rec.id))
        return;
    m_tracks[rec.id] = rec;
    const int row = rowOf(rec.id);
    if (row >= 0)
        emit dataChanged(index(row), index(row));
    emit recordChanged(rec.id);
    scheduleSave();
}

void Library::analyze(const QString &id)
{
    TrackRecord &r = m_tracks[id];
    if (r.status == QLatin1String("analyzing"))
        return;
    r.status = QStringLiteral("analyzing");
    ++m_analyzing;
    emit analyzingCountChanged();
    const int row = rowOf(id);
    if (row >= 0)
        emit dataChanged(index(row), index(row));
    emit recordChanged(id);

    const QString path = r.path;
    m_pool.start([this, id, path] {
        constexpr uint32_t kSr = 22050;
        std::vector<float> mono;
        std::string err;
        TempoResult tr;
        KeyResult kr;
        double duration = 0.0;
        const bool ok = decoder::decodeMono(path.toStdString(), kSr, mono, &err);
        if (ok) {
            duration = double(mono.size()) / kSr;
            tr = estimateTempo(mono, kSr);
            kr = estimateKey(mono, kSr);
        }
        QMetaObject::invokeMethod(this, [this, id, ok, err, tr, kr, duration] {
            --m_analyzing;
            emit analyzingCountChanged();
            if (!m_tracks.contains(id))
                return;
            TrackRecord r = m_tracks[id];
            r.status.clear();
            if (!ok) {
                r.status = QStringLiteral("error: ") + QString::fromStdString(err);
            } else {
                r.durationSec = duration;
                r.analyzed = true;
                r.analysisVersion = kAnalysisVersion;
                if (!r.gridManual && tr.ok) {
                    r.bpm = tr.bpm;
                    r.firstBeatSec = tr.firstBeatSec;
                    r.gridConfidence = tr.confidence;
                }
                if (!r.keyManual && kr.ok) {
                    r.key = kr.key;
                    r.keyConfidence = kr.confidence;
                }
            }
            update(r);
        }, Qt::QueuedConnection);
    });
}

QString Library::storePath() const
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dir);
    return dir + QStringLiteral("/library.json");
}

void Library::load()
{
    QFile f(storePath());
    if (!f.open(QIODevice::ReadOnly))
        return;
    const QJsonObject root = QJsonDocument::fromJson(f.readAll()).object();
    const QJsonObject tracks = root["tracks"].toObject();
    for (auto it = tracks.begin(); it != tracks.end(); ++it)
        m_tracks.insert(it.key(), TrackRecord::fromJson(it.key(), it.value().toObject()));
    for (const QJsonValue &v : root["playlist"].toArray()) {
        const QString id = v.toString();
        if (m_tracks.contains(id) && !m_playlist.contains(id))
            m_playlist.append(id);
    }
}

void Library::scheduleSave()
{
    m_saveTimer.start();
}

void Library::save()
{
    QJsonObject tracks;
    for (auto it = m_tracks.cbegin(); it != m_tracks.cend(); ++it)
        tracks.insert(it.key(), it.value().toJson());
    QJsonArray playlist;
    for (const QString &id : std::as_const(m_playlist))
        playlist.append(id);
    QJsonObject root;
    root["version"] = 1;
    root["tracks"] = tracks;
    root["playlist"] = playlist;
    QSaveFile f(storePath());
    if (f.open(QIODevice::WriteOnly)) {
        f.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
        f.commit();
    }
}

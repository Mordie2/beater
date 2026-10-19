#pragma once
#include "app/TrackRecord.h"

#include <QAbstractListModel>
#include <QHash>
#include <QList>
#include <QThreadPool>
#include <QTimer>
#include <QUrl>
#include <QtQml/qqmlregistration.h>

// Playlist model + persistent track store (library.json in the app data folder).
class Library : public QAbstractListModel {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Owned by AppController")
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(int analyzingCount READ analyzingCount NOTIFY analyzingCountChanged)

public:
    enum Roles {
        IdRole = Qt::UserRole + 1,
        TitleRole,
        PathRole,
        BpmRole,
        BpmTextRole,
        KeyTextRole,
        DurationTextRole,
        StatusRole,
        GridManualRole,
    };

    explicit Library(QObject *parent = nullptr);
    ~Library() override;

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    int count() const { return int(m_playlist.size()); }
    int analyzingCount() const { return m_analyzing; }

    Q_INVOKABLE void addUrls(const QList<QUrl> &urls);
    Q_INVOKABLE void removeAt(int row);
    Q_INVOKABLE void move(int from, int to);
    Q_INVOKABLE void reanalyze(int row);
    Q_INVOKABLE QString idAt(int row) const;

    bool contains(const QString &id) const { return m_tracks.contains(id); }
    TrackRecord record(const QString &id) const { return m_tracks.value(id); }
    void update(const TrackRecord &rec); // persists + notifies

    static QStringList audioExtensions();

signals:
    void countChanged();
    void analyzingCountChanged();
    void recordChanged(const QString &id);

private:
    void addFile(const QString &path);
    void analyze(const QString &id);
    void load();
    void scheduleSave();
    void save();
    QString storePath() const;
    int rowOf(const QString &id) const;

    QHash<QString, TrackRecord> m_tracks; // every track ever seen
    QList<QString> m_playlist;            // ids in playlist order
    QThreadPool m_pool;
    QTimer m_saveTimer;
    int m_analyzing = 0;
};

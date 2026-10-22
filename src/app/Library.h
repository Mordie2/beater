#pragma once
#include "app/TrackRecord.h"
#include "app/StemCache.h"

#include <QAbstractListModel>
#include <QHash>
#include <QList>
#include <QThreadPool>
#include <QTimer>
#include <QUrl>
#include <QtQml/qqmlregistration.h>

// The library: every track added, and saved playlists of them, persisted in
// library.json in the app data folder. As a list model it shows one view: all
// tracks (view -1) or one playlist.
class Library : public QAbstractListModel {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Owned by AppController")
    Q_PROPERTY(int count READ count NOTIFY countChanged)          // tracks in the view
    Q_PROPERTY(int totalCount READ totalCount NOTIFY countChanged)  // in the library
    Q_PROPERTY(int view READ view WRITE setView NOTIFY viewChanged) // -1: all tracks, else a playlist
    Q_PROPERTY(QStringList playlists READ playlistNames NOTIFY playlistsChanged)
    //! The playlist + adds to from All tracks: the one opened last (-1: none).
    Q_PROPERTY(int target READ target NOTIFY viewChanged)
    Q_PROPERTY(int analyzingCount READ analyzingCount NOTIFY analyzingCountChanged)
    //! Keys as Camelot codes ("11A") instead of names ("F#m"). Remembered.
    Q_PROPERTY(bool camelot READ camelot WRITE setCamelot NOTIFY camelotChanged)

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

    int count() const { return int(shown().size()); }
    int totalCount() const { return int(m_all.size()); }
    int view() const { return m_view; }
    void setView(int v);
    QStringList playlistNames() const;
    int target() const { return m_target; }
    int analyzingCount() const { return m_analyzing; }
    bool camelot() const { return m_camelot; }
    void setCamelot(bool on);
    QString keyText(int key) const;

    // Adds to the library, and to the playlist in view. Returns the tracks' ids.
    Q_INVOKABLE QStringList addUrls(const QList<QUrl> &urls);
    Q_INVOKABLE int newPlaylist(const QString &name); // and shows it
    Q_INVOKABLE void renamePlaylist(int i, const QString &name);
    Q_INVOKABLE void deletePlaylist(int i);
    Q_INVOKABLE void addToPlaylist(int row, int playlist); // row in the view
    Q_INVOKABLE void removeAt(int row); // from the playlist in view, or (All tracks) the library
    Q_INVOKABLE void move(int from, int to);
    Q_INVOKABLE void reanalyze(int row); // keeps manual edits
    // Drops the manual grid and/or key and detects them again.
    void autoDetect(const QString &id, bool grid, bool key);
    Q_INVOKABLE QString idAt(int row) const;

    bool contains(const QString &id) const { return m_tracks.contains(id); }
    TrackRecord record(const QString &id) const { return m_tracks.value(id); }
    StemCache *stems() { return &m_stems; }
    void update(const TrackRecord &rec); // persists + notifies

    static QStringList audioExtensions();

signals:
    void countChanged();
    void viewChanged();
    void playlistsChanged();
    void analyzingCountChanged();
    void camelotChanged();
    void recordChanged(const QString &id);

private:
    QString addFile(const QString &path);
    const QList<QString> &shown() const { return m_view < 0 ? m_all : m_lists[m_view].ids; }
    QList<QString> &shown() { return m_view < 0 ? m_all : m_lists[m_view].ids; }
    void analyze(const QString &id);
    void load();
    void scheduleSave();
    void save();
    QString storePath() const;
    int rowOf(const QString &id) const;

    QHash<QString, TrackRecord> m_tracks; // every track ever seen
    StemCache m_stems;
    QList<QString> m_all; // the library, in the order added
    struct Playlist {
        QString name;
        QList<QString> ids;
    };
    QList<Playlist> m_lists;
    int m_view = -1;
    int m_target = -1;
    QThreadPool m_pool;
    QTimer m_saveTimer;
    int m_analyzing = 0;
    bool m_camelot = false;
};

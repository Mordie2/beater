#pragma once
#include <QHash>
#include <QObject>
#include <QProcess>
#include <QQueue>
#include <QLockFile>
#include <memory>

// One background separator shared by both decks. Never runs inference on the audio thread.
class StemCache : public QObject {
    Q_OBJECT
public:
    explicit StemCache(QObject *parent = nullptr);
    ~StemCache() override;
    void request(const QString &id, const QString &source);
    QString cachedSource(const QString &id, const QString &source) const;
    QString status(const QString &id) const;
    QString vocals(const QString &id) const;
    bool busy(const QString &id) const;
signals:
    void changed(const QString &id);
private:
    struct Item { QString source, status, vocals; bool busy = false; };
    QString folder(const QString &id) const;
    void startNext();
    void readOutput();
    void finish(bool success);
    QHash<QString, Item> m_items;
    QQueue<QString> m_queue;
    QString m_root, m_active;
    QByteArray m_output, m_errors;
    QProcess m_process;
    std::unique_ptr<QLockFile> m_lock;
};

#include "app/AppController.h"

#include <QFileInfo>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QStandardPaths>
#include <QTimer>

#include <cstdio>

// Debug hooks for checking the app without a person at the screen:
//  BEATER_TEST=1          own settings and test data dirs (leaves the real library alone)
//  BEATER_AUTOPLAY=1      load playlist rows 0 and 1 into decks A and B, play, log deck state
//  BEATER_SNAPSHOT=x.png  save a grab of the window after a few seconds, then quit
static void installDebugHooks(QQmlApplicationEngine &engine, AppController &controller)
{
    if (qEnvironmentVariableIsSet("BEATER_AUTOPLAY")) {
        auto *log = new QTimer(&controller);
        for (DeckController *deck : {controller.deckA(), controller.deckB()}) {
            QObject::connect(deck, &DeckController::trackChanged, deck, [deck] { deck->play(); });
            QObject::connect(log, &QTimer::timeout, deck, [deck] {
                std::fprintf(stderr, "deck %s: loaded %d playing %d pos %.2f/%.2f bpm %.2f -> %.2f level %.3f phase %.4f %s\n",
                             qPrintable(deck->name()), deck->loaded(), deck->playing(), deck->position(),
                             deck->duration(), deck->bpm(), deck->playingBpm(), deck->level(), deck->phaseError(),
                             qPrintable(deck->error()));
            });
        }
        // Rows 0 and 1, after analysis has had a moment so the grids are known.
        QTimer::singleShot(500, &controller, [&controller] { controller.loadToDeck(0, 0); });
        QTimer::singleShot(4000, &controller, [&controller] { controller.loadToDeck(1, 1); });
        log->start(1000);
    }
    const QString snapshot = qEnvironmentVariable("BEATER_SNAPSHOT");
    if (!snapshot.isEmpty()) {
        QTimer::singleShot(qEnvironmentVariableIntValue("BEATER_SNAPSHOT_MS") ?: 6000, &engine, [&engine, snapshot] {
            if (auto *w = engine.rootObjects().isEmpty() ? nullptr
                                                        : qobject_cast<QQuickWindow *>(engine.rootObjects().first()))
                w->grabWindow().save(snapshot);
            QCoreApplication::quit();
        });
    }
}

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    QGuiApplication::setOrganizationName("BLOCK SYSTEM");
    QGuiApplication::setOrganizationDomain("blocksystem.nl");
    QGuiApplication::setApplicationName("beater");
    QQuickStyle::setStyle("Basic");
    if (qEnvironmentVariableIsSet("BEATER_TEST")) {
        QGuiApplication::setApplicationName("beater-test"); // own QSettings and panel memory
        QStandardPaths::setTestModeEnabled(true);
    }

    AppController controller;

    // Files or folders on the command line go into the playlist.
    QList<QUrl> urls;
    for (const QString &a : app.arguments().mid(1))
        if (QFileInfo::exists(a))
            urls << QUrl::fromLocalFile(QFileInfo(a).absoluteFilePath());
    if (!urls.isEmpty())
        controller.addUrls(urls);

    QQmlApplicationEngine engine;
    engine.setInitialProperties({{"app", QVariant::fromValue(&controller)}});
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &app,
                     [] { QCoreApplication::exit(-1); }, Qt::QueuedConnection);
    engine.loadFromModule("Beater", "Main");
    installDebugHooks(engine, controller);
    return app.exec();
}

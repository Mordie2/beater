#include "app/AppController.h"
#include "app/WaveformItem.h"

#include <QFileInfo>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QStandardPaths>
#include <QTimer>

#include <cstdio>

// Debug hooks for checking the app without a person at the screen:
//  BEATER_TEST=1          own settings and test data dirs (leaves the real library alone);
//                         the window ignores real clicks and focus
//  BEATER_AUTOPLAY=1      load playlist rows 0 and 1 into decks A and B, play, log deck state
//  BEATER_SNAPSHOT=x.png  save a grab of the window after a few seconds, then quit
//  BEATER_MODE=column     start in that mode: edit, column or strip (perform = column)
//  BEATER_FPS=1           log frames per second and the time spent painting waveforms
//  BEATER_SIZE=WxH        start at this window size (never below the view's minimum)
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
    if (qEnvironmentVariableIsSet("BEATER_FPS") && !engine.rootObjects().isEmpty()) {
        if (auto *w = qobject_cast<QQuickWindow *>(engine.rootObjects().first())) {
            auto *frames = new int(0);
            QObject::connect(w, &QQuickWindow::frameSwapped, w, [frames] { ++*frames; }, Qt::DirectConnection);
            auto *t = new QTimer(w);
            QObject::connect(t, &QTimer::timeout, w, [w, frames] {
                std::fprintf(stderr, "fps %d, waveform render %.1f ms/s%s\n", *frames, WaveformItem::takeRenderMs(),
                             w->isExposed() ? "" : " (window hidden: not drawing)");
                *frames = 0;
            });
            t->start(1000);
        }
    }
    const QString mode = qEnvironmentVariable("BEATER_MODE");
    if (!mode.isEmpty() && !engine.rootObjects().isEmpty())
        QMetaObject::invokeMethod(engine.rootObjects().first(), "setMode", Q_ARG(QVariant, mode == "perform" ? QStringLiteral("column") : mode));
    const QStringList size = qEnvironmentVariable("BEATER_SIZE").split('x');
    if (size.size() == 2 && !engine.rootObjects().isEmpty()) {
        if (auto *w = qobject_cast<QQuickWindow *>(engine.rootObjects().first())) {
            w->resize(std::max(w->minimumWidth(), size[0].toInt()), std::max(w->minimumHeight(), size[1].toInt()));
            std::fprintf(stderr, "window %dx%d (minimum %dx%d)\n", w->width(), w->height(), w->minimumWidth(), w->minimumHeight());
        }
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
    engine.setInitialProperties({{"app", QVariant::fromValue(&controller)},
                                 {"inert", qEnvironmentVariableIsSet("BEATER_TEST")}});
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &app,
                     [] { QCoreApplication::exit(-1); }, Qt::QueuedConnection);
    engine.loadFromModule("Beater", "Main");
    installDebugHooks(engine, controller);
    return app.exec();
}

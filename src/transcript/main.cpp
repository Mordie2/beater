#include "TranscriptController.h"
#include <QCommandLineParser>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QTimer>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QScreen>

int main(int argc, char **argv)
{
    QGuiApplication app(argc, argv);
    app.setApplicationName("beater-transcript");
    QQuickStyle::setStyle("Basic");
    QCommandLineParser parser;
    parser.addHelpOption();
    parser.addOption({"track-session", "Track status file provided by beater", "path"});
    parser.process(app);
    if (!parser.isSet("track-session")) return 2;
    TranscriptController controller(parser.value("track-session"));
    QQmlApplicationEngine engine;
    engine.setInitialProperties({{"transcript", QVariant::fromValue(&controller)}});
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &app,
                     [] { QCoreApplication::exit(1); }, Qt::QueuedConnection);
    engine.loadFromModule("BeaterTranscript", "TranscriptWindow");
    if (!engine.rootObjects().isEmpty()) {
        if (auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first())) {
            const auto area = window->screen()->availableGeometry();
            const int offset = controller.deck() == "B" ? 40 : -40;
            window->setPosition(area.x() + qMax(0, (area.width() - window->width()) / 2 + offset),
                                area.y() + qMax(0, (area.height() - window->height()) / 2 + offset));
            const int minimizeMs = qEnvironmentVariableIntValue("BEATER_TRANSCRIPT_TEST_MINIMIZE_MS");
            if (minimizeMs > 0) QTimer::singleShot(minimizeMs, window, &QQuickWindow::showMinimized);
        }
    }
    const QString snapshot = qEnvironmentVariable("BEATER_TRANSCRIPT_SNAPSHOT");
    if (!snapshot.isEmpty()) {
        const int delay = qEnvironmentVariableIntValue("BEATER_TRANSCRIPT_SNAPSHOT_MS");
        QTimer::singleShot(delay > 0 ? delay : 15000, &engine, [&, snapshot] {
            if (!engine.rootObjects().isEmpty())
                if (auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first()))
                    window->grabWindow().save(snapshot + "-" + controller.deck() + ".png");
            QFile state(snapshot + "-" + controller.deck() + ".json");
            if (state.open(QIODevice::WriteOnly)) {
                auto *window = engine.rootObjects().isEmpty() ? nullptr
                    : qobject_cast<QQuickWindow *>(engine.rootObjects().first());
                const auto words = controller.words();
                const int active = controller.activeWord();
                const auto word = active >= 0 && active < words.size() ? words[active].toMap() : QVariantMap();
                state.write(QJsonDocument(QJsonObject{{"position", controller.position()},
                    {"title", controller.title()}, {"wordCount", words.size()},
                    {"visible", window && window->isVisible()},
                    {"minimized", window && window->visibility() == QWindow::Minimized},
                    {"playing", controller.playing()}, {"connected", controller.playbackConnected()},
                    {"activeWord", active}, {"word", QJsonObject::fromVariantMap(word)}}).toJson());
            }
        });
    }
    return app.exec();
}

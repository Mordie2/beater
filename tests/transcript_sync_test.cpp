#include "transcript/TranscriptController.h"
#include "transcript/WordTiming.h"
#include <QGuiApplication>
#include <QCryptographicHash>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocalServer>
#include <QSaveFile>
#include <QTemporaryDir>
#include <QThread>
#include <QUuid>
#include <cstdio>
#include <limits>
#include <functional>

static int failures = 0;
static void check(bool value, const char *label)
{
    if (!value) { ++failures; std::printf("FAIL: %s\n", label); }
}
static bool until(const std::function<bool()> &predicate, int timeout = 5000)
{
    QElapsedTimer clock; clock.start();
    do {
        QCoreApplication::processEvents();
        if (predicate()) return true;
        QThread::msleep(2);
    } while (clock.elapsed() < timeout);
    return false;
}

int main(int argc, char **argv)
{
    QGuiApplication app(argc, argv);
    const std::vector<WordTiming> ranges{{0.1, 0.4}, {0.4, 0.8}, {1.2, 1.5}, {2, 2}, {-1, -1}};
    check(activeWordAt(ranges, 0) == -1, "before vocals");
    check(activeWordAt(ranges, 0.1) == 0, "inclusive start");
    check(activeWordAt(ranges, 0.4) == 1, "adjacent word boundary");
    check(activeWordAt(ranges, 0.8) == -1, "exclusive end / silence");
    check(activeWordAt(ranges, 2) == -1, "zero length timestamp");
    check(activeWordAt(ranges, std::numeric_limits<double>::quiet_NaN()) == -1, "invalid playhead");

    QTemporaryDir dir;
    const QString audio = dir.filePath("vocals.wav");
    const QByteArray content("cached-test-audio");
    QFile file(audio); file.open(QIODevice::WriteOnly); file.write(content); file.close();
    QJsonArray words{QJsonObject{{"word", "One"}, {"start", .1}, {"end", .4}},
                     QJsonObject{{"word", "two"}, {"start", .4}, {"end", .8}},
                     QJsonObject{{"word", "three"}, {"start", 1.2}, {"end", 1.5}}};
    const QFileInfo model(QCoreApplication::applicationDirPath() + "/stem-runtime/models/whisper-large-v3/model.bin");
    const QString modelId = model.exists() ? QStringLiteral("whisper-large-v3:%1:%2")
        .arg(model.size()).arg(model.lastModified().toSecsSinceEpoch()) : QStringLiteral("whisper-large-v3");
    const QFileInfo alignModel(QCoreApplication::applicationDirPath() + "/stem-runtime/models/alignment-en/wav2vec2-base-960h-alignment.pt");
    const QString alignmentId = alignModel.exists() ? QStringLiteral("%1:%2").arg(alignModel.size())
        .arg(alignModel.lastModified().toSecsSinceEpoch()) : QStringLiteral("missing");
    QJsonObject cached{{"version", "large-v3-fp16-ctc-en-v2"},
        {"settings", QJsonObject{{"model", modelId}, {"language", "auto"}, {"align", true}, {"alignment_model", alignmentId}}},
        {"sha256", QString::fromLatin1(QCryptographicHash::hash(content, QCryptographicHash::Sha256).toHex())},
        {"text", "One two three"}, {"language", "en"},
        {"segments", QJsonArray{QJsonObject{{"words", words}}}}};
    QFile cache(dir.filePath("vocals.transcript.json"));
    cache.open(QIODevice::WriteOnly); cache.write(QJsonDocument(cached).toJson()); cache.close();
    QLocalServer server;
    check(server.listen("beater-sync-test-" + QUuid::createUuid().toString(QUuid::WithoutBraces)), "local server");
    const QString session = dir.filePath("session.json");
    auto setSession = [&](int generation, const QString &stage) {
        QSaveFile f(session); f.open(QIODevice::WriteOnly);
        f.write(QJsonDocument(QJsonObject{{"generation", generation}, {"title", "Timing test"},
            {"deck", "T"}, {"stage", stage}, {"status", "Waiting"}, {"vocals", audio},
            {"playbackSocket", server.serverName()}}).toJson()); f.commit();
    };
    setSession(1, "ready");
    TranscriptController controller(session);
    check(until([&] { return server.hasPendingConnections(); }), "preview connects");
    auto *socket = server.nextPendingConnection();
    if (!socket) return 1;
    check(until([&] { return !controller.busy() && controller.words().size() == 3; }, 15000), "cached word timestamps loaded");
    if (app.arguments().contains("--require-cpu-fallback")) {
        check(controller.backend().contains("CPU"), "native GPU failure retried on CPU");
        check(!controller.words().isEmpty() && controller.words().first().toMap()["text"] == "One",
              "GPU partial text replaced by CPU result");
    }
    auto send = [&](int generation, double position, bool playing) {
        socket->write(QJsonDocument(QJsonObject{{"generation", generation}, {"position", position},
            {"playing", playing}}).toJson(QJsonDocument::Compact) + '\n'); socket->flush();
    };
    send(1, .2, true);
    check(until([&] { return controller.activeWord() == 0 && controller.playing(); }), "playing first word");
    send(1, .5, false);
    check(until([&] { return controller.activeWord() == 1 && !controller.playing(); }), "paused playhead");
    until([] { return false; }, 120);
    check(controller.position() == .5 && controller.activeWord() == 1, "pause does not advance");
    send(1, 1.3, true);
    check(until([&] { return controller.activeWord() == 2; }), "seek / faster-tempo position jump");
    send(1, .25, true);
    check(until([&] { return controller.activeWord() == 0; }), "loop back follows source position");
    send(1, .95, true);
    check(until([&] { return controller.activeWord() == -1; }), "no highlight between vocals");
    send(1, .6, true);
    check(until([&] { return controller.activeWord() == 1; }), "resume word");
    send(0, 1.3, true);
    until([] { return false; }, 100);
    check(controller.activeWord() == 1, "old track sample ignored");
    check(until([&] { return !controller.playbackConnected(); }, 1600), "stalled clock cleared");
    check(controller.activeWord() == -1, "stalled highlight cleared");
    send(1, 1.3, true);
    check(until([&] { return controller.activeWord() == 2; }), "clock recovery");
    socket->disconnectFromServer();
    check(until([&] { return !controller.playbackConnected() && controller.activeWord() == -1; }), "disconnect clears highlight");
    setSession(2, "waiting");
    check(until([&] { return controller.words().isEmpty() && controller.text().isEmpty(); }), "new track clears old words");
    std::printf("%s\n", failures ? "Synchronization checks failed" : "All word timing and playback synchronization checks passed");
    return failures ? 1 : 0;
}

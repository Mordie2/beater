#include "app/StemCache.h"
#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QTemporaryDir>
#include <QThread>
#include <functional>
#include <cstdio>

static bool until(const std::function<bool()> &predicate, int timeout = 6000)
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
    QCoreApplication app(argc, argv);
    QTemporaryDir dir;
    const auto root = dir.path();
    QDir().mkpath(root + "/cached");
    QDir().mkpath(root + "/miss");
    for (const auto &name : {"cached/manifest.json", "miss/manifest.json", "vocals.wav"}) {
        QFile f(root + '/' + name); f.open(QIODevice::WriteOnly); f.write("fixture");
    }
    QFile script(root + "/worker.py");
    script.open(QIODevice::WriteOnly);
    script.write(R"PY(
import argparse, json, time
from pathlib import Path
p=argparse.ArgumentParser()
p.add_argument('--input'); p.add_argument('--cache'); p.add_argument('--probe', action='store_true')
a=p.parse_args(); folder=Path(a.cache)
if a.probe and folder.name != 'cached':
    print(json.dumps(dict(state='miss')), flush=True)
else:
    if not a.probe:
        print(json.dumps(dict(state='separating')), flush=True)
        time.sleep(3 if folder.name == 'slow' else .1)
    print(json.dumps(dict(state='ready', cached=a.probe, vocals=str(folder.parent/'vocals.wav'))), flush=True)
)PY");
    script.close();
    qputenv("BEATER_STEM_CACHE", root.toUtf8());
    qputenv("BEATER_STEM_WORKER", script.fileName().toUtf8());
    StemCache cache;
    cache.request("slow", "unused");
    if (!until([&] { return cache.status("slow") == "Separating vocals…"; })) return 1;
    cache.request("cached", "unused");
    cache.request("miss", "unused");
    if (!until([&] { return !cache.busy("cached"); }, 2000)
        || cache.vocals("cached").isEmpty() || !cache.busy("slow")) {
        std::puts("FAIL: cached vocals waited for separation"); return 1;
    }
    if (!until([&] { return !cache.busy("slow") && !cache.busy("miss"); })
        || cache.vocals("miss").isEmpty()) {
        std::puts("FAIL: cache miss did not complete in the normal queue"); return 1;
    }
    std::puts("Cached vocals bypass pending separation; cache misses complete normally");
    return 0;
}

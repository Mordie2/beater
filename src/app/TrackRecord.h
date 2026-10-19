#pragma once
#include "core/BeatGrid.h"

#include <QJsonObject>
#include <QString>

// Everything the app remembers about a track. Keyed by a content hash so edits
// survive renames and moves.
struct TrackRecord {
    QString id;
    QString path;
    QString title;
    double durationSec = 0.0;

    double bpm = 0.0;          // 0 = unknown
    double firstBeatSec = 0.0; // a downbeat
    bool gridManual = false;   // user edited: analysis never overwrites
    double gridConfidence = 0.0;

    int key = -1;              // 0..23, see core/Key.h
    bool keyManual = false;

    // Loop region in beats. end <= start = automatic end loop.
    double loopStartBeat = 0.0;
    double loopEndBeat = 0.0;

    bool analyzed = false;
    QString status;            // transient: "analyzing", "error: ..."

    BeatGrid grid() const { return BeatGrid{bpm, firstBeatSec}; }
    bool hasManualLoop() const { return loopEndBeat > loopStartBeat; }

    QJsonObject toJson() const
    {
        QJsonObject o;
        o["path"] = path;
        o["title"] = title;
        o["duration"] = durationSec;
        o["bpm"] = bpm;
        o["firstBeat"] = firstBeatSec;
        o["gridManual"] = gridManual;
        o["gridConfidence"] = gridConfidence;
        o["key"] = key;
        o["keyManual"] = keyManual;
        o["loopStart"] = loopStartBeat;
        o["loopEnd"] = loopEndBeat;
        o["analyzed"] = analyzed;
        return o;
    }

    static TrackRecord fromJson(const QString &id, const QJsonObject &o)
    {
        TrackRecord r;
        r.id = id;
        r.path = o["path"].toString();
        r.title = o["title"].toString();
        r.durationSec = o["duration"].toDouble();
        r.bpm = o["bpm"].toDouble();
        r.firstBeatSec = o["firstBeat"].toDouble();
        r.gridManual = o["gridManual"].toBool();
        r.gridConfidence = o["gridConfidence"].toDouble();
        r.key = o["key"].toInt(-1);
        r.keyManual = o["keyManual"].toBool();
        r.loopStartBeat = o["loopStart"].toDouble();
        r.loopEndBeat = o["loopEnd"].toDouble();
        r.analyzed = o["analyzed"].toBool();
        return r;
    }
};

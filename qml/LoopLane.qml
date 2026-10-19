import QtQuick
import HighlighterUI

// The loops as bars under the timeline, on the same scale: the intro at the
// start and the main loop (the end loop, or a drawn one). The one the deck
// loops right now is solid, the other dim.
//  Drag an end to resize a loop, its middle to move it, or the empty lane to
//  draw a new main loop. Snaps to bars; hold Option for beats.
Item {
    id: lane
    required property DeckController deck
    property color accent: Look.loop
    readonly property color tint: accent

    implicitHeight: Theme.s(18)

    readonly property bool ready: deck.loaded && deck.bpm > 0 && deck.duration > 0
    readonly property real handle: Theme.s(7)

    function xAtBeat(b) { return (deck.firstBeat + b * 60 / deck.bpm) / deck.duration * width }
    function beatAtX(x) { return (x / width * deck.duration - deck.firstBeat) * deck.bpm / 60 }
    function snap(b, fine) { const q = fine ? 1 : 4; return Math.round(b / q) * q }
    // The bar lines (beats with Option) a loop may use: as BeatGrid's
    // first/lastLoopBeat, a bar cut short by under half a beat still counts.
    function firstBeat(fine) { const q = fine ? 1 : 4; return Math.ceil((beatAtX(0) - 0.5) / q) * q }
    function lastBeat(fine) { const q = fine ? 1 : 4; return Math.floor((beatAtX(width) + 0.5) / q) * q }

    // while dragging: which loop ("intro" / "main") and where it's going
    property string editKind: ""
    property real editA: 0
    property real editB: 0

    Rectangle { anchors.fill: parent; color: Theme.well }

    component Region: Rectangle {
        id: r
        property bool intro: false
        readonly property bool editing: lane.editKind === (intro ? "intro" : "main")
        readonly property real a: editing ? lane.editA : intro ? lane.deck.introStartBeat : lane.deck.loopStartBeat
        readonly property real b: editing ? lane.editB : intro ? lane.deck.introEndBeat : lane.deck.loopEndBeat
        readonly property bool active: intro === lane.deck.introActive
        readonly property bool auto: intro ? lane.deck.introAuto : lane.deck.loopAuto

        visible: lane.ready && b > a
        x: lane.xAtBeat(a)
        width: Math.max(Theme.s(2), lane.xAtBeat(b) - x)
        height: parent.height
        z: active || editing ? 1 : 0
        color: (lane.deck.loopEnabled && active) || editing ? lane.tint : Theme.line
        opacity: editing ? 1 : active ? (auto ? 0.8 : 1) : 0.35
        // the ends you can grab
        Rectangle { width: Math.min(lane.handle, parent.width / 2); height: parent.height; color: Theme.edgeOf(r.color) }
        Rectangle { x: parent.width - width; width: Math.min(lane.handle, parent.width / 2); height: parent.height; color: Theme.edgeOf(r.color) }
        Text {
            anchors.centerIn: parent
            visible: implicitWidth < parent.width - 2 * lane.handle - Theme.s(4)
            readonly property real beats: r.b - r.a
            readonly property string len: beats % 4 === 0 ? beats / 4 + (beats === 4 ? " bar" : " bars") : beats + " beats"
            text: r.intro ? "intro · " + len : len + (r.auto && !r.editing ? " · end" : "")
            color: Theme.inkOf(r.color)
            font.family: Theme.family
            font.pixelSize: Theme.smallSize
            font.weight: Font.Bold
        }
    }

    Region { id: introRegion; intro: true }
    Region { id: mainRegion; intro: false }

    MouseArea {
        id: ma
        anchors.fill: parent
        z: 2
        enabled: lane.ready
        hoverEnabled: true
        preventStealing: true
        property string part: ""     // "start", "end", "move", "draw"
        property real pressBeat: 0
        property real fromA: 0
        property real fromB: 0

        // What's under x: the loop on top (the active one) first.
        function hit(x) {
            const order = lane.deck.introActive ? [introRegion, mainRegion] : [mainRegion, introRegion]
            for (const r of order) {
                if (!r.visible) continue
                const x0 = r.x, x1 = r.x + r.width
                const kind = r.intro ? "intro" : "main"
                if (Math.abs(x - x0) <= lane.handle && x < x1 - 1) return { kind: kind, part: "start", r: r }
                if (Math.abs(x - x1) <= lane.handle) return { kind: kind, part: "end", r: r }
                if (x > x0 && x < x1) return { kind: kind, part: "move", r: r }
            }
            return { kind: "main", part: "draw", r: null }
        }
        cursorShape: {
            const p = pressed ? part : hit(mouseX).part
            return p === "start" || p === "end" ? Qt.SizeHorCursor
                 : p === "move" ? (pressed ? Qt.ClosedHandCursor : Qt.OpenHandCursor)
                 : Qt.CrossCursor
        }
        onPressed: m => {
            const h = hit(m.x)
            part = h.part
            pressBeat = lane.beatAtX(m.x)
            if (h.r) {
                fromA = h.r.a
                fromB = h.r.b
            } else {
                fromA = fromB = lane.snap(pressBeat, m.modifiers & Qt.AltModifier)
            }
            lane.editA = fromA
            lane.editB = fromB
            lane.editKind = h.kind
        }
        onPositionChanged: m => {
            if (!pressed) return
            const fine = m.modifiers & Qt.AltModifier
            const min = fine ? 1 : 4
            const lo = lane.firstBeat(fine), hi = lane.lastBeat(fine)
            const beat = Math.max(lo, Math.min(hi, lane.snap(lane.beatAtX(m.x), fine)))
            if (part === "start") {
                lane.editA = Math.min(beat, fromB - min)
            } else if (part === "end") {
                lane.editB = Math.max(beat, fromA + min)
            } else if (part === "move") {
                const len = fromB - fromA
                const d = lane.snap(lane.beatAtX(m.x) - pressBeat, fine)
                const s = Math.max(lo, Math.min(hi - len, fromA + d))
                lane.editA = s
                lane.editB = s + len
            } else {
                lane.editA = Math.min(fromA, beat)
                lane.editB = Math.max(fromA, beat)
            }
        }
        onReleased: {
            if (lane.editB - lane.editA >= 1 && (lane.editA !== fromA || lane.editB !== fromB)) {
                if (lane.editKind === "intro")
                    lane.deck.setIntroBeats(lane.editA, lane.editB)
                else
                    lane.deck.setLoopBeats(lane.editA, lane.editB)
            }
            lane.editKind = ""
            part = ""
        }
        onCanceled: { lane.editKind = ""; part = "" }
    }
}

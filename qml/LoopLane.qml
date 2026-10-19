import QtQuick
import HighlighterUI

// The loop region as a bar under the timeline, on the same scale.
//  Drag an end to resize it, the middle to move it, or the empty lane to
//  draw a new one. Snaps to bars; hold Option for beats. The automatic end
//  loop shows dimmer until it is edited.
Item {
    id: lane
    required property DeckController deck
    readonly property color tint: Theme.accentAt(lane, Window.window)

    implicitHeight: Theme.s(18)

    readonly property bool ready: deck.loaded && deck.bpm > 0 && deck.duration > 0
    readonly property real handle: Theme.s(7)

    function xAtBeat(b) { return (deck.firstBeat + b * 60 / deck.bpm) / deck.duration * width }
    function beatAtX(x) { return (x / width * deck.duration - deck.firstBeat) * deck.bpm / 60 }
    function snap(b, fine) { const q = fine ? 1 : 4; return Math.round(b / q) * q }
    // whole bars (beats with Option) inside the track
    function firstBeat(fine) { const q = fine ? 1 : 4; return Math.ceil(beatAtX(0) / q) * q }
    function lastBeat(fine) { const q = fine ? 1 : 4; return Math.floor(beatAtX(width) / q) * q }

    // while dragging: the region being edited; NaN = show the deck's
    property real editA: NaN
    property real editB: NaN
    readonly property real a: isNaN(editA) ? deck.loopStartBeat : editA
    readonly property real b: isNaN(editB) ? deck.loopEndBeat : editB
    readonly property bool editing: !isNaN(editA)

    Rectangle { anchors.fill: parent; color: Theme.well }

    Rectangle {
        id: region
        visible: lane.ready && lane.b > lane.a
        x: lane.xAtBeat(lane.a)
        width: Math.max(Theme.s(2), lane.xAtBeat(lane.b) - x)
        height: parent.height
        color: lane.deck.loopEnabled || lane.editing ? lane.tint : Theme.line
        opacity: lane.deck.loopAuto && !lane.editing ? 0.55 : 1
        // the ends you can grab
        Rectangle { width: Math.min(lane.handle, parent.width / 2); height: parent.height; color: Theme.edgeOf(region.color) }
        Rectangle { x: parent.width - width; width: Math.min(lane.handle, parent.width / 2); height: parent.height; color: Theme.edgeOf(region.color) }
        Text {
            anchors.centerIn: parent
            visible: implicitWidth < parent.width - 2 * lane.handle - Theme.s(4)
            readonly property real beats: lane.b - lane.a
            text: (beats % 4 === 0 ? beats / 4 + (beats === 4 ? " bar" : " bars") : beats + " beats")
                  + (lane.deck.loopAuto && !lane.editing ? " · end" : "")
            color: Theme.inkOf(region.color)
            font.family: Theme.family
            font.pixelSize: Theme.smallSize
            font.weight: Font.Bold
        }
    }

    MouseArea {
        id: ma
        anchors.fill: parent
        enabled: lane.ready
        hoverEnabled: true
        preventStealing: true
        property string mode: ""     // "start", "end", "move", "draw"
        property real pressBeat: 0
        property real fromA: 0
        property real fromB: 0

        function zoneAt(x) {
            if (!region.visible) return "draw"
            const x0 = region.x, x1 = region.x + region.width
            if (Math.abs(x - x0) <= lane.handle && x < x1 - 1) return "start"
            if (Math.abs(x - x1) <= lane.handle) return "end"
            if (x > x0 && x < x1) return "move"
            return "draw"
        }
        cursorShape: {
            const z = pressed ? mode : zoneAt(mouseX)
            return z === "start" || z === "end" ? Qt.SizeHorCursor
                 : z === "move" ? (pressed ? Qt.ClosedHandCursor : Qt.OpenHandCursor)
                 : Qt.CrossCursor
        }
        onPressed: m => {
            mode = zoneAt(m.x)
            pressBeat = lane.beatAtX(m.x)
            fromA = lane.a
            fromB = lane.b
            if (mode === "draw") {
                const s = lane.snap(pressBeat, m.modifiers & Qt.AltModifier)
                lane.editA = s
                lane.editB = s
            } else {
                lane.editA = fromA
                lane.editB = fromB
            }
        }
        onPositionChanged: m => {
            if (!pressed) return
            const fine = m.modifiers & Qt.AltModifier
            const min = fine ? 1 : 4
            const lo = lane.firstBeat(fine), hi = lane.lastBeat(fine)
            const beat = Math.max(lo, Math.min(hi, lane.snap(lane.beatAtX(m.x), fine)))
            if (mode === "start") {
                lane.editA = Math.min(beat, fromB - min)
            } else if (mode === "end") {
                lane.editB = Math.max(beat, fromA + min)
            } else if (mode === "move") {
                const len = fromB - fromA
                const d = lane.snap(lane.beatAtX(m.x) - pressBeat, fine)
                const s = Math.max(lo, Math.min(hi - len, fromA + d))
                lane.editA = s
                lane.editB = s + len
            } else {
                const s = lane.snap(pressBeat, fine)
                lane.editA = Math.min(s, beat)
                lane.editB = Math.max(s, beat)
            }
        }
        onReleased: {
            if (lane.editB - lane.editA >= 1 && (lane.editA !== fromA || lane.editB !== fromB))
                lane.deck.setLoopBeats(lane.editA, lane.editB)
            lane.editA = NaN
            lane.editB = NaN
            mode = ""
        }
        onCanceled: { lane.editA = NaN; lane.editB = NaN; mode = "" }
    }
}

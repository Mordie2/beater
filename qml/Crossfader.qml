import QtQuick
import HighlighterUI

// The crossfader, on the kit's XY pad, standing up: A at the top, B at the
// bottom, the middle leaves both at their faders (AppController.crossfader).
// Only y counts for now; x is kept for when there are four decks, and
// springs back to the middle. The pad alone, no value boxes.
ParamXY {
    id: root
    required property AppController app
    property color accent: Look.neutral

    // no label: too narrow for one, and the A / B marks say what it is
    showValues: false
    xFrom: -1; xTo: 1
    yFrom: -1; yTo: 1
    yDown: true // -1 (A) at the top
    stepSize: 0.01
    defaultX: 0
    defaultY: 0
    xValue: 0
    yValue: app.crossfader
    gridStep: 0.5
    // A and B a little inside the ends, so their labels fit on the pad
    ghosts: [{ x: 0, y: -0.8, label: "A" }, { x: 0, y: 0.8, label: "B" }]
    onMoved: (x, y) => {
        root.app.crossfader = y
        root.xValue = 0
    }
}

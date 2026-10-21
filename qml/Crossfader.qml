import QtQuick
import HighlighterUI

// The crossfader, on the kit's XY pad: left is A, right is B, the middle
// leaves both at their faders (AppController.crossfader). Only x counts for
// now; y is kept for when there are four decks, and springs back to the middle.
ParamXY {
    id: root
    required property AppController app
    property color accent: Look.neutral

    label: "crossfader"
    xFrom: -1; xTo: 1
    yFrom: -1; yTo: 1
    stepSize: 0.01
    defaultX: 0
    defaultY: 0
    xValue: app.crossfader
    yValue: 0
    yDown: false
    gridStep: 0.5
    // A and B a little inside the ends, so their labels fit on the pad
    ghosts: [{ x: -0.85, y: 0, label: "A" }, { x: 0.85, y: 0, label: "B" }]
    onMoved: (x, y) => {
        root.app.crossfader = x
        root.yValue = 0
    }
}

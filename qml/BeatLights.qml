import QtQuick
import HighlighterUI

// Where the master clock is in the bar: the lit square walks 1-2-3-4.
Row {
    id: root
    required property AppController app
    property real size: Theme.s(18)
    spacing: Theme.s(4)
    Repeater {
        model: 4
        ParamLight {
            required property int index
            width: root.size
            height: root.size
            on: Math.floor(root.app.masterBeat) % 4 === index
            opacity: on && index > 0 ? 0.6 : 1
        }
    }
}

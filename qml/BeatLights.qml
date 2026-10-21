import QtQuick
import HighlighterUI

// Where a beat is in its 4/4 bar: the lit square walks 1-2-3-4, the 1 at full
// brightness. The master clock's (top bar), or a deck's own.
Row {
    id: root
    required property real beat
    //! Dimmed: a hint, not the thing itself (a stopped deck shows the master).
    property bool dim: false
    property real size: Theme.s(18)
    spacing: Theme.s(4)
    readonly property int inBar: ((Math.floor(beat) % 4) + 4) % 4 // before beat 0 too
    opacity: dim ? 0.45 : 1
    Repeater {
        model: 4
        ParamLight {
            required property int index
            width: root.size
            height: root.size
            on: root.inBar === index
            opacity: on && index > 0 ? 0.6 : 1
        }
    }
}

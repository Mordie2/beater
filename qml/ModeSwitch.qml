import QtQuick
import HighlighterUI

// Edit (everything) / Perform (the compact column next to Resolume). ⌘E flips it.
ParamChoice {
    id: root
    property bool perform: false
    signal switched(bool perform)
    implicitWidth: Theme.s(140)   // layouts size by implicit width
    model: ["Edit", "Perform"]
    currentIndex: perform ? 1 : 0
    onActivated: i => root.switched(i === 1)
}

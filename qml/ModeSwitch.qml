import QtQuick
import HighlighterUI

// Edit (everything), Column (Perform, tall and narrow) or Strip (Perform,
// wide and short): two ways to sit next to Resolume, to see which works.
// ⌘E flips between Edit and the Perform layout used last.
ParamChoice {
    id: root
    property string mode: "edit"
    signal switched(string mode)
    readonly property var modes: ["edit", "column", "strip"]
    implicitWidth: Theme.s(190)   // layouts size by implicit width
    model: ["Edit", "Column", "Strip"]
    currentIndex: modes.indexOf(mode)
    onActivated: i => root.switched(modes[i])
}

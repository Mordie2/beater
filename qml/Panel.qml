import QtQuick
import HighlighterUI

// A titled raised panel that looks like a ParamSection, but whose body
// stretches to the panel's height, so a waveform or list inside can fill it.
// Lay its content out with anchors.fill: parent.
Item {
    id: root
    property string title: ""
    property string note: ""
    property color titleColor: Theme.text
    default property alias content: body.data
    //! Items at the right end of the title row.
    property alias tools: tools.data

    Item {
        id: head
        width: parent.width
        height: Theme.s(30)
        Text {
            id: titleText
            anchors.verticalCenter: parent.verticalCenter
            text: root.title
            color: root.titleColor
            font.family: Theme.family
            font.pixelSize: Theme.labelSize
            font.weight: Font.Bold
            font.letterSpacing: Theme.s(0.5)
            font.capitalization: Font.AllUppercase
        }
        Text {
            x: titleText.width + Theme.s(8)
            width: Math.max(0, tools.x - x - Theme.s(8))
            anchors.baseline: titleText.baseline
            text: root.note
            elide: Text.ElideRight
            color: Theme.textFaint
            font.family: Theme.family
            font.pixelSize: Theme.smallSize
        }
        Row {
            id: tools
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            spacing: Theme.s(8)
        }
    }
    ParamPlate {
        y: head.height
        width: parent.width
        height: parent.height - head.height
        Item {
            id: body
            anchors.fill: parent
            anchors.leftMargin: Theme.sectionInset
            anchors.rightMargin: Theme.sectionInset
            anchors.topMargin: Theme.s(12)
            anchors.bottomMargin: Theme.s(12)
        }
    }
}

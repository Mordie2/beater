import QtQuick

// Small caps label above a monospace value.
Column {
    id: root
    property string label
    property string value
    property color valueColor: Theme.text
    property int valueSize: 15
    spacing: 1

    Text {
        text: root.label
        color: Theme.faint
        font.family: Theme.mono
        font.pixelSize: 9
        font.bold: true
        font.letterSpacing: 1
    }
    Text {
        text: root.value.length ? root.value : "—"
        color: root.value.length ? root.valueColor : Theme.faint
        font.family: Theme.mono
        font.pixelSize: root.valueSize
    }
}

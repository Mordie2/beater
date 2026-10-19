import QtQuick

// Flat button. `checked` lights it with the accent color.
Rectangle {
    id: root
    property string text
    property bool checked: false
    property color accent: Theme.text
    property bool small: false
    signal clicked()

    implicitWidth: Math.max(small ? 28 : 40, label.implicitWidth + (small ? 12 : 18))
    implicitHeight: small ? 22 : 28
    radius: Theme.radius
    color: checked ? accent : (mouse.pressed ? Theme.line : Theme.raised)
    border.color: checked ? accent : Theme.line
    opacity: enabled ? 1.0 : 0.35

    Text {
        id: label
        anchors.centerIn: parent
        text: root.text
        color: root.checked ? Theme.bg : Theme.text
        font.family: Theme.mono
        font.pixelSize: root.small ? 10 : 11
        font.bold: true
        font.letterSpacing: 0.5
    }

    MouseArea {
        id: mouse
        anchors.fill: parent
        onClicked: root.clicked()
    }
}

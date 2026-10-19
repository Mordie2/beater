import QtQuick
import HighlighterUI

// The master BPM on a seven-segment screen: drag up / down (Shift: fine),
// scroll, or double-click to type (Up / Down step it while typing).
Rectangle {
    id: root
    required property AppController app
    property real digitHeight: Theme.s(30)

    implicitWidth: lcd.width + Theme.s(20)
    implicitHeight: lcd.height + Theme.s(12)
    color: Theme.screen
    border.width: 1
    border.color: mouse.pressed || typed.editing ? Theme.accent : Theme.line

    ParamSevenSeg {
        id: lcd
        anchors.centerIn: parent
        visible: !typed.editing
        digits: 5
        digitHeight: root.digitHeight
        text: root.app.masterBpm.toFixed(2)
    }
    ParamValueBox {
        id: typed
        anchors.centerIn: parent
        opacity: editing ? 1 : 0
        value: root.app.masterBpm
        decimals: 2
        fontSize: Math.round(root.digitHeight * 0.75)
        boxHeight: Math.round(root.digitHeight * 1.05)
        onSubmitted: v => root.app.masterBpm = v
        onStepped: (d, big) => root.app.nudgeMasterBpm(d * (big ? 1 : 0.1))
    }
    MouseArea {
        id: mouse
        anchors.fill: parent
        enabled: !typed.editing
        cursorShape: Qt.SizeVerCursor
        property real startY: 0
        property real startBpm: 0
        onPressed: m => { startY = m.y; startBpm = root.app.masterBpm }
        onPositionChanged: m => {
            const fine = m.modifiers & Qt.ShiftModifier
            root.app.masterBpm = startBpm + (startY - m.y) * (fine ? 0.01 : 0.1)
        }
        onWheel: w => root.app.nudgeMasterBpm((w.angleDelta.y > 0 ? 1 : -1) * ((w.modifiers & Qt.ShiftModifier) ? 0.01 : 0.1))
        onDoubleClicked: typed.begin()
    }
}

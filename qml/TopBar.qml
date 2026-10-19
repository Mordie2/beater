import QtQuick
import QtQuick.Layouts
import HighlighterUI

Item {
    id: root
    required property AppController app
    implicitHeight: Theme.s(58)

    RowLayout {
        anchors.fill: parent
        spacing: Theme.s(24)

        Text {
            text: "BEATER"
            color: Theme.accent
            font.family: Theme.family
            font.pixelSize: Theme.s(20)
            font.weight: Font.Black
            font.letterSpacing: Theme.s(2)
        }

        Item { Layout.fillWidth: true }

        // ---- master tempo: drag up/down, scroll, double-click to type ----
        Column {
            spacing: Theme.s(4)
            ParamText { text: "master bpm"; caps: true; wrapMode: Text.NoWrap }
            Row {
                spacing: Theme.s(4)
                ParamButton { text: "−1"; onClicked: root.app.nudgeMasterBpm(-1); anchors.verticalCenter: parent.verticalCenter }
                ParamButton { text: "−.1"; onClicked: root.app.nudgeMasterBpm(-0.1); anchors.verticalCenter: parent.verticalCenter }
                Rectangle {
                    width: lcd.width + Theme.s(20)
                    height: lcd.height + Theme.s(12)
                    color: Theme.screen
                    border.width: 1
                    border.color: tempoMouse.pressed || typed.editing ? Theme.accent : Theme.line
                    ParamSevenSeg {
                        id: lcd
                        anchors.centerIn: parent
                        visible: !typed.editing
                        digits: 5
                        digitHeight: Theme.s(30)
                        text: root.app.masterBpm.toFixed(2)
                    }
                    ParamValueBox {
                        id: typed
                        anchors.centerIn: parent
                        opacity: editing ? 1 : 0
                        value: root.app.masterBpm
                        decimals: 2
                        fontSize: Theme.s(22)
                        boxHeight: Theme.s(32)
                        onSubmitted: v => root.app.masterBpm = v
                        onStepped: (d, big) => root.app.nudgeMasterBpm(d * (big ? 1 : 0.1))
                    }
                    MouseArea {
                        id: tempoMouse
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
                ParamButton { text: "+.1"; onClicked: root.app.nudgeMasterBpm(0.1); anchors.verticalCenter: parent.verticalCenter }
                ParamButton { text: "+1"; onClicked: root.app.nudgeMasterBpm(1); anchors.verticalCenter: parent.verticalCenter }
            }
        }

        // ---- where the master clock is in the bar ----
        Column {
            spacing: Theme.s(4)
            ParamText { text: "bar"; caps: true; wrapMode: Text.NoWrap }
            Row {
                spacing: Theme.s(4)
                Repeater {
                    model: 4
                    ParamLight {
                        required property int index
                        width: Theme.s(18)
                        height: Theme.s(18)
                        on: Math.floor(root.app.masterBeat) % 4 === index
                        opacity: on && index > 0 ? 0.6 : 1
                    }
                }
            }
        }

        // ---- phase lock granularity ----
        Column {
            spacing: Theme.s(4)
            ParamText { text: "phase lock"; caps: true; wrapMode: Text.NoWrap }
            ParamChoice {
                width: Theme.s(110)
                model: ["Beat", "Bar"]
                currentIndex: root.app.phaseMode
                onActivated: i => root.app.phaseMode = i
            }
        }

        Item { Layout.fillWidth: true }

        ParamText {
            text: root.app.audioStatus
            tone: "faint"
            color: root.app.audioOk ? Theme.textFaint : Look.warn
            wrapMode: Text.NoWrap
            elide: Text.ElideLeft
            Layout.maximumWidth: Theme.s(240)
        }
    }
}

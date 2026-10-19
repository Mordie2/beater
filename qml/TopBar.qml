import QtQuick
import QtQuick.Layouts

Rectangle {
    id: root
    required property AppController app
    color: Theme.panel
    implicitHeight: 64

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 16
        anchors.rightMargin: 16
        spacing: 18

        Text {
            text: "beater"
            color: Theme.text
            font.family: Theme.mono
            font.pixelSize: 18
            font.bold: true
        }

        Item { Layout.fillWidth: true }

        // ---- master tempo: drag up/down, scroll, double-click to type ----
        RowLayout {
            spacing: 6
            Btn { text: "−1"; small: true; onClicked: root.app.nudgeMasterBpm(-1) }
            Btn { text: "−.1"; small: true; onClicked: root.app.nudgeMasterBpm(-0.1) }

            Rectangle {
                implicitWidth: 150
                implicitHeight: 46
                radius: Theme.radius
                color: Theme.bg
                border.color: tempoMouse.pressed || tempoEdit.visible ? Theme.text : Theme.line

                Text {
                    anchors.top: parent.top
                    anchors.topMargin: 4
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: "MASTER BPM"
                    color: Theme.faint
                    font.family: Theme.mono
                    font.pixelSize: 8
                    font.bold: true
                    font.letterSpacing: 1
                }
                Text {
                    visible: !tempoEdit.visible
                    anchors.bottom: parent.bottom
                    anchors.bottomMargin: 3
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: root.app.masterBpm.toFixed(2)
                    color: Theme.text
                    font.family: Theme.mono
                    font.pixelSize: 24
                    font.bold: true
                }
                TextInput {
                    id: tempoEdit
                    visible: false
                    anchors.bottom: parent.bottom
                    anchors.bottomMargin: 3
                    anchors.horizontalCenter: parent.horizontalCenter
                    color: Theme.text
                    font.family: Theme.mono
                    font.pixelSize: 24
                    font.bold: true
                    selectByMouse: true
                    validator: DoubleValidator { bottom: 60; top: 200; decimals: 2 }
                    onAccepted: { root.app.masterBpm = parseFloat(text); visible = false }
                    onActiveFocusChanged: if (!activeFocus) visible = false
                    Keys.onEscapePressed: visible = false
                }
                MouseArea {
                    id: tempoMouse
                    anchors.fill: parent
                    enabled: !tempoEdit.visible
                    property real startY: 0
                    property real startBpm: 0
                    cursorShape: Qt.SizeVerCursor
                    onPressed: (m) => { startY = m.y; startBpm = root.app.masterBpm }
                    onPositionChanged: (m) => {
                        const fine = m.modifiers & Qt.ShiftModifier
                        root.app.masterBpm = startBpm + (startY - m.y) * (fine ? 0.01 : 0.1)
                    }
                    onWheel: (w) => root.app.nudgeMasterBpm((w.angleDelta.y > 0 ? 1 : -1) * ((w.modifiers & Qt.ShiftModifier) ? 0.01 : 0.1))
                    onDoubleClicked: {
                        tempoEdit.text = root.app.masterBpm.toFixed(2)
                        tempoEdit.visible = true
                        tempoEdit.forceActiveFocus()
                        tempoEdit.selectAll()
                    }
                }
            }

            Btn { text: "+.1"; small: true; onClicked: root.app.nudgeMasterBpm(0.1) }
            Btn { text: "+1"; small: true; onClicked: root.app.nudgeMasterBpm(1) }
        }

        // ---- bar position of the master clock ----
        Row {
            spacing: 4
            Repeater {
                model: 4
                Rectangle {
                    required property int index
                    width: 14
                    height: 14
                    radius: 2
                    readonly property int current: Math.floor(root.app.masterBeat) % 4
                    color: current === index ? (index === 0 ? Theme.text : Theme.dim) : Theme.line
                }
            }
        }

        // ---- phase lock granularity ----
        Column {
            spacing: 3
            Text {
                text: "PHASE"
                color: Theme.faint
                font.family: Theme.mono
                font.pixelSize: 9
                font.bold: true
                font.letterSpacing: 1
            }
            Row {
                spacing: 2
                Btn { text: "BEAT"; small: true; checked: root.app.phaseMode === 0; onClicked: root.app.phaseMode = 0 }
                Btn { text: "BAR"; small: true; checked: root.app.phaseMode === 1; onClicked: root.app.phaseMode = 1 }
            }
        }

        Item { Layout.fillWidth: true }

        Column {
            spacing: 4
            Text {
                text: "MASTER"
                color: Theme.faint
                font.family: Theme.mono
                font.pixelSize: 9
                font.bold: true
                font.letterSpacing: 1
            }
            Fader {
                horizontal: true
                width: 150
                value: root.app.masterVolume
                level: root.app.masterLevel
                resetValue: 0.85
                onMoved: (v) => root.app.masterVolume = v
            }
        }

        Text {
            text: root.app.audioStatus
            color: root.app.audioOk ? Theme.faint : Theme.warn
            font.family: Theme.mono
            font.pixelSize: 10
            elide: Text.ElideLeft
            Layout.maximumWidth: 200
        }
    }

    Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: Theme.line }
}

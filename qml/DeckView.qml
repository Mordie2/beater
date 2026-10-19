import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

Rectangle {
    id: root
    required property DeckController deck
    required property AppController app
    property color accent: Theme.deckA

    color: Theme.panel
    radius: Theme.radius
    border.color: Theme.line

    function fmt(s) {
        if (!(s > 0)) return "0:00"
        const t = Math.floor(s)
        return Math.floor(t / 60) + ":" + String(t % 60).padStart(2, "0")
    }

    RowLayout {
        anchors.fill: parent
        anchors.margins: 12
        spacing: 12

        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 8

            // ---- header ----
            RowLayout {
                spacing: 10
                Rectangle {
                    implicitWidth: 26
                    implicitHeight: 26
                    radius: Theme.radius
                    color: root.accent
                    Text {
                        anchors.centerIn: parent
                        text: root.deck.name
                        color: Theme.bg
                        font.family: Theme.mono
                        font.pixelSize: 14
                        font.bold: true
                    }
                }
                Text {
                    Layout.fillWidth: true
                    text: root.deck.loading ? "loading…"
                        : root.deck.error.length ? root.deck.error
                        : root.deck.loaded ? root.deck.title : "empty — double-click a track or press " + root.deck.name + " in the playlist"
                    color: root.deck.error.length ? Theme.warn : (root.deck.loaded ? Theme.text : Theme.faint)
                    font.pixelSize: 14
                    elide: Text.ElideRight
                }
                Text {
                    visible: root.deck.loaded
                    text: root.fmt(root.deck.position) + "  −" + root.fmt(root.deck.duration - root.deck.position)
                    color: Theme.dim
                    font.family: Theme.mono
                    font.pixelSize: 12
                }
            }

            // ---- waveforms ----
            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.minimumHeight: 60
                color: Theme.bg
                radius: Theme.radius
                clip: true
                WaveformItem {
                    anchors.fill: parent
                    deck: root.deck
                    zoom: true
                    zoomBeats: 16
                    color: root.accent
                }
                // phase error vs master (only meaningful with sync on)
                Rectangle {
                    visible: root.deck.playing && root.deck.sync
                    anchors.bottom: parent.bottom
                    x: parent.width / 2 + root.deck.phaseError * parent.width / 16 - 1
                    width: 3
                    height: 4
                    color: Math.abs(root.deck.phaseError) < 0.02 ? Theme.good : Theme.warn
                }
            }
            Rectangle {
                Layout.fillWidth: true
                implicitHeight: 46
                color: Theme.bg
                radius: Theme.radius
                clip: true
                WaveformItem {
                    anchors.fill: parent
                    deck: root.deck
                    color: root.accent
                }
            }

            // ---- transport + loop ----
            RowLayout {
                spacing: 6
                Btn {
                    text: root.deck.playing ? "PAUSE" : "PLAY"
                    checked: root.deck.playing
                    accent: root.accent
                    implicitWidth: 64
                    enabled: root.deck.loaded
                    onClicked: root.deck.togglePlay()
                }
                Btn { text: "CUE"; enabled: root.deck.loaded; onClicked: root.deck.cue() }
                Btn {
                    text: "SYNC"
                    checked: root.deck.sync
                    accent: root.accent
                    onClicked: root.deck.sync = !root.deck.sync
                }
                Btn { text: "◀◀"; small: true; enabled: root.deck.loaded; onClicked: root.deck.jumpBeats(-16) }
                Btn { text: "▶▶"; small: true; enabled: root.deck.loaded; onClicked: root.deck.jumpBeats(16) }

                Item { Layout.fillWidth: true }

                Btn {
                    text: "LOOP"
                    checked: root.deck.loopEnabled
                    accent: root.deck.inLoop ? Theme.good : Theme.text
                    onClicked: root.deck.loopEnabled = !root.deck.loopEnabled
                }
                Row {
                    spacing: 2
                    Repeater {
                        model: [1, 2, 4, 8, 16, 32]
                        Btn {
                            required property int modelData
                            text: modelData
                            small: true
                            checked: root.deck.loopBars === modelData
                            onClicked: root.deck.loopBars = modelData
                        }
                    }
                }
                Btn { text: "HERE"; small: true; enabled: root.deck.loaded && root.deck.bpm > 0; onClicked: root.deck.loopHere() }
                Btn {
                    text: "END"
                    small: true
                    checked: root.deck.loopAuto
                    enabled: root.deck.loaded
                    onClicked: root.deck.endLoop()
                }
            }

            // ---- tempo / grid / key ----
            RowLayout {
                spacing: 14

                Item {
                    implicitWidth: bpmReadout.implicitWidth
                    implicitHeight: bpmReadout.implicitHeight
                    Readout {
                        id: bpmReadout
                        label: root.deck.gridManual ? "BPM · EDITED" : "BPM"
                        value: root.deck.bpm > 0 ? root.deck.bpm.toFixed(2) : ""
                        visible: !bpmEdit.visible
                    }
                    TextInput {
                        id: bpmEdit
                        visible: false
                        y: 12
                        width: 70
                        color: Theme.text
                        font.family: Theme.mono
                        font.pixelSize: 15
                        selectByMouse: true
                        validator: DoubleValidator { bottom: 40; top: 250; decimals: 2 }
                        onAccepted: { root.deck.setBpm(parseFloat(text)); visible = false }
                        onActiveFocusChanged: if (!activeFocus) visible = false
                        Keys.onEscapePressed: visible = false
                    }
                    MouseArea {
                        anchors.fill: parent
                        enabled: root.deck.loaded && !bpmEdit.visible
                        onDoubleClicked: {
                            bpmEdit.text = root.deck.bpm > 0 ? root.deck.bpm.toFixed(2) : ""
                            bpmEdit.visible = true
                            bpmEdit.forceActiveFocus()
                            bpmEdit.selectAll()
                        }
                    }
                }
                Readout {
                    label: "PLAYING"
                    value: root.deck.playingBpm > 0 ? root.deck.playingBpm.toFixed(2) : ""
                    valueColor: root.deck.sync ? root.accent : Theme.text
                }
                Row {
                    spacing: 2
                    Layout.alignment: Qt.AlignBottom
                    Btn { text: "×2"; small: true; enabled: root.deck.bpm > 0; onClicked: root.deck.doubleBpm() }
                    Btn { text: "÷2"; small: true; enabled: root.deck.bpm > 0; onClicked: root.deck.halveBpm() }
                    Btn { text: "TAP"; small: true; enabled: root.deck.loaded; onClicked: root.deck.tap() }
                    Btn { text: "=M"; small: true; enabled: root.deck.bpm > 0; onClicked: root.app.matchMasterTo(root.deck.index) }
                }
                Column {
                    spacing: 3
                    Layout.alignment: Qt.AlignBottom
                    Text { text: "GRID"; color: Theme.faint; font.family: Theme.mono; font.pixelSize: 9; font.bold: true; font.letterSpacing: 1 }
                    Row {
                        spacing: 2
                        Btn { text: "◀"; small: true; enabled: root.deck.bpm > 0; onClicked: root.deck.nudgeGrid(-5) }
                        Btn { text: "▶"; small: true; enabled: root.deck.bpm > 0; onClicked: root.deck.nudgeGrid(5) }
                        Btn { text: "SET 1"; small: true; enabled: root.deck.bpm > 0; onClicked: root.deck.setDownbeatHere() }
                    }
                }

                Item { Layout.fillWidth: true }

                Column {
                    spacing: 3
                    Layout.alignment: Qt.AlignBottom
                    Text { text: "KEY"; color: Theme.faint; font.family: Theme.mono; font.pixelSize: 9; font.bold: true; font.letterSpacing: 1 }
                    ComboBox {
                        id: keyBox
                        implicitWidth: 104
                        implicitHeight: 24
                        enabled: root.deck.loaded
                        model: root.deck.keyNames()
                        currentIndex: root.deck.key
                        displayText: root.deck.key >= 0 ? root.deck.keyText : "set key"
                        font.family: Theme.mono
                        font.pixelSize: 11
                        onActivated: (i) => root.deck.setKey(i)
                        background: Rectangle { color: Theme.raised; border.color: Theme.line; radius: Theme.radius }
                        contentItem: Text {
                            leftPadding: 8
                            text: keyBox.displayText
                            color: root.deck.key >= 0 ? Theme.text : Theme.faint
                            font: keyBox.font
                            verticalAlignment: Text.AlignVCenter
                        }
                    }
                }
                Column {
                    spacing: 3
                    Layout.alignment: Qt.AlignBottom
                    Text { text: "SHIFT"; color: Theme.faint; font.family: Theme.mono; font.pixelSize: 9; font.bold: true; font.letterSpacing: 1 }
                    Row {
                        spacing: 2
                        Btn { text: "−"; small: true; onClicked: root.deck.keyShift = root.deck.keyShift - 1 }
                        Btn {
                            text: (root.deck.keyShift > 0 ? "+" : "") + root.deck.keyShift
                            small: true
                            implicitWidth: 34
                            checked: root.deck.keyShift !== 0
                            accent: root.accent
                            onClicked: root.deck.keyShift = 0
                        }
                        Btn { text: "+"; small: true; onClicked: root.deck.keyShift = root.deck.keyShift + 1 }
                    }
                }
                Readout {
                    visible: root.deck.keyShift !== 0 && root.deck.key >= 0
                    label: "NOW"
                    value: root.deck.shiftedKeyText
                    valueColor: root.accent
                }
            }
        }

        // ---- channel fader ----
        ColumnLayout {
            Layout.fillHeight: true
            spacing: 6
            Fader {
                Layout.fillHeight: true
                Layout.alignment: Qt.AlignHCenter
                value: root.deck.volume
                level: root.deck.level
                accent: root.accent
                onMoved: (v) => root.deck.volume = v
            }
            Text {
                Layout.alignment: Qt.AlignHCenter
                text: Math.round(root.deck.volume * 100)
                color: Theme.dim
                font.family: Theme.mono
                font.pixelSize: 10
            }
        }
    }
}

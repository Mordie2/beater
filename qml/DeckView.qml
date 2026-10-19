import QtQuick
import QtQuick.Layouts
import HighlighterUI

// One deck in Edit mode, full width: track, the big waveform, the timeline
// with its loop lane, then three rows of controls. The rows keep a fixed
// height whatever their labels say, so the waveform never jumps.
Panel {
    id: root
    required property DeckController deck
    required property AppController app
    //! The deck's colour; every kit control inside draws in it (Theme.accentAt).
    property color accent: Look.deckA

    function fmt(s) {
        if (!(s > 0)) return "0:00"
        const t = Math.floor(s)
        return Math.floor(t / 60) + ":" + String(t % 60).padStart(2, "0")
    }

    DeckDrop {
        anchors.fill: parent
        anchors.margins: -Theme.s(8)
        deck: root.deck
        app: root.app
    }

    component Label: ParamText { caps: true; wrapMode: Text.NoWrap; Layout.alignment: Qt.AlignVCenter }
    component Gap: Item { implicitWidth: Theme.s(14) }

    ColumnLayout {
        anchors.fill: parent
        spacing: Theme.s(8)

        // ---- deck, track, time ----
        RowLayout {
            spacing: Theme.s(10)
            Rectangle {
                implicitWidth: Theme.s(22)
                implicitHeight: Theme.s(22)
                color: root.accent
                Text {
                    anchors.centerIn: parent
                    text: root.deck.name
                    color: Theme.inkOf(root.accent)
                    font.family: Theme.family
                    font.pixelSize: Theme.labelSize
                    font.weight: Font.Bold
                }
            }
            ParamText {
                Layout.fillWidth: true
                text: root.deck.loading ? "loading…"
                    : root.deck.error.length ? root.deck.error
                    : root.deck.loaded ? root.deck.title
                    : "empty: drag a track here, double-click one, or press " + root.deck.name + " in the playlist"
                color: root.deck.error.length ? Look.warn : root.deck.loaded ? Theme.text : Theme.textFaint
                wrapMode: Text.NoWrap
                elide: Text.ElideRight
            }
            ParamText {
                visible: root.deck.loaded
                text: root.fmt(root.deck.position) + "  −" + root.fmt(root.deck.duration - root.deck.position)
                mono: true
                wrapMode: Text.NoWrap
            }
        }

        // ---- the big waveform ----
        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: Theme.s(60)
            color: Look.wave
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
                color: Math.abs(root.deck.phaseError) < 0.02 ? Look.good : Look.warn
            }
        }

        // ---- timeline + loop lane ----
        ColumnLayout {
            spacing: Theme.s(2)
            Rectangle {
                Layout.fillWidth: true
                implicitHeight: Theme.s(40)
                color: Look.wave
                clip: true
                WaveformItem {
                    anchors.fill: parent
                    deck: root.deck
                    color: root.accent
                }
            }
            LoopLane {
                Layout.fillWidth: true
                deck: root.deck
            }
        }

        // ---- transport, tempo ----
        RowLayout {
            spacing: Theme.s(4)
            ParamButton {
                implicitWidth: Theme.s(92)
                accent: true
                icon: root.deck.playing ? "❚❚" : "▶"
                text: root.deck.playing ? "Pause" : "Play"
                enabled: root.deck.loaded
                onClicked: root.deck.togglePlay()
            }
            ParamButton { text: "Cue"; enabled: root.deck.loaded; onClicked: root.deck.cue() }
            ParamButton {
                text: "Sync"
                checkable: true
                checked: root.deck.sync
                onToggled: v => root.deck.sync = v
            }
            ParamButton { icon: "◀◀"; implicitWidth: Theme.s(34); enabled: root.deck.loaded; onClicked: root.deck.jumpBeats(-16) }
            ParamButton { icon: "▶▶"; implicitWidth: Theme.s(34); enabled: root.deck.loaded; onClicked: root.deck.jumpBeats(16) }

            Item { Layout.fillWidth: true }

            Label { text: root.deck.analyzing ? "bpm …" : root.deck.gridManual ? "bpm *" : "bpm" }
            // click to type, Up / Down step 0.01 (Shift 1)
            ParamValueBox {
                visible: root.deck.bpm > 0
                Layout.alignment: Qt.AlignVCenter
                value: root.deck.bpm
                decimals: 2
                interactive: root.deck.loaded
                fontSize: Theme.s(16)
                boxHeight: Theme.s(26)
                onSubmitted: v => root.deck.setBpm(v)
                onStepped: (d, big) => root.deck.setBpm(root.deck.bpm + d * (big ? 1 : 0.01))
            }
            ParamText {
                text: root.deck.playingBpm > 0 ? "→ " + root.deck.playingBpm.toFixed(2) : ""
                color: root.deck.sync ? root.accent : Theme.text
                mono: true
                wrapMode: Text.NoWrap
                Layout.preferredWidth: Theme.s(70)
            }
            Gap {}
            ParamButton { text: "×2"; enabled: root.deck.bpm > 0; onClicked: root.deck.doubleBpm() }
            ParamButton { text: "÷2"; enabled: root.deck.bpm > 0; onClicked: root.deck.halveBpm() }
            ParamButton { text: "Tap"; enabled: root.deck.loaded; onClicked: root.deck.tap() }
            ParamButton { text: "=M"; enabled: root.deck.bpm > 0; onClicked: root.app.matchMasterTo(root.deck.index) }
        }

        // ---- loop ----
        RowLayout {
            spacing: Theme.s(4)
            Label { text: "loop"; Layout.rightMargin: Theme.s(4) }
            ParamButton {
                text: "On"
                checkable: true
                checked: root.deck.loopEnabled
                onToggled: v => root.deck.loopEnabled = v
            }
            ParamChoice {
                Layout.preferredWidth: implicitWidth
                Layout.alignment: Qt.AlignVCenter
                implicitWidth: 6 * Theme.s(34)
                cellMinWidth: Theme.s(30)
                model: ["1", "2", "4", "8", "16", "32"]
                // the loop's actual length (none lit when it's not one of these)
                currentIndex: model.indexOf(String(root.deck.loopAuto ? root.deck.loopBars
                                                   : (root.deck.loopEndBeat - root.deck.loopStartBeat) / 4))
                onActivated: i => root.deck.loopBars = parseInt(model[i])
            }
            Label { text: "bars" }
            ParamButton { text: "Here"; enabled: root.deck.loaded && root.deck.bpm > 0; onClicked: root.deck.loopHere() }
            ParamButton {
                // not checkable: End goes back to the automatic end loop, it doesn't switch off
                text: "End"
                checked: root.deck.loopAuto
                enabled: root.deck.loaded
                onClicked: root.deck.endLoop()
            }
            ParamText {
                text: "drag the loop under the timeline: ends resize, middle moves, empty lane draws · ⌥ beats"
                tone: "faint"
                small: true
                wrapMode: Text.NoWrap
                elide: Text.ElideRight
                Layout.fillWidth: true
                Layout.leftMargin: Theme.s(10)
            }
        }

        // ---- grid, key ----
        RowLayout {
            spacing: Theme.s(4)
            Label { text: "grid"; Layout.rightMargin: Theme.s(4) }
            ParamButton { icon: "◀"; implicitWidth: Theme.s(28); enabled: root.deck.bpm > 0; onClicked: root.deck.nudgeGrid(-5) }
            ParamButton { icon: "▶"; implicitWidth: Theme.s(28); enabled: root.deck.bpm > 0; onClicked: root.deck.nudgeGrid(5) }
            ParamButton { text: "Set 1"; enabled: root.deck.bpm > 0; onClicked: root.deck.setDownbeatHere() }
            // back to the detected BPM and grid
            ParamButton { text: "Auto"; enabled: root.deck.gridManual && !root.deck.analyzing; onClicked: root.deck.autoGrid() }

            Item { Layout.fillWidth: true }

            Label { text: root.deck.analyzing ? "key …" : root.deck.keyManual ? "key *" : "key"; Layout.rightMargin: Theme.s(4) }
            ParamCombo {
                Layout.preferredWidth: Theme.s(110)
                enabled: root.deck.loaded
                model: root.deck.keyNames
                currentIndex: root.deck.key
                onActivated: i => root.deck.setKey(i)
            }
            // back to the detected key
            ParamButton { text: "Auto"; enabled: root.deck.keyManual && !root.deck.analyzing; onClicked: root.deck.autoKey() }
            Gap {}
            Label { text: "shift"; Layout.rightMargin: Theme.s(4) }
            ParamButton { text: "−"; implicitWidth: Theme.s(28); enabled: root.deck.keyShift > -12; onClicked: root.deck.keyShift = root.deck.keyShift - 1 }
            ParamValueBox {
                Layout.alignment: Qt.AlignVCenter
                value: root.deck.keyShift
                unit: " st"
                boxHeight: Theme.s(26)
                onSubmitted: v => root.deck.keyShift = Math.round(v)
                onStepped: (d, big) => root.deck.keyShift = root.deck.keyShift + d * (big ? 12 : 1)
            }
            ParamButton { text: "+"; implicitWidth: Theme.s(28); enabled: root.deck.keyShift < 12; onClicked: root.deck.keyShift = root.deck.keyShift + 1 }
            ParamButton { text: "0"; implicitWidth: Theme.s(28); enabled: root.deck.keyShift !== 0; onClicked: root.deck.keyShift = 0 }
            ParamText {
                // always there (empty when not shifted), so nothing moves when it appears
                text: root.deck.keyShift !== 0 && root.deck.key >= 0 ? "→ " + root.deck.shiftedKeyText : ""
                color: root.accent
                mono: true
                wrapMode: Text.NoWrap
                Layout.preferredWidth: Theme.s(60)
                Layout.leftMargin: Theme.s(6)
            }
        }
    }
}

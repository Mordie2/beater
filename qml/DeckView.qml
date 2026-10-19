import QtQuick
import QtQuick.Layouts
import HighlighterUI

// One deck in Edit mode, full width: track, the big waveform, the timeline
// with its loop lane and tags, one row of controls, and the space reserved
// for effects. The pencil at the end of the row switches it to grid edit:
// BPM, grid and key, and the big waveform drags the grid instead of the track.
// Rows keep a fixed height whatever their labels say, so nothing jumps.
Panel {
    id: root
    required property DeckController deck
    required property AppController app
    //! The deck's colour; every kit control inside draws in it (Theme.accentAt).
    property color accent: Look.deckA
    //! Editing the grid, BPM and key (the pencil).
    property bool gridEdit: false
    //! A narrower deck: the row drops the playing BPM and the shift reset.
    readonly property bool tight: width < Theme.s(1180)

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
    component Gap: Item { implicitWidth: Theme.s(6) }
    // the row is full: a little less padding than the kit's default
    component Btn: ParamButton { horizontalPadding: Theme.s(8) }

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
                gridColor: Look.tempo
                loopColor: Look.loop
                gridEdit: root.gridEdit
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

        // ---- timeline + loop lane with tags ----
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
                    gridColor: Look.tempo
                    loopColor: Look.loop
                }
            }
            LoopLane {
                Layout.fillWidth: true
                deck: root.deck
                tags: true
            }
        }

        // ---- one row: playing, or (pencil) editing the grid ----
        Item {
            Layout.fillWidth: true
            implicitHeight: Theme.s(30)

            // playing: transport · sync · in / out · length · tempo · key · shift
            RowLayout {
                anchors.fill: parent
                visible: !root.gridEdit
                spacing: Theme.s(4)

                RowLayout {
                    property color accent: Look.transport
                    Btn {
                        implicitWidth: Theme.s(86)
                        accent: true
                        icon: root.deck.playing ? "❚❚" : "▶"
                        text: root.deck.playing ? "Pause" : "Play"
                        enabled: root.deck.loaded
                        onClicked: root.deck.togglePlay()
                    }
                }
                Gap {}
                RowLayout {
                    property color accent: Look.tempo
                    Btn {
                        text: "Sync"
                        checkable: true
                        checked: root.deck.sync
                        onToggled: v => root.deck.sync = v
                    }
                }
                Gap {}
                RowLayout {
                    property color accent: Look.loop
                    spacing: Theme.s(4)
                    Btn {
                        text: "In"
                        checked: root.deck.customPending   // lit while it waits for an Out
                        enabled: root.deck.loaded && root.deck.bpm > 0
                        onClicked: root.deck.loopIn()
                    }
                    Btn {
                        text: "Out"
                        enabled: root.deck.customPending || root.deck.hasCustom
                        onClicked: root.deck.loopOut()
                    }
                    Btn {
                        // the In / Out loop on / off (back on past it jumps back in)
                        text: "Loop"
                        checkable: true
                        checked: root.deck.customOn
                        enabled: root.deck.hasCustom
                        onToggled: v => root.deck.customOn = v
                    }
                    Gap {}
                    ParamChoice {
                        Layout.preferredWidth: implicitWidth
                        Layout.alignment: Qt.AlignVCenter
                        implicitWidth: 4 * Theme.s(28) + 2 * Theme.s(36)   // what its cells take
                        cellMinWidth: Theme.s(26)
                        model: ["1", "2", "4", "8", "16", "32"]
                        // the length of the loop in use (none lit when it's not one of these)
                        currentIndex: model.indexOf(String((root.deck.activeLoopEndBeat - root.deck.activeLoopStartBeat) / 4))
                        onActivated: i => root.deck.setLoopLength(parseInt(model[i]))
                    }
                }
                Gap {}
                RowLayout {
                    property color accent: Look.tempo
                    spacing: Theme.s(4)
                    Label { text: root.deck.analyzing ? "bpm …" : root.deck.gridManual ? "bpm *" : "bpm"; color: Look.label(Look.tempo) }
                    ParamValueBox {
                        // click to type, Up / Down step 0.01 (Shift 1)
                        visible: root.deck.bpm > 0
                        Layout.alignment: Qt.AlignVCenter
                        value: root.deck.bpm
                        decimals: 2
                        interactive: root.deck.loaded
                        fontSize: Theme.s(15)
                        boxHeight: Theme.s(26)
                        onSubmitted: v => root.deck.setBpm(v)
                        onStepped: (d, big) => root.deck.setBpm(root.deck.bpm + d * (big ? 1 : 0.01))
                    }
                    ParamText {
                        visible: !root.tight
                        text: root.deck.playingBpm > 0 ? "→ " + root.deck.playingBpm.toFixed(2) : ""
                        color: Look.tempo
                        mono: true
                        wrapMode: Text.NoWrap
                        Layout.preferredWidth: Theme.s(58)
                    }
                    Btn { text: "×2"; enabled: root.deck.bpm > 0; onClicked: root.deck.doubleBpm() }
                    Btn { text: "÷2"; enabled: root.deck.bpm > 0; onClicked: root.deck.halveBpm() }
                }
                Gap {}
                RowLayout {
                    property color accent: Look.key
                    spacing: Theme.s(4)
                    Label { text: root.deck.analyzing ? "key …" : root.deck.keyManual ? "key *" : "key"; color: Look.label(Look.key) }
                    ParamCombo {
                        Layout.preferredWidth: Theme.s(84)
                        enabled: root.deck.loaded
                        model: root.deck.keyNames
                        currentIndex: root.deck.key
                        onActivated: i => root.deck.setKey(i)
                    }
                    Btn { text: "Auto"; enabled: root.deck.keyManual && !root.deck.analyzing; onClicked: root.deck.autoKey() }
                    Gap {}
                    Label { text: "shift"; color: Look.label(Look.key) }
                    Btn { text: "−"; implicitWidth: Theme.s(26); enabled: root.deck.keyShift > -12; onClicked: root.deck.keyShift = root.deck.keyShift - 1 }
                    ParamValueBox {
                        Layout.alignment: Qt.AlignVCenter
                        value: root.deck.keyShift
                        unit: " st"
                        boxHeight: Theme.s(26)
                        onSubmitted: v => root.deck.keyShift = Math.round(v)
                        onStepped: (d, big) => root.deck.keyShift = root.deck.keyShift + d * (big ? 12 : 1)
                    }
                    Btn { text: "+"; implicitWidth: Theme.s(26); enabled: root.deck.keyShift < 12; onClicked: root.deck.keyShift = root.deck.keyShift + 1 }
                    Btn { visible: !root.tight; text: "0"; implicitWidth: Theme.s(26); enabled: root.deck.keyShift !== 0; onClicked: root.deck.keyShift = 0 }
                }
                Item { Layout.fillWidth: true }
                RowLayout {
                    property color accent: Look.tempo   // the grid, BPM and key
                    Btn {
                        objectName: "gridEditButton"
                        icon: "✎"
                        implicitWidth: Theme.s(34)
                        enabled: root.deck.loaded
                        onClicked: root.gridEdit = true
                    }
                }
            }

            // editing: the track's BPM and grid, and its key
            RowLayout {
                anchors.fill: parent
                visible: root.gridEdit
                spacing: Theme.s(4)

                RowLayout {
                    property color accent: Look.tempo
                    spacing: Theme.s(4)
                    Label { text: "bpm"; color: Look.label(Look.tempo) }
                    ParamValueBox {
                        Layout.alignment: Qt.AlignVCenter
                        value: root.deck.bpm
                        decimals: 2
                        interactive: root.deck.loaded
                        fontSize: Theme.s(15)
                        boxHeight: Theme.s(26)
                        onSubmitted: v => root.deck.setBpm(v)
                        onStepped: (d, big) => root.deck.setBpm(root.deck.bpm + d * (big ? 1 : 0.01))
                    }
                    Btn { text: "×2"; enabled: root.deck.bpm > 0; onClicked: root.deck.doubleBpm() }
                    Btn { text: "÷2"; enabled: root.deck.bpm > 0; onClicked: root.deck.halveBpm() }
                    Btn { text: "Tap"; onClicked: root.deck.tap() }
                    Btn { text: "=M"; enabled: root.deck.bpm > 0; onClicked: root.app.matchMasterTo(root.deck.index) }
                    Gap {}
                    Label { text: "grid"; color: Look.label(Look.tempo) }
                    Btn { icon: "◀"; implicitWidth: Theme.s(28); enabled: root.deck.bpm > 0; onClicked: root.deck.nudgeGrid(-5) }
                    Btn { icon: "▶"; implicitWidth: Theme.s(28); enabled: root.deck.bpm > 0; onClicked: root.deck.nudgeGrid(5) }
                    Btn { text: "Set 1"; enabled: root.deck.bpm > 0; onClicked: root.deck.setDownbeatHere() }
                    // back to the detected BPM and grid
                    Btn { text: "Auto"; enabled: root.deck.gridManual && !root.deck.analyzing; onClicked: root.deck.autoGrid() }
                }
                Gap {}
                RowLayout {
                    property color accent: Look.key
                    spacing: Theme.s(4)
                    Label { text: "key"; color: Look.label(Look.key) }
                    ParamCombo {
                        Layout.preferredWidth: Theme.s(84)
                        model: root.deck.keyNames
                        currentIndex: root.deck.key
                        onActivated: i => root.deck.setKey(i)
                    }
                    Btn { text: "Auto"; enabled: root.deck.keyManual && !root.deck.analyzing; onClicked: root.deck.autoKey() }
                }
                ParamText {
                    text: "drag the waveform to move the grid · double-click a beat to make it the 1"
                    tone: "faint"
                    small: true
                    wrapMode: Text.NoWrap
                    elide: Text.ElideRight
                    Layout.fillWidth: true
                    Layout.leftMargin: Theme.s(10)
                }
                RowLayout {
                    property color accent: Look.tempo
                    Btn {
                        // back to playing
                        icon: "✎"
                        text: "Done"
                        checkable: true
                        checked: true
                        onToggled: root.gridEdit = false
                    }
                }
            }
        }

        // ---- reserved: effects go here ----
        Item {
            Layout.fillWidth: true
            implicitHeight: Theme.s(64)
            Rectangle {
                anchors.fill: parent
                color: "transparent"
                border.width: 1
                border.color: Qt.rgba(1, 1, 1, 0.05)
            }
            ParamText {
                anchors.centerIn: parent
                text: "effects"
                caps: true
                tone: "faint"
                opacity: 0.4
                wrapMode: Text.NoWrap
            }
        }
    }
}

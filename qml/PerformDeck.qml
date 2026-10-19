import QtQuick
import QtQuick.Layouts
import HighlighterUI

// One deck, as small as it gets: track and time, the overview (click seeks,
// drag sets the loop), play / sync / loop, and the deck's volume.
ParamPlate {
    id: root
    required property DeckController deck
    property color accent: Look.deckA

    implicitHeight: col.implicitHeight + 2 * Theme.s(8)

    function fmt(s) {
        if (!(s > 0)) return "0:00"
        const t = Math.floor(s)
        return Math.floor(t / 60) + ":" + String(t % 60).padStart(2, "0")
    }

    ColumnLayout {
        id: col
        anchors.fill: parent
        anchors.margins: Theme.s(8)
        spacing: Theme.s(6)

        RowLayout {
            spacing: Theme.s(6)
            Rectangle {
                implicitWidth: Theme.s(18)
                implicitHeight: Theme.s(18)
                color: root.accent
                Text {
                    anchors.centerIn: parent
                    text: root.deck.name
                    color: Theme.inkOf(root.accent)
                    font.family: Theme.family
                    font.pixelSize: Theme.smallSize
                    font.weight: Font.Bold
                }
            }
            ParamText {
                Layout.fillWidth: true
                text: root.deck.loading ? "loading…" : root.deck.error.length ? root.deck.error
                    : root.deck.loaded ? root.deck.title : "empty"
                tone: root.deck.loaded ? "normal" : "faint"
                small: true
                wrapMode: Text.NoWrap
                elide: Text.ElideRight
            }
            ParamText {
                visible: root.deck.loaded
                text: "−" + root.fmt(root.deck.duration - root.deck.position)
                mono: true
                small: true
                wrapMode: Text.NoWrap
            }
        }

        Rectangle {
            Layout.fillWidth: true
            implicitHeight: Theme.s(30)
            color: Look.wave
            clip: true
            WaveformItem {
                anchors.fill: parent
                deck: root.deck
                color: root.accent
            }
            // the deck's level along the bottom edge
            Rectangle {
                anchors.bottom: parent.bottom
                height: Theme.s(2)
                width: parent.width * Math.min(1, root.deck.level)
                color: root.deck.level > 0.9 ? Theme.danger : root.accent
            }
        }

        RowLayout {
            spacing: Theme.s(4)
            ParamButton {
                implicitWidth: Theme.s(40)
                accent: true
                icon: root.deck.playing ? "❚❚" : "▶"
                enabled: root.deck.loaded
                onClicked: root.deck.togglePlay()
            }
            ParamButton {
                text: "Sync"
                checkable: true
                checked: root.deck.sync
                onToggled: v => root.deck.sync = v
            }
            ParamButton {
                text: root.deck.inLoop ? "Loop ●" : "Loop"
                checkable: true
                checked: root.deck.loopEnabled
                onToggled: v => root.deck.loopEnabled = v
            }
            Item { Layout.fillWidth: true }
            ParamText {
                // key (shifted key when shifted) and tempo, at a glance
                text: (root.deck.keyShift !== 0 ? root.deck.shiftedKeyText : root.deck.keyText)
                      + (root.deck.bpm > 0 ? "   " + root.deck.bpm.toFixed(1) : "")
                color: Theme.textDim
                mono: true
                small: true
                wrapMode: Text.NoWrap
            }
        }

        ParamSlider {
            Layout.fillWidth: true
            key: "perform.deck" + root.deck.name
            compact: true
            sizeToggle: false
            unit: " %"
            from: 0; to: 100; limitFrom: 0; limitTo: 100
            defaultValue: 80
            value: Math.round(root.deck.volume * 100)
            onMoved: v => root.deck.volume = v / 100
        }
    }
}

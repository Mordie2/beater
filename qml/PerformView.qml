import QtQuick
import QtQuick.Layouts
import HighlighterUI

// The compact column to keep open next to Resolume: master tempo, both decks'
// essentials, master volume and the playlist to load the next track.
Item {
    id: root
    required property AppController app
    property bool onTop: false
    signal editRequested()
    signal onTopToggled(bool on)

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.s(8)
        spacing: Theme.s(8)

        RowLayout {
            spacing: Theme.s(8)
            MasterTempo {
                app: root.app
                digitHeight: Theme.s(20)
            }
            BeatLights {
                app: root.app
                size: Theme.s(10)
                spacing: Theme.s(3)
                Layout.alignment: Qt.AlignVCenter
            }
            Item { Layout.fillWidth: true }
            ParamButton {
                // keep the window above Resolume
                text: "Pin"
                checkable: true
                checked: root.onTop
                onToggled: v => root.onTopToggled(v)
            }
            ModeSwitch {
                Layout.preferredWidth: Theme.s(116)
                perform: true
                onSwitched: p => { if (!p) root.editRequested() }
            }
        }

        PerformDeck {
            deck: root.app.deckA
            accent: Look.deckA
            Layout.fillWidth: true
        }
        PerformDeck {
            deck: root.app.deckB
            accent: Look.deckB
            Layout.fillWidth: true
        }

        ParamSlider {
            property color accent: Theme.text   // neutral, like the edit view's master fader
            Layout.fillWidth: true
            key: "perform.master"
            label: "master"
            compact: true
            sizeToggle: false
            unit: " %"
            from: 0; to: 100; limitFrom: 0; limitTo: 100
            defaultValue: 85
            value: Math.round(root.app.masterVolume * 100)
            onMoved: v => root.app.masterVolume = v / 100
        }

        Playlist {
            app: root.app
            compact: true
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: Theme.s(120)
        }
    }
}

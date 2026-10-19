import QtQuick
import QtQuick.Layouts
import HighlighterUI

// Everything: waveforms, transport, loops, grid / BPM / key editing, mix, playlist.
Item {
    id: root
    required property AppController app
    signal performRequested()

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.s(12)
        spacing: Theme.s(12)

        TopBar {
            app: root.app
            Layout.fillWidth: true
            onPerformRequested: root.performRequested()
        }

        RowLayout {
            Layout.fillWidth: true
            // a nested layout inherits fillHeight from its children; the playlist gets the rest
            Layout.fillHeight: false
            Layout.preferredHeight: Theme.s(480)
            spacing: Theme.s(12)
            DeckView {
                app: root.app
                deck: root.app.deckA
                accent: Look.deckA
                Layout.fillWidth: true
                Layout.fillHeight: true
            }
            MixView {
                app: root.app
                Layout.preferredWidth: Theme.s(250)
                Layout.fillHeight: true
            }
            DeckView {
                app: root.app
                deck: root.app.deckB
                accent: Look.deckB
                Layout.fillWidth: true
                Layout.fillHeight: true
            }
        }

        Playlist {
            app: root.app
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: Theme.s(160)
        }
    }
}

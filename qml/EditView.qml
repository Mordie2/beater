import QtQuick
import QtQuick.Layouts
import HighlighterUI

// Everything: decks stacked full width (big waveforms), and beside them the
// mix and the playlist.
Item {
    id: root
    required property AppController app
    signal performRequested()
    //! The smallest window that shows everything uncut.
    readonly property real minWidth: 2 * Theme.s(12) + Math.max(topBar.minWidth, deckA.minWidth + Theme.s(12) + Theme.s(300))
    readonly property real minHeight: 2 * Theme.s(12) + topBar.implicitHeight + Theme.s(12)
                                      + Math.max(deckA.minHeight + deckB.minHeight + Theme.s(12),
                                                 mix.implicitHeight + Theme.s(12) + Theme.s(160))

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.s(12)
        spacing: Theme.s(12)

        TopBar {
            id: topBar
            app: root.app
            Layout.fillWidth: true
            onPerformRequested: root.performRequested()
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: Theme.s(12)

            ColumnLayout {
                // sized by the window only, never by what's inside, so the
                // waveforms keep their scale while labels change
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.preferredWidth: 1
                Layout.minimumWidth: deckA.minWidth
                spacing: Theme.s(12)
                DeckView {
                    id: deckA
                    app: root.app
                    deck: root.app.deckA
                    accent: Look.deckA
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.preferredHeight: 1
                }
                DeckView {
                    id: deckB
                    app: root.app
                    deck: root.app.deckB
                    accent: Look.deckB
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.preferredHeight: 1
                }
            }

            ColumnLayout {
                Layout.preferredWidth: Theme.s(300)
                Layout.maximumWidth: Theme.s(300)
                Layout.fillHeight: true
                spacing: Theme.s(12)
                MixView {
                    id: mix
                    app: root.app
                    Layout.fillWidth: true
                }
                Playlist {
                    app: root.app
                    compact: true
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                }
            }
        }
    }
}

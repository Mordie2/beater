import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import HighlighterUI

ApplicationWindow {
    id: window
    required property AppController app

    width: 1440
    height: 900
    minimumWidth: 1200
    minimumHeight: 720
    visible: true
    title: "beater"
    color: Theme.page

    Component.onCompleted: {
        Theme.accent = Look.deckA
        Theme.slidersStartMinimised = false   // volume faders want the full bar
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.s(12)
        spacing: Theme.s(12)

        TopBar {
            app: window.app
            Layout.fillWidth: true
        }

        RowLayout {
            Layout.fillWidth: true
            // a nested layout inherits fillHeight from its children; the playlist gets the rest
            Layout.fillHeight: false
            Layout.preferredHeight: Theme.s(480)
            spacing: Theme.s(12)
            DeckView {
                app: window.app
                deck: window.app.deckA
                accent: Look.deckA
                Layout.fillWidth: true
                Layout.fillHeight: true
            }
            MixView {
                app: window.app
                Layout.preferredWidth: Theme.s(250)
                Layout.fillHeight: true
            }
            DeckView {
                app: window.app
                deck: window.app.deckB
                accent: Look.deckB
                Layout.fillWidth: true
                Layout.fillHeight: true
            }
        }

        Playlist {
            app: window.app
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: Theme.s(160)
        }
    }
}

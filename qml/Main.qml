import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

ApplicationWindow {
    id: window
    required property AppController app

    width: 1400
    height: 860
    minimumWidth: 1100
    minimumHeight: 640
    visible: true
    title: "beater"
    color: Theme.bg

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        TopBar {
            app: window.app
            Layout.fillWidth: true
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: 330
            Layout.margins: 10
            spacing: 10
            DeckView {
                app: window.app
                deck: window.app.deckA
                accent: Theme.deckA
                Layout.fillWidth: true
                Layout.fillHeight: true
            }
            DeckView {
                app: window.app
                deck: window.app.deckB
                accent: Theme.deckB
                Layout.fillWidth: true
                Layout.fillHeight: true
            }
        }

        Playlist {
            app: window.app
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.leftMargin: 10
            Layout.rightMargin: 10
            Layout.bottomMargin: 10
        }
    }
}

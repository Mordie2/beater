import QtQuick
import HighlighterUI

// Takes a track dragged from the playlist and loads it into this deck. While
// one hovers it, it says what will happen (in red over a playing deck).
DropArea {
    id: root
    required property DeckController deck
    required property AppController app
    readonly property color tint: Theme.accentAt(root, Window.window)

    keys: ["beater-track"]
    z: 10
    onDropped: d => {
        root.app.loadToDeck(d.source.row, root.deck.index)
        d.accept()
    }

    Rectangle {
        anchors.fill: parent
        visible: root.containsDrag
        color: Qt.rgba(0, 0, 0, 0.45)
        border.width: Theme.s(2)
        border.color: root.deck.playing ? Theme.danger : root.tint
        ParamText {
            // at the top, clear of the dragged track under the pointer
            anchors.top: parent.top
            anchors.topMargin: Theme.s(10)
            anchors.horizontalCenter: parent.horizontalCenter
            text: root.deck.playing ? "replace the playing track" : "load into deck " + root.deck.name
            tone: "normal"
            color: root.deck.playing ? Theme.danger : root.tint
            font.weight: Font.Bold
            wrapMode: Text.NoWrap
        }
    }
}

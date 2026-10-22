import QtQuick
import QtQuick.Layouts
import HighlighterUI

RowLayout {
    id: root
    required property DeckController deck
    visible: deck.loaded
    spacing: Theme.s(6)
    ParamText {
        Layout.fillWidth: true
        text: root.deck.stemStatus
        small: true
        wrapMode: Text.NoWrap
        elide: Text.ElideRight
        tone: "dim"
    }
    ParamButton {
        text: "Lyrics"
        onClicked: root.deck.showLyrics()
    }
    ParamButton {
        text: root.deck.stemReady ? "Vocal folder" : "Retry vocals"
        visible: !root.deck.stemBusy
        onClicked: root.deck.stemReady ? root.deck.openVocalFolder() : root.deck.retryVocals()
    }
}

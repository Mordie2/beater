import QtQuick
import HighlighterUI

// A deck's place in its bar, so you can start the next track on the 1:
// playing, the deck's own beats; stopped, the master's (dimmed), which it
// locks to when it starts.
BeatLights {
    required property DeckController deck
    required property AppController app
    beat: deck.playing ? deck.beat : app.masterBeat
    dim: !deck.playing
    visible: deck.loaded
}

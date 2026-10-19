pragma Singleton
import QtQuick

// beater's colour code, on top of the HighlighterUI Theme (which holds
// everything else). One theme, a dusk sweep from warm to cool, with the two
// decks as its anchors, all at a similar lightness so none shouts:
//
//   amber -> orange (deck A) -> rose -> lilac (deck B) -> periwinkle
//   loops                       key                       tempo, sync, grid
//
// A colour means a kind of control, the same on both decks. Transport and
// the app's own controls stay neutral (Play is the one solid light button).
QtObject {
    // the decks: badge, waveform, timeline, volume
    readonly property color deckA: "#ff8a3d"     // orange
    readonly property color deckB: "#b794ff"     // lilac
    // kinds of control
    readonly property color loop: "#ffbe3d"      // amber: loops, intro, the loop lane
    readonly property color key: "#ff7a9e"       // rose: key, key shift
    readonly property color tempo: "#8aa4ff"     // periwinkle: sync, BPM, grid, master tempo, beat
    readonly property color neutral: "#e8ecef"   // transport, the app's own controls, master
    readonly property color transport: neutral

    readonly property color wave: "#262827"      // behind the waveforms
    readonly property color good: "#8aa4ff"      // phase locked: tempo's colour
    readonly property color warn: "#ffbe3d"

    //! A group's label: its colour, a little dimmed.
    function label(c) { return Qt.rgba(c.r, c.g, c.b, 0.75) }
}

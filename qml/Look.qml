pragma Singleton
import QtQuick

// beater's own colours, on top of the HighlighterUI Theme (which holds everything else).
QtObject {
    readonly property color deckA: "#ff8a3d"   // Theme.accents "orange", also the app accent
    readonly property color deckB: "#b794ff"   // Theme.accents "violet"
    readonly property color wave: "#262827"    // behind the waveforms
    readonly property color good: "#7dffa8"
    readonly property color warn: "#ffbe3d"
}

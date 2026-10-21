pragma Singleton
import QtQuick

// beater's colours, on top of the HighlighterUI Theme (the "hardware" skin
// holds everything else). Orange is the main glow and deck A; deck B is the
// one cool accent. Everything in a deck lights up in its colour; what isn't
// lit is neutral, and labels and grouping tell controls apart.
QtObject {
    readonly property color deckA: "#ff7a2f"     // orange: deck A, and the app's own glow
    readonly property color deckB: "#3fd2e0"     // cyan: deck B
    readonly property color neutral: "#ece8df"   // the master, loop regions on the waveforms
    readonly property color grid: "#ece8df"      // beat / bar lines on the waveforms
    readonly property color wave: "#141516"      // behind the waveforms: a dark screen
    readonly property color warn: "#ffbe3d"
}

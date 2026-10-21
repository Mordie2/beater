pragma Singleton
import QtQuick

// beater's colours, on top of the HighlighterUI Theme (the "hardware" skin
// holds everything else). Orange is the main glow and deck A; deck B is the
// a pink-red, like a red neon. Everything in a deck lights up in its colour; what isn't
// lit is neutral, and labels and grouping tell controls apart.
QtObject {
    readonly property color deckA: "#ff7a2f"     // orange: deck A, and the app's own glow
    readonly property color deckB: "#ff4a6e"     // pink-red neon: deck B
    readonly property color neutral: "#ece8df"   // the master, loop regions on the waveforms
    readonly property color grid: "#ece8df"      // beat / bar lines on the waveforms
    readonly property color wave: "#141516"      // behind the waveforms: a dark screen
    readonly property color warn: "#ffbe3d"

    //! A BPM as people say it: whole when it is ("125"), else no more
    //! decimals than it has ("122.5", "127.98").
    function bpmDecimals(v) {
        if (Math.abs(v - Math.round(v)) < 0.005) return 0
        if (Math.abs(v * 10 - Math.round(v * 10)) < 0.05) return 1
        return 2
    }
    function fmtBpm(v) { return v.toFixed(bpmDecimals(v)) }
}

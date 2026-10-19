pragma Singleton
import QtQuick

QtObject {
    readonly property color bg: "#0a0a0c"
    readonly property color panel: "#121215"
    readonly property color raised: "#1a1a1f"
    readonly property color line: "#25252c"
    readonly property color text: "#ececf1"
    readonly property color dim: "#8b8b95"
    readonly property color faint: "#55555e"
    readonly property color deckA: "#ff5a36"
    readonly property color deckB: "#36c5ff"
    readonly property color good: "#7dffa0"
    readonly property color warn: "#ffcc33"
    readonly property string mono: Qt.platform.os === "osx" ? "Menlo" : "monospace"
    readonly property int radius: 3
}

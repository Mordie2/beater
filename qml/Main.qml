import QtCore
import QtQuick
import QtQuick.Controls.Basic
import HighlighterUI

ApplicationWindow {
    id: window
    required property AppController app

    //! "edit", or one of the two Perform layouts: "column" or "strip".
    readonly property string mode: prefs.mode
    readonly property bool perform: mode !== "edit"
    //! Test runs (BEATER_TEST): the window takes no clicks or focus, so it
    //! can't catch input meant for the windows around it.
    property bool inert: false

    visible: true
    title: "beater"
    color: Theme.page
    // never smaller than the view needs to show everything uncut
    minimumWidth: view.item ? Math.ceil(view.item.minWidth) : Theme.s(360)
    minimumHeight: view.item ? Math.ceil(view.item.minHeight) : Theme.s(480)
    flags: (perform && prefs.onTop ? (Qt.Window | Qt.WindowStaysOnTopHint) : Qt.Window)
           | (inert ? (Qt.WindowTransparentForInput | Qt.WindowDoesNotAcceptFocus) : 0)

    // Each mode remembers its own window size.
    Settings {
        id: prefs
        category: "window"
        property string mode: "edit"
        property string lastPerform: "column" // where ⌘E goes from Edit
        property bool onTop: false
        property int editWidth: 1440
        property int editHeight: 900
        property int performWidth: 400     // the column
        property int performHeight: 760
        property int stripWidth: 1600
        property int stripHeight: 240
    }

    function rememberSize() {
        if (mode === "column") { prefs.performWidth = width; prefs.performHeight = height }
        else if (mode === "strip") { prefs.stripWidth = width; prefs.stripHeight = height }
        else { prefs.editWidth = width; prefs.editHeight = height }
    }
    function applySize() {
        width = mode === "column" ? prefs.performWidth : mode === "strip" ? prefs.stripWidth : prefs.editWidth
        height = mode === "column" ? prefs.performHeight : mode === "strip" ? prefs.stripHeight : prefs.editHeight
    }
    //! m: "edit", "column", "strip" (true / false: Perform / Edit, as the test hook says)
    function setMode(m) {
        if (m === true || m === "perform") m = prefs.lastPerform
        if (m === false) m = "edit"
        if (m === mode || ["edit", "column", "strip"].indexOf(m) < 0) return
        rememberSize()
        prefs.mode = m
        if (m !== "edit") prefs.lastPerform = m
        applySize()
    }

    // A window from before the minimum grew (a remembered size, a new control) grows with it.
    onMinimumWidthChanged: if (width < minimumWidth) width = minimumWidth
    onMinimumHeightChanged: if (height < minimumHeight) height = minimumHeight

    Component.onCompleted: {
        Theme.skin = "hardware"
        Theme.accent = Look.deckA     // the main glow: the app's own controls
        app.library.camelot = false   // keys as names, always
        Theme.slidersStartMinimised = false   // volume faders want the full bar
        applySize()
    }
    onClosing: rememberSize()

    Shortcut {
        sequence: "Ctrl+E"   // ⌘E on macOS
        onActivated: window.setMode(window.perform ? "edit" : prefs.lastPerform)
    }

    Loader {
        id: view
        anchors.fill: parent
        sourceComponent: window.mode === "column" ? performView : window.mode === "strip" ? stripView : editView
    }
    Component {
        id: editView
        EditView {
            app: window.app
            onModeRequested: m => window.setMode(m)
        }
    }
    Component {
        id: performView
        PerformView {
            app: window.app
            onTop: prefs.onTop
            onModeRequested: m => window.setMode(m)
            onOnTopToggled: on => prefs.onTop = on
        }
    }
    Component {
        id: stripView
        PerformStrip {
            app: window.app
            onTop: prefs.onTop
            onModeRequested: m => window.setMode(m)
            onOnTopToggled: on => prefs.onTop = on
        }
    }
}

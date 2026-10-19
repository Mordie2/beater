import QtCore
import QtQuick
import QtQuick.Controls.Basic
import HighlighterUI

ApplicationWindow {
    id: window
    required property AppController app

    readonly property bool perform: prefs.perform
    //! Test runs (BEATER_TEST): the window takes no clicks or focus, so it
    //! can't catch input meant for the windows around it.
    property bool inert: false

    visible: true
    title: "beater"
    color: Theme.page
    minimumWidth: perform ? Theme.s(360) : Theme.s(1100)
    minimumHeight: perform ? Theme.s(480) : 720
    flags: (perform && prefs.onTop ? (Qt.Window | Qt.WindowStaysOnTopHint) : Qt.Window)
           | (inert ? (Qt.WindowTransparentForInput | Qt.WindowDoesNotAcceptFocus) : 0)

    // Each mode remembers its own window size.
    Settings {
        id: prefs
        category: "window"
        property bool perform: false
        property bool onTop: false
        property int editWidth: 1440
        property int editHeight: 900
        property int performWidth: 400
        property int performHeight: 760
    }

    function rememberSize() {
        if (perform) { prefs.performWidth = width; prefs.performHeight = height }
        else { prefs.editWidth = width; prefs.editHeight = height }
    }
    function setMode(p) {
        if (p === perform) return
        rememberSize()
        prefs.perform = p
        width = p ? prefs.performWidth : prefs.editWidth
        height = p ? prefs.performHeight : prefs.editHeight
    }

    Component.onCompleted: {
        Theme.accent = Look.neutral   // the app's own controls; colour means a kind of control (Look)
        Theme.slidersStartMinimised = false   // volume faders want the full bar
        width = perform ? prefs.performWidth : prefs.editWidth
        height = perform ? prefs.performHeight : prefs.editHeight
    }
    onClosing: rememberSize()

    Shortcut {
        sequence: "Ctrl+E"   // ⌘E on macOS
        onActivated: window.setMode(!window.perform)
    }

    Loader {
        anchors.fill: parent
        sourceComponent: window.perform ? performView : editView
    }
    Component {
        id: editView
        EditView {
            app: window.app
            onPerformRequested: window.setMode(true)
        }
    }
    Component {
        id: performView
        PerformView {
            app: window.app
            onTop: prefs.onTop
            onEditRequested: window.setMode(false)
            onOnTopToggled: on => prefs.onTop = on
        }
    }
}

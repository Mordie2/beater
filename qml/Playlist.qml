import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Dialogs
import QtQuick.Layouts
import HighlighterUI

Panel {
    id: root
    required property AppController app
    //! Perform mode: shorter rows, no time column, no hints.
    property bool compact: false

    title: "Playlist"
    note: root.app.library.count + " tracks" + (root.app.library.analyzingCount > 0 ? " · analyzing " + root.app.library.analyzingCount : "")
           + (root.compact ? "" : "  ·  drop files or folders · double-click loads to the free deck · ⌫ removes")
    tools: ParamButton { text: "Add…"; onClicked: dialog.open() }

    FileDialog {
        id: dialog
        fileMode: FileDialog.OpenFiles
        nameFilters: ["Audio (*.mp3 *.wav *.aif *.aiff *.flac *.m4a *.aac)", "All files (*)"]
        onAccepted: root.app.addUrls(selectedFiles)
    }

    DropArea {
        id: drop
        anchors.fill: parent
        keys: ["text/uri-list"]
        onDropped: d => { if (d.hasUrls) root.app.addUrls(d.urls) }
        Rectangle {
            anchors.fill: parent
            anchors.margins: -Theme.s(6)
            visible: drop.containsDrag
            color: "transparent"
            border.width: Theme.s(2)
            border.color: Theme.accent
        }
    }

    // What follows the pointer while a row is dragged onto a deck (DeckDrop).
    // In the overlay, so it can leave the list.
    Rectangle {
        id: ghost
        parent: Overlay.overlay
        property int row: -1
        property string title
        property bool dragging: false
        visible: dragging
        width: Math.min(Theme.s(280), ghostText.implicitWidth + Theme.s(20))
        height: Theme.s(28)
        color: Theme.sunken
        border.width: 1
        border.color: Theme.accent
        opacity: 0.95
        Drag.active: dragging
        Drag.source: ghost
        Drag.keys: ["beater-track"]
        Drag.hotSpot.x: width / 2
        Drag.hotSpot.y: height / 2
        ParamText {
            id: ghostText
            anchors.centerIn: parent
            width: Math.min(implicitWidth, parent.width - Theme.s(20))
            text: ghost.title
            tone: "normal"
            wrapMode: Text.NoWrap
            elide: Text.ElideRight
        }
    }

    // A deck letter button in that deck's colour.
    component DeckButton: Item {
        id: db
        property color accent
        property alias text: b.text
        signal clicked()
        implicitWidth: Theme.s(28)
        implicitHeight: Theme.s(22)
        ParamButton { id: b; anchors.fill: parent; horizontalPadding: 0; onClicked: db.clicked() }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: Theme.s(6)

        // column header
        RowLayout {
            visible: !root.compact
            Layout.fillWidth: true
            spacing: 0
            ParamText { text: "#"; caps: true; Layout.preferredWidth: Theme.s(36) }
            ParamText { text: "title"; caps: true; Layout.fillWidth: true }
            ParamText { text: "bpm"; caps: true; Layout.preferredWidth: Theme.s(76) }
            ParamText { text: "key"; caps: true; Layout.preferredWidth: Theme.s(64) }
            ParamText { text: "time"; caps: true; Layout.preferredWidth: Theme.s(60) }
            Item { Layout.preferredWidth: Theme.s(64) }
        }

        ListView {
            id: list
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: root.app.library
            currentIndex: -1
            boundsBehavior: Flickable.StopAtBounds
            // Mouse drags on rows are for dragging tracks onto decks, not flicking;
            // the wheel / trackpad and the scroll bar scroll.
            interactive: false
            ScrollBar.vertical: ParamScrollBar { }
            WheelHandler {
                onWheel: e => {
                    const dy = e.pixelDelta.y !== 0 ? e.pixelDelta.y : e.angleDelta.y / 120 * Theme.s(90)
                    const max = Math.max(0, list.contentHeight - list.height)
                    list.contentY = Math.max(0, Math.min(max, list.contentY - dy))
                }
            }
            focus: true
            Keys.onDeletePressed: if (currentIndex >= 0) root.app.library.removeAt(currentIndex)
            Keys.onPressed: e => {
                if (e.key === Qt.Key_Backspace && currentIndex >= 0) { root.app.library.removeAt(currentIndex); e.accepted = true }
                else if (e.key === Qt.Key_Return && currentIndex >= 0) { root.app.loadToFreeDeck(currentIndex); e.accepted = true }
            }

            delegate: Rectangle {
                id: row
                required property int index
                required property string title
                required property string bpmText
                required property string keyText
                required property string durationText
                required property string status
                required property bool gridManual

                width: ListView.view.width
                height: root.compact ? Theme.s(26) : Theme.s(30)
                color: ListView.isCurrentItem ? Theme.sunken : hover.hovered ? Theme.sunkenHover : "transparent"
                HoverHandler { id: hover }

                MouseArea {
                    anchors.fill: parent
                    // drag a row onto a deck to load it
                    drag.target: ghost
                    drag.threshold: Theme.s(6)
                    onPressed: m => {
                        const p = mapToItem(ghost.parent, m.x, m.y)
                        ghost.x = p.x - ghost.width / 2
                        ghost.y = p.y - ghost.height / 2
                        ghost.row = row.index
                        ghost.title = row.title
                    }
                    drag.onActiveChanged: if (drag.active) ghost.dragging = true
                    // drop only on a real release; a cancelled drag (focus lost) loads nothing
                    onReleased: if (ghost.dragging) { ghost.Drag.drop(); ghost.dragging = false }
                    onCanceled: ghost.dragging = false
                    onClicked: { list.currentIndex = row.index; list.forceActiveFocus() }
                    onDoubleClicked: root.app.loadToFreeDeck(row.index)
                }

                RowLayout {
                    anchors.fill: parent
                    spacing: 0
                    ParamText {
                        visible: !root.compact
                        text: row.index + 1
                        tone: "faint"
                        mono: true
                        wrapMode: Text.NoWrap
                        Layout.preferredWidth: Theme.s(36)
                        Layout.leftMargin: Theme.s(4)
                    }
                    ParamText {
                        text: row.title
                        tone: "normal"
                        small: root.compact
                        Layout.leftMargin: root.compact ? Theme.s(4) : 0
                        wrapMode: Text.NoWrap
                        elide: Text.ElideRight
                        Layout.fillWidth: true
                        Layout.rightMargin: Theme.s(10)
                    }
                    ParamText {
                        text: row.status === "analyzing" ? "…" : row.status.startsWith("error") ? "error" : row.bpmText + (row.gridManual ? "*" : "")
                        color: row.status.startsWith("error") ? Theme.danger : Theme.accent
                        mono: true
                        wrapMode: Text.NoWrap
                        small: root.compact
                        Layout.preferredWidth: root.compact ? Theme.s(44) : Theme.s(76)
                    }
                    ParamText {
                        text: row.keyText
                        mono: true
                        small: root.compact
                        wrapMode: Text.NoWrap
                        Layout.preferredWidth: root.compact ? Theme.s(40) : Theme.s(64)
                    }
                    ParamText {
                        visible: !root.compact
                        text: row.durationText
                        mono: true
                        wrapMode: Text.NoWrap
                        Layout.preferredWidth: Theme.s(60)
                    }
                    Row {
                        spacing: Theme.s(4)
                        Layout.preferredWidth: Theme.s(60)
                        DeckButton { text: "A"; accent: Look.deckA; onClicked: root.app.loadToDeck(row.index, 0) }
                        DeckButton { text: "B"; accent: Look.deckB; onClicked: root.app.loadToDeck(row.index, 1) }
                    }
                }
            }
        }
    }
}

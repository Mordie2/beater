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
            ParamText { text: "key"; caps: true; Layout.preferredWidth: Theme.s(96) }
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
            ScrollBar.vertical: ParamScrollBar { }
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
                        text: root.compact ? row.keyText.split(" ")[0] : row.keyText   // Camelot only when compact
                        mono: true
                        small: root.compact
                        wrapMode: Text.NoWrap
                        Layout.preferredWidth: root.compact ? Theme.s(36) : Theme.s(96)
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

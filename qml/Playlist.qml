import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Dialogs
import QtQuick.Layouts

Rectangle {
    id: root
    required property AppController app
    color: Theme.panel
    radius: Theme.radius
    border.color: drop.containsDrag ? Theme.text : Theme.line

    component Head: Text {
        color: Theme.faint
        font.family: Theme.mono
        font.pixelSize: 9
        font.bold: true
        font.letterSpacing: 1
    }

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
        onDropped: (d) => { if (d.hasUrls) root.app.addUrls(d.urls) }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 10
        spacing: 6

        RowLayout {
            spacing: 10
            Text {
                text: "PLAYLIST"
                color: Theme.dim
                font.family: Theme.mono
                font.pixelSize: 11
                font.bold: true
                font.letterSpacing: 1
            }
            Text {
                text: root.app.library.count + " tracks" + (root.app.library.analyzingCount > 0 ? " · analyzing " + root.app.library.analyzingCount : "")
                color: Theme.faint
                font.family: Theme.mono
                font.pixelSize: 11
            }
            Item { Layout.fillWidth: true }
            Text {
                text: "drop files or folders · double-click loads to the free deck · ⌫ removes"
                color: Theme.faint
                font.pixelSize: 11
            }
            Btn { text: "ADD…"; small: true; onClicked: dialog.open() }
        }

        // column header
        RowLayout {
            Layout.fillWidth: true
            spacing: 0
            Head { text: "#"; Layout.preferredWidth: 34 }
            Head { text: "TITLE"; Layout.fillWidth: true }
            Head { text: "BPM"; Layout.preferredWidth: 70 }
            Head { text: "KEY"; Layout.preferredWidth: 80 }
            Head { text: "TIME"; Layout.preferredWidth: 56 }
            Item { Layout.preferredWidth: 64 }
        }

        ListView {
            id: list
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: root.app.library
            currentIndex: -1
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: ScrollBar { }
            focus: true
            Keys.onDeletePressed: if (currentIndex >= 0) root.app.library.removeAt(currentIndex)
            Keys.onBackPressed: if (currentIndex >= 0) root.app.library.removeAt(currentIndex)
            Keys.onPressed: (e) => {
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
                height: 28
                color: ListView.isCurrentItem ? Theme.raised : (index % 2 ? "transparent" : "#0e0e11")
                radius: 2

                MouseArea {
                    anchors.fill: parent
                    onClicked: { list.currentIndex = row.index; list.forceActiveFocus() }
                    onDoubleClicked: root.app.loadToFreeDeck(row.index)
                }

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 4
                    spacing: 0
                    Text {
                        text: row.index + 1
                        color: Theme.faint
                        font.family: Theme.mono
                        font.pixelSize: 11
                        Layout.preferredWidth: 34
                    }
                    Text {
                        text: row.title
                        color: Theme.text
                        font.pixelSize: 13
                        elide: Text.ElideRight
                        Layout.fillWidth: true
                        Layout.rightMargin: 10
                    }
                    Text {
                        text: row.status === "analyzing" ? "…" : row.bpmText + (row.gridManual ? "*" : "")
                        color: row.status.startsWith("error") ? Theme.warn : Theme.text
                        font.family: Theme.mono
                        font.pixelSize: 12
                        Layout.preferredWidth: 70
                    }
                    Text {
                        text: row.keyText
                        color: Theme.dim
                        font.family: Theme.mono
                        font.pixelSize: 12
                        Layout.preferredWidth: 80
                    }
                    Text {
                        text: row.durationText
                        color: Theme.dim
                        font.family: Theme.mono
                        font.pixelSize: 12
                        Layout.preferredWidth: 56
                    }
                    Row {
                        spacing: 3
                        Layout.preferredWidth: 64
                        Btn { text: "A"; small: true; accent: Theme.deckA; onClicked: root.app.loadToDeck(row.index, 0) }
                        Btn { text: "B"; small: true; accent: Theme.deckB; onClicked: root.app.loadToDeck(row.index, 1) }
                    }
                }
            }
        }
    }
}

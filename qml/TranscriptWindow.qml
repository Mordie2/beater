import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ApplicationWindow {
    id: window
    required property var transcript
    visible: true
    width: 820
    height: 600
    minimumWidth: 440
    minimumHeight: 480
    title: "Vocal text test — Deck " + transcript.deck + " — " + transcript.title
    color: "#14171b"
    palette.text: "#eef0f3"
    palette.windowText: "#eef0f3"
    palette.buttonText: "#eef0f3"
    palette.button: "#343b45"
    palette.highlight: "#e99251"
    palette.base: "#1d2229"

    Connections {
        target: window.transcript
        function onRevealRequested() { window.reveal() }
        function onActiveWordChanged() { Qt.callLater(window.followWord) }
        function onWordsChanged() { Qt.callLater(window.followWord) }
    }
    function reveal() {
        if (visibility === Window.Minimized) showNormal()
        else show()
        raise()
        requestActivate()
    }
    Component.onCompleted: reveal()
    function followWord() {
        if (!follow.checked || transcript.activeWord < 0) return
        const item = wordRepeater.itemAt(transcript.activeWord)
        if (!item) return
        const y = item.mapToItem(wordContent, 0, 0).y
        if (y < wordView.contentY + 12 || y + item.height > wordView.contentY + wordView.height - 12)
            wordView.contentY = Math.max(0, Math.min(wordView.contentHeight - wordView.height,
                                                   y - wordView.height * 0.35))
    }
    function timeLabel(seconds) {
        const t = Math.max(0, seconds)
        return Math.floor(t / 60) + ":" + (t % 60).toFixed(1).padStart(4, "0")
    }
    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 24
        spacing: 14
        Label {
            text: "VOCAL TEXT TEST  /  DECK " + window.transcript.deck
            color: "#e99251"
            font.pixelSize: 12
            font.letterSpacing: 1.5
        }
        Label {
            Layout.fillWidth: true
            text: window.transcript.title
            color: "#eef0f3"
            font.pixelSize: 25
            font.bold: true
            elide: Text.ElideRight
        }
        RowLayout {
            Layout.fillWidth: true
            BusyIndicator { running: window.transcript.busy; visible: running; implicitWidth: 24; implicitHeight: 24 }
            Label {
                Layout.fillWidth: true
                text: window.transcript.status + (window.transcript.busy
                    ? " · " + window.transcript.elapsedSeconds + " s elapsed" : "")
                wrapMode: Text.WordWrap
                color: "#aeb9c7"
            }
        }
        RowLayout {
            Layout.fillWidth: true
            Label { text: "Language"; color: "#aeb9c7" }
            ComboBox {
                id: languageChoice
                textRole: "label"; valueRole: "code"
                model: [{label: "Auto detect", code: "auto"}, {label: "English", code: "en"},
                    {label: "Dutch", code: "nl"}, {label: "German", code: "de"},
                    {label: "French", code: "fr"}, {label: "Spanish", code: "es"},
                    {label: "Italian", code: "it"}, {label: "Portuguese", code: "pt"},
                    {label: "Japanese", code: "ja"}, {label: "Korean", code: "ko"}, {label: "Chinese", code: "zh"}]
                onActivated: window.transcript.setLanguage(currentValue)
            }
            Label {
                Layout.fillWidth: true
                text: window.transcript.backend
                wrapMode: Text.WordWrap
                color: "#8490a0"
                font.pixelSize: 12
            }
        }
        RowLayout {
            Layout.fillWidth: true
            Label {
                text: !window.transcript.playbackConnected ? "Waiting for playback connection"
                    : (window.transcript.playing ? "Playing  ·  " : "Paused  ·  ") + window.timeLabel(window.transcript.position)
                color: window.transcript.playing ? "#f5b67d" : "#aeb9c7"
                font.pixelSize: 14
            }
            Item { Layout.fillWidth: true }
            CheckBox { id: follow; text: "Follow words"; checked: true; onToggled: window.followWord() }
        }
        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: "#1d2229"
            radius: 8
            clip: true
            Flickable {
                id: wordView
                anchors.fill: parent
                contentWidth: width
                contentHeight: wordContent.height
                boundsBehavior: Flickable.StopAtBounds
                ScrollBar.vertical: ScrollBar {}
                Item {
                    id: wordContent
                    width: wordView.width
                    height: Math.max(wordView.height, wordFlow.height + 36, fallback.visible ? fallback.implicitHeight + 36 : 0)
                    Flow {
                        id: wordFlow
                        x: 18; y: 18
                        width: Math.max(1, parent.width - 36)
                        spacing: 5
                        Repeater {
                            id: wordRepeater
                            model: window.transcript.words
                            Label {
                                required property int index
                                required property var modelData
                                text: modelData.text
                                textFormat: Text.PlainText
                                font.pixelSize: 27
                                font.weight: Font.Medium
                                leftPadding: 5; rightPadding: 5; topPadding: 4; bottomPadding: 4
                                color: window.transcript.activeWord === index ? "#18130e" : "#e3e7ec"
                                background: Rectangle {
                                    radius: 5
                                    color: window.transcript.activeWord === parent.index ? "#ffad61" : "transparent"
                                }
                            }
                        }
                    }
                    Label {
                        id: fallback
                        x: 18; y: 18
                        width: Math.max(1, parent.width - 36)
                        visible: window.transcript.words.length === 0
                        text: window.transcript.text.length ? window.transcript.text
                            : window.transcript.busy ? "The words will appear here once the vocal stem is ready." : "No transcript to display."
                        textFormat: Text.PlainText
                        wrapMode: Text.WordWrap
                        color: "#8490a0"
                        font.pixelSize: 26
                    }
                }
            }
        }
        RowLayout {
            Button { text: "Copy text"; enabled: window.transcript.text.length > 0; onClicked: window.transcript.copyText() }
            Button { text: "Open files"; enabled: window.transcript.canRetry; onClicked: window.transcript.openFolder() }
            Item { Layout.fillWidth: true }
            Button { text: "Transcribe again"; enabled: window.transcript.canRetry; onClicked: window.transcript.retry() }
        }
        Label {
            Layout.fillWidth: true
            text: window.transcript.busy ? "First-time preparation can take several minutes. Playback can run ahead of the lyrics."
                : "Follows the deck playhead · word timing is estimated from the vocal stem"
            wrapMode: Text.WordWrap
            color: "#8490a0"
            font.pixelSize: 12
        }
    }
}

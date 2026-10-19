import QtQuick
import QtQuick.Layouts
import HighlighterUI

Item {
    id: root
    required property AppController app
    signal performRequested()
    implicitHeight: Theme.s(64)

    RowLayout {
        anchors.fill: parent
        spacing: Theme.s(24)

        Text {
            // the two decks' colours
            text: "<font color='" + Look.deckA + "'>BEAT</font><font color='" + Look.deckB + "'>ER</font>"
            textFormat: Text.RichText
            font.family: Theme.family
            font.pixelSize: Theme.s(20)
            font.weight: Font.Black
            font.letterSpacing: Theme.s(2)
        }

        Column {
            spacing: Theme.s(4)
            ParamText { text: "mode"; caps: true; wrapMode: Text.NoWrap }
            ModeSwitch { perform: false; onSwitched: p => { if (p) root.performRequested() } }
        }

        Item { Layout.fillWidth: true }

        Column {
            property color accent: Look.tempo
            spacing: Theme.s(4)
            ParamText { text: "master bpm"; caps: true; wrapMode: Text.NoWrap; color: Look.label(Look.tempo) }
            Row {
                spacing: Theme.s(4)
                ParamButton { text: "−1"; onClicked: root.app.nudgeMasterBpm(-1); anchors.verticalCenter: parent.verticalCenter }
                ParamButton { text: "−.1"; onClicked: root.app.nudgeMasterBpm(-0.1); anchors.verticalCenter: parent.verticalCenter }
                MasterTempo { app: root.app }
                ParamButton { text: "+.1"; onClicked: root.app.nudgeMasterBpm(0.1); anchors.verticalCenter: parent.verticalCenter }
                ParamButton { text: "+1"; onClicked: root.app.nudgeMasterBpm(1); anchors.verticalCenter: parent.verticalCenter }
            }
        }

        Column {
            property color accent: Look.tempo
            spacing: Theme.s(4)
            ParamText { text: "bar"; caps: true; wrapMode: Text.NoWrap; color: Look.label(Look.tempo) }
            BeatLights { app: root.app }
        }

        Column {
            property color accent: Look.tempo
            spacing: Theme.s(4)
            ParamText { text: "phase lock"; caps: true; wrapMode: Text.NoWrap; color: Look.label(Look.tempo) }
            ParamChoice {
                width: Theme.s(110)
                model: ["Beat", "Bar"]
                currentIndex: root.app.phaseMode
                onActivated: i => root.app.phaseMode = i
            }
        }

        Column {
            spacing: Theme.s(4)
            ParamText { text: "keys"; caps: true; wrapMode: Text.NoWrap }
            ParamChoice {
                width: Theme.s(150)
                model: ["Names", "Camelot"]
                currentIndex: root.app.library.camelot ? 1 : 0
                onActivated: i => root.app.library.camelot = i === 1
            }
        }

        Item { Layout.fillWidth: true }

        Column {
            spacing: Theme.s(4)
            Layout.preferredWidth: Theme.s(230)
            ParamText { text: "output"; caps: true; wrapMode: Text.NoWrap }
            ParamCombo {
                width: parent.width
                model: root.app.outputDevices
                currentIndex: root.app.outputDevices.indexOf(root.app.outputDevice.length ? root.app.outputDevice : "System default")
                onActivated: i => root.app.outputDevice = root.app.outputDevices[i]
            }
            ParamText {
                // the device in use and its rate, or what went wrong
                width: parent.width
                text: root.app.audioStatus
                small: true
                tone: "faint"
                color: root.app.audioOk ? Theme.textFaint : Look.warn
                wrapMode: Text.NoWrap
                elide: Text.ElideRight
            }
        }
        // devices come and go (an interface plugged in): keep the list current
        Timer {
            interval: 3000
            running: root.visible
            repeat: true
            onTriggered: root.app.refreshOutputs()
        }
    }
}

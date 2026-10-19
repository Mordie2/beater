import QtQuick
import QtQuick.Layouts
import HighlighterUI

Item {
    id: root
    required property AppController app
    signal performRequested()
    implicitHeight: Theme.s(58)

    RowLayout {
        anchors.fill: parent
        spacing: Theme.s(24)

        Text {
            text: "BEATER"
            color: Theme.accent
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
            spacing: Theme.s(4)
            ParamText { text: "master bpm"; caps: true; wrapMode: Text.NoWrap }
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
            spacing: Theme.s(4)
            ParamText { text: "bar"; caps: true; wrapMode: Text.NoWrap }
            BeatLights { app: root.app }
        }

        Column {
            spacing: Theme.s(4)
            ParamText { text: "phase lock"; caps: true; wrapMode: Text.NoWrap }
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

        ParamText {
            text: root.app.audioStatus
            tone: "faint"
            color: root.app.audioOk ? Theme.textFaint : Look.warn
            wrapMode: Text.NoWrap
            elide: Text.ElideLeft
            Layout.maximumWidth: Theme.s(240)
        }
    }
}

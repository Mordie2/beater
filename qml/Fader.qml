import QtQuick

// Vertical fader with a level meter. Drag, scroll, double-click to reset.
Item {
    id: root
    property real value: 0.8
    property real level: 0.0
    property real resetValue: 0.8
    property color accent: Theme.text
    property bool horizontal: false
    signal moved(real v)

    implicitWidth: horizontal ? 160 : 34
    implicitHeight: horizontal ? 20 : 160

    function setFrom(px) {
        const span = horizontal ? width - knob.width : height - knob.height
        const t = horizontal ? (px - knob.width / 2) / span : 1 - (px - knob.height / 2) / span
        moved(Math.max(0, Math.min(1, t)))
    }

    // track
    Rectangle {
        id: track
        x: root.horizontal ? 0 : root.width / 2 - 2
        y: root.horizontal ? root.height / 2 - 2 : 0
        width: root.horizontal ? root.width : 4
        height: root.horizontal ? 4 : root.height
        radius: 2
        color: Theme.line
    }
    // meter beside the track
    Rectangle {
        visible: !root.horizontal
        x: root.width - 4
        width: 3
        y: root.height * (1 - Math.min(1, root.level))
        height: root.height * Math.min(1, root.level)
        color: root.level > 0.98 ? "#ff3b3b" : root.accent
        opacity: 0.85
    }
    Rectangle {
        visible: root.horizontal
        y: root.height - 3
        height: 3
        width: root.width * Math.min(1, root.level)
        color: root.level > 0.98 ? "#ff3b3b" : root.accent
        opacity: 0.85
    }
    Rectangle {
        id: knob
        width: root.horizontal ? 12 : root.width - 8
        height: root.horizontal ? root.height : 14
        radius: 2
        color: mouse.pressed ? root.accent : Theme.text
        x: root.horizontal ? root.value * (root.width - width) : 2
        y: root.horizontal ? 0 : (1 - root.value) * (root.height - height)
        Rectangle {
            anchors.centerIn: parent
            width: root.horizontal ? 1 : parent.width - 6
            height: root.horizontal ? parent.height - 6 : 1
            color: Theme.bg
        }
    }
    MouseArea {
        id: mouse
        anchors.fill: parent
        preventStealing: true
        onPressed: (m) => root.setFrom(root.horizontal ? m.x : m.y)
        onPositionChanged: (m) => { if (pressed) root.setFrom(root.horizontal ? m.x : m.y) }
        onDoubleClicked: root.moved(root.resetValue)
        onWheel: (w) => root.moved(Math.max(0, Math.min(1, root.value + (w.angleDelta.y + w.angleDelta.x) / 120 * 0.02)))
    }
}

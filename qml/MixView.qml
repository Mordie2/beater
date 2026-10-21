import QtQuick
import QtQuick.Layouts
import HighlighterUI

// Levels and volume per deck plus master, and the crossfader. No automatic
// transitions: mixing is these faders.
Panel {
    id: root
    required property AppController app
    title: "Mix"
    implicitHeight: Theme.s(30) + 2 * Theme.s(12) + mixColumn.implicitHeight

    ColumnLayout {
        id: mixColumn
        anchors.fill: parent
        spacing: Theme.s(12)

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.s(16)

            Row {
                id: meters
                Layout.fillHeight: true
                spacing: Theme.s(10)
                readonly property real barHeight: Math.max(Theme.s(40), height - Theme.s(24))
                ParamLevel {
                    property color accent: Look.deckA
                    label: "A"
                    value: root.app.deckA.level
                    warnAt: 0.9
                    barWidth: Theme.s(12)
                    barHeight: meters.barHeight
                }
                ParamLevel {
                    property color accent: Look.deckB
                    label: "B"
                    value: root.app.deckB.level
                    warnAt: 0.9
                    barWidth: Theme.s(12)
                    barHeight: meters.barHeight
                }
                ParamLevel {
                    property color accent: Look.neutral
                    label: "M"
                    value: root.app.masterLevel
                    warnAt: 0.9
                    barWidth: Theme.s(12)
                    barHeight: meters.barHeight
                }
            }

            ColumnLayout {
                id: faders
                Layout.fillWidth: true
                spacing: Theme.s(8)
                ParamSlider {
                    property color accent: Look.deckA
                    Layout.fillWidth: true
                    key: "mix.deckA"
                    label: "deck A"
                    unit: " %"
                    from: 0; to: 100; limitFrom: 0; limitTo: 100
                    defaultValue: 80
                    value: Math.round(root.app.deckA.volume * 100)
                    onMoved: v => root.app.deckA.volume = v / 100
                }
                ParamSlider {
                    property color accent: Look.deckB
                    Layout.fillWidth: true
                    key: "mix.deckB"
                    label: "deck B"
                    unit: " %"
                    from: 0; to: 100; limitFrom: 0; limitTo: 100
                    defaultValue: 80
                    value: Math.round(root.app.deckB.volume * 100)
                    onMoved: v => root.app.deckB.volume = v / 100
                }
                ParamSlider {
                    property color accent: Look.neutral   // neutral, like the master meter
                    Layout.fillWidth: true
                    key: "mix.master"
                    label: "master"
                    unit: " %"
                    from: 0; to: 100; limitFrom: 0; limitTo: 100
                    defaultValue: 85
                    value: Math.round(root.app.masterVolume * 100)
                    onMoved: v => root.app.masterVolume = v / 100
                }
            }
        }

        Crossfader {
            app: root.app
            key: "mix.crossfader"
            Layout.fillWidth: true
            aspect: 4
        }
    }
}

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import GlassAlarm

ColumnLayout {
    id: root
    spacing: 18

    property int elapsedMs: 0
    property bool swRunning: false
    property var swTimer: Timer {
        interval: 50
        repeat: true
        onTriggered: root.elapsedMs += 50
    }
    property var laps: []

    function formatTime(ms) {
        var totalSecs = Math.floor(ms / 1000)
        var h = Math.floor(totalSecs / 3600)
        var m = Math.floor((totalSecs % 3600) / 60)
        var s = totalSecs % 60
        if (configManager.stopwatchShowMs) {
            var cs = Math.floor((ms % 1000) / 10)
            return ("00" + h).slice(-2) + ":" +
                   ("00" + m).slice(-2) + ":" +
                   ("00" + s).slice(-2) + "." +
                   ("00" + cs).slice(-2)
        }
        return ("00" + h).slice(-2) + ":" +
               ("00" + m).slice(-2) + ":" +
               ("00" + s).slice(-2)
    }

    Text {
        text: "Stopwatch"
        color: "#FFFFFF"
        font.pixelSize: 22
        font.bold: true
    }

    Item { Layout.fillHeight: true }

    Text {
        text: formatTime(elapsedMs)
        color: "#FFFFFF"
        font.pixelSize: 72
        font.bold: true
        opacity: 0.9
        Layout.alignment: Qt.AlignHCenter
        font.letterSpacing: 3
    }

    RowLayout {
        spacing: 18
        Layout.alignment: Qt.AlignHCenter

        GlassButton {
            text: swRunning ? "Pause" : "Start"
            baseColor: swRunning ? Qt.rgba(1,0.6,0,0.25) : Qt.rgba(0.3,1,0.3,0.2)
            hoverColor: swRunning ? Qt.rgba(1,0.6,0,0.35) : Qt.rgba(0.3,1,0.3,0.35)
            onClicked: {
                if (swRunning) {
                    swTimer.stop()
                    swRunning = false
                } else {
                    swRunning = true
                    swTimer.start()
                }
            }
        }

        GlassButton {
            text: "Lap"
            enabled: swRunning
            onClicked: {
                laps = laps.concat([elapsedMs])
                lapView.positionViewAtEnd()
            }
        }

        GlassButton {
            text: "Reset"
            baseColor: Qt.rgba(1,0.2,0.2,0.15)
            hoverColor: Qt.rgba(1,0.2,0.2,0.3)
            onClicked: {
                swTimer.stop()
                swRunning = false
                elapsedMs = 0
                laps = []
            }
        }
    }

    Rectangle {
        Layout.fillWidth: true
        Layout.preferredHeight: 120
        color: "transparent"
        visible: laps.length > 0

        ListView {
            id: lapView
            anchors.fill: parent
            spacing: 4
            clip: true
            model: laps

            delegate: RowLayout {
                width: parent.width
                spacing: 12
                Text {
                    text: "Lap " + (index + 1)
                    color: Qt.rgba(1,1,1,0.5)
                    font.pixelSize: 16
                }
                Text {
                    text: formatTime(modelData)
                    color: "#FFFFFF"
                    font.pixelSize: 16
                }
                Text {
                    text: index > 0 ? "+" + formatTime(modelData - laps[index - 1]) : ""
                    color: Qt.rgba(1,1,1,0.4)
                    font.pixelSize: 14
                }
            }
        }
    }

    Item { Layout.fillHeight: true }
}

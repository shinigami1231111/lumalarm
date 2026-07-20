import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import GlassAlarm

ColumnLayout {
    id: root
    spacing: 16

    property int elapsedMs: 0
    property bool swRunning: false
    property var swTimer: Timer {
        interval: 50; repeat: true
        onTriggered: root.elapsedMs += 50
    }
    property var laps: []

    function fmt(ms) {
        var t = Math.floor(ms/1000), h=Math.floor(t/3600), m=Math.floor((t%3600)/60), s=t%60
        if (configManager.stopwatchShowMs) {
            var cs = Math.floor((ms%1000)/10)
            return ("00"+h).slice(-2)+":"+("00"+m).slice(-2)+":"+("00"+s).slice(-2)+"."+("00"+cs).slice(-2)
        }
        return ("00"+h).slice(-2)+":"+("00"+m).slice(-2)+":"+("00"+s).slice(-2)
    }

    Text { text: "Stopwatch"; color: configManager.themeTextPrimary; font.pixelSize: 24; font.bold: true }

    Item { Layout.fillHeight: true }

    Text {
        text: fmt(elapsedMs)
        color: configManager.themeTextPrimary
        font.pixelSize: 72; font.bold: true; opacity: 0.92
        Layout.alignment: Qt.AlignHCenter
        font.letterSpacing: 3
    }

    RowLayout {
        spacing: 12
        Layout.alignment: Qt.AlignHCenter
        GlassButton {
            text: swRunning ? "Pause" : "Start"
            pixelSize: 14
            implicitWidth: 110; implicitHeight: 40
            filled: !swRunning
            baseColor: swRunning ? Qt.rgba(1,0.6,0,0.22) : Qt.rgba(0.3,1,0.3,0.18)
            hoverColor: swRunning ? Qt.rgba(1,0.6,0,0.32) : Qt.rgba(0.3,1,0.3,0.3)
            onClicked: { if (swRunning) { swTimer.stop(); swRunning=false } else { swRunning=true; swTimer.start() } }
        }
        GlassButton {
            text: "Lap"
            pixelSize: 14; implicitWidth: 100; implicitHeight: 40
            enabled: swRunning
            onClicked: { laps = laps.concat([elapsedMs]); lapView.positionViewAtEnd() }
        }
        GlassButton {
            text: "Reset"
            pixelSize: 14; implicitWidth: 100; implicitHeight: 40
            baseColor: Qt.rgba(1,0.2,0.2,0.15)
            hoverColor: Qt.rgba(1,0.2,0.2,0.28)
            onClicked: { swTimer.stop(); swRunning=false; elapsedMs=0; laps=[] }
        }
    }

    Rectangle {
        Layout.fillWidth: true; Layout.fillHeight: true
        color: "transparent"
        visible: laps.length > 0
        radius: 12
        clip: true

        ListView {
            id: lapView
            anchors.fill: parent
            spacing: 6
            clip: true
            model: laps
            delegate: Rectangle {
                width: parent.width; height: 40; radius: 8
                color: Qt.rgba(1,1,1,0.04)
                border.color: Qt.rgba(1,1,1,0.06); border.width: 1
                RowLayout {
                    anchors.fill: parent; anchors.margins: 12; spacing: 12
                    Text { text: "Lap " + (index+1); color: configManager.themeTextSecondary; font.pixelSize: 14 }
                    Item { Layout.fillWidth: true }
                    Text { text: fmt(modelData); color: configManager.themeTextPrimary; font.pixelSize: 15; font.bold: true }
                    Text {
                        text: index > 0 ? "+" + fmt(modelData - laps[index-1]) : ""
                        color: Qt.rgba(1,1,1,0.4); font.pixelSize: 13
                        Layout.preferredWidth: 90; horizontalAlignment: Text.AlignRight
                    }
                }
            }
        }
    }

    Item { Layout.fillHeight: true }
}

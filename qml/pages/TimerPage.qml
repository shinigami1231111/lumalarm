import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import GlassAlarm

ColumnLayout {
    id: root
    spacing: 16

    property int remainingSecs: 0
    property bool timerRunning: false
    property string timerSoundFile: ""
    property bool timerPreviewing: false

    property var hourField: null
    property var minField: null
    property var secField: null

    Connections {
        target: audioPlayer
        function onIsPlayingChanged() { if (!audioPlayer.isPlaying) timerPreviewing = false }
    }

    property var timerObj: Timer {
        interval: 1000; repeat: true
        onTriggered: {
            if (root.remainingSecs > 0) root.remainingSecs--
            else { stop(); root.timerRunning = false; if (root.timerSoundFile !== "") audioPlayer.play(root.timerSoundFile) }
        }
    }

    function formatTime(secs) {
        var h = Math.floor(secs / 3600), m = Math.floor((secs % 3600) / 60), s = secs % 60
        return ("00"+h).slice(-2) + ":" + ("00"+m).slice(-2) + ":" + ("00"+s).slice(-2)
    }

    Text { text: "Timer"; color: configManager.themeTextPrimary; font.pixelSize: 24; font.bold: true }

    Item { Layout.fillHeight: true }

    Text {
        text: formatTime(remainingSecs)
        color: configManager.themeTextPrimary
        font.pixelSize: 76; font.bold: true; opacity: 0.92
        Layout.alignment: Qt.AlignHCenter
        font.letterSpacing: 3
    }

    Text {
        text: timerRunning ? "Running" : (remainingSecs > 0 ? "Paused" : "Set a duration")
        color: timerRunning ? Qt.rgba(0.3,1,0.3,0.8) : configManager.themeTextSecondary
        font.pixelSize: 14
        Layout.alignment: Qt.AlignHCenter
    }

    Item { Layout.fillHeight: true }

    RowLayout {
        spacing: 14
        Layout.alignment: Qt.AlignHCenter
        Repeater {
            model: [
                {label: "Hours", max: 99},
                {label: "Min", max: 59},
                {label: "Sec", max: 59}
            ]
            ColumnLayout {
                spacing: 4
                Label { text: modelData.label; color: configManager.themeTextSecondary; font.pixelSize: 13; Layout.alignment: Qt.AlignHCenter }
                TextField {
                    text: "00"
                    validator: IntValidator { bottom: 0; top: modelData.max }
                    color: configManager.themeTextPrimary; horizontalAlignment: TextInput.AlignHCenter
                    inputMethodHints: Qt.ImhDigitsOnly
                    implicitWidth: 64; implicitHeight: 44
                    font.pixelSize: 18
                    background: Rectangle { color: Qt.rgba(1,1,1,0.08); radius: 10; border.color: Qt.rgba(1,1,1,0.15); border.width: 1 }
                    Component.onCompleted: {
                        if (modelData.label === "Hours") hourField = this
                        else if (modelData.label === "Min") minField = this
                        else secField = this
                    }
                }
            }
        }
    }

    RowLayout {
        spacing: 12
        Layout.alignment: Qt.AlignHCenter
        GlassButton {
            text: timerRunning ? "Pause" : "Start"
            pixelSize: 14
            implicitWidth: 110; implicitHeight: 40
            filled: !timerRunning
            baseColor: timerRunning ? Qt.rgba(1,0.6,0,0.22) : Qt.rgba(0.3,1,0.3,0.18)
            hoverColor: timerRunning ? Qt.rgba(1,0.6,0,0.32) : Qt.rgba(0.3,1,0.3,0.3)
            onClicked: {
                if (timerRunning) { timerObj.stop(); timerRunning = false }
                else {
                    if (remainingSecs === 0) {
                        var h = parseInt(hourField.text)||0, m = parseInt(minField.text)||0, s = parseInt(secField.text)||0
                        remainingSecs = h*3600 + m*60 + s
                    }
                    if (remainingSecs > 0) { timerRunning = true; timerObj.start() }
                }
            }
        }
        GlassButton {
            text: "Reset"
            pixelSize: 14
            implicitWidth: 100; implicitHeight: 40
            baseColor: Qt.rgba(1,0.2,0.2,0.15)
            hoverColor: Qt.rgba(1,0.2,0.2,0.28)
            onClicked: { timerObj.stop(); timerRunning = false; remainingSecs = 0; audioPlayer.stop() }
        }
    }

    Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: Qt.rgba(1,1,1,0.07) }

    RowLayout {
        Layout.fillWidth: true; spacing: 10
        Text { text: "Sound when done:"; color: configManager.themeTextSecondary; font.pixelSize: 14; Layout.alignment: Qt.AlignVCenter }
        RoundedCombo {
            id: timerSoundCmb
            Layout.fillWidth: true
            model: { var t = configManager.availableTones(); var a = ["none"]; for (var i=0;i<t.length;i++) a.push(t[i]); return a }
            onActivated: timerSoundFile = (currentIndex === 0) ? "" : currentText
        }
        GlassButton {
            text: timerPreviewing ? "■" : "▶"
            pixelSize: 11; implicitWidth: 36; implicitHeight: 30; radius: 8
            onClicked: {
                if (timerPreviewing) { audioPlayer.stop(); timerPreviewing = false }
                else if (timerSoundCmb.currentIndex > 0) { audioPlayer.stop(); audioPlayer.preview(timerSoundCmb.currentText); timerPreviewing = true }
            }
        }
    }

    Connections {
        target: configManager
        function onConfigChanged() {
            var prev = timerSoundCmb.currentText
            var t = configManager.availableTones(); var a = ["none"]; for (var i=0;i<t.length;i++) a.push(t[i])
            timerSoundCmb.model = a
            timerSoundCmb.currentIndex = (prev === "" || timerSoundCmb.find(prev) < 0) ? 0 : timerSoundCmb.find(prev)
        }
    }

    Item { Layout.fillHeight: true }
}

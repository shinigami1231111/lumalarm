import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import GlassAlarm

ColumnLayout {
    id: root
    spacing: 22

    property int remainingSecs: 0
    property bool timerRunning: false
    property string timerSoundFile: ""
    property var timerObj: Timer {
        interval: 1000
        repeat: true
        onTriggered: {
            if (root.remainingSecs > 0) {
                root.remainingSecs--
            } else {
                stop()
                root.timerRunning = false
                if (root.timerSoundFile !== "")
                    audioPlayer.play(root.timerSoundFile)
            }
        }
    }

    function formatTime(secs) {
        var h = Math.floor(secs / 3600)
        var m = Math.floor((secs % 3600) / 60)
        var s = secs % 60
        return ("00" + h).slice(-2) + ":" +
               ("00" + m).slice(-2) + ":" +
               ("00" + s).slice(-2)
    }

    Text {
        text: "Countdown Timer"
        color: configManager.themeTextPrimary
        font.pixelSize: 22
        font.bold: true
    }

    Item { Layout.fillHeight: true }

    Text {
        text: formatTime(remainingSecs)
        color: configManager.themeTextPrimary
        font.pixelSize: 80
        font.bold: true
        opacity: 0.9
        Layout.alignment: Qt.AlignHCenter
        font.letterSpacing: 4
    }

    Text {
        text: timerRunning ? "Running" : (remainingSecs > 0 ? "Paused" : "Set time below")
        color: timerRunning ? Qt.rgba(0.3,1,0.3,0.7) : configManager.themeTextSecondary
        font.pixelSize: 14
        Layout.alignment: Qt.AlignHCenter
    }

    Item { Layout.fillHeight: true }

    RowLayout {
        spacing: 18
        Layout.alignment: Qt.AlignHCenter

        ColumnLayout {
            spacing: 3
            Label { text: "Hours"; color: configManager.themeTextSecondary; font.pixelSize: 14; Layout.alignment: Qt.AlignHCenter }
            TextField {
                id: hourField
                text: "00"
                validator: IntValidator { bottom: 0; top: 99 }
                color: configManager.themeTextPrimary; horizontalAlignment: TextInput.AlignHCenter
                inputMethodHints: Qt.ImhDigitsOnly
                implicitWidth: 60; topPadding: 8; bottomPadding: 8
                background: Rectangle { color: Qt.rgba(1,1,1,0.1); radius: 8; border.color: Qt.rgba(1,1,1,0.2) }
            }
        }

        ColumnLayout {
            spacing: 3
            Label { text: "Minutes"; color: configManager.themeTextSecondary; font.pixelSize: 14; Layout.alignment: Qt.AlignHCenter }
            TextField {
                id: minField
                text: "05"
                validator: IntValidator { bottom: 0; top: 59 }
                color: configManager.themeTextPrimary; horizontalAlignment: TextInput.AlignHCenter
                inputMethodHints: Qt.ImhDigitsOnly
                implicitWidth: 60; topPadding: 8; bottomPadding: 8
                background: Rectangle { color: Qt.rgba(1,1,1,0.1); radius: 8; border.color: Qt.rgba(1,1,1,0.2) }
            }
        }

        ColumnLayout {
            spacing: 3
            Label { text: "Seconds"; color: configManager.themeTextSecondary; font.pixelSize: 14; Layout.alignment: Qt.AlignHCenter }
            TextField {
                id: secField
                text: "00"
                validator: IntValidator { bottom: 0; top: 59 }
                color: configManager.themeTextPrimary; horizontalAlignment: TextInput.AlignHCenter
                inputMethodHints: Qt.ImhDigitsOnly
                implicitWidth: 60; topPadding: 8; bottomPadding: 8
                background: Rectangle { color: Qt.rgba(1,1,1,0.1); radius: 8; border.color: Qt.rgba(1,1,1,0.2) }
            }
        }
    }

    RowLayout {
        spacing: 18
        Layout.alignment: Qt.AlignHCenter

        GlassButton {
            text: timerRunning ? "Pause" : "Start"
            baseColor: timerRunning ? Qt.rgba(1,0.6,0,0.25) : Qt.rgba(0.3,1,0.3,0.2)
            hoverColor: timerRunning ? Qt.rgba(1,0.6,0,0.35) : Qt.rgba(0.3,1,0.3,0.35)
            onClicked: {
                if (timerRunning) {
                    timerObj.stop()
                    timerRunning = false
                } else {
                    if (remainingSecs === 0) {
                        var h = parseInt(hourField.text) || 0
                        var m = parseInt(minField.text) || 0
                        var s = parseInt(secField.text) || 0
                        remainingSecs = h * 3600 + m * 60 + s
                    }
                    if (remainingSecs > 0) {
                        timerRunning = true
                        timerObj.start()
                    }
                }
            }
            enabled: !timerRunning || remainingSecs > 0
        }

        GlassButton {
            text: "Reset"
            baseColor: Qt.rgba(1,0.2,0.2,0.15)
            hoverColor: Qt.rgba(1,0.2,0.2,0.3)
            onClicked: {
                timerObj.stop()
                timerRunning = false
                remainingSecs = 0
                audioPlayer.stop()
            }
        }
    }

    Item { height: 8 }

    Rectangle {
        Layout.fillWidth: true; Layout.preferredHeight: 1
        color: Qt.rgba(1, 1, 1, 0.06)
    }

    RowLayout {
        Layout.fillWidth: true; spacing: 8
        Text { text: "Sound when done:"; color: configManager.themeTextSecondary; font.pixelSize: 14; Layout.alignment: Qt.AlignVCenter }

        RoundedCombo {
            id: timerSoundCmb
            Layout.fillWidth: true
            model: {
                var tones = configManager.availableTones()
                var arr = ["None"]
                for (var i = 0; i < tones.length; i++)
                    arr.push(tones[i])
                return arr
            }
            onActivated: {
                timerSoundFile = (currentIndex === 0) ? "" : currentText
            }
        }

        GlassButton {
            text: "▶"
            pixelSize: 11
            implicitWidth: 32; implicitHeight: 28
            radius: 8
            onClicked: {
                if (timerSoundCmb.currentIndex > 0)
                    audioPlayer.preview(timerSoundCmb.currentText)
            }
        }
    }

    Item { Layout.fillHeight: true }
}

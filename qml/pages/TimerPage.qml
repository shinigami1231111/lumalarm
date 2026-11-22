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
    // How long the completion sound may play after the countdown reaches zero.
    property int autoStopSecs: 120

    property var hourField: null
    property var minField: null
    property var secField: null
    property var autoField: null

    function stopAutoStop() {
        if (autoStopTimer && autoStopTimer.running)
            autoStopTimer.stop()
    }

    Connections {
        target: audioPlayer
        function onIsPlayingChanged() { if (!audioPlayer.isPlaying) timerPreviewing = false }
    }

    property var timerObj: Timer {
        interval: 1000; repeat: true
        onTriggered: {
            if (root.remainingSecs <= 0) {
                stop()
                root.timerRunning = false
                return
            }
            root.remainingSecs--
            if (root.remainingSecs === 0) {
                // Sound on the tick that reaches zero, not one tick later.
                stop()
                root.timerRunning = false
                if (root.timerSoundFile !== "") {
                    audioPlayer.play(root.timerSoundFile)
                    autoStopTimer.interval = Math.max(1, root.autoStopSecs) * 1000
                    autoStopTimer.start()
                }
            }
        }
    }

    property var autoStopTimer: Timer {
        interval: 120000; repeat: false
        onTriggered: {
            audioPlayer.stop()
            root.timerPreviewing = false
        }
    }

    function formatTime(secs) {
        var h = Math.floor(secs / 3600), m = Math.floor((secs % 3600) / 60), s = secs % 60
        return ("00"+h).slice(-2) + ":" + ("00"+m).slice(-2) + ":" + ("00"+s).slice(-2)
    }

    Text { text: (configManager.emojiLevel, configManager.emoji("⏳", 1)) + qsTr("Timer"); color: configManager.themeTextPrimary; font.pixelSize: 24; font.bold: true }

    Item { Layout.fillHeight: true }

    Text {
        text: formatTime(remainingSecs)
        color: configManager.themeTextPrimary
        font.pixelSize: 76; font.bold: true; opacity: 0.92
        Layout.alignment: Qt.AlignHCenter
        font.letterSpacing: 3
    }

    Text {
        text: (configManager.emojiLevel, configManager.emoji("⏱️", 2)) + (timerRunning ? qsTr("Running") : (remainingSecs > 0 ? qsTr("Paused") : qsTr("Set a duration")))
        color: timerRunning ? Qt.rgba(0.3,1,0.3,0.8) : configManager.themeTextSecondary
        font.pixelSize: 14
        Layout.alignment: Qt.AlignHCenter
    }

    Item { Layout.fillHeight: true }

    RowLayout {
        spacing: 8
        Layout.alignment: Qt.AlignHCenter
        Repeater {
            // `key` is a stable identifier: comparing the localized label
            // (which may carry an emoji prefix) made every field lookup fail
            // and Start silently do nothing.
            model: [
                {key: "h",     text: "00",  label: (configManager.emojiLevel, configManager.emoji("🕐", 2)) + qsTr("Hours"), max: 99,  min: 0},
                {key: "m",     text: "00",  label: (configManager.emojiLevel, configManager.emoji("🕑", 2)) + qsTr("Min"), max: 59,  min: 0},
                {key: "s",     text: "00",  label: (configManager.emojiLevel, configManager.emoji("🕒", 2)) + qsTr("Sec"), max: 59,  min: 0},
                {key: "auto",  text: "120", label: (configManager.emojiLevel, configManager.emoji("⏱️", 2)) + qsTr("Auto Stop") + " (s)", max: 600, min: 10}
            ]
            ColumnLayout {
                spacing: 3
                Label {
                    text: modelData.label
                    color: configManager.themeTextSecondary; font.pixelSize: 12
                    Layout.alignment: Qt.AlignHCenter
                }
                TextField {
                    text: modelData.text
                    validator: IntValidator { bottom: modelData.min; top: modelData.max }
                    color: configManager.themeTextPrimary; horizontalAlignment: TextInput.AlignHCenter
                    inputMethodHints: Qt.ImhDigitsOnly
                    implicitWidth: modelData.key === "auto" ? 62 : 54; implicitHeight: 40
                    maximumLength: modelData.key === "auto" ? 3 : 2
                    font.pixelSize: 16
                    background: Rectangle {
                        color: Qt.rgba(1,1,1,0.06); radius: 9
                        border.color: Qt.rgba(1,1,1,0.12); border.width: 1
                    }
                    Component.onCompleted: {
                        if (modelData.key === "h") root.hourField = this
                        else if (modelData.key === "m") root.minField = this
                        else if (modelData.key === "s") root.secField = this
                        else root.autoField = this
                    }
                    onEditingFinished: {
                        const v = parseInt(text, 10)
                        if (modelData.key === "auto") {
                            root.autoStopSecs = isNaN(v) ? 120 : Math.max(10, Math.min(600, v))
                            text = String(root.autoStopSecs)
                        } else {
                            text = ("00" + (isNaN(v) ? 0 : Math.max(modelData.min, Math.min(modelData.max, v)))).slice(-2)
                        }
                    }
                }
            }
        }
    }

    RowLayout {
        spacing: 12
        Layout.alignment: Qt.AlignHCenter
        GlassButton {
            text: timerRunning ? (configManager.emojiLevel, configManager.emoji("⏸️", 2)) + qsTr("Pause") : (configManager.emojiLevel, configManager.emoji("▶️", 2)) + qsTr("Start")
            pixelSize: 14
            implicitWidth: configManager.emojiLevel >= 2 ? 132 : 110; implicitHeight: 40
            filled: !timerRunning
            baseColor: timerRunning ? Qt.rgba(1,0.6,0,0.22) : Qt.rgba(0.3,1,0.3,0.18)
            hoverColor: timerRunning ? Qt.rgba(1,0.6,0,0.32) : Qt.rgba(0.3,1,0.3,0.3)
            onClicked: {
                stopAutoStop()
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
            text: (configManager.emojiLevel, configManager.emoji("🔄", 2)) + qsTr("Reset")
            pixelSize: 14
            implicitWidth: configManager.emojiLevel >= 2 ? 122 : 100; implicitHeight: 40
            baseColor: Qt.rgba(1,0.2,0.2,0.15)
            hoverColor: Qt.rgba(1,0.2,0.2,0.28)
            onClicked: { timerObj.stop(); stopAutoStop(); timerRunning = false; remainingSecs = 0; audioPlayer.stop() }
        }
    }

    Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: Qt.rgba(1,1,1,0.07) }

    RowLayout {
        Layout.fillWidth: true; spacing: 10
        Text { text: (configManager.emojiLevel, configManager.emoji("🔔", 2)) + qsTr("Sound when done:"); color: configManager.themeTextSecondary; font.pixelSize: 14; Layout.alignment: Qt.AlignVCenter }
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

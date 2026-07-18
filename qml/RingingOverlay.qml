import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import QtMultimedia
import GlassAlarm

Rectangle {
    id: root

    property bool ringing: false
    property bool wakeUpActive: false
    property var alarmData: ({})
    property int alarmHour: 0
    property int alarmMinute: 0
    property string alarmMedia: ""

    property bool challengeDone: false
    property string challengeTarget: ""
    property string challengeInput: ""

    property int wakeCountdown: 30

    property var challengePool: [
        "wakeup", "morning", "sunshine", "active", "awake",
        "ready", "focus", "energy", "bright", "alive",
        "arise", "fresh", "start", "power", "alert"
    ]

    signal stopAlarm()
    signal snoozeAlarm()
    signal reTriggerAlarm()

    visible: ringing || wakeUpActive
    anchors.fill: parent
    color: Qt.rgba(0, 0, 0, 0.85)
    z: 999

    function pickChallenge() {
        if (alarmData.challengeText && alarmData.challengeText !== "") {
            challengeTarget = alarmData.challengeText
        } else {
            challengeTarget = challengePool[Math.floor(Math.random() * challengePool.length)]
        }
    }

    Item {
        anchors.fill: parent
        anchors.margins: 40
        AnimatedImage {
            id: gifPlayer
            anchors.fill: parent
            fillMode: Image.PreserveAspectFit
            playing: root.ringing
            visible: false
        }
        MediaPlayer {
            id: vidPlayer
            videoOutput: videoOut
            loops: MediaPlayer.Infinite
        }
        VideoOutput {
            id: videoOut
            anchors.fill: parent
            visible: false
        }
    }

    onRingingChanged: {
        if (ringing) {
            wakeUpActive = false
            wakeDelayTimer.stop()
            wakeCountdownTimer.stop()
            challengeDone = !alarmData.enableChallenge
            if (alarmData.enableChallenge) {
                pickChallenge()
                challengeInput = ""
            }
            if (alarmMedia.endsWith(".mp4") || alarmMedia.endsWith(".avi") ||
                alarmMedia.endsWith(".mkv") || alarmMedia.endsWith(".webm"))
                vidPlayer.play()
            else if (alarmMedia.endsWith(".gif"))
                gifPlayer.playing = true
        } else {
            vidPlayer.stop()
            gifPlayer.playing = false
        }
    }

    onAlarmMediaChanged: {
        var isVideo = alarmMedia.endsWith(".mp4") || alarmMedia.endsWith(".avi") ||
                      alarmMedia.endsWith(".mkv") || alarmMedia.endsWith(".webm")
        var isGif = alarmMedia.endsWith(".gif")
        if (isVideo) {
            vidPlayer.source = "file://" + configManager.tonesDirectory() + "/" + alarmMedia
            videoOut.visible = true; gifPlayer.visible = false
            if (ringing) vidPlayer.play()
        } else if (isGif) {
            gifPlayer.source = "file://" + configManager.tonesDirectory() + "/" + alarmMedia
            gifPlayer.visible = true; videoOut.visible = false
            if (ringing) gifPlayer.playing = true
        } else {
            gifPlayer.visible = false; videoOut.visible = false
        }
    }

    Timer { id: wakeDelayTimer; repeat: false; onTriggered: startWakeCheck() }
    Timer {
        id: wakeCountdownTimer; interval: 1000; repeat: true
        onTriggered: {
            wakeCountdown--
            if (wakeCountdown <= 0) {
                wakeCountdownTimer.stop()
                wakeUpActive = false
                reTriggerAlarm()
            }
        }
    }

    function startWakeCheck() {
        wakeUpActive = true
        wakeCountdown = 30
        wakeCountdownTimer.start()
    }

    // --- Normal alarm / challenge UI (visible when ringing) ---
    ColumnLayout {
        anchors.centerIn: parent
        spacing: 24
        z: 10
        visible: ringing && !wakeUpActive

        Text {
            text: "ALARM"
            color: "#FFFFFF"; font.pixelSize: 52; font.bold: true; opacity: 0.9
            Layout.alignment: Qt.AlignHCenter
            style: Text.Raised; styleColor: Qt.rgba(0,0,0,0.5)
        }

        Text {
            text: ("00" + alarmHour).slice(-2) + ":" + ("00" + alarmMinute).slice(-2)
            color: Qt.rgba(1, 1, 1, 0.6); font.pixelSize: 22
            Layout.alignment: Qt.AlignHCenter
        }

        // Challenge typing section
        Rectangle {
            visible: !challengeDone
            Layout.fillWidth: true; Layout.maximumWidth: 340
            Layout.preferredHeight: 80; radius: 12
            color: Qt.rgba(1,1,1,0.06); border.color: Qt.rgba(1,1,1,0.12); border.width: 1

            ColumnLayout {
                anchors.centerIn: parent; spacing: 8
                Text {
                    text: "Type: " + challengeTarget
                    color: configManager.themeAccent; font.pixelSize: 20; font.bold: true
                    Layout.alignment: Qt.AlignHCenter
                }
                TextField {
                    id: challengeField
                    Layout.preferredWidth: 200
                    horizontalAlignment: TextInput.AlignHCenter
                    font.pixelSize: 18; font.bold: true
                    color: configManager.themeTextPrimary
                    background: Rectangle {
                        color: Qt.rgba(1,1,1,0.1); radius: 6
                        border.color: Qt.rgba(1,1,1,0.2); border.width: 1
                    }
                    onTextChanged: {
                        if (text === challengeTarget) {
                            challengeDone = true
                        }
                    }
                }
            }
        }

        Text {
            visible: !challengeDone
            text: "Type the word above to dismiss"
            color: Qt.rgba(1,1,1,0.4); font.pixelSize: 13
            Layout.alignment: Qt.AlignHCenter
        }

        RowLayout {
            spacing: 32
            Layout.alignment: Qt.AlignHCenter

            GlassButton {
                text: "STOP"
                pixelSize: 20; implicitWidth: 140; implicitHeight: 60; radius: 16
                enabled: challengeDone
                opacity: enabled ? 1.0 : 0.4
                baseColor: Qt.rgba(1, 0.2, 0.2, 0.2)
                hoverColor: Qt.rgba(1, 0.2, 0.2, 0.35)
                pressedColor: Qt.rgba(1, 0.2, 0.2, 0.5)
                onClicked: {
                    audioPlayer.stop()
                    ringing = false
                    if (alarmData.wakeUpCheckEnabled) {
                        wakeDelayTimer.interval = alarmData.wakeUpCheckInterval * 60 * 1000
                        wakeDelayTimer.start()
                    }
                }
            }

            GlassButton {
                text: "SNOOZE"
                pixelSize: 20; implicitWidth: 140; implicitHeight: 60; radius: 16
                enabled: challengeDone
                opacity: enabled ? 1.0 : 0.4
                baseColor: Qt.rgba(0.2, 0.6, 1, 0.2)
                hoverColor: Qt.rgba(0.2, 0.6, 1, 0.35)
                pressedColor: Qt.rgba(0.2, 0.6, 1, 0.5)
                onClicked: {
                    audioPlayer.stop()
                    ringing = false
                    scheduler.snooze(configManager.defaultSnooze())
                }
            }
        }
    }

    // --- Wake-up check UI (visible when wakeUpActive) ---
    ColumnLayout {
        anchors.centerIn: parent
        spacing: 32
        z: 10
        visible: wakeUpActive

        Text {
            text: "Still awake?"
            color: "#FFFFFF"; font.pixelSize: 42; font.bold: true
            Layout.alignment: Qt.AlignHCenter
        }

        Text {
            text: wakeCountdown + "s"
            color: Qt.rgba(1, 1, 1, 0.4); font.pixelSize: 28
            Layout.alignment: Qt.AlignHCenter
        }

        GlassButton {
            text: "YES, I'M AWAKE"
            pixelSize: 18; implicitWidth: 200; implicitHeight: 56; radius: 16
            Layout.alignment: Qt.AlignHCenter
            baseColor: Qt.rgba(0.3, 1, 0.3, 0.15)
            hoverColor: Qt.rgba(0.3, 1, 0.3, 0.3)
            onClicked: {
                wakeCountdownTimer.stop()
                wakeUpActive = false
            }
        }
    }

    SequentialAnimation {
        running: root.ringing && !wakeUpActive
        loops: Animation.Infinite
        NumberAnimation { target: root; property: "opacity"; to: 0.7; duration: 800; easing.type: Easing.InOutQuad }
        NumberAnimation { target: root; property: "opacity"; to: 1.0; duration: 800; easing.type: Easing.InOutQuad }
    }
}

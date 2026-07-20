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
    property int alarmIndex: -1
    property string alarmId: ""
    property int alarmHour: 0
    property int alarmMinute: 0
    property string alarmMedia: ""

    property bool snoozing: false

    // Challenge state
    property bool challengeDone: false
    property string challengeTarget: ""
    property string challengeInput: ""

    // Math challenge state
    property string mathProblem: ""
    property int mathAnswer: 0

    // Typing challenge pool
    property var challengePool: [
        "wakeup", "morning", "sunshine", "active", "awake",
        "ready", "focus", "energy", "bright", "alive",
        "arise", "fresh", "start", "power", "alert"
    ]

    // Escalating wake state
    property int escalateStage: 0  // 0=inactive, 1=brightness, 2=sound, 3=forced challenge
    property bool escalateForcedChallenge: false

    // Wake-up check
    property int wakeCountdown: 30

    signal stopAlarm()
    signal snoozeAlarm()

    function cancelSnooze() {
        snoozing = false
        if (alarmId !== "")
            scheduler.cancelSnooze(alarmId)
    }

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

    function generateMathProblem() {
        var diff = alarmData.mathDifficulty || 0
        var a, b, op, answer
        if (diff === 1) {
            a = Math.floor(Math.random() * 12) + 2
            b = Math.floor(Math.random() * 9) + 2
            op = "\u00D7"
            answer = a * b
        } else {
            if (Math.random() < 0.5) {
                a = Math.floor(Math.random() * 50) + 1
                b = Math.floor(Math.random() * a) + 1
                op = "+"
                answer = a + b
            } else {
                a = Math.floor(Math.random() * 30) + 10
                b = Math.floor(Math.random() * a) + 1
                op = "\u2212"
                answer = a - b
            }
        }
        mathProblem = a + " " + op + " " + b + " = ?"
        mathAnswer = answer
    }

    function setupChallenge() {
        var mode = alarmData.challengeMode || "none"
        if (mode === "none") {
            challengeDone = true
        } else if (mode === "math") {
            challengeDone = false
            challengeInput = ""
            generateMathProblem()
        } else {
            challengeDone = false
            challengeInput = ""
            pickChallenge()
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
            escalateStage = 0
            escalateForcedChallenge = false
            escalateTimer.stop()

            setupChallenge()

            if (alarmData.escalatingWake) {
                escalateStage = 1
                escalateTimer.start()
            }

            if (alarmMedia.endsWith(".mp4") || alarmMedia.endsWith(".avi") ||
                alarmMedia.endsWith(".mkv") || alarmMedia.endsWith(".webm"))
                vidPlayer.play()
            else if (alarmMedia.endsWith(".gif"))
                gifPlayer.playing = true
        } else {
            vidPlayer.stop()
            gifPlayer.playing = false
            escalateTimer.stop()
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

    // Escalating wake timer
    Timer {
        id: escalateTimer
        interval: 15000
        repeat: false
        onTriggered: {
            if (!root.ringing) return
            if (escalateStage === 1) {
                // Stage 2: sound began (already playing), advance to stage 3 tracking
                escalateStage = 2
                escalateTimer.interval = (alarmData.escalatingTimeout || 60) * 1000
                escalateTimer.start()
            } else if (escalateStage === 2) {
                // Stage 3: force challenge if not already dismissed
                escalateStage = 3
                escalateForcedChallenge = true
                setupChallenge()
            }
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
                root.wakeFromSnooze()
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
            visible: alarmData.name && alarmData.name !== ""
            text: alarmData.name
            color: Qt.rgba(1, 1, 1, 0.85); font.pixelSize: 26; font.bold: true
            Layout.alignment: Qt.AlignHCenter
            style: Text.Raised; styleColor: Qt.rgba(0,0,0,0.3)
        }

        Text {
            text: ("00" + alarmHour).slice(-2) + ":" + ("00" + alarmMinute).slice(-2)
            color: Qt.rgba(1, 1, 1, 0.6); font.pixelSize: 22
            Layout.alignment: Qt.AlignHCenter
        }

        // Alarm note (if set)
        Text {
            visible: alarmData.note && alarmData.note !== ""
            text: alarmData.note
            color: Qt.rgba(1, 1, 1, 0.8); font.pixelSize: 32; font.bold: true
            Layout.alignment: Qt.AlignHCenter
            style: Text.Raised; styleColor: Qt.rgba(0,0,0,0.3)
        }

        // Escalating wake indicator
        Text {
            visible: alarmData.escalatingWake && escalateStage > 0
            text: escalateStage === 1 ? "Waking up..." :
                  escalateStage === 2 ? "Time to get up!" :
                  escalateStage === 3 ? "Please respond!" : ""
            color: Qt.rgba(1, 0.8, 0.2, 0.7); font.pixelSize: 16; font.bold: true
            Layout.alignment: Qt.AlignHCenter
        }

        // Challenge section (typing or math)
        Rectangle {
            visible: !challengeDone
            Layout.fillWidth: true; Layout.maximumWidth: 340
            Layout.preferredHeight: challengeRectHeight.implicitHeight + 24
            radius: 12
            color: Qt.rgba(1,1,1,0.06); border.color: Qt.rgba(1,1,1,0.12); border.width: 1

            ColumnLayout {
                id: challengeRectHeight
                anchors.centerIn: parent
                spacing: 8
                width: parent.width - 24

                // Typing challenge mode
                Text {
                    visible: alarmData.challengeMode !== "math"
                    text: "Type: " + challengeTarget
                    color: configManager.themeAccent; font.pixelSize: 20; font.bold: true
                    Layout.alignment: Qt.AlignHCenter
                }

                // Math challenge mode
                Text {
                    visible: alarmData.challengeMode === "math"
                    text: mathProblem
                    color: configManager.themeAccent; font.pixelSize: 24; font.bold: true
                    Layout.alignment: Qt.AlignHCenter
                }

                TextField {
                    id: challengeField
                    Layout.preferredWidth: 200
                    Layout.alignment: Qt.AlignHCenter
                    horizontalAlignment: TextInput.AlignHCenter
                    font.pixelSize: 18; font.bold: true
                    color: configManager.themeTextPrimary
                    background: Rectangle {
                        color: Qt.rgba(1,1,1,0.1); radius: 6
                        border.color: Qt.rgba(1,1,1,0.2); border.width: 1
                    }
                    onTextChanged: {
                        if (alarmData.challengeMode === "math") {
                            var num = parseInt(text)
                            if (!isNaN(num) && num === mathAnswer) {
                                challengeDone = true
                            }
                        } else {
                            if (text === challengeTarget) {
                                challengeDone = true
                            }
                        }
                    }
                }
            }
        }

        Text {
            visible: !challengeDone
            text: alarmData.challengeMode === "math" ? "Type the correct answer" : "Type the word above to dismiss"
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
                    cancelSnooze()
                    alarmManager.alarmDismissed(alarmIndex, escalateStage)
                    if (alarmData.wakeUpCheckEnabled) {
                        wakeDelayTimer.interval = alarmData.wakeUpCheckInterval * 60 * 1000
                        wakeDelayTimer.start()
                    }
                }
            }

            GlassButton {
                id: snoozeBtn
                text: "SNOOZE"
                pixelSize: 20; implicitWidth: 140; implicitHeight: 60; radius: 16
                enabled: challengeDone && snoozeAllowed
                opacity: enabled ? 1.0 : 0.4
                baseColor: Qt.rgba(0.2, 0.6, 1, 0.2)
                hoverColor: Qt.rgba(0.2, 0.6, 1, 0.35)
                pressedColor: Qt.rgba(0.2, 0.6, 1, 0.5)

                property bool snoozeAllowed: {
                    if (!root.ringing) return false
                    if (root.alarmId === "") return false
                    var maxS = alarmData.maxSnoozes
                    if (maxS === -1) return true
                    if (maxS === 0) return false
                    var current = alarmManager.snoozeCount(alarmIndex)
                    return current < maxS
                }

                onClicked: {
                    audioPlayer.stop()
                    ringing = false
                    alarmManager.incrementSnooze(alarmIndex)
                    scheduler.startSnoozeTimer(root.alarmId, snoozeMin)
                }

                property int snoozeMin: alarmData.snoozeInterval && alarmData.snoozeInterval > 0 ? alarmData.snoozeInterval : configManager.defaultSnooze
            }
        }

        // Snooze status indicator
        Text {
            visible: ringing && alarmData.maxSnoozes !== 0
            text: {
                if (!snoozeBtn.snoozeAllowed)
                    return "Snooze limit reached"
                var used = alarmManager.snoozeCount(alarmIndex)
                if (alarmData.maxSnoozes === -1)
                    return used > 0 ? "Snoozed " + used + " times" : ""
                return "Snoozes: " + used + " / " + alarmData.maxSnoozes
            }
            color: !snoozeBtn.snoozeAllowed ? Qt.rgba(1, 0.6, 0, 0.7) : Qt.rgba(1,1,1,0.4)
            font.pixelSize: 13
            Layout.alignment: Qt.AlignHCenter
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

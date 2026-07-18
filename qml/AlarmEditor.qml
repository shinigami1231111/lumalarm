import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import GlassAlarm

GlassCard {
    id: root

    property bool hasSelection: false
    property int editIndex: -1

    property int editHour: 7
    property int editMinute: 0
    property var editDays: [false, false, false, false, false, false, false]
    property string editWakeMode: "mem"
    property int editFadeDuration: 15
    property int editBaseVolume: 20
    property int editAutoStop: 120
    property bool editEnableSound: true
    property bool editEnableCommand: false
    property string editCommand: ""
    property bool editEnabled: true
    property string editSoundFile: ""
    property bool editEnableChallenge: false
    property string editChallengeText: ""
    property bool editWakeUpCheck: false
    property int editWakeUpInterval: 3

    // Phase 1 fields
    property string editSoundscape: ""
    property int editMaxSnoozes: -1
    property string editChallengeMode: "none"
    property int editMathDifficulty: 0
    property bool editEscalatingWake: false
    property int editEscalatingTimeout: 60
    property string editNote: ""

    function loadAlarm(index) {
        var alarms = alarmManager.alarms
        if (index < 0 || index >= alarms.length) return

        var a = alarms[index]
        editIndex = index
        editHour = a.hour
        editMinute = a.minute
        editDays = a.days.slice()
        editWakeMode = a.wakeMode
        editFadeDuration = a.fadeDuration
        editBaseVolume = a.baseVolume
        editAutoStop = a.autoStopDuration
        editEnableSound = a.enableSound
        editEnableCommand = a.enableCommand
        editCommand = a.command
        editEnabled = a.enabled
        editSoundFile = a.soundFile
        editEnableChallenge = a.enableChallenge || false
        editChallengeText = a.challengeText || ""
        editWakeUpCheck = a.wakeUpCheckEnabled || false
        editWakeUpInterval = a.wakeUpCheckInterval || 3

        editSoundscape = a.soundscape || ""
        editMaxSnoozes = a.maxSnoozes !== undefined ? a.maxSnoozes : -1
        editChallengeMode = a.challengeMode || "none"
        editMathDifficulty = a.mathDifficulty || 0
        editEscalatingWake = a.escalatingWake || false
        editEscalatingTimeout = a.escalatingTimeout || 60
        editNote = a.note || ""

        hasSelection = true
        Qt.callLater(function() {
            soundCmb.currentIndex = Math.max(0, soundCmb.find(editSoundFile))
            scCmb.currentIndex = Math.max(0, scCmb.find(editSoundscape))
        })
    }

    function clearSelection() {
        hasSelection = false
        editIndex = -1
        editHour = 7
        editMinute = 0
        editDays = [false, false, false, false, false, false, false]
        editWakeMode = "mem"
        editFadeDuration = 15
        editBaseVolume = 20
        editEnabled = true
        editSoundFile = ""
        editEnableSound = true
        editEnableCommand = false
        editCommand = ""
        editAutoStop = 120
        editEnableChallenge = false
        editChallengeText = ""
        editWakeUpCheck = false
        editWakeUpInterval = 3
        editSoundscape = ""
        editMaxSnoozes = -1
        editChallengeMode = "none"
        editMathDifficulty = 0
        editEscalatingWake = false
        editEscalatingTimeout = 60
        editNote = ""
    }

    function commitAlarm() {
        if (!hasSelection) return

        var a = {
            "hour": editHour,
            "minute": editMinute,
            "days": editDays.slice(),
            "wakeMode": editWakeMode,
            "fadeDuration": editFadeDuration,
            "baseVolume": editBaseVolume,
            "autoStopDuration": editAutoStop,
            "enableSound": editEnableSound,
            "enableCommand": editEnableCommand,
            "command": editCommand,
            "enabled": editEnabled,
            "soundFile": editSoundFile,
            "isSnooze": false,
            "enableChallenge": editEnableChallenge,
            "challengeText": editChallengeText,
            "wakeUpCheckEnabled": editWakeUpCheck,
            "wakeUpCheckInterval": editWakeUpInterval,
            "soundscape": editSoundscape,
            "maxSnoozes": editMaxSnoozes,
            "challengeMode": editChallengeMode,
            "mathDifficulty": editMathDifficulty,
            "escalatingWake": editEscalatingWake,
            "escalatingTimeout": editEscalatingTimeout,
            "note": editNote
        }

        if (editIndex >= 0) {
            alarmManager.updateAlarm(editIndex, a)
        } else {
            alarmManager.addAlarm(a)
        }
    }

    visible: hasSelection
    implicitHeight: contentColumn.implicitHeight + 32

    ColumnLayout {
        id: contentColumn
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: 24
        spacing: 16

        Text {
            text: editIndex >= 0 ? "Edit Alarm" : "New Alarm"
            color: configManager.themeTextPrimary
            font.pixelSize: 22
            font.bold: true
        }

        RowLayout {
            spacing: 12

            ColumnLayout {
                spacing: 3
                Label {
                    text: "Hour"
                    color: configManager.themeTextSecondary
                    font.pixelSize: 14
                }
                TextField {
                    id: hourField
                    text: {
                        var h = editHour % 12
                        return (h === 0 ? 12 : h).toString()
                    }
                    onTextChanged: {
                        var v = parseInt(text)
                        if (isNaN(v) || v < 1) v = 12
                        if (v > 12) v = 12
                        editHour = amPmBtn.isPM ? (v % 12) + 12 : v % 12
                    }
                    validator: IntValidator { bottom: 1; top: 12 }
                    color: configManager.themeTextPrimary
                    horizontalAlignment: TextInput.AlignHCenter
                    inputMethodHints: Qt.ImhDigitsOnly
                    background: Rectangle {
                        color: Qt.rgba(1,1,1,0.1)
                        radius: 8
                        border.color: Qt.rgba(1,1,1,0.2)
                    }
                    implicitWidth: 76
                    topPadding: 6
                    bottomPadding: 6
                }
            }

            ColumnLayout {
                spacing: 3
                Label {
                    text: "Minute"
                    color: configManager.themeTextSecondary
                    font.pixelSize: 14
                }
                TextField {
                    id: minuteField
                    text: ("00" + editMinute).slice(-2)
                    onTextChanged: {
                        var v = parseInt(text)
                        if (isNaN(v) || v < 0) v = 0
                        if (v > 59) v = 59
                        editMinute = v
                    }
                    validator: IntValidator { bottom: 0; top: 59 }
                    color: configManager.themeTextPrimary
                    horizontalAlignment: TextInput.AlignHCenter
                    inputMethodHints: Qt.ImhDigitsOnly
                    background: Rectangle {
                        color: Qt.rgba(1,1,1,0.1)
                        radius: 8
                        border.color: Qt.rgba(1,1,1,0.2)
                    }
                    implicitWidth: 76
                    topPadding: 6
                    bottomPadding: 6
                }
            }

            ColumnLayout {
                spacing: 3
                Label {
                    text: "Period"
                    color: configManager.themeTextSecondary
                    font.pixelSize: 14
                }
                GlassButton {
                    id: amPmBtn
                    property bool isPM: editHour >= 12
                    text: isPM ? "PM" : "AM"
                    pixelSize: 12
                    implicitWidth: 50
                    implicitHeight: 32
                    radius: 8
                    baseColor: isPM ? Qt.rgba(0.2, 0.6, 1, 0.3) : Qt.rgba(1, 1, 1, 0.1)
                    onClicked: {
                        isPM = !isPM
                        var h = parseInt(hourField.text) || 12
                        editHour = isPM ? (h % 12) + 12 : h % 12
                    }
                }
            }
        }

        GlassCheckBox {
            id: daysCheck
            labelText: "Set Days"
            checked: {
                for (var i = 0; i < 7; i++)
                    if (editDays[i]) return true
                return false
            }
            onCheckedChanged: {
                if (!checked)
                    editDays = [false, false, false, false, false, false, false]
            }
        }

        ColumnLayout {
            spacing: 6
            visible: daysCheck.checked
            Flow {
                spacing: 5
                Repeater {
                    model: ["Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"]
                    GlassButton {
                        text: modelData
                        pixelSize: 10
                        implicitWidth: 46
                        implicitHeight: 26
                        radius: 8
                        baseColor: editDays[index] ? Qt.rgba(0.2, 0.6, 1, 0.3) : Qt.rgba(1, 1, 1, 0.1)
                        onClicked: {
                            var d = editDays.slice()
                            d[index] = !d[index]
                            editDays = d
                        }
                    }
                }
            }
        }

        RowLayout {
            spacing: 16

            ValueCtrl { label: "Fade (sec)"; value: editFadeDuration; minVal: 5; maxVal: 60; step: 5; suffix: ""; onValueChanged: editFadeDuration = value }
            ValueCtrl { label: "Auto Stop"; value: editAutoStop; minVal: 10; maxVal: 600; step: 10; suffix: "s"; onValueChanged: editAutoStop = value }
            ValueCtrl { label: "Start Vol"; value: editBaseVolume; minVal: 0; maxVal: 50; step: 5; suffix: "%"; onValueChanged: editBaseVolume = value }
        }

        GlassCheckBox {
            labelText: "Enable Sound"
            checked: editEnableSound
            onCheckedChanged: editEnableSound = checked
        }

        RowLayout {
            spacing: 6
            visible: editEnableSound

            RoundedCombo {
                id: soundCmb
                Layout.fillWidth: true
                model: configManager.availableTones()
                onActivated: {
                    editSoundFile = currentText
                }
                Component.onCompleted: {
                    currentIndex = Math.max(0, find(editSoundFile))
                }
                Connections {
                    target: configManager
                    function onConfigChanged() {
                        soundCmb.model = configManager.availableTones()
                    }
                }
            }

            GlassButton {
                text: "▶"
                pixelSize: 11
                implicitWidth: 32; implicitHeight: 28
                radius: 8
                onClicked: {
                    if (soundCmb.currentIndex >= 0)
                        audioPlayer.preview(soundCmb.currentText)
                }
            }
        }

        GlassCheckBox {
            labelText: "Enable Command"
            checked: editEnableCommand
            onCheckedChanged: editEnableCommand = checked
        }

        TextField {
            visible: editEnableCommand
            Layout.fillWidth: true
            placeholderText: "e.g. notify-send 'Wake up!'"
            text: editCommand
            onTextChanged: editCommand = text
            color: configManager.themeTextPrimary
            placeholderTextColor: configManager.themeTextSecondary
            background: Rectangle {
                color: Qt.rgba(1,1,1,0.08)
                radius: 8
                border.color: Qt.rgba(1,1,1,0.15)
            }
            leftPadding: 8
            rightPadding: 8
            topPadding: 6
            bottomPadding: 6
        }

        Rectangle {
            Layout.fillWidth: true; Layout.preferredHeight: 1
            color: Qt.rgba(1, 1, 1, 0.06)
        }

        // --- Dismissal Method ---
        Text {
            text: "Dismissal Method"
            color: configManager.themeTextSecondary
            font.pixelSize: 14
        }

        RowLayout {
            spacing: 8
            Repeater {
                model: [
                    {label: "None", mode: "none"},
                    {label: "Typing", mode: "typing"},
                    {label: "Math", mode: "math"}
                ]
                GlassButton {
                    text: modelData.label
                    pixelSize: 11
                    implicitWidth: 64; implicitHeight: 28; radius: 8
                    baseColor: editChallengeMode === modelData.mode ? Qt.rgba(0.2, 0.6, 1, 0.3) : Qt.rgba(1, 1, 1, 0.1)
                    onClicked: editChallengeMode = modelData.mode
                }
            }
        }

        TextField {
            visible: editChallengeMode === "typing"
            Layout.fillWidth: true
            placeholderText: "Custom word (leave empty for random)"
            text: editChallengeText
            onTextChanged: editChallengeText = text
            color: configManager.themeTextPrimary
            placeholderTextColor: configManager.themeTextSecondary
            background: Rectangle {
                color: Qt.rgba(1,1,1,0.08)
                radius: 8
                border.color: Qt.rgba(1,1,1,0.15)
            }
            leftPadding: 8; rightPadding: 8; topPadding: 6; bottomPadding: 6
        }

        RowLayout {
            visible: editChallengeMode === "math"
            spacing: 8
            Text {
                text: "Difficulty:"
                color: configManager.themeTextSecondary
                font.pixelSize: 14
                Layout.alignment: Qt.AlignVCenter
            }
            Repeater {
                model: [
                    {label: "Easy (+/-)", val: 0},
                    {label: "Hard (×)", val: 1}
                ]
                GlassButton {
                    text: modelData.label
                    pixelSize: 10
                    implicitWidth: 80; implicitHeight: 26; radius: 8
                    baseColor: editMathDifficulty === modelData.val ? Qt.rgba(0.2, 0.6, 1, 0.3) : Qt.rgba(1, 1, 1, 0.1)
                    onClicked: editMathDifficulty = modelData.val
                }
            }
        }

        GlassCheckBox {
            labelText: "Wake-Up Check"
            checked: editWakeUpCheck
            onCheckedChanged: editWakeUpCheck = checked
        }

        ValueCtrl {
            visible: editWakeUpCheck
            label: "Check after"; value: editWakeUpInterval; minVal: 1; maxVal: 30; step: 1; suffix: "min"
            onValueChanged: editWakeUpInterval = value
        }

        Rectangle {
            Layout.fillWidth: true; Layout.preferredHeight: 1
            color: Qt.rgba(1, 1, 1, 0.06)
        }

        // --- Soundscape ---
        Text {
            text: "Soundscape (pre-alarm ambient)"
            color: configManager.themeTextSecondary
            font.pixelSize: 14
        }

        RowLayout {
            spacing: 6
            RoundedCombo {
                id: scCmb
                Layout.fillWidth: true
                model: {
                    var t = configManager.availableTones()
                    t.unshift("(none)")
                    return t
                }
                onActivated: {
                    editSoundscape = currentIndex === 0 ? "" : currentText
                }
                Component.onCompleted: {
                    currentIndex = editSoundscape === "" ? 0 : Math.max(0, find(editSoundscape))
                }
            }
            GlassButton {
                text: "▶"
                pixelSize: 11
                implicitWidth: 32; implicitHeight: 28; radius: 8
                onClicked: {
                    if (scCmb.currentIndex > 0)
                        audioPlayer.preview(scCmb.currentText)
                }
            }
        }

        // --- Snooze Limit ---
        ValueCtrl {
            label: "Max Snoozes"; value: editMaxSnoozes === -1 ? 99 : editMaxSnoozes
            minVal: 0; maxVal: 20; step: 1; suffix: editMaxSnoozes === -1 ? " (∞)" : ""
            onValueChanged: {
                if (value === 99) editMaxSnoozes = -1
                else editMaxSnoozes = value
            }
        }

        // --- Escalating Wake ---
        GlassCheckBox {
            labelText: "Escalating Wake"
            checked: editEscalatingWake
            onCheckedChanged: editEscalatingWake = checked
        }

        ValueCtrl {
            visible: editEscalatingWake
            label: "Force challenge after"; value: editEscalatingTimeout
            minVal: 15; maxVal: 300; step: 15; suffix: "s"
            onValueChanged: editEscalatingTimeout = value
        }

        // --- Note ---
        Rectangle {
            Layout.fillWidth: true; Layout.preferredHeight: 1
            color: Qt.rgba(1, 1, 1, 0.06)
        }

        Text {
            text: "Note (why did I set this?)"
            color: configManager.themeTextSecondary
            font.pixelSize: 14
        }

        TextField {
            Layout.fillWidth: true
            placeholderText: "e.g. flight to Istanbul"
            text: editNote
            onTextChanged: editNote = text
            color: configManager.themeTextPrimary
            placeholderTextColor: configManager.themeTextSecondary
            background: Rectangle {
                color: Qt.rgba(1,1,1,0.08)
                radius: 8
                border.color: Qt.rgba(1,1,1,0.15)
            }
            leftPadding: 8; rightPadding: 8; topPadding: 6; bottomPadding: 6
        }

        Item { height: 8 }
    }

    component ValueCtrl: ColumnLayout {
        id: vc
        property string label; property int value; property int minVal; property int maxVal; property int step; property string suffix
        spacing: 6; Layout.fillWidth: true

        Label {
            text: vc.label + ": " + (vc.suffix ? vc.value + vc.suffix : vc.value.toString())
            color: configManager.themeTextSecondary; font.pixelSize: 14
        }

        Rectangle {
            Layout.fillWidth: true; Layout.preferredHeight: 42; radius: 9
            color: Qt.rgba(1, 1, 1, 0.06); border.color: Qt.rgba(1, 1, 1, 0.1); border.width: 1

            RowLayout {
                anchors.fill: parent; anchors.margins: 3; spacing: 3
                Text {
                    Layout.fillWidth: true; Layout.fillHeight: true
                    verticalAlignment: Qt.AlignVCenter; horizontalAlignment: Qt.AlignHCenter
                    color: configManager.themeTextPrimary; font.pixelSize: 18; font.bold: true
                    text: vc.value + vc.suffix
                }

                ColumnLayout { spacing: 2; Layout.preferredWidth: 28
                    Rectangle {
                        Layout.fillWidth: true; Layout.preferredHeight: 17; radius: 5
                        color: upMa.containsMouse ? Qt.rgba(1,1,1,0.12) : "transparent"
                        Text { text: "▲"; anchors.centerIn: parent; color: configManager.themeTextSecondary; font.pixelSize: 10 }
                        MouseArea { id: upMa; anchors.fill: parent; hoverEnabled: true
                            onClicked: vc.value = Math.min(vc.maxVal, vc.value + vc.step)
                        }
                    }
                    Rectangle {
                        Layout.fillWidth: true; Layout.preferredHeight: 17; radius: 5
                        color: dnMa.containsMouse ? Qt.rgba(1,1,1,0.12) : "transparent"
                        Text { text: "▼"; anchors.centerIn: parent; color: configManager.themeTextSecondary; font.pixelSize: 10 }
                        MouseArea { id: dnMa; anchors.fill: parent; hoverEnabled: true
                            onClicked: vc.value = Math.max(vc.minVal, vc.value - vc.step)
                        }
                    }
                }
            }
        }
    }
}

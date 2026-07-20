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
    property int editSnoozeInterval: 0
    property bool soundPreviewing: false
    property bool scPreviewing: false

    Connections {
        target: audioPlayer
        function onIsPlayingChanged() {
            if (!audioPlayer.isPlaying) {
                soundPreviewing = false
                scPreviewing = false
            }
        }
    }
    property string editChallengeMode: "none"
    property int editMathDifficulty: 0
    property bool editEscalatingWake: false
    property int editEscalatingTimeout: 60
    property string editNote: ""
    property string editName: ""

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
        editSnoozeInterval = a.snoozeInterval && a.snoozeInterval > 0 ? a.snoozeInterval : configManager.defaultSnooze
        editChallengeMode = a.challengeMode || "none"
        editMathDifficulty = a.mathDifficulty || 0
        editEscalatingWake = a.escalatingWake || false
        editEscalatingTimeout = a.escalatingTimeout || 60
        editNote = a.note || ""
        editName = a.name || ""

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
        editMaxSnoozes = 1
        editSnoozeInterval = configManager.defaultSnooze
        editChallengeMode = "none"
        editMathDifficulty = 0
        editEscalatingWake = false
        editEscalatingTimeout = 60
        editNote = ""
        editName = ""
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
            "snoozeInterval": editSnoozeInterval === configManager.defaultSnooze ? 0 : editSnoozeInterval,
            "challengeMode": editChallengeMode,
            "mathDifficulty": editMathDifficulty,
            "escalatingWake": editEscalatingWake,
            "escalatingTimeout": editEscalatingTimeout,
            "note": editNote,
            "name": editName.trim()
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

        ColumnLayout {
            spacing: 4
            Layout.fillWidth: true
            Text {
                text: "Name (optional)"
                color: configManager.themeTextSecondary; font.pixelSize: 12
            }
            TextField {
                id: nameField
                Layout.fillWidth: true
                text: editName
                placeholderText: "e.g. Morning, Work, Wake up"
                color: configManager.themeTextPrimary; font.pixelSize: 14
                background: Rectangle {
                    color: Qt.rgba(1,1,1,0.05); radius: 8
                    border.color: Qt.rgba(1,1,1,0.12); border.width: 1
                }
                onTextChanged: editName = text
            }
        }

        RowLayout {
            spacing: 16

            // Time picker — style chosen in Settings (Wheels / Dual Clocks / Single Clock)
            ColumnLayout {
                spacing: 10
                Layout.alignment: Qt.AlignVCenter

                Loader {
                    id: pickerLoader
                    Layout.alignment: Qt.AlignHCenter
                    property int pickStyle: configManager.timePickerStyle
                    sourceComponent: pickStyle === 0 ? wheelPicker :
                                      pickStyle === 1 ? dualPicker : singlePicker
                    onLoaded: {
                        item.hour = editHour
                        item.minute = editMinute
                    }
                    Connections {
                        target: pickerLoader.item
                        function onTimeChanged(h, m) { editHour = h; editMinute = m }
                    }
                }

                Text {
                    text: ("00" + (editHour % 12 || 12)).slice(-2) + ":" + ("00" + editMinute).slice(-2)
                    color: configManager.themeTextPrimary
                    font.pixelSize: 28; font.bold: true
                    Layout.alignment: Qt.AlignHCenter
                }

                RowLayout {
                    spacing: 6
                    Layout.alignment: Qt.AlignHCenter
                    GlassButton {
                        text: "AM"
                        pixelSize: 12; implicitWidth: 46; implicitHeight: 28; radius: 6
                        property bool isPM: editHour >= 12
                        baseColor: isPM ? Qt.rgba(1, 1, 1, 0.1) : Qt.rgba(0.2, 0.6, 1, 0.3)
                        onClicked: { if (isPM) editHour -= 12 }
                    }
                    GlassButton {
                        text: "PM"
                        pixelSize: 12; implicitWidth: 46; implicitHeight: 28; radius: 6
                        property bool isPM: editHour >= 12
                        baseColor: isPM ? Qt.rgba(0.2, 0.6, 1, 0.3) : Qt.rgba(1, 1, 1, 0.1)
                        onClicked: { if (!isPM) editHour += 12 }
                    }
                }
            }

            Item { width: 8 }

            ColumnLayout {
                spacing: 6
                Layout.alignment: Qt.AlignVCenter

                RowLayout {
                    spacing: 8
                    ColumnLayout {
                        spacing: 2
                        Label { text: "Hour"; color: configManager.themeTextSecondary; font.pixelSize: 12 }
                        TextField {
                            text: {
                                var h = editHour % 12
                                return (h === 0 ? 12 : h).toString()
                            }
                            onTextChanged: {
                                var v = parseInt(text)
                                if (isNaN(v) || v < 1) v = 12
                                if (v > 12) v = 12
                                var isPM = editHour >= 12
                                editHour = isPM ? (v % 12) + 12 : v % 12
                            }
                            validator: IntValidator { bottom: 1; top: 12 }
                            color: configManager.themeTextPrimary
                            horizontalAlignment: TextInput.AlignHCenter
                            inputMethodHints: Qt.ImhDigitsOnly
                            implicitWidth: 50; implicitHeight: 30
                            background: Rectangle { color: Qt.rgba(1,1,1,0.1); radius: 6; border.color: Qt.rgba(1,1,1,0.2) }
                            topPadding: 4; bottomPadding: 4
                        }
                    }
                    ColumnLayout {
                        spacing: 2
                        Label { text: "Min"; color: configManager.themeTextSecondary; font.pixelSize: 12 }
                        TextField {
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
                            implicitWidth: 50; implicitHeight: 30
                            background: Rectangle { color: Qt.rgba(1,1,1,0.1); radius: 6; border.color: Qt.rgba(1,1,1,0.2) }
                            topPadding: 4; bottomPadding: 4
                        }
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
                model: {
                    var t = configManager.availableTones()
                    t.unshift("none")
                    return t
                }
                onActivated: {
                    editSoundFile = currentIndex === 0 ? "" : currentText
                }
                Component.onCompleted: {
                    currentIndex = editSoundFile === "" ? 0 : Math.max(0, find(editSoundFile))
                }
                Connections {
                    target: configManager
                    function onConfigChanged() {
                        var idx = soundCmb.currentIndex
                        var prev = soundCmb.currentText
                        var t = configManager.availableTones()
                        t.unshift("none")
                        soundCmb.model = t
                        soundCmb.currentIndex = prev === "" || idx === 0 ? 0 : Math.max(0, soundCmb.find(prev))
                    }
                }
            }

            GlassButton {
                text: root.soundPreviewing ? "■" : "▶"
                pixelSize: 11
                implicitWidth: 32; implicitHeight: 28
                radius: 8
                onClicked: {
                    if (root.soundPreviewing) {
                        audioPlayer.stop()
                        root.soundPreviewing = false
                    } else if (soundCmb.currentIndex > 0) {
                        audioPlayer.stop()
                        audioPlayer.preview(soundCmb.currentText)
                        root.soundPreviewing = true
                    }
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
                    t.unshift("none")
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
                text: root.scPreviewing ? "■" : "▶"
                pixelSize: 11
                implicitWidth: 32; implicitHeight: 28; radius: 8
                onClicked: {
                    if (root.scPreviewing) {
                        audioPlayer.stop()
                        root.scPreviewing = false
                    } else if (scCmb.currentIndex > 0) {
                        audioPlayer.stop()
                        audioPlayer.preview(scCmb.currentText)
                        root.scPreviewing = true
                    }
                }
            }
        }

        // --- Snooze ---
        GlassCard {
            id: snoozeCard
            Layout.fillWidth: true
            cardColor: Qt.rgba(1, 1, 1, 0.04)
            Layout.preferredHeight: snoozeBody.implicitHeight + 32

            ColumnLayout {
                id: snoozeBody
                x: 16; width: parent.width - 32; y: 16
                spacing: 12
                RowLayout {
                    Layout.fillWidth: true
                    Text { text: "Snooze"; color: configManager.themeTextPrimary; font.pixelSize: 16; font.bold: true; Layout.alignment: Qt.AlignVCenter }
                    Item { Layout.fillWidth: true }
                    ToggleSwitch {
                        checked: editMaxSnoozes !== 0
                        onToggled: function(v) {
                            if (!v) editMaxSnoozes = 0
                            else if (editMaxSnoozes === 0) editMaxSnoozes = 1
                        }
                    }
                }

                ValueCtrl {
                    visible: editMaxSnoozes !== 0
                    label: "Max Snoozes"; value: editMaxSnoozes === -1 ? 99 : editMaxSnoozes
                    minVal: 1; maxVal: 20; step: 1; suffix: editMaxSnoozes === -1 ? " (∞)" : ""
                    onValueChanged: {
                        if (value >= 99) editMaxSnoozes = -1
                        else editMaxSnoozes = value
                    }
                }

                ValueCtrl {
                    visible: editMaxSnoozes !== 0
                    label: "Snooze Interval"; value: editSnoozeInterval
                    minVal: 1; maxVal: 60; step: 1; suffix: editSnoozeInterval === configManager.defaultSnooze ? " min (global)" : " min"
                    onValueChanged: editSnoozeInterval = value
                }
            }
        }

        // --- Escalating Wake ---
        GlassCard {
            id: escCard
            Layout.fillWidth: true
            cardColor: Qt.rgba(1, 1, 1, 0.04)
            Layout.preferredHeight: escBody.implicitHeight + 32

            ColumnLayout {
                id: escBody
                x: 16; width: parent.width - 32; y: 16
                spacing: 12
                RowLayout {
                    Layout.fillWidth: true
                    Text { text: "Escalating Wake"; color: configManager.themeTextPrimary; font.pixelSize: 16; font.bold: true; Layout.alignment: Qt.AlignVCenter }
                    Item { Layout.fillWidth: true }
                    ToggleSwitch {
                        checked: editEscalatingWake
                        onToggled: function(v) { editEscalatingWake = v }
                    }
                }

                ValueCtrl {
                    visible: editEscalatingWake
                    label: "Force challenge after"; value: editEscalatingTimeout
                    minVal: 15; maxVal: 300; step: 15; suffix: "s"
                    onValueChanged: editEscalatingTimeout = value
                }
            }
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

    Component {
        id: wheelPicker
        WheelTimePicker {
            hour: editHour; minute: editMinute
        }
    }
    Component {
        id: dualPicker
        DualClockPicker {
            hour: editHour; minute: editMinute
        }
    }
    Component {
        id: singlePicker
        UnifiedClockPicker {
            hour: editHour; minute: editMinute
        }
    }
}

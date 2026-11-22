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
    property int editAutoStop: 120
    property bool editEnableSound: true
    property bool editEnableCommand: false
    property string editCommand: ""
    property bool editEnabled: true
    property string editSoundFile: ""
    property bool soundPreviewing: false
    property string editName: ""

    Connections {
        target: audioPlayer
        function onIsPlayingChanged() {
            if (!audioPlayer.isPlaying) {
                soundPreviewing = false
            }
        }
    }

    // The Classic fields are typed TextFields: writing text would break a
    // binding, so they are pushed from these properties instead. The wheel
    // keeps its own live bindings and does not need this.
    function classicDisplayHour() {
        if (configManager.timeFormat !== "12h")
            return editHour
        const h = editHour % 12
        return h === 0 ? 12 : h
    }

    function syncClassicFields() {
        if (!classicHourField || !classicMinuteField || !classicAutoField || !classicAmPm)
            return
        classicHourField.text = ("00" + classicDisplayHour()).slice(-2)
        classicMinuteField.text = ("00" + editMinute).slice(-2)
        classicAutoField.text = String(editAutoStop)
        classicAmPm.currentIndex = editHour >= 12 ? 1 : 0
    }

    function commitClassicHour() {
        let v = parseInt(classicHourField.text, 10)
        if (isNaN(v))
            v = configManager.timeFormat === "12h" ? 12 : 0
        if (configManager.timeFormat === "12h") {
            const h12 = Math.max(1, Math.min(12, v)) % 12
            v = h12 + (classicAmPm.currentIndex === 1 ? 12 : 0)
        } else {
            v = Math.max(0, Math.min(23, v))
        }
        editHour = v
        syncClassicFields()
    }

    Connections {
        target: root
        function onEditHourChanged() { syncClassicFields() }
        function onEditMinuteChanged() { syncClassicFields() }
        function onEditAutoStopChanged() { syncClassicFields() }
    }
    Connections {
        target: configManager
        function onConfigChanged() { syncClassicFields() }
    }

    function loadAlarm(index) {
        // alarmManager.alarms (QVariantList) was replaced by the alarm model;
        // get() returns the same QVariantMap shape per row, undefined if the
        // row does not exist (replaces the old alarms.length bounds check).
        var a = alarmManager.model.get(index)
        if (!a) return

        editIndex = index
        editHour = a.hour
        editMinute = a.minute
        editDays = a.days.slice()
        editWakeMode = a.wakeMode
        editAutoStop = a.autoStopDuration
        editEnableSound = a.enableSound
        editEnableCommand = a.enableCommand
        editCommand = a.command
        editEnabled = a.enabled
        editSoundFile = a.soundFile
        editName = a.name || ""

        hasSelection = true
        Qt.callLater(function() {
            soundCmb.currentIndex = Math.max(0, soundCmb.find(editSoundFile))
        })
    }

    function clearSelection() {
        hasSelection = false
        editIndex = -1
        editHour = 7
        editMinute = 0
        editDays = [false, false, false, false, false, false, false]
        editWakeMode = "mem"
        editAutoStop = 120
        editEnabled = true
        editSoundFile = ""
        editEnableSound = true
        editEnableCommand = false
        editCommand = ""
        editName = ""
    }

    function commitAlarm() {
        if (!hasSelection) return

        var a = {
            "hour": editHour,
            "minute": editMinute,
            "days": editDays.slice(),
            "wakeMode": editWakeMode,
            "autoStopDuration": editAutoStop,
            "enableSound": editEnableSound,
            "enableCommand": editEnableCommand,
            "command": editCommand,
            "enabled": editEnabled,
            "soundFile": editSoundFile,
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
            text: editIndex >= 0 ? (configManager.emojiLevel, configManager.emoji("✏️", 2)) + qsTr("Edit Alarm") : (configManager.emojiLevel, configManager.emoji("➕", 2)) + qsTr("New Alarm")
            color: configManager.themeTextPrimary
            font.pixelSize: 22
            font.bold: true
        }

        ColumnLayout {
            spacing: 4
            Layout.fillWidth: true
            Text {
                text: (configManager.emojiLevel, configManager.emoji("🏷️", 2)) + qsTr("Name (optional)")
                color: configManager.themeTextSecondary; font.pixelSize: 12
            }
            TextField {
                id: nameField
                Layout.fillWidth: true
                text: editName
                placeholderText: qsTr("e.g. Morning, Work, Wake up")
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
            Layout.alignment: Qt.AlignHCenter

            WheelTimePicker {
                id: wheelPicker
                Layout.alignment: Qt.AlignHCenter
                visible: configManager.timePickerMode === "wheel"
                hour: editHour
                minute: editMinute
                onTimeChanged: function(h, m) { editHour = h; editMinute = m }
            }

            // Classic mode (Settings -> "Time picker style"): one compact row
            // of typed hour, minute and Auto Stop fields instead of stacked
            // arrow controls. Values are still the same editHour/editMinute/
            // editAutoStop properties, so switching modes never loses an edit.
            RowLayout {
                visible: configManager.timePickerMode === "spin"
                spacing: 8
                Layout.alignment: Qt.AlignVCenter
                Component.onCompleted: syncClassicFields()

                ColumnLayout {
                    spacing: 3
                    Label {
                        text: (configManager.emojiLevel, configManager.emoji("🕐", 2)) + qsTr("Hour")
                        color: configManager.themeTextSecondary; font.pixelSize: 12
                        Layout.alignment: Qt.AlignHCenter
                    }
                    TextField {
                        id: classicHourField
                        implicitWidth: 54; implicitHeight: 40
                        maximumLength: 2
                        validator: IntValidator {
                            bottom: configManager.timeFormat === "12h" ? 1 : 0
                            top: configManager.timeFormat === "12h" ? 12 : 23
                        }
                        inputMethodHints: Qt.ImhDigitsOnly
                        horizontalAlignment: TextInput.AlignHCenter
                        color: configManager.themeTextPrimary; font.pixelSize: 16
                        background: Rectangle {
                            color: Qt.rgba(1, 1, 1, 0.06); radius: 9
                            border.color: Qt.rgba(1, 1, 1, 0.12); border.width: 1
                        }
                        onEditingFinished: commitClassicHour()
                    }
                }

                Text {
                    text: ":"
                    visible: configManager.timeFormat !== "12h"
                    color: configManager.themeTextSecondary; font.pixelSize: 18
                    Layout.alignment: Qt.AlignVCenter; Layout.bottomMargin: 10
                }

                ColumnLayout {
                    spacing: 3
                    Label {
                        text: (configManager.emojiLevel, configManager.emoji("🕑", 2)) + qsTr("Minute")
                        color: configManager.themeTextSecondary; font.pixelSize: 12
                        Layout.alignment: Qt.AlignHCenter
                    }
                    TextField {
                        id: classicMinuteField
                        implicitWidth: 54; implicitHeight: 40
                        maximumLength: 2
                        validator: IntValidator { bottom: 0; top: 59 }
                        inputMethodHints: Qt.ImhDigitsOnly
                        horizontalAlignment: TextInput.AlignHCenter
                        color: configManager.themeTextPrimary; font.pixelSize: 16
                        background: Rectangle {
                            color: Qt.rgba(1, 1, 1, 0.06); radius: 9
                            border.color: Qt.rgba(1, 1, 1, 0.12); border.width: 1
                        }
                        onEditingFinished: {
                            const v = parseInt(text, 10)
                            editMinute = isNaN(v) ? 0 : Math.max(0, Math.min(59, v))
                            syncClassicFields()
                        }
                    }
                }

                RoundedCombo {
                    id: classicAmPm
                    visible: configManager.timeFormat === "12h"
                    model: [qsTr("AM"), qsTr("PM")]
                    Layout.preferredWidth: 68
                    Layout.bottomMargin: 10
                    currentIndex: editHour >= 12 ? 1 : 0
                    onActivated: {
                        const h = parseInt(classicHourField.text, 10)
                        const h12 = (isNaN(h) ? 12 : Math.max(1, Math.min(12, h))) % 12
                        editHour = h12 + (currentIndex === 1 ? 12 : 0)
                        syncClassicFields()
                    }
                }

                Text {
                    text: "·"
                    color: configManager.themeTextSecondary; font.pixelSize: 18
                    Layout.alignment: Qt.AlignVCenter; Layout.bottomMargin: 10
                }

                ColumnLayout {
                    spacing: 3
                    Label {
                        text: (configManager.emojiLevel, configManager.emoji("⏱️", 2)) + qsTr("Auto Stop")
                        color: configManager.themeTextSecondary; font.pixelSize: 12
                        Layout.alignment: Qt.AlignHCenter
                    }
                    RowLayout {
                        spacing: 4
                        TextField {
                            id: classicAutoField
                            implicitWidth: 62; implicitHeight: 40
                            maximumLength: 3
                            validator: IntValidator { bottom: 10; top: 600 }
                            inputMethodHints: Qt.ImhDigitsOnly
                            horizontalAlignment: TextInput.AlignHCenter
                            color: configManager.themeTextPrimary; font.pixelSize: 16
                            background: Rectangle {
                                color: Qt.rgba(1, 1, 1, 0.06); radius: 9
                                border.color: Qt.rgba(1, 1, 1, 0.12); border.width: 1
                            }
                            onEditingFinished: {
                                const v = parseInt(text, 10)
                                editAutoStop = isNaN(v) ? 120 : Math.max(10, Math.min(600, v))
                                syncClassicFields()
                            }
                        }
                        Text {
                            text: "s"
                            color: configManager.themeTextSecondary; font.pixelSize: 12
                            Layout.bottomMargin: 10
                        }
                    }
                }
            }
        }

        GlassCheckBox {
            id: daysCheck
            labelText: (configManager.emojiLevel, configManager.emoji("📅", 2)) + qsTr("Set Days")
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
                    model: [qsTr("Mon"), qsTr("Tue"), qsTr("Wed"), qsTr("Thu"), qsTr("Fri"), qsTr("Sat"), qsTr("Sun")]
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

        ValueCtrl {
            visible: configManager.timePickerMode === "wheel"
            label: (configManager.emojiLevel, configManager.emoji("⏱️", 2)) + qsTr("Auto Stop")
            value: editAutoStop; minVal: 10; maxVal: 600; step: 10; suffix: "s"
            onValueChanged: editAutoStop = value
        }

        GlassCheckBox {
            labelText: (configManager.emojiLevel, configManager.emoji("🔔", 2)) + qsTr("Enable Sound")
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
            labelText: (configManager.emojiLevel, configManager.emoji("💻", 2)) + qsTr("Enable Command")
            checked: editEnableCommand
            onCheckedChanged: editEnableCommand = checked
        }

        TextField {
            visible: editEnableCommand
            Layout.fillWidth: true
            placeholderText: qsTr("e.g. notify-send 'Wake up!'")
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

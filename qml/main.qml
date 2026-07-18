import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import Qt5Compat.GraphicalEffects
import GlassAlarm

Window {
    id: root
    visible: true
    width: 960
    height: 660
    minimumWidth: 720
    minimumHeight: 540
    title: "Lumalarm"

    flags: Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint
    color: "transparent"

    property int countdownValue: -1
    property string currentMode: "alarms"
    property bool hasNextAlarm: false

    function checkNextAlarm() {
        hasNextAlarm = scheduler.secondsUntilNextAlarm() > 0
    }
    onCountdownValueChanged: checkNextAlarm()
    Connections { target: alarmManager; function onAlarmListChanged() { root.checkNextAlarm() } }
    Connections {
        target: configManager
        function onConfigChanged() {
            root.checkNextAlarm()
            bgLayer.layer.enabled = false
            bgLayer.layer.enabled = configManager.themeBlur > 0
        }
    }
    Component.onCompleted: checkNextAlarm()

    Rectangle {
        id: windowRoot
        anchors.fill: parent
        radius: 18
        color: "transparent"
        clip: true

        Rectangle {
            id: bgLayer
            anchors.fill: parent
            color: configManager.themeBg
            opacity: configManager.themeOpacity

            Rectangle {
                anchors.fill: parent
                gradient: Gradient {
                    orientation: Gradient.Horizontal
                    GradientStop { position: 0.0; color: Qt.rgba(1, 1, 1, 0.04) }
                    GradientStop { position: 0.3; color: "transparent" }
                    GradientStop { position: 0.7; color: "transparent" }
                    GradientStop { position: 1.0; color: Qt.rgba(0, 0, 0, 0.08) }
                }
            }

            Rectangle {
                anchors.fill: parent
                gradient: Gradient {
                    orientation: Gradient.Vertical
                    GradientStop { position: 0.0; color: Qt.rgba(1, 1, 1, 0.03) }
                    GradientStop { position: 0.5; color: "transparent" }
                    GradientStop { position: 1.0; color: Qt.rgba(0, 0, 0, 0.06) }
                }
            }

            Rectangle {
                anchors.fill: parent
                border.color: configManager.themeAccent
                border.width: 1
                color: "transparent"
                opacity: 0.12
            }

            layer.enabled: configManager.themeBlur > 0
            layer.effect: FastBlur {
                radius: configManager.themeBlur
                transparentBorder: false
            }
        }

        Rectangle {
            anchors.fill: parent
            color: "transparent"
            border.color: Qt.rgba(1, 1, 1, 0.06)
            border.width: 1
        }

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 1
            spacing: 0

            Rectangle {
                Layout.fillWidth: true; Layout.preferredHeight: 54
                color: "transparent"

                RowLayout {
                    anchors.fill: parent; anchors.leftMargin: 20; anchors.rightMargin: 10; spacing: 12
                    Text { text: "Luma"; color: configManager.themeTextSecondary; font.pixelSize: 22; font.bold: true; Layout.alignment: Qt.AlignVCenter }
                    Text { text: "larm"; color: configManager.themeTextPrimary; font.pixelSize: 22; font.bold: true; Layout.alignment: Qt.AlignVCenter }
                    Item { Layout.fillWidth: true }
                    Text {
                        text: {
                            if (currentMode !== "alarms") return ""
                            var s = root.countdownValue
                            if (s < 0) return ""
                            var h = Math.floor(s / 3600), m = Math.floor((s % 3600) / 60), sec = s % 60
                            return ("00" + h).slice(-2) + "h " + ("00" + m).slice(-2) + "m " + ("00" + sec).slice(-2) + "s"
                        }
                        color: configManager.themeTextSecondary; font.pixelSize: 14; font.bold: true
                        Layout.alignment: Qt.AlignVCenter
                    }
                    GlassButton {
                        text: "\u25A0 Stop"; pixelSize: 11; implicitWidth: 70; implicitHeight: 28; buttonRadius: 8
                        visible: audioPlayer.isPlaying
                        baseColor: Qt.rgba(1, 0.2, 0.2, 0.2); hoverColor: Qt.rgba(1, 0.2, 0.2, 0.35)
                        Layout.alignment: Qt.AlignVCenter
                        onClicked: audioPlayer.stop()
                    }
                    GlassButton {
                        text: "\u2715"; pixelSize: 16; implicitWidth: 40; implicitHeight: 40; buttonRadius: 20
                        baseColor: Qt.rgba(1, 1, 1, 0.06); hoverColor: Qt.rgba(1, 0.2, 0.2, 0.25)
                        Layout.alignment: Qt.AlignVCenter
                        onClicked: Qt.quit()
                    }
                }

                MouseArea {
                    anchors.top: parent.top; anchors.left: parent.left
                    anchors.bottom: parent.bottom; anchors.right: closeBtnSpacer.left
                    property real lx: 0; property real ly: 0
                    onPressed: { lx = mouseX; ly = mouseY }
                    onMouseXChanged: root.x += mouseX - lx
                    onMouseYChanged: root.y += mouseY - ly
                }

                Item { id: closeBtnSpacer; width: 1; height: 1; visible: false }
            }

            Rectangle {
                Layout.fillWidth: true; Layout.preferredHeight: 44; color: "transparent"
                Row {
                    anchors.centerIn: parent; spacing: 8
                    Repeater {
                        model: [
                            {label: "Alarms", mode: "alarms", icon: "🔔"},
                            {label: "Timer", mode: "timer", icon: "⏱"},
                            {label: "Stopwatch", mode: "stopwatch", icon: "⏱"},
                            {label: "Sounds", mode: "sounds", icon: "🎵"},
                            {label: "Settings", mode: "settings", icon: "⚙"}
                        ]
                        Rectangle {
                            id: tp
                            property bool isActive: currentMode === modelData.mode
                            width: tr.implicitWidth + 32; height: 36; radius: 18
                            color: isActive ? Qt.rgba(1, 1, 1, 0.1) : "transparent"
                            border.color: isActive ? Qt.rgba(1, 1, 1, 0.15) : "transparent"
                            border.width: 1
                            Behavior on color { ColorAnimation { duration: 150 } }

                            Row {
                                id: tr; anchors.centerIn: parent; spacing: 6
                                Text { text: modelData.icon; color: tp.isActive ? configManager.themeTextPrimary : configManager.themeTextSecondary; font.pixelSize: 16 }
                                Text { text: modelData.label; color: tp.isActive ? configManager.themeTextPrimary : configManager.themeTextSecondary; font.pixelSize: 16; font.bold: tp.isActive }
                            }

                            MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: currentMode = modelData.mode }
                        }
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true; Layout.preferredHeight: 1
                color: Qt.rgba(1, 1, 1, 0.04)
            }

            RowLayout {
                Layout.fillWidth: true; Layout.fillHeight: true; spacing: 0

                Rectangle {
                    Layout.preferredWidth: 300; Layout.minimumWidth: 260; Layout.fillHeight: true
                    color: "transparent"; visible: currentMode === "alarms"

                    AlarmListView {
                        id: alarmList
                        anchors.fill: parent; anchors.margins: 14
                        onEditAlarm: function(i) { alarmEditor.loadAlarm(i) }
                        onDeleteAlarm: function(i) { alarmManager.removeAlarm(i) }
                        onAddAlarm: function() { alarmEditor.clearSelection(); alarmEditor.hasSelection = true; alarmEditor.editIndex = -1 }
                    }
                }

                Rectangle {
                    Layout.preferredWidth: 1; Layout.fillHeight: true
                    color: Qt.rgba(1, 1, 1, 0.04); visible: currentMode === "alarms"
                }

                Rectangle {
                    Layout.fillWidth: true; Layout.fillHeight: true; color: "transparent"

                    ColumnLayout {
                        anchors.fill: parent; anchors.margins: 20; spacing: 16
                        visible: currentMode === "alarms"

                        Item {
                            Layout.fillWidth: true; Layout.fillHeight: true; clip: true
                            ScrollView {
                                anchors.fill: parent; contentWidth: availableWidth; clip: true
                                ScrollBar.vertical.policy: ScrollBar.AsNeeded
                                ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
                                AlarmEditor { id: alarmEditor; width: parent.width; height: implicitHeight }
                            }
                            Text {
                                anchors.centerIn: parent; visible: !alarmEditor.hasSelection
                                text: "Select or create an alarm"
                                color: configManager.themeTextSecondary; font.pixelSize: 14
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true; visible: alarmEditor.hasSelection; spacing: 8
                            Item { Layout.fillWidth: true }
                            GlassButton { text: "Cancel"; pixelSize: 13; onClicked: alarmEditor.clearSelection() }
                            GlassButton {
                                text: alarmEditor.editIndex >= 0 ? "Update" : "Create"; pixelSize: 13
                                baseColor: Qt.rgba(0.3, 0.6, 1, 0.2); hoverColor: Qt.rgba(0.3, 0.6, 1, 0.3)
                                onClicked: { alarmEditor.commitAlarm(); alarmEditor.clearSelection() }
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true; Layout.bottomMargin: 10
                            Text { text: "Wake:"; color: configManager.themeTextSecondary; font.pixelSize: 14; Layout.alignment: Qt.AlignVCenter }
                            RoundedCombo { id: wakeModeCombo; model: ["mem", "disk", "none"]; Layout.preferredWidth: 90 }
                            Item { Layout.fillWidth: true }
                            GlassButton {
                                id: armBtn
                                text: "Arm & Suspend"; pixelSize: 12
                                baseColor: root.hasNextAlarm ? Qt.rgba(0.3, 0.75, 0.95, 0.12) : Qt.rgba(1, 1, 1, 0.06)
                                hoverColor: root.hasNextAlarm ? Qt.rgba(0.3, 0.75, 0.95, 0.22) : Qt.rgba(1, 1, 1, 0.15)
                                borderColor: root.hasNextAlarm ? Qt.rgba(0.4, 0.8, 1, 0.3) : Qt.rgba(1, 1, 1, 0.1)
                                textColor: root.hasNextAlarm ? Qt.rgba(0.5, 0.85, 1, 1) : configManager.themeTextSecondary
                                enabled: root.hasNextAlarm
                                opacity: root.hasNextAlarm ? 1.0 : 0.5
                                onClicked: {
                                    var s = scheduler.secondsUntilNextAlarm()
                                    if (s > 0) wakeManager.prepareWake(s, wakeModeCombo.currentText)
                                }
                            }
                        }
                    }

                    TimerPage { anchors.fill: parent; anchors.margins: 18; visible: currentMode === "timer" }
                    StopwatchPage { anchors.fill: parent; anchors.margins: 18; visible: currentMode === "stopwatch" }
                    SoundManager { anchors.fill: parent; anchors.margins: 18; visible: currentMode === "sounds" }
                    SettingsPage { anchors.fill: parent; anchors.margins: 18; visible: currentMode === "settings" }
                }
            }
        }
    }

    Rectangle {
        anchors.fill: parent
        color: "transparent"
        visible: ringingOverlay.ringing || ringingOverlay.wakeUpActive
        RingingOverlay {
            id: ringingOverlay
            onReTriggerAlarm: {
                var a = ringingOverlay.alarmData
                if (a.soundFile && a.soundFile !== "") {
                    audioPlayer.setBaseVolume(a.baseVolume || 20)
                    audioPlayer.setFadeDuration(a.fadeDuration || 15)
                    audioPlayer.play(a.soundFile)
                }
                ringingOverlay.ringing = true
            }
        }
    }

    Connections {
        target: scheduler
        function onAlarmTriggered(index) {
            var alarms = alarmManager.alarms
            if (index < 0 || index >= alarms.length) return
            var a = alarms[index]
            ringingOverlay.alarmHour = a.hour; ringingOverlay.alarmMinute = a.minute
            ringingOverlay.alarmMedia = a.soundFile
            ringingOverlay.alarmData = a
            ringingOverlay.ringing = true
        }
        function onCountdownUpdated(seconds) { root.countdownValue = seconds }
    }
}

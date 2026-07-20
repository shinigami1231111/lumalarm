import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import GlassAlarm

Window {
    id: root
    visible: true
    width: 960
    height: 660
    minimumWidth: 720
    minimumHeight: 540
    title: "Lumalarm"

    // Frameless + on-top. Native decorations are opaque and would break the
    // per-pixel-alpha (compositor blur) effect, so we never use them.
    flags: Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint
    color: "transparent"

    property int countdownValue: -1

    // True window-level transparency helpers.
    // In "compositor" blur mode the background is drawn with a REAL rgba alpha
    // (not QML opacity) so the Wayland/X11 compositor sees genuine per-pixel
    // transparency and can blur the desktop behind the window.
    // In "app" blur mode (no compositor blur available) we force the panel to
    // be nearly opaque to avoid transparency artifacts on X11/GNOME.
    property bool compositorBlur: themeManager.blur_mode === "compositor"
    property real bgAlpha: compositorBlur ? themeManager.card_opacity : Math.max(themeManager.card_opacity, 0.92)
    property color bgBase: themeManager.background_color
    property color accentColor: themeManager.accent_color
    property color bgWithAlpha: Qt.alpha(bgBase, bgAlpha)
    property int cornerRadius: themeManager.corner_radius


    property string currentMode: "alarms"
    property bool hasNextAlarm: false

    function checkNextAlarm() {
        hasNextAlarm = scheduler.secondsUntilNextAlarm() > 0
    }
    onCountdownValueChanged: checkNextAlarm()
    Connections { target: alarmManager; function onAlarmListChanged() { root.checkNextAlarm(); armBtn.armed = false } }
    Connections {
        target: configManager
        function onConfigChanged() {
            root.checkNextAlarm()
        }
    }
    Component.onCompleted: checkNextAlarm()

    Rectangle {
        id: windowRoot
        anchors.fill: parent
        radius: root.cornerRadius
        color: "transparent"
        clip: false
        border.color: Qt.alpha(root.accentColor, 0.45)
        border.width: 1.5

        Rectangle {
            id: bgLayer
            anchors.fill: parent
            radius: root.cornerRadius
            clip: true
            // Real per-pixel alpha — NOT QML opacity — so the compositor's
            // own blur works on genuinely transparent window pixels.
            color: root.bgWithAlpha
            opacity: 1

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
                border.color: themeManager.accent_color
                border.width: 1
                color: "transparent"
                opacity: 0.12
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
                    Text { text: "Lumalarm"; color: configManager.themeTextPrimary; font.pixelSize: 22; font.bold: true; Layout.alignment: Qt.AlignVCenter }
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
                Layout.fillWidth: true; Layout.preferredHeight: 52; color: "transparent"
                SegmentedControl {
                    anchors.centerIn: parent
                    items: [
                        {label: "Alarms", mode: "alarms", icon: "alarms"},
                        {label: "Timer", mode: "timer", icon: "timer"},
                        {label: "Stopwatch", mode: "stopwatch", icon: "stopwatch"},
                        {label: "Sounds", mode: "sounds", icon: "sounds"},
                        {label: "Settings", mode: "settings", icon: "settings"}
                    ]
                    current: currentMode
                    onSelected: function(m) { currentMode = m }
                }
            }

            Rectangle {
                Layout.fillWidth: true; Layout.preferredHeight: 1
                color: Qt.rgba(1, 1, 1, 0.04)
            }

            RowLayout {
                Layout.fillWidth: true; Layout.fillHeight: true; spacing: 0

                Rectangle {
                    Layout.preferredWidth: 264; Layout.minimumWidth: 240; Layout.fillHeight: true
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
                                property bool armed: false
                                text: !root.hasNextAlarm ? "No alarm armed" : (armed ? "Armed ✓" : "Arm Wake")
                                pixelSize: 12
                                baseColor: !root.hasNextAlarm ? Qt.rgba(1, 1, 1, 0.05)
                                            : (armed ? Qt.rgba(0.3, 0.85, 0.4, 0.18) : Qt.rgba(0.3, 0.75, 0.95, 0.12))
                                hoverColor: !root.hasNextAlarm ? Qt.rgba(1, 1, 1, 0.1)
                                            : (armed ? Qt.rgba(0.3, 0.85, 0.4, 0.28) : Qt.rgba(0.3, 0.75, 0.95, 0.22))
                                borderColor: !root.hasNextAlarm ? Qt.rgba(1, 1, 1, 0.08)
                                            : (armed ? Qt.rgba(0.3, 0.9, 0.4, 0.35) : Qt.rgba(0.4, 0.8, 1, 0.3))
                                textColor: !root.hasNextAlarm ? configManager.themeTextSecondary
                                            : (armed ? Qt.rgba(0.5, 0.95, 0.6, 1) : Qt.rgba(0.5, 0.85, 1, 1))
                                enabled: root.hasNextAlarm
                                opacity: root.hasNextAlarm ? 1.0 : 0.5
                                onClicked: {
                                    if (!root.hasNextAlarm) return
                                    var s = scheduler.secondsUntilNextAlarm()
                                    if (s <= 0) return
                                    wakeManager.prepareWake(s, wakeModeCombo.currentText)
                                    armed = true
                                    armResetTimer.restart()
                                }
                                Timer {
                                    id: armResetTimer
                                    interval: 3000; repeat: false
                                    onTriggered: armed = false
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
        }
    }

    Connections {
        target: scheduler
        function onAlarmTriggered(index) {
            ringingOverlay.cancelSnooze()
            var alarms = alarmManager.alarms
            if (index < 0 || index >= alarms.length) return
            var a = alarms[index]
            ringingOverlay.alarmIndex = index
            ringingOverlay.alarmId = a.id || ""
            ringingOverlay.alarmHour = a.hour; ringingOverlay.alarmMinute = a.minute
            ringingOverlay.alarmMedia = a.soundFile
            ringingOverlay.alarmData = a
            ringingOverlay.ringing = true
        }
        function onCountdownUpdated(seconds) { root.countdownValue = seconds }
        function onSoundscapeStarting(index) {
            var alarms = alarmManager.alarms
            if (index < 0 || index >= alarms.length) return
            var a = alarms[index]
            if (a.soundscape && a.soundscape !== "") {
                audioPlayer.playSoundscape(a.soundscape, 5)
            }
        }
    }
}

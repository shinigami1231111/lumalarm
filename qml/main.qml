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

    // Clip the window content to the rounded rectangle shape.
    // Without this, the square window surface shows behind the rounded bgLayer.
    Component.onCompleted: {
        var contentItem = root.contentItem
        if (contentItem) {
            contentItem.clip = true
        }
        checkNextAlarm()
    }
    property int countdownValue: -1

    // When tray is enabled, closing the window hides it instead of destroying it.
    onClosing: function(close) {
        if (themeManager.show_tray_icon) {
            close.accepted = false
            root.visible = false
        }
        // If tray is disabled, allow the close (accepted = true by default) → app exits.
    }

    // True window-level transparency helpers.
    // In "compositor" blur mode the background is drawn with a REAL rgba alpha
    // (not QML opacity) so the Wayland/X11 compositor sees genuine per-pixel
    // transparency and can blur the desktop behind the window.
    // Blur and opacity are independent.
    // Opacity controls the background alpha.
    // Blur mode toggles compositor blur on/off behind the transparent panel.
    property bool blurEnabled: themeManager.blur_mode === "compositor"
    property real bgAlpha: configManager.themeOpacity
    property color bgBase: themeManager.background_color
    property color accentColor: themeManager.accent_color
    property color bgWithAlpha: Qt.alpha(bgBase, bgAlpha)
    property int cornerRadius: themeManager.corner_radius

    // Background presets: each defines gradient stops for the glassmorphism effect
    property var backgroundPresets: ({
        "Dark Glass": {
            h: [{pos:0, color:Qt.rgba(1,1,1,0.04)}, {pos:0.3, color:"transparent"}, {pos:0.7, color:"transparent"}, {pos:1, color:Qt.rgba(0,0,0,0.08)}],
            v: [{pos:0, color:Qt.rgba(1,1,1,0.03)}, {pos:0.5, color:"transparent"}, {pos:1, color:Qt.rgba(0,0,0,0.06)}]
        },
        "Night Sky": {
            h: [{pos:0, color:Qt.rgba(0.15,0.1,0.35,0.12)}, {pos:0.33, color:Qt.rgba(0.05,0.05,0.2,0.04)}, {pos:0.67, color:Qt.rgba(0.08,0.04,0.25,0.06)}, {pos:1, color:Qt.rgba(0.1,0.05,0.3,0.1)}],
            v: [{pos:0, color:Qt.rgba(0.2,0.15,0.4,0.08)}, {pos:0.5, color:"transparent"}, {pos:1, color:Qt.rgba(0.05,0.02,0.15,0.12)}]
        },
        "Soft Aurora": {
            h: [{pos:0, color:Qt.rgba(0.1,0.3,0.2,0.08)}, {pos:0.33, color:Qt.rgba(0.05,0.15,0.25,0.04)}, {pos:0.67, color:Qt.rgba(0.15,0.1,0.3,0.04)}, {pos:1, color:Qt.rgba(0.05,0.2,0.15,0.08)}],
            v: [{pos:0, color:Qt.rgba(0.1,0.25,0.2,0.06)}, {pos:0.5, color:"transparent"}, {pos:1, color:Qt.rgba(0.08,0.15,0.25,0.1)}]
        },
        "Minimal Geometric": {
            h: [{pos:0, color:Qt.rgba(1,1,1,0.02)}, {pos:0.25, color:Qt.rgba(1,1,1,0.04)}, {pos:0.5, color:Qt.rgba(1,1,1,0.02)}, {pos:1, color:Qt.rgba(1,1,1,0.03)}],
            v: [{pos:0, color:Qt.rgba(1,1,1,0.02)}, {pos:0.5, color:Qt.rgba(1,1,1,0.03)}, {pos:1, color:Qt.rgba(1,1,1,0.02)}]
        },
        "Atmospheric Dark": {
            h: [{pos:0, color:Qt.rgba(0.15,0.08,0.05,0.1)}, {pos:0.33, color:Qt.rgba(0.1,0.06,0.04,0.06)}, {pos:0.67, color:Qt.rgba(0.08,0.05,0.1,0.06)}, {pos:1, color:Qt.rgba(0.08,0.05,0.12,0.08)}],
            v: [{pos:0, color:Qt.rgba(0.12,0.06,0.04,0.06)}, {pos:0.5, color:"transparent"}, {pos:1, color:Qt.rgba(0.06,0.04,0.1,0.1)}]
        },
        "Subtle Gradient": {
            h: [{pos:0, color:Qt.rgba(0.1,0.1,0.2,0.06)}, {pos:0.33, color:Qt.rgba(0.12,0.1,0.22,0.04)}, {pos:0.67, color:Qt.rgba(0.14,0.1,0.24,0.04)}, {pos:1, color:Qt.rgba(0.15,0.1,0.25,0.06)}],
            v: [{pos:0, color:Qt.rgba(0.12,0.08,0.2,0.05)}, {pos:0.5, color:"transparent"}, {pos:1, color:Qt.rgba(0.08,0.06,0.15,0.08)}]
        },
        "Abstract Light": {
            h: [{pos:0, color:Qt.rgba(0.2,0.15,0.1,0.06)}, {pos:0.33, color:Qt.rgba(0.1,0.15,0.2,0.04)}, {pos:0.67, color:Qt.rgba(0.15,0.1,0.18,0.04)}, {pos:1, color:Qt.rgba(0.18,0.12,0.08,0.06)}],
            v: [{pos:0, color:Qt.rgba(0.18,0.14,0.1,0.04)}, {pos:0.5, color:Qt.rgba(0.12,0.1,0.14,0.04)}, {pos:1, color:Qt.rgba(0.1,0.08,0.15,0.08)}]
        }
    })
    property var currentBgPreset: backgroundPresets[themeManager.background_preset] || backgroundPresets["Dark Glass"]


    property string currentMode: "alarms"
    property bool hasNextAlarm: false

    function checkNextAlarm() {
        hasNextAlarm = scheduler.secondsUntilNextAlarm() > 0
    }
    onCountdownValueChanged: checkNextAlarm()
    Connections { target: alarmManager; function onAlarmsChanged() { root.checkNextAlarm() } }
    Connections {
        target: configManager
        function onConfigChanged() {
            root.checkNextAlarm()
        }
    }

    property bool isMaximized: visibility === Window.Maximized

    Rectangle {
        id: windowRoot
        anchors.fill: parent
        radius: root.cornerRadius
        color: root.bgWithAlpha
        clip: true
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
                    GradientStop { position: currentBgPreset.h[0].pos; color: currentBgPreset.h[0].color }
                    GradientStop { position: currentBgPreset.h[1].pos; color: currentBgPreset.h[1].color }
                    GradientStop { position: currentBgPreset.h[2].pos; color: currentBgPreset.h[2].color }
                    GradientStop { position: currentBgPreset.h[3].pos; color: currentBgPreset.h[3].color }
                }
            }

            Rectangle {
                anchors.fill: parent
                gradient: Gradient {
                    orientation: Gradient.Vertical
                    GradientStop { position: currentBgPreset.v[0].pos; color: currentBgPreset.v[0].color }
                    GradientStop { position: currentBgPreset.v[1].pos; color: currentBgPreset.v[1].color }
                    GradientStop { position: currentBgPreset.v[2].pos; color: currentBgPreset.v[2].color }
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
                        id: minimizeBtn
                        visible: themeManager.show_minimize_button
                        text: "\u2014"; pixelSize: 16; implicitWidth: 40; implicitHeight: 40; buttonRadius: 20
                        baseColor: Qt.rgba(1, 1, 1, 0.06); hoverColor: Qt.rgba(1, 1, 1, 0.14)
                        Layout.alignment: Qt.AlignVCenter
                        onClicked: root.showMinimized()
                    }
                    GlassButton {
                        id: maximizeBtn
                        visible: themeManager.show_maximize_button
                        text: root.isMaximized ? "\u2752" : "\u25A1"; pixelSize: 16; implicitWidth: 40; implicitHeight: 40; buttonRadius: 20
                        baseColor: Qt.rgba(1, 1, 1, 0.06); hoverColor: Qt.rgba(1, 1, 1, 0.14)
                        Layout.alignment: Qt.AlignVCenter
                        onClicked: {
                            if (root.isMaximized) root.showNormal(); else root.showMaximized()
                        }
                    }
                    GlassButton {
                        id: closeBtn
                        visible: themeManager.show_close_button
                        text: "\u2715"; pixelSize: 16; implicitWidth: 40; implicitHeight: 40; buttonRadius: 20
                        baseColor: Qt.rgba(1, 1, 1, 0.06); hoverColor: Qt.rgba(1, 0.2, 0.2, 0.25)
                        Layout.alignment: Qt.AlignVCenter
                        onClicked: {
                            if (themeManager.show_tray_icon)
                                root.visible = false
                            else
                                Qt.quit()
                        }
                    }
                }

                MouseArea {
                    anchors.top: parent.top; anchors.left: parent.left
                    anchors.bottom: parent.bottom; anchors.right: parent.right
                    property real lx: 0; property real ly: 0
                    onPressed: { lx = mouseX; ly = mouseY; if (root.isMaximized) root.showNormal() }
                    onMouseXChanged: root.x += mouseX - lx
                    onMouseYChanged: root.y += mouseY - ly
                    onDoubleClicked: {
                        if (root.isMaximized) root.showNormal(); else root.showMaximized()
                    }
                    z: -1
                }
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

                        ColumnLayout {
                            Layout.fillWidth: true; Layout.bottomMargin: 10; spacing: 8

                            RowLayout {
                                Layout.fillWidth: true; spacing: 8
                                Item { Layout.fillWidth: true }
                                GlassButton {
                                    id: armBtn
                                    text: {
                                        if (!root.hasNextAlarm) return "No alarm armed"
                                        var ws = scheduler.wakeState
                                        if (ws === "arming") return "Arming..."
                                        if (ws === "armed") return "Armed ✓"
                                        if (ws === "failed") return "Arm FAILED"
                                        return "Arm Wake"
                                    }
                                    pixelSize: 12
                                    baseColor: {
                                        if (!root.hasNextAlarm) return Qt.rgba(1, 1, 1, 0.05)
                                        var ws = scheduler.wakeState
                                        if (ws === "armed") return Qt.rgba(0.3, 0.85, 0.4, 0.18)
                                        if (ws === "failed") return Qt.rgba(1, 0.3, 0.2, 0.18)
                                        return Qt.rgba(0.3, 0.75, 0.95, 0.12)
                                    }
                                    hoverColor: {
                                        if (!root.hasNextAlarm) return Qt.rgba(1, 1, 1, 0.1)
                                        var ws = scheduler.wakeState
                                        if (ws === "armed") return Qt.rgba(0.3, 0.85, 0.4, 0.28)
                                        if (ws === "failed") return Qt.rgba(1, 0.3, 0.2, 0.28)
                                        return Qt.rgba(0.3, 0.75, 0.95, 0.22)
                                    }
                                    borderColor: {
                                        if (!root.hasNextAlarm) return Qt.rgba(1, 1, 1, 0.08)
                                        var ws = scheduler.wakeState
                                        if (ws === "armed") return Qt.rgba(0.3, 0.9, 0.4, 0.35)
                                        if (ws === "failed") return Qt.rgba(1, 0.3, 0.2, 0.35)
                                        return Qt.rgba(0.4, 0.8, 1, 0.3)
                                    }
                                    textColor: {
                                        if (!root.hasNextAlarm) return configManager.themeTextSecondary
                                        var ws = scheduler.wakeState
                                        if (ws === "armed") return Qt.rgba(0.5, 0.95, 0.6, 1)
                                        if (ws === "failed") return Qt.rgba(1, 0.5, 0.4, 1)
                                        return Qt.rgba(0.5, 0.85, 1, 1)
                                    }
                                    enabled: root.hasNextAlarm && (scheduler.wakeState === "idle" || scheduler.wakeState === "failed")
                                    opacity: root.hasNextAlarm ? 1.0 : 0.5
                                    onClicked: {
                                        if (!root.hasNextAlarm) return
                                        scheduler.armWake()
                                    }
                                }
                                GlassButton {
                                    id: poweroffBtn
                                    text: "Power Off & Wake"
                                    visible: scheduler.wakeState === "armed"
                                    enabled: scheduler.wakeState === "armed"
                                    pixelSize: 12
                                    baseColor: Qt.rgba(0.95, 0.35, 0.3, 0.2)
                                    hoverColor: Qt.rgba(0.95, 0.35, 0.3, 0.32)
                                    borderColor: Qt.rgba(1, 0.4, 0.35, 0.35)
                                    textColor: Qt.rgba(1, 0.55, 0.5, 1)
                                    onClicked: scheduler.powerOffMachine()
                                }
                            }

                            Text {
                                Layout.fillWidth: true; wrapMode: Text.Wrap
                                visible: scheduler.wakeState === "armed"
                                text: scheduler.pendingInfo + " — RTC alarm armed and verified; power off to wake the machine"
                                color: configManager.themeTextSecondary; font.pixelSize: 11
                            }
                            Text {
                                Layout.fillWidth: true; wrapMode: Text.Wrap
                                visible: scheduler.lastError !== ""
                                text: "Error: " + scheduler.lastError
                                color: "#ff6b6b"; font.pixelSize: 11
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

    // Night mode veil (STG-5.6): above all pages, below the ringing overlay, so
    // the alarm UI is never dimmed by its own pre-alarm dimming.
    NightDim {
        anchors.fill: parent
        z: 900
    }

    Rectangle {
        anchors.fill: parent
        color: "transparent"
        visible: ringingOverlay.ringing
        z: 950
        RingingOverlay {
            id: ringingOverlay
        }
    }

    Connections {
        target: scheduler
        function onAlarmTriggered(index) {
            var a = alarmManager.model.get(index)
            if (!a) return
            ringingOverlay.alarmIndex = index
            ringingOverlay.alarmHour = a.hour; ringingOverlay.alarmMinute = a.minute
            ringingOverlay.alarmMedia = a.soundFile
            ringingOverlay.alarmData = a
            ringingOverlay.ringing = true
        }
        function onCountdownUpdated(seconds) { root.countdownValue = seconds }
    }

    // Audio error surfacing — never fail silently.
    property string audioErrorText: ""
    Connections {
        target: audioPlayer
        function onPlaybackError(msg) { root.audioErrorText = msg; audioErrorTimer.restart() }
    }
    Timer { id: audioErrorTimer; interval: 6000; repeat: false; onTriggered: root.audioErrorText = "" }
    Text {
        visible: root.audioErrorText !== ""
        text: "🔊 " + root.audioErrorText
        color: "#ff6b6b"; font.pixelSize: 12
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom; anchors.bottomMargin: 8
        z: 1000
    }
}

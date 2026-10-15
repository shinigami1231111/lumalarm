import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import GlassAlarm

ColumnLayout {
    id: root
    spacing: 18
    property bool __allowThemeWrite: false

    // Speaker selector state (Phase 6): the controller fills outputDevices
    // asynchronously, so we mirror it here to re-evaluate the combo model.
    property var sinkDevices: []
    Component.onCompleted: audioPlayer.refreshOutputDevices()
    Connections {
        target: audioPlayer
        function onOutputDevicesChanged() {
            root.sinkDevices = audioPlayer.outputDevices
        }
    }

    Text {
        text: qsTr("Settings")
        color: configManager.themeTextPrimary
        font.pixelSize: 24
        font.bold: true
    }

    ScrollView {
        Layout.fillWidth: true
        Layout.fillHeight: true
        contentWidth: availableWidth
        clip: true
        ScrollBar.vertical.policy: ScrollBar.AsNeeded

        ColumnLayout {
            width: parent.width
            spacing: 16

            // ================================================================
            // APPEARANCE
            // ================================================================
            Text { text: qsTr("Appearance"); color: configManager.themeTextSecondary; font.pixelSize: 13; font.bold: true; Layout.bottomMargin: -4 }

            // 1. Palette Presets
            Text { text: qsTr("Palette Presets"); color: configManager.themeTextPrimary; font.pixelSize: 15; Layout.bottomMargin: -6 }
            Flow {
                Layout.fillWidth: true
                spacing: 8
                Repeater {
                    model: themeManager.presetNames()
                    GlassButton {
                        text: modelData
                        pixelSize: 12; implicitHeight: 32; radius: 7; implicitWidth: Math.max(120, text.length * 8 + 24)
                        property bool active: themeManager.current_preset === modelData
                        baseColor: active ? Qt.rgba(0.3, 0.6, 1, 0.28) : Qt.rgba(1, 1, 1, 0.06)
                        textColor: active ? Qt.rgba(0.6, 0.85, 1, 1) : configManager.themeTextSecondary
                        borderColor: active ? Qt.rgba(0.4, 0.8, 1, 0.4) : Qt.rgba(1, 1, 1, 0.1)
                        onClicked: themeManager.applyPreset(modelData)
                    }
                }
            }

            // 2. Background Style
            Text { text: qsTr("Background Style"); color: configManager.themeTextPrimary; font.pixelSize: 15; Layout.bottomMargin: -6 }
            Flow {
                Layout.fillWidth: true
                spacing: 8
                Repeater {
                    model: ["Dark Glass", "Night Sky", "Soft Aurora", "Minimal Geometric", "Atmospheric Dark", "Subtle Gradient", "Abstract Light"]
                    GlassButton {
                        text: modelData
                        pixelSize: 12; implicitHeight: 32; radius: 7; implicitWidth: Math.max(110, text.length * 7 + 24)
                        property bool active: themeManager.background_preset === modelData
                        baseColor: active ? Qt.rgba(0.3, 0.6, 1, 0.28) : Qt.rgba(1, 1, 1, 0.06)
                        textColor: active ? Qt.rgba(0.6, 0.85, 1, 1) : configManager.themeTextSecondary
                        borderColor: active ? Qt.rgba(0.4, 0.8, 1, 0.4) : Qt.rgba(1, 1, 1, 0.1)
                        onClicked: themeManager.background_preset = modelData
                    }
                }
            }

            // 3. Compositor Blur
            RowLayout {
                Layout.fillWidth: true; spacing: 14
                Text { text: qsTr("Compositor Blur"); color: configManager.themeTextPrimary; font.pixelSize: 15; Layout.preferredWidth: 180 }
                Item { Layout.fillWidth: true }
                ToggleSwitch {
                    checked: themeManager.blur_mode === "compositor"
                    onToggled: function(checked) { themeManager.blur_mode = checked ? "compositor" : "app" }
                }
            }
            Text {
                text: themeManager.blur_mode === "compositor"
                    ? qsTr("ON — compositor blurs the desktop behind the transparent panel. Requires Hyprland/Sway + a blur window rule.")
                    : qsTr("OFF — no compositor blur. Opacity still controls panel transparency.")
                color: configManager.themeTextSecondary; font.pixelSize: 12
                wrapMode: Text.Wrap; Layout.fillWidth: true
            }

            // 4. Background Opacity
            RowLayout {
                Layout.fillWidth: true; spacing: 14
                Text { text: qsTr("Background Opacity"); color: configManager.themeTextPrimary; font.pixelSize: 15; Layout.preferredWidth: 180 }
                Item { Layout.fillWidth: true }
                Slider {
                    id: opSlider
                    from: 0.0; to: 1.0; stepSize: 0.01
                    value: configManager.themeOpacity
                    implicitWidth: 200
                    onPressedChanged: {
                        if (!pressed && value !== configManager.themeOpacity)
                            configManager.themeOpacity = value
                    }
                }
                Text { text: Math.round(opSlider.value * 100) + "%"; color: configManager.themeTextSecondary; font.pixelSize: 14; Layout.preferredWidth: 46; horizontalAlignment: Text.AlignRight }
            }

            // 5. Manual Colors
            Text { text: qsTr("Manual Colors"); color: configManager.themeTextPrimary; font.pixelSize: 15; Layout.bottomMargin: -6 }
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 12
                SettingRow {
                    label: qsTr("Background")
                    control: ColorEdit { color: configManager.themeBg; onPicked: { if (root.__allowThemeWrite) configManager.themeBg = hex } }
                }
                SettingRow {
                    label: qsTr("Accent")
                    control: ColorEdit { color: configManager.themeAccent; onPicked: { if (root.__allowThemeWrite) configManager.themeAccent = hex } }
                }
                SettingRow {
                    label: qsTr("Text Primary")
                    control: ColorEdit { color: configManager.themeTextPrimary; onPicked: { if (root.__allowThemeWrite) configManager.themeTextPrimary = hex } }
                }
                SettingRow {
                    label: qsTr("Text Secondary")
                    control: ColorEdit { color: configManager.themeTextSecondary; onPicked: { if (root.__allowThemeWrite) configManager.themeTextSecondary = hex } }
                }
                SettingRow {
                    label: qsTr("Border Color")
                    control: ColorEdit { color: themeManager.border_color; onPicked: themeManager.border_color = hex }
                }
            }

            RowLayout {
                Layout.fillWidth: true; spacing: 14
                Text { text: qsTr("Corner Radius"); color: configManager.themeTextPrimary; font.pixelSize: 15; Layout.preferredWidth: 180 }
                Item { Layout.fillWidth: true }
                Spinny { from: 0; to: 40; value: themeManager.corner_radius; suffix: " px"; onChanged: function() { themeManager.corner_radius = current } }
            }

            RowLayout {
                Layout.fillWidth: true; spacing: 14
                Text { text: qsTr("Font Family"); color: configManager.themeTextPrimary; font.pixelSize: 15; Layout.preferredWidth: 180 }
                Item { Layout.fillWidth: true }
                TextField {
                    Layout.preferredWidth: 220; implicitHeight: 34
                    text: themeManager.font_family
                    color: configManager.themeTextPrimary; font.pixelSize: 13
                    placeholderText: qsTr("system default")
                    background: Rectangle { radius: 7; color: Qt.rgba(1,1,1,0.06); border.color: Qt.rgba(1,1,1,0.1); border.width: 1 }
                    onEditingFinished: themeManager.font_family = text.trim()
                }
            }

            RowLayout {
                Layout.fillWidth: true; spacing: 8
                GlassButton {
                    text: themeManager.pywalAvailable() ? qsTr("Import from pywal") : qsTr("pywal not found")
                    pixelSize: 12
                    enabled: themeManager.pywalAvailable()
                    baseColor: themeManager.pywalAvailable() ? Qt.rgba(0.3, 0.6, 1, 0.2) : Qt.rgba(1, 1, 1, 0.05)
                    hoverColor: Qt.rgba(0.3, 0.6, 1, 0.3)
                    onClicked: themeManager.importFromPywal()
                }
                Text {
                    text: qsTr("Reads ~/.cache/wal/colors.json (manual, no auto-apply)")
                    color: configManager.themeTextSecondary; font.pixelSize: 12; Layout.fillWidth: true
                }
            }

            Text {
                text: qsTr("Theme is stored in ~/.config/lumalarm/theme.conf and reloads live when you edit it.")
                color: configManager.themeTextSecondary; font.pixelSize: 12; wrapMode: Text.Wrap; Layout.fillWidth: true
            }

            Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: Qt.rgba(1,1,1,0.07) }

            // ================================================================
            // DEFAULT (functional settings)
            // ================================================================
            Text { text: qsTr("Default"); color: configManager.themeTextSecondary; font.pixelSize: 13; font.bold: true; Layout.bottomMargin: -4 }

            // Window Controls
            Text { text: qsTr("Window Controls"); color: configManager.themeTextPrimary; font.pixelSize: 15; Layout.bottomMargin: -6 }
            RowLayout {
                Layout.fillWidth: true; spacing: 14
                Text { text: qsTr("Show Minimize Button"); color: configManager.themeTextPrimary; font.pixelSize: 15; Layout.fillWidth: true }
                ToggleSwitch {
                    checked: themeManager.show_minimize_button
                    onToggled: function(checked) { themeManager.show_minimize_button = checked }
                }
            }
            RowLayout {
                Layout.fillWidth: true; spacing: 14
                Text { text: qsTr("Show Maximize Button"); color: configManager.themeTextPrimary; font.pixelSize: 15; Layout.fillWidth: true }
                ToggleSwitch {
                    checked: themeManager.show_maximize_button
                    onToggled: function(checked) { themeManager.show_maximize_button = checked }
                }
            }
            RowLayout {
                Layout.fillWidth: true; spacing: 14
                Text { text: qsTr("Show Close Button"); color: configManager.themeTextPrimary; font.pixelSize: 15; Layout.fillWidth: true }
                ToggleSwitch {
                    checked: themeManager.show_close_button
                    onToggled: function(checked) { themeManager.show_close_button = checked }
                }
            }

            // System Tray
            Text { text: qsTr("System Tray"); color: configManager.themeTextPrimary; font.pixelSize: 15; Layout.bottomMargin: -6 }
            RowLayout {
                Layout.fillWidth: true; spacing: 14
                Text { text: qsTr("Show system tray icon"); color: configManager.themeTextPrimary; font.pixelSize: 15; Layout.fillWidth: true }
                ToggleSwitch {
                    checked: configManager.trayIconEnabled
                    onToggled: function(checked) { configManager.trayIconEnabled = checked }
                }
            }

            // What the close button does (STG-5.4). This is where the close
            // prompt's "Don't ask me again" choice comes back to: choosing
            // "Ask me" here reopens the question the next time X is pressed.
            Text { text: qsTr("When I close the window"); color: configManager.themeTextPrimary; font.pixelSize: 15 }
            RowLayout {
                Layout.fillWidth: true; spacing: 8
                GlassButton {
                    text: qsTr("Ask me")
                    filled: configManager.closeAction === "ask"
                    onClicked: configManager.closeAction = "ask"
                }
                GlassButton {
                    // Disabled rather than silently degrading: with no tray
                    // host there is nothing to hide behind, so the option is
                    // visible but unselectable instead of quietly becoming a
                    // quit.
                    text: qsTr("Hide to tray")
                    filled: configManager.closeAction === "hide"
                    enabled: lifecycle.canHideToTray
                    onClicked: configManager.closeAction = "hide"
                }
                GlassButton {
                    text: qsTr("Quit")
                    filled: configManager.closeAction === "quit"
                    onClicked: configManager.closeAction = "quit"
                }
                Item { Layout.fillWidth: true }
            }
            Text {
                visible: !lifecycle.canHideToTray
                text: qsTr("“Hide to tray” needs a running tray host. Until one appears, closing will minimize the window instead.")
                color: configManager.themeTextSecondary
                font.pixelSize: 12
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }

            // What to show when an alarm rings while the window is hidden
            // (STG-5.4). "alarmOnly" shows just the ringing overlay; the main
            // window stays in the tray.
            Text { text: qsTr("When alarm rings while hidden"); color: configManager.themeTextPrimary; font.pixelSize: 15 }
            RowLayout {
                Layout.fillWidth: true; spacing: 8
                GlassButton {
                    text: qsTr("Alarm only")
                    filled: configManager.alarmUiMode === "alarmOnly"
                    onClicked: configManager.alarmUiMode = "alarmOnly"
                }
                GlassButton {
                    text: qsTr("Main window + alarm")
                    filled: configManager.alarmUiMode === "mainAndAlarm"
                    onClicked: configManager.alarmUiMode = "mainAndAlarm"
                }
                Item { Layout.fillWidth: true }
            }

            Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: Qt.rgba(1,1,1,0.07) }

            Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: Qt.rgba(1,1,1,0.07) }

            // Alarm Defaults
            Text { text: qsTr("Alarm Defaults"); color: configManager.themeTextSecondary; font.pixelSize: 13; font.bold: true; Layout.bottomMargin: -4 }

            // The one and only mode control: this value is what the helper runs
            // as `rtcwake -m <mode>`.
            //
            // It used to be shadowed by a second row, "Default Wake Mode", whose
            // value NOTHING consumed — the mode handed to the helper comes from
            // WakeManager::m_sleepMode, fed by this setting alone. That row also
            // carried a hardcoded model offering "none", which is not a valid
            // rtcwake mode at all. Removed 2026-10-08 (user report).
            //
            // Order comes from ConfigManager::sleepModeChoices(), which keeps
            // standby between mem and disk (user, 2026-10-08).
            RowLayout {
                Layout.fillWidth: true; spacing: 14
                Text { text: qsTr("RTCwake method"); color: configManager.themeTextPrimary; font.pixelSize: 15; Layout.preferredWidth: 180 }
                Item { Layout.fillWidth: true }
                RoundedCombo {
                    id: sleepModeCombo
                    model: configManager.sleepModeChoices()
                    currentIndex: Math.max(0, sleepModeCombo.find(configManager.sleepMode))
                    Layout.preferredWidth: 140
                    onActivated: configManager.sleepMode = currentText
                }
            }

            RowLayout {
                Layout.fillWidth: true; spacing: 14
                Text { text: qsTr("Time picker style"); color: configManager.themeTextPrimary; font.pixelSize: 15; Layout.preferredWidth: 180 }
                Item { Layout.fillWidth: true }
                RoundedCombo {
                    id: timePickerCombo
                    model: [qsTr("Wheel"), qsTr("Spin")]
                    currentIndex: configManager.timePickerMode === "spin" ? 1 : 0
                    Layout.preferredWidth: 140
                    onActivated: configManager.timePickerMode = currentIndex === 1 ? "spin" : "wheel"
                }
            }

            // ================================================================
            // ALARM SOUND — what an alarm ring is allowed to change
            // (the rebuilt audio system; live, the controller re-reads the
            // settings on configChanged).
            // ================================================================
            Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: Qt.rgba(1,1,1,0.07) }

            Text { text: qsTr("Alarm Sound"); color: configManager.themeTextSecondary; font.pixelSize: 13; font.bold: true; Layout.bottomMargin: -4 }

            Text { text: qsTr("When an alarm rings"); color: configManager.themeTextPrimary; font.pixelSize: 15 }
            RowLayout {
                Layout.fillWidth: true; spacing: 8
                GlassButton {
                    text: qsTr("Raise system volume")
                    filled: configManager.audioPolicy === "guarantee"
                    onClicked: configManager.audioPolicy = "guarantee"
                }
                GlassButton {
                    text: qsTr("Never touch system volume")
                    filled: configManager.audioPolicy === "alarmOnly"
                    onClicked: configManager.audioPolicy = "alarmOnly"
                }
                Item { Layout.fillWidth: true }
            }

            RowLayout {
                Layout.fillWidth: true; spacing: 14
                enabled: configManager.audioPolicy === "guarantee"
                opacity: enabled ? 1.0 : 0.5
                Text { text: qsTr("Minimum volume"); color: configManager.themeTextPrimary; font.pixelSize: 15; Layout.preferredWidth: 180 }
                Item { Layout.fillWidth: true }
                Slider {
                    from: 0; to: 100; stepSize: 5
                    value: configManager.audioFloorPercent
                    implicitWidth: 200
                    onPressedChanged: {
                        if (!pressed && value !== configManager.audioFloorPercent)
                            configManager.audioFloorPercent = value
                    }
                }
                Text {
                    text: Math.round(configManager.audioFloorPercent) + "%"
                    color: configManager.themeTextSecondary; font.pixelSize: 14
                    Layout.preferredWidth: 46; horizontalAlignment: Text.AlignRight
                }
            }

            Text {
                visible: configManager.audioPolicy === "guarantee"
                text: qsTr("The system volume is raised to this level while an alarm rings, then restored exactly.")
                color: configManager.themeTextSecondary; font.pixelSize: 12
                Layout.fillWidth: true
            }
            Text {
                visible: configManager.audioPolicy === "alarmOnly"
                text: qsTr("Lumalarm plays its own sound only; your system volume is never changed.")
                color: configManager.themeTextSecondary; font.pixelSize: 12
                Layout.fillWidth: true
            }

            RowLayout {
                Layout.fillWidth: true; spacing: 14
                Text { text: qsTr("Output device"); color: configManager.themeTextPrimary; font.pixelSize: 15; Layout.preferredWidth: 180 }
                Item { Layout.fillWidth: true }
                RoundedCombo {
                    id: outputSinkCombo
                    Layout.preferredWidth: 240
                    model: {
                        // index 0 = follow the system default sink; distinct
                        // wording from the language picker's "System default"
                        // so the Arabic can differ per meaning.
                        const labels = [qsTr("Default output device")]
                        for (let i = 0; i < root.sinkDevices.length; ++i)
                            labels.push(root.sinkDevices[i].description)
                        return labels
                    }
                    currentIndex: {
                        const wanted = configManager.audioOutputSink
                        if (wanted === "")
                            return 0
                        for (let i = 0; i < root.sinkDevices.length; ++i)
                            if (root.sinkDevices[i].name === wanted)
                                return i + 1
                        return 0 // configured device is not present (unplugged)
                    }
                    onActivated: configManager.audioOutputSink =
                        currentIndex === 0 ? "" : root.sinkDevices[currentIndex - 1].name
                }
            }
            Text {
                text: qsTr("The alarm rings through this device; other apps keep using the system default.")
                color: configManager.themeTextSecondary; font.pixelSize: 12
                Layout.fillWidth: true
            }

            // ================================================================
            // TEST ALARM
            // ================================================================
            Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: Qt.rgba(1,1,1,0.07) }

            Text { text: qsTr("Test alarm"); color: configManager.themeTextSecondary; font.pixelSize: 13; font.bold: true; Layout.bottomMargin: -4 }
            Text {
                text: qsTr("Arm a real wake without creating an alarm, to check that this machine actually wakes.")
                color: configManager.themeTextSecondary; font.pixelSize: 12
                wrapMode: Text.WordWrap; Layout.fillWidth: true; Layout.bottomMargin: 4
            }

            ColumnLayout {
                id: testAlarmCard
                Layout.fillWidth: true; spacing: 10

                // Presets (60 s .. 60 min) plus a custom numeric field.
                RowLayout {
                    Layout.fillWidth: true; spacing: 14
                    Text { text: qsTr("Wake in"); color: configManager.themeTextPrimary; font.pixelSize: 15; Layout.preferredWidth: 80 }
                    Item { Layout.fillWidth: true }
                    // The custom-seconds field. It sits to the LEFT of the combo so
                    // the combo stays pinned to the row's right edge and never moves
                    // — the spacer absorbs the growth instead of shoving the dropdown
                    // sideways. Constant row height, zero reflow.
                    //
                    // Defect A fix (2026-10-09). The combo used to carry
                    // `visible: !customField.visible`, so picking "Custom…" hid it
                    // FOREVER — no line ever set it back, only a restart recovered the
                    // presets. `expanded` replaces `visible` as the mode flag: the
                    // combo is never hidden, and the field opens into an animated
                    // space and closes the same way when a preset is chosen again.
                    TextField {
                        id: customField

                        // Single source of truth for "are we in custom mode".
                        property bool expanded: false

                        // Stay in the layout at all times — `visible: false` removes
                        // an item from the layout entirely, so it could never animate
                        // back in. Collapsed it is simply 0 wide and transparent.
                        visible: true
                        enabled: expanded
                        clip: true

                        // minimumWidth must be 0 explicitly: an unset Layout minimum
                        // falls back to implicitWidth, which would refuse to collapse.
                        Layout.minimumWidth: 0
                        Layout.preferredWidth: expanded ? 160 : 0
                        opacity: expanded ? 1 : 0

                        height: 32
                        color: configManager.themeTextPrimary
                        font.pixelSize: 14
                        leftPadding: 10
                        placeholderText: qsTr("Custom seconds (60-3600)")
                        validator: IntValidator { bottom: 60; top: 3600 }

                        // Open: unhurried, ease-out — the space breathes out from
                        // alongside the Custom label. Close: noticeably snappier, so
                        // switching back to a preset feels instant rather than laggy.
                        Behavior on Layout.preferredWidth {
                            NumberAnimation { duration: 240; easing.type: Easing.OutCubic }
                        }
                        Behavior on opacity {
                            NumberAnimation { duration: 150; easing.type: Easing.OutCubic }
                        }

                        background: Rectangle {
                            radius: 7
                            color: Qt.rgba(1,1,1,0.08)
                            border.color: Qt.rgba(1,1,1,0.12); border.width: 1
                        }
                        onEditingFinished: {
                            const v = parseInt(text)
                            if (isNaN(v))
                                return
                            // Persist only a value the validation boundary would
                            // accept. Anything else stays in the field untouched so
                            // armTestWake() sees the real number and refuses it —
                            // never silently rewritten into an accepted one.
                            if (v >= 60 && v <= 3600)
                                persistTestWake(v)
                        }
                    }

                    RoundedCombo {
                        id: testWakeCombo
                        model: [qsTr("60 s"), qsTr("2 min"), qsTr("5 min"), qsTr("10 min"), qsTr("30 min"), qsTr("60 min"), qsTr("Custom…")]
                        // Initial selection comes from the persisted duration.
                        currentIndex: presetIndexFor(configManager.testWakeDuration)
                        // Never hidden. The old `visible: !customField.visible` was
                        // Defect A — it made "Custom…" a one-way door.
                        Layout.preferredWidth: 130
                        onActivated: {
                            if (currentIndex === 6) {
                                customField.text = configManager.testWakeDuration
                                customField.expanded = true
                                customField.forceActiveFocus()
                                customField.selectAll()
                            } else {
                                // Reverse the motion: the space closes up and the
                                // presets are usable again — no restart required.
                                customField.expanded = false
                                persistTestWake(presetSeconds(currentIndex))
                            }
                        }
                        function presetSeconds(i) { return [60, 120, 300, 600, 1800, 3600][i] }
                        function presetIndexFor(seconds) {
                            const idx = [60, 120, 300, 600, 1800, 3600].indexOf(seconds)
                            return idx >= 0 ? idx : 6
                        }
                    }
                }

                RowLayout {
                    Layout.fillWidth: true; spacing: 10
                    GlassButton {
                        // Stays enabled while armed — pressing it again just swaps the
                        // wake. armTestWake() already cancels the previous one first
                        // ("Never stack test wakes"), so no relabel is needed: it is
                        // simply the test button.
                        text: qsTr("Arm test alarm")
                        pixelSize: 13
                        onClicked: wakeManager.armTestWake(testAlarmCard.selectedSeconds())
                    }
                    GlassButton {
                        text: qsTr("Cancel test alarm")
                        pixelSize: 13
                        enabled: wakeManager.wakeState === "armed"
                        onClicked: wakeManager.cancelTestWake()
                    }
                    Item { Layout.fillWidth: true }
                    // Raw `wakeState` readout (idle/armed/failed) removed 2026-10-10:
                    // the card is only really read again AFTER the wake, when that
                    // word is already stale. The notification carries it instead.
                }

                Text {
                    id: testAlarmStatus
                    Layout.fillWidth: true
                    visible: text.length > 0
                    wrapMode: Text.WordWrap
                    font.pixelSize: 13
                    color: configManager.themeTextPrimary
                }

                // Duration currently offered, whichever input produced it.
                // While the custom field is on screen, return what is typed
                // RIGHT NOW rather than the persisted value. The setter clamps
                // to [60, 3600] on write, so reading it back would launder an
                // invalid number into an accepted one before armTestWake() ever
                // got the chance to refuse it with "out of range".
                function selectedSeconds() {
                    if (customField.expanded) {
                        const v = parseInt(customField.text)
                        return isNaN(v) ? 0 : v
                    }
                    if (testWakeCombo.currentIndex === 6)
                        return configManager.testWakeDuration
                    return testWakeCombo.presetSeconds(testWakeCombo.currentIndex)
                }
                // Persist on every change so the choice survives a restart.
                function persistTestWake(seconds) {
                    configManager.testWakeDuration = seconds
                }

                Connections {
                    target: wakeManager
                    function onTestWakeArmed(seconds) {
                        testAlarmStatus.text = qsTr("Test alarm armed in %1 s").arg(seconds)
                    }
                    function onTestWakeFailed(reason) {
                        testAlarmStatus.text = qsTr("Test alarm failed: %1").arg(reason)
                    }
                    function onWakeStateChanged() {
                        if (wakeManager.wakeState === "idle")
                            testAlarmStatus.text = ""
                    }
                }
            }

            Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: Qt.rgba(1,1,1,0.07) }

            // Behavior
            Text { text: qsTr("Behavior"); color: configManager.themeTextSecondary; font.pixelSize: 13; font.bold: true; Layout.bottomMargin: -4 }
            RowLayout {
                Layout.fillWidth: true; spacing: 14
                Text { text: qsTr("Show milliseconds in stopwatch"); color: configManager.themeTextPrimary; font.pixelSize: 15; Layout.fillWidth: true }
                ToggleSwitch {
                    checked: configManager.stopwatchShowMs
                    onToggled: function(checked) { configManager.stopwatchShowMs = checked }
                }
            }
            RowLayout {
                Layout.fillWidth: true; spacing: 14
                Text { text: qsTr("Desktop notifications"); color: configManager.themeTextPrimary; font.pixelSize: 15; Layout.fillWidth: true }
                ToggleSwitch {
                    checked: configManager.notificationsEnabled
                    onToggled: function(checked) { configManager.notificationsEnabled = checked }
                }
            }
            RowLayout {
                Layout.fillWidth: true; spacing: 14
                Text { text: qsTr("Setup wizard"); color: configManager.themeTextPrimary; font.pixelSize: 15; Layout.fillWidth: true }
                GlassButton {
                    text: qsTr("Run again")
                    pixelSize: 13
                    // Clears wizard/completed and brings the overlay straight back.
                    onClicked: wizard.reset()
                }
            }
            RowLayout {
                Layout.fillWidth: true; spacing: 14
                Text { text: qsTr("Language"); color: configManager.themeTextPrimary; font.pixelSize: 15; Layout.fillWidth: true }
                RoundedCombo {
                    id: languageCombo
                    // Language names are shown in their own script on purpose:
                    // a label for a language the reader cannot read defeats the
                    // purpose. Only "System default" is translatable, and it is
                    // what the empty ui/language value resolves to in main.cpp.
                    model: [qsTr("System default"), "English", "العربية"]
                    currentIndex: configManager.language === "en" ? 1
                                  : (configManager.language === "ar" ? 2 : 0)
                    Layout.preferredWidth: 170
                    onActivated: {
                        const codes = ["", "en", "ar"]
                        configManager.language = codes[currentIndex]
                    }
                }
            }

            // ================================================================
            // NIGHT MODE
            // ================================================================
            Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: Qt.rgba(1,1,1,0.07) }

            Text { text: qsTr("Night Mode"); color: configManager.themeTextSecondary; font.pixelSize: 13; font.bold: true; Layout.bottomMargin: -4 }
            Text {
                text: qsTr("Dim the window before an alarm so it does not light up a dark room.")
                color: configManager.themeTextSecondary; font.pixelSize: 12
                wrapMode: Text.WordWrap; Layout.fillWidth: true; Layout.bottomMargin: 4
            }

            RowLayout {
                Layout.fillWidth: true; spacing: 14
                Text { text: qsTr("Enable night mode"); color: configManager.themeTextPrimary; font.pixelSize: 15; Layout.fillWidth: true }
                ToggleSwitch {
                    checked: nightMode.enabled
                    onToggled: function(checked) { nightMode.enabled = checked }
                }
            }

            RowLayout {
                Layout.fillWidth: true; spacing: 14
                enabled: nightMode.enabled
                opacity: nightMode.enabled ? 1.0 : 0.5
                Text { text: qsTr("Dim before alarm"); color: configManager.themeTextPrimary; font.pixelSize: 15; Layout.preferredWidth: 180 }
                Item { Layout.fillWidth: true }
                Slider {
                    id: nightMinutesSlider
                    from: 1; to: 180; stepSize: 1
                    value: nightMode.minutesBefore
                    implicitWidth: 200
                    onPressedChanged: {
                        if (!pressed && value !== nightMode.minutesBefore)
                            nightMode.minutesBefore = Math.round(value)
                    }
                }
                Text {
                    text: nightMode.minutesBefore + qsTr(" min")
                    color: configManager.themeTextSecondary; font.pixelSize: 14
                    Layout.preferredWidth: 62; horizontalAlignment: Text.AlignRight
                }
            }

            RowLayout {
                Layout.fillWidth: true; spacing: 14
                enabled: nightMode.enabled
                opacity: nightMode.enabled ? 1.0 : 0.5
                Text { text: qsTr("Dim strength"); color: configManager.themeTextPrimary; font.pixelSize: 15; Layout.preferredWidth: 180 }
                Item { Layout.fillWidth: true }
                Slider {
                    id: nightOpacitySlider
                    from: 0.0; to: 1.0; stepSize: 0.05
                    value: nightMode.dimOpacity
                    implicitWidth: 200
                    onPressedChanged: {
                        if (!pressed && value !== nightMode.dimOpacity)
                            nightMode.dimOpacity = value
                    }
                }
                Text {
                    text: Math.round(nightMode.dimOpacity * 100) + "%"
                    color: configManager.themeTextSecondary; font.pixelSize: 14
                    Layout.preferredWidth: 46; horizontalAlignment: Text.AlignRight
                }
            }

            RowLayout {
                Layout.fillWidth: true; spacing: 10
                GlassButton {
                    text: qsTr("Preview")
                    pixelSize: 13
                    enabled: !nightMode.active
                    // NightMode.activate() holds the veil for two seconds and
                    // then switches it off by itself — see kPreviewMillis.
                    onClicked: nightMode.activate()
                }
                Item { Layout.fillWidth: true }
                Text {
                    visible: nightMode.active
                    text: nightMode.active ? qsTr("Dimming") : ""
                    color: configManager.themeTextSecondary; font.pixelSize: 12
                }
            }

            Item { Layout.fillHeight: true }

            RowLayout {
                Layout.fillWidth: true; Layout.bottomMargin: 4
                Item { Layout.fillWidth: true }
                GlassButton {
                    text: qsTr("Reset to Defaults")
                    pixelSize: 13
                    baseColor: Qt.rgba(1, 0.3, 0.3, 0.12)
                    hoverColor: Qt.rgba(1, 0.3, 0.3, 0.22)
                    onClicked: {
                        configManager.themeBg = "#0d0d1a"
                        configManager.themeAccent = "#3d7fff"
                        configManager.themeTextPrimary = "#ffffff"
                        configManager.themeTextSecondary = "#808090"
                        configManager.themeOpacity = 0.55
                        themeManager.border_color = "#3d7fff"
                        themeManager.blur_mode = "compositor"
                        themeManager.blur_radius = 20
                        themeManager.font_family = ""
                        themeManager.corner_radius = 22
                        themeManager.background_preset = "Dark Glass"
                        themeManager.show_minimize_button = true
                        themeManager.show_maximize_button = true
                        themeManager.show_close_button = true
                        themeManager.show_tray_icon = true
                        configManager.sleepMode = "mem" // the rtcwake -m layer, not the dead defaultWakeMode
                        configManager.stopwatchShowMs = true
                        configManager.trayIconEnabled = true
                        configManager.notificationsEnabled = true
                    }
                }
            }
        }
    }

    // ---------- Reusable bits ----------
    component SettingRow: RowLayout {
        property string label
        default property alias control: holder.data
        spacing: 14
        Text { text: label; color: configManager.themeTextPrimary; font.pixelSize: 15; Layout.alignment: Qt.AlignVCenter }
        Item { Layout.fillWidth: true }
        Item {
            id: holder
            Layout.alignment: Qt.AlignVCenter
            implicitWidth: childrenRect.width
            implicitHeight: childrenRect.height
        }
    }

    component ColorEdit: RowLayout {
        id: cer
        property string color: "#ffffff"
        signal picked(string hex)
        spacing: 8
        onColorChanged: { if (initialized) hexField.text = cer.color }
        property bool initialized: false
        Rectangle {
            width: 30; height: 30; radius: 7
            border.color: Qt.rgba(1,1,1,0.2); border.width: 1
            color: cer.color
            MouseArea {
                anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                onPressed: { root.__allowThemeWrite = true }
                onClicked: { picker.currentColor = cer.color; picker.open() }
            }
        }
        TextField {
            id: hexField
            text: cer.color
            color: configManager.themeTextPrimary; font.pixelSize: 13
            leftPadding: 8; implicitWidth: 110; implicitHeight: 30
            background: Rectangle { radius: 7; color: Qt.rgba(1,1,1,0.06); border.color: Qt.rgba(1,1,1,0.1); border.width: 1 }
            Component.onCompleted: { initialized = true }
            onEditingFinished: { if (text.length === 7 && text[0] === '#') cer.picked(text) }
        }
        ColorPickerPopup {
            id: picker
            onColorPicked: cer.picked(hex)
        }
    }

    component Spinny: RowLayout {
        id: sn
        property int from: 0; property int to: 100; property int value: 0; property int step: 1; property string suffix: ""
        signal changed(int v)
        spacing: 8
        property int current: sn.value
        onCurrentChanged: sn.changed(current)
        Rectangle {
            Layout.preferredWidth: 130; Layout.preferredHeight: 38; radius: 8
            color: Qt.rgba(1,1,1,0.06); border.color: Qt.rgba(1,1,1,0.1); border.width: 1
            RowLayout {
                anchors.fill: parent; anchors.margins: 2; spacing: 2
                Text {
                    Layout.fillWidth: true; Layout.fillHeight: true
                    verticalAlignment: Qt.AlignVCenter; horizontalAlignment: Qt.AlignHCenter
                    color: configManager.themeTextPrimary; font.pixelSize: 15; font.bold: true
                    text: sn.current + sn.suffix
                }
                ColumnLayout { spacing: 1; Layout.preferredWidth: 22
                    Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 14; radius: 4
                        color: upMa.containsMouse ? Qt.rgba(1,1,1,0.15) : "transparent"
                        Text { text: "\u25B2"; anchors.centerIn: parent; color: configManager.themeTextSecondary; font.pixelSize: 10 }
                        MouseArea { id: upMa; anchors.fill: parent; hoverEnabled: true
                            onClicked: sn.current = Math.min(sn.to, sn.current + sn.step) } }
                    Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 14; radius: 4
                        color: dnMa.containsMouse ? Qt.rgba(1,1,1,0.15) : "transparent"
                        Text { text: "\u25BC"; anchors.centerIn: parent; color: configManager.themeTextSecondary; font.pixelSize: 10 }
                        MouseArea { id: dnMa; anchors.fill: parent; hoverEnabled: true
                            onClicked: sn.current = Math.max(sn.from, sn.current - sn.step) } }
                }
            }
        }
    }

    component ColorPickerPopup: Popup {
        id: popup
        property string currentColor: "#ffffff"
        width: 300; height: 340
        z: 1000
        parent: Overlay.overlay
        x: Math.max(8, (Overlay.overlay.width - width) / 2)
        y: Math.max(8, (Overlay.overlay.height - height) / 2)
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        padding: 0
        background: Rectangle { color: Qt.rgba(0.08,0.08,0.12,0.97); radius: 14; border.color: Qt.rgba(1,1,1,0.12); border.width: 1 }

        function hsvToHex(h, s, v) {
            h = ((h % 360) + 360) % 360
            var hi = Math.floor(h / 60)
            var f = h / 60 - hi
            var p = v * (1 - s), q = v * (1 - f * s), t = v * (1 - (1 - f) * s)
            var r=0,g=0,b=0
            if (hi===0){r=v;g=t;b=p} else if(hi===1){r=q;g=v;b=p} else if(hi===2){r=p;g=v;b=t}
            else if(hi===3){r=p;g=q;b=v} else if(hi===4){r=t;g=p;b=v} else {r=v;g=p;b=q}
            var ri=Math.round(r*255),gi=Math.round(g*255),bi=Math.round(b*255)
            return '#'+ri.toString(16).padStart(2,'0')+gi.toString(16).padStart(2,'0')+bi.toString(16).padStart(2,'0')
        }
        function hexToHsv(hex) {
            hex = hex.replace('#',''); if (hex.length < 6) hex = hex.padEnd(6,'0')
            var r=parseInt(hex.substring(0,2),16)/255,g=parseInt(hex.substring(2,4),16)/255,b=parseInt(hex.substring(4,6),16)/255
            var mx=Math.max(r,g,b),mn=Math.min(r,g,b),h=0,s=0,v=mx,d=mx-mn
            if (mx!==0) s=d/mx
            if (mx!==mn){ if(mx===r)h=((g-b)/d+(g<b?6:0))*60; else if(mx===g)h=((b-r)/d+2)*60; else h=((r-g)/d+4)*60 }
            return {h:h,s:s,v:v}
        }
        property var hsv: hexToHsv(currentColor)
        function emitHex() { colorPicked(hsvToHex(hsv.h,hsv.s,hsv.v)) }
        signal colorPicked(string hex)
        onCurrentColorChanged: hsv = hexToHsv(currentColor)
        onColorPicked: currentColor = hex

        ColumnLayout {
            anchors.fill: parent; anchors.margins: 18; spacing: 12
            Rectangle {
                id: svSquare; Layout.fillWidth: true; Layout.preferredHeight: 170; clip: true; radius: 8
                Rectangle { anchors.fill: parent; color: "white" }
                Rectangle { anchors.fill: parent; gradient: Gradient { orientation: Gradient.Horizontal; GradientStop { position: 0; color: "white" } GradientStop { position: 1; color: popup.hsvToHex(popup.hsv.h,1,1) } } }
                Rectangle { anchors.fill: parent; gradient: Gradient { orientation: Gradient.Vertical; GradientStop { position: 0; color: "transparent" } GradientStop { position: 1; color: "black" } } }
                MouseArea {
                    anchors.fill: parent
                    onPressed: pk(mouse.x,mouse.y); onPositionChanged: { if(pressed) pk(mouse.x,mouse.y) }
                    function pk(x,y){ popup.hsv.s=Math.min(1,Math.max(0,x/svSquare.width)); popup.hsv.v=Math.min(1,Math.max(0,1-y/svSquare.height)); popup.emitHex() }
                }
                Rectangle { x: popup.hsv.s*parent.width-5; y: (1-popup.hsv.v)*parent.height-5; width:10; height:10; radius:5; border.color:"white"; border.width:2; color:"transparent" }
            }
            Rectangle {
                id: hueBar; Layout.fillWidth: true; Layout.preferredHeight: 14; radius: 7
                gradient: Gradient { orientation: Gradient.Horizontal
                    GradientStop { position:0.00; color:"#ff0000" } GradientStop { position:0.17; color:"#ffff00" }
                    GradientStop { position:0.33; color:"#00ff00" } GradientStop { position:0.50; color:"#00ffff" }
                    GradientStop { position:0.67; color:"#0000ff" } GradientStop { position:0.83; color:"#ff00ff" }
                    GradientStop { position:1.00; color:"#ff0000" } }
                MouseArea {
                    anchors.fill: parent
                    onPressed: ph(mouse.x); onPositionChanged: { if(pressed) ph(mouse.x) }
                    function ph(x){ popup.hsv.h=Math.min(360,Math.max(0,(x/hueBar.width)*360)); popup.emitHex() }
                }
                Rectangle { x: (popup.hsv.h/360)*parent.width-5; y:-1; width:10; height:16; radius:3; border.color:"white"; border.width:2; color:"transparent" }
            }
            RowLayout { Layout.fillWidth: true; spacing: 10
                Rectangle { Layout.preferredWidth: 28; Layout.preferredHeight: 28; radius: 6; border.color: Qt.rgba(1,1,1,0.2); border.width: 1; color: popup.currentColor }
                TextField {
                    Layout.fillWidth: true; height: 32; color: configManager.themeTextPrimary; font.pixelSize: 14
                    text: popup.hsvToHex(popup.hsv.h,popup.hsv.s,popup.hsv.v); leftPadding: 10
                    background: Rectangle { radius: 7; color: Qt.rgba(1,1,1,0.08); border.color: Qt.rgba(1,1,1,0.12); border.width: 1 }
                    onEditingFinished: { if (text.length===7 && text[0]==='#'){ popup.hsv = popup.hexToHsv(text); popup.colorPicked(text) } }
                }
            }
        }
    }
}

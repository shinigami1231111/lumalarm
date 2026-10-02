import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import GlassAlarm

ColumnLayout {
    id: root
    spacing: 18
    property bool __allowThemeWrite: false

    Text {
        text: "Settings"
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
            Text { text: "Appearance"; color: configManager.themeTextSecondary; font.pixelSize: 13; font.bold: true; Layout.bottomMargin: -4 }

            // 1. Palette Presets
            Text { text: "Palette Presets"; color: configManager.themeTextPrimary; font.pixelSize: 15; Layout.bottomMargin: -6 }
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
            Text { text: "Background Style"; color: configManager.themeTextPrimary; font.pixelSize: 15; Layout.bottomMargin: -6 }
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
                Text { text: "Compositor Blur"; color: configManager.themeTextPrimary; font.pixelSize: 15; Layout.preferredWidth: 180 }
                Item { Layout.fillWidth: true }
                ToggleSwitch {
                    checked: themeManager.blur_mode === "compositor"
                    onToggled: function(checked) { themeManager.blur_mode = checked ? "compositor" : "app" }
                }
            }
            Text {
                text: themeManager.blur_mode === "compositor"
                    ? "ON — compositor blurs the desktop behind the transparent panel. Requires Hyprland/Sway + a blur window rule."
                    : "OFF — no compositor blur. Opacity still controls panel transparency."
                color: configManager.themeTextSecondary; font.pixelSize: 12
                wrapMode: Text.Wrap; Layout.fillWidth: true
            }

            // 4. Background Opacity
            RowLayout {
                Layout.fillWidth: true; spacing: 14
                Text { text: "Background Opacity"; color: configManager.themeTextPrimary; font.pixelSize: 15; Layout.preferredWidth: 180 }
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
            Text { text: "Manual Colors"; color: configManager.themeTextPrimary; font.pixelSize: 15; Layout.bottomMargin: -6 }
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 12
                SettingRow {
                    label: "Background"
                    control: ColorEdit { color: configManager.themeBg; onPicked: { if (root.__allowThemeWrite) configManager.themeBg = hex } }
                }
                SettingRow {
                    label: "Accent"
                    control: ColorEdit { color: configManager.themeAccent; onPicked: { if (root.__allowThemeWrite) configManager.themeAccent = hex } }
                }
                SettingRow {
                    label: "Text Primary"
                    control: ColorEdit { color: configManager.themeTextPrimary; onPicked: { if (root.__allowThemeWrite) configManager.themeTextPrimary = hex } }
                }
                SettingRow {
                    label: "Text Secondary"
                    control: ColorEdit { color: configManager.themeTextSecondary; onPicked: { if (root.__allowThemeWrite) configManager.themeTextSecondary = hex } }
                }
                SettingRow {
                    label: "Border Color"
                    control: ColorEdit { color: themeManager.border_color; onPicked: themeManager.border_color = hex }
                }
            }

            RowLayout {
                Layout.fillWidth: true; spacing: 14
                Text { text: "Corner Radius"; color: configManager.themeTextPrimary; font.pixelSize: 15; Layout.preferredWidth: 180 }
                Item { Layout.fillWidth: true }
                Spinny { from: 0; to: 40; value: themeManager.corner_radius; suffix: " px"; onChanged: function() { themeManager.corner_radius = current } }
            }

            RowLayout {
                Layout.fillWidth: true; spacing: 14
                Text { text: "Font Family"; color: configManager.themeTextPrimary; font.pixelSize: 15; Layout.preferredWidth: 180 }
                Item { Layout.fillWidth: true }
                TextField {
                    Layout.preferredWidth: 220; implicitHeight: 34
                    text: themeManager.font_family
                    color: configManager.themeTextPrimary; font.pixelSize: 13
                    placeholderText: "system default"
                    background: Rectangle { radius: 7; color: Qt.rgba(1,1,1,0.06); border.color: Qt.rgba(1,1,1,0.1); border.width: 1 }
                    onEditingFinished: themeManager.font_family = text.trim()
                }
            }

            RowLayout {
                Layout.fillWidth: true; spacing: 8
                GlassButton {
                    text: themeManager.pywalAvailable() ? "Import from pywal" : "pywal not found"
                    pixelSize: 12
                    enabled: themeManager.pywalAvailable()
                    baseColor: themeManager.pywalAvailable() ? Qt.rgba(0.3, 0.6, 1, 0.2) : Qt.rgba(1, 1, 1, 0.05)
                    hoverColor: Qt.rgba(0.3, 0.6, 1, 0.3)
                    onClicked: themeManager.importFromPywal()
                }
                Text {
                    text: "Reads ~/.cache/wal/colors.json (manual, no auto-apply)"
                    color: configManager.themeTextSecondary; font.pixelSize: 12; Layout.fillWidth: true
                }
            }

            Text {
                text: "Theme is stored in ~/.config/lumalarm/theme.conf and reloads live when you edit it."
                color: configManager.themeTextSecondary; font.pixelSize: 12; wrapMode: Text.Wrap; Layout.fillWidth: true
            }

            Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: Qt.rgba(1,1,1,0.07) }

            // ================================================================
            // DEFAULT (functional settings)
            // ================================================================
            Text { text: "Default"; color: configManager.themeTextSecondary; font.pixelSize: 13; font.bold: true; Layout.bottomMargin: -4 }

            // Window Controls
            Text { text: "Window Controls"; color: configManager.themeTextPrimary; font.pixelSize: 15; Layout.bottomMargin: -6 }
            RowLayout {
                Layout.fillWidth: true; spacing: 14
                Text { text: "Show Minimize Button"; color: configManager.themeTextPrimary; font.pixelSize: 15; Layout.fillWidth: true }
                ToggleSwitch {
                    checked: themeManager.show_minimize_button
                    onToggled: function(checked) { themeManager.show_minimize_button = checked }
                }
            }
            RowLayout {
                Layout.fillWidth: true; spacing: 14
                Text { text: "Show Maximize Button"; color: configManager.themeTextPrimary; font.pixelSize: 15; Layout.fillWidth: true }
                ToggleSwitch {
                    checked: themeManager.show_maximize_button
                    onToggled: function(checked) { themeManager.show_maximize_button = checked }
                }
            }
            RowLayout {
                Layout.fillWidth: true; spacing: 14
                Text { text: "Show Close Button"; color: configManager.themeTextPrimary; font.pixelSize: 15; Layout.fillWidth: true }
                ToggleSwitch {
                    checked: themeManager.show_close_button
                    onToggled: function(checked) { themeManager.show_close_button = checked }
                }
            }

            // System Tray
            Text { text: "System Tray"; color: configManager.themeTextPrimary; font.pixelSize: 15; Layout.bottomMargin: -6 }
            RowLayout {
                Layout.fillWidth: true; spacing: 14
                Text { text: "Show system tray icon"; color: configManager.themeTextPrimary; font.pixelSize: 15; Layout.fillWidth: true }
                ToggleSwitch {
                    checked: themeManager.show_tray_icon
                    onToggled: function(checked) { themeManager.show_tray_icon = checked }
                }
            }

            Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: Qt.rgba(1,1,1,0.07) }

            Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: Qt.rgba(1,1,1,0.07) }

            // Alarm Defaults
            Text { text: "Alarm Defaults"; color: configManager.themeTextSecondary; font.pixelSize: 13; font.bold: true; Layout.bottomMargin: -4 }
            RowLayout {
                Layout.fillWidth: true; spacing: 14
                Text { text: "Default Wake Mode"; color: configManager.themeTextPrimary; font.pixelSize: 15; Layout.preferredWidth: 180 }
                Item { Layout.fillWidth: true }
                RoundedCombo {
                    id: wakeCombo
                    model: ["mem", "disk", "none"]
                    currentIndex: Math.max(0, wakeCombo.find(configManager.defaultWakeMode))
                    Layout.preferredWidth: 140
                    onActivated: configManager.defaultWakeMode = currentText
                }
            }

            RowLayout {
                Layout.fillWidth: true; spacing: 14
                Text { text: "Time picker style"; color: configManager.themeTextPrimary; font.pixelSize: 15; Layout.preferredWidth: 180 }
                Item { Layout.fillWidth: true }
                RoundedCombo {
                    id: timePickerCombo
                    model: ["Wheel", "Spin"]
                    currentIndex: configManager.timePickerMode === "spin" ? 1 : 0
                    Layout.preferredWidth: 140
                    onActivated: configManager.timePickerMode = currentIndex === 1 ? "spin" : "wheel"
                }
            }

            // ================================================================
            // TEST ALARM
            // ================================================================
            Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: Qt.rgba(1,1,1,0.07) }

            Text { text: "Test alarm"; color: configManager.themeTextSecondary; font.pixelSize: 13; font.bold: true; Layout.bottomMargin: -4 }
            Text {
                text: "Arm a real wake without creating an alarm, to check that this machine actually wakes."
                color: configManager.themeTextSecondary; font.pixelSize: 12
                wrapMode: Text.WordWrap; Layout.fillWidth: true; Layout.bottomMargin: 4
            }

            ColumnLayout {
                id: testAlarmCard
                Layout.fillWidth: true; spacing: 10

                // Presets (60 s .. 60 min) plus a custom numeric field.
                RowLayout {
                    Layout.fillWidth: true; spacing: 14
                    Text { text: "Wake in"; color: configManager.themeTextPrimary; font.pixelSize: 15; Layout.preferredWidth: 80 }
                    Item { Layout.fillWidth: true }
                    RoundedCombo {
                        id: testWakeCombo
                        model: ["60 s", "2 min", "5 min", "10 min", "30 min", "60 min", "Custom…"]
                        // Initial selection comes from the persisted duration.
                        currentIndex: presetIndexFor(configManager.testWakeDuration())
                        visible: !customField.visible
                        Layout.preferredWidth: 130
                        onActivated: {
                            if (currentIndex === 6) {
                                customField.visible = true
                                customField.text = configManager.testWakeDuration()
                                customField.forceActiveFocus()
                                customField.selectAll()
                            } else {
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

                TextField {
                    id: customField
                    visible: false
                    Layout.fillWidth: true
                    height: 32
                    color: configManager.themeTextPrimary
                    font.pixelSize: 14
                    leftPadding: 10
                    placeholderText: "Custom seconds (60-3600)"
                    validator: IntValidator { bottom: 60; top: 3600 }
                    background: Rectangle {
                        radius: 7
                        color: Qt.rgba(1,1,1,0.08)
                        border.color: Qt.rgba(1,1,1,0.12); border.width: 1
                    }
                    onEditingFinished: {
                        const v = parseInt(text)
                        if (!isNaN(v))
                            persistTestWake(v)
                    }
                }

                RowLayout {
                    Layout.fillWidth: true; spacing: 10
                    GlassButton {
                        text: "Arm test alarm"
                        pixelSize: 13
                        enabled: wakeManager.wakeState !== "armed"
                        onClicked: wakeManager.armTestWake(testAlarmCard.selectedSeconds())
                    }
                    GlassButton {
                        text: "Cancel test alarm"
                        pixelSize: 13
                        enabled: wakeManager.wakeState === "armed"
                        onClicked: wakeManager.cancelTestWake()
                    }
                    Item { Layout.fillWidth: true }
                    Text {
                        text: wakeManager.wakeState
                        color: configManager.themeTextSecondary; font.pixelSize: 12
                    }
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
                function selectedSeconds() {
                    if (testWakeCombo.currentIndex === 6)
                        return configManager.testWakeDuration()
                    return testWakeCombo.presetSeconds(testWakeCombo.currentIndex)
                }
                // Persist on every change so the choice survives a restart.
                function persistTestWake(seconds) {
                    configManager.setTestWakeDuration(seconds)
                }

                Connections {
                    target: wakeManager
                    function onTestWakeArmed(seconds) {
                        testAlarmStatus.text = "Test alarm armed in " + seconds + " s"
                    }
                    function onTestWakeFailed(reason) {
                        testAlarmStatus.text = "Test alarm failed: " + reason
                    }
                    function onWakeStateChanged() {
                        if (wakeManager.wakeState === "idle")
                            testAlarmStatus.text = ""
                    }
                }
            }

            Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: Qt.rgba(1,1,1,0.07) }

            // Behavior
            Text { text: "Behavior"; color: configManager.themeTextSecondary; font.pixelSize: 13; font.bold: true; Layout.bottomMargin: -4 }
            RowLayout {
                Layout.fillWidth: true; spacing: 14
                Text { text: "Show milliseconds in stopwatch"; color: configManager.themeTextPrimary; font.pixelSize: 15; Layout.fillWidth: true }
                ToggleSwitch {
                    checked: configManager.stopwatchShowMs
                    onToggled: function(checked) { configManager.stopwatchShowMs = checked }
                }
            }

            Item { Layout.fillHeight: true }

            RowLayout {
                Layout.fillWidth: true; Layout.bottomMargin: 4
                Item { Layout.fillWidth: true }
                GlassButton {
                    text: "Reset to Defaults"
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
                        configManager.defaultWakeMode = "mem"
                        configManager.stopwatchShowMs = true
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

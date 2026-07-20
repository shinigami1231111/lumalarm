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

            // ---- Appearance ----
            Text { text: "Appearance"; color: configManager.themeTextSecondary; font.pixelSize: 13; font.bold: true; Layout.bottomMargin: -4 }

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
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 14
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

            Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: Qt.rgba(1,1,1,0.07) }

            // ---- Theming / ricing ----
            Text { text: "Theming"; color: configManager.themeTextSecondary; font.pixelSize: 13; font.bold: true; Layout.bottomMargin: -4 }

            RowLayout {
                Layout.fillWidth: true; spacing: 14
                Text { text: "Blur Mode"; color: configManager.themeTextPrimary; font.pixelSize: 15; Layout.preferredWidth: 180 }
                Item { Layout.fillWidth: true }
                RowLayout {
                    spacing: 6
                    GlassButton {
                        text: "Compositor"
                        pixelSize: 12; implicitWidth: 100; implicitHeight: 30; radius: 7
                        property bool active: themeManager.blur_mode === "compositor"
                        baseColor: active ? Qt.rgba(0.2, 0.6, 1, 0.28) : Qt.rgba(1, 1, 1, 0.06)
                        textColor: active ? Qt.rgba(0.6, 0.85, 1, 1) : configManager.themeTextSecondary
                        borderColor: active ? Qt.rgba(0.4, 0.8, 1, 0.4) : Qt.rgba(1, 1, 1, 0.1)
                        onClicked: themeManager.blur_mode = "compositor"
                    }
                    GlassButton {
                        text: "App (fallback)"
                        pixelSize: 12; implicitWidth: 110; implicitHeight: 30; radius: 7
                        property bool active: themeManager.blur_mode === "app"
                        baseColor: active ? Qt.rgba(0.2, 0.6, 1, 0.28) : Qt.rgba(1, 1, 1, 0.06)
                        textColor: active ? Qt.rgba(0.6, 0.85, 1, 1) : configManager.themeTextSecondary
                        borderColor: active ? Qt.rgba(0.4, 0.8, 1, 0.4) : Qt.rgba(1, 1, 1, 0.1)
                        onClicked: themeManager.blur_mode = "app"
                    }
                }
            }

            Text {
                text: "Compositor: window stays transparent and Hyprland/Sway blur the desktop behind it. App: panel is opaque (no compositor blur needed)."
                color: Qt.rgba(1, 0.7, 0.3, 0.85)
                font.pixelSize: 12; wrapMode: Text.Wrap; Layout.fillWidth: true
            }

            Text { text: "Palette Presets"; color: configManager.themeTextPrimary; font.pixelSize: 15; Layout.bottomMargin: -6 }
            Flow {
                Layout.fillWidth: true
                spacing: 8
                Repeater {
                    model: themeManager.presetNames()
                    GlassButton {
                        text: modelData
                        pixelSize: 12; implicitHeight: 32; radius: 7; implicitWidth: Math.max(120, text.length * 8 + 24)
                        onClicked: themeManager.applyPreset(modelData)
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true; spacing: 14
                Text { text: "Border Color"; color: configManager.themeTextPrimary; font.pixelSize: 15; Layout.preferredWidth: 180 }
                Item { Layout.fillWidth: true }
                ColorEdit { color: themeManager.border_color; onPicked: themeManager.border_color = hex }
            }

            RowLayout {
                Layout.fillWidth: true; spacing: 14
                Text { text: "Corner Radius"; color: configManager.themeTextPrimary; font.pixelSize: 15; Layout.preferredWidth: 180 }
                Item { Layout.fillWidth: true }
                Spinny { from: 0; to: 40; value: themeManager.corner_radius; suffix: " px"; onChanged: themeManager.corner_radius = v }
            }

            RowLayout {
                Layout.fillWidth: true; spacing: 14
                Text { text: "Card Opacity"; color: configManager.themeTextPrimary; font.pixelSize: 15; Layout.preferredWidth: 180 }
                Item { Layout.fillWidth: true }
                Slider {
                    id: cardOpacitySlider
                    from: 0.0; to: 1.0; stepSize: 0.01
                    value: themeManager.card_opacity
                    implicitWidth: 200
                    onPressedChanged: { if (!pressed && value !== themeManager.card_opacity) themeManager.card_opacity = value }
                }
                Text { text: Math.round(cardOpacitySlider.value * 100) + "%"; color: configManager.themeTextSecondary; font.pixelSize: 14; Layout.preferredWidth: 46; horizontalAlignment: Text.AlignRight }
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

            // ---- Interface ----
            Text { text: "Interface"; color: configManager.themeTextSecondary; font.pixelSize: 13; font.bold: true; Layout.bottomMargin: -4 }

            RowLayout {
                Layout.fillWidth: true; spacing: 14
                Text { text: "Time Picker Style"; color: configManager.themeTextPrimary; font.pixelSize: 15; Layout.preferredWidth: 180 }
                Item { Layout.fillWidth: true }
                RowLayout {
                    spacing: 6
                    Repeater {
                        id: styleRepeater
                        model: [
                            {t: "Wheels", v: 0},
                            {t: "Dual Clocks", v: 1},
                            {t: "Single Clock", v: 2}
                        ]
                        GlassButton {
                            text: modelData.t
                            pixelSize: 12
                            implicitWidth: 92; implicitHeight: 30; radius: 7
                            property bool active: configManager.timePickerStyle === modelData.v
                            baseColor: active ? Qt.rgba(0.2, 0.6, 1, 0.28) : Qt.rgba(1, 1, 1, 0.06)
                            textColor: active ? Qt.rgba(0.6, 0.85, 1, 1) : configManager.themeTextSecondary
                            borderColor: active ? Qt.rgba(0.4, 0.8, 1, 0.4) : Qt.rgba(1, 1, 1, 0.1)
                            onClicked: configManager.timePickerStyle = modelData.v
                        }
                    }
                }
            }

            Text {
                text: "Note: the Dual Clocks and Single Clock pickers are rendered with Canvas-style drawing and may feel laggy or glitchy on some systems. Wheels is the smoothest option."
                color: Qt.rgba(1, 0.7, 0.3, 0.85)
                font.pixelSize: 12
                wrapMode: Text.Wrap
                Layout.fillWidth: true
            }

            Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: Qt.rgba(1,1,1,0.07) }

            // ---- Alarm Defaults ----
            Text { text: "Alarm Defaults"; color: configManager.themeTextSecondary; font.pixelSize: 13; font.bold: true; Layout.bottomMargin: -4 }

            RowLayout {
                Layout.fillWidth: true; spacing: 14
                Text { text: "Default Snooze"; color: configManager.themeTextPrimary; font.pixelSize: 15; Layout.preferredWidth: 180 }
                Item { Layout.fillWidth: true }
                Spinny { from: 1; to: 60; value: configManager.defaultSnooze; suffix: " min"; onChanged: configManager.defaultSnooze = v }
            }
            RowLayout {
                Layout.fillWidth: true; spacing: 14
                Text { text: "Default Fade"; color: configManager.themeTextPrimary; font.pixelSize: 15; Layout.preferredWidth: 180 }
                Item { Layout.fillWidth: true }
                Spinny { from: 5; to: 60; value: configManager.defaultFadeDuration; step: 5; suffix: " sec"; onChanged: configManager.defaultFadeDuration = v }
            }
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

            Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: Qt.rgba(1,1,1,0.07) }

            // ---- Behavior ----
            Text { text: "Behavior"; color: configManager.themeTextSecondary; font.pixelSize: 13; font.bold: true; Layout.bottomMargin: -4 }

            RowLayout {
                Layout.fillWidth: true; spacing: 14
                Text { text: "Show milliseconds in stopwatch"; color: configManager.themeTextPrimary; font.pixelSize: 15; Layout.fillWidth: true }
                ToggleSwitch {
                    checked: configManager.stopwatchShowMs
                    onToggled: function(v) { configManager.stopwatchShowMs = v }
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
                        themeManager.card_opacity = 0.55
                        themeManager.font_family = ""
                        themeManager.corner_radius = 18
                        configManager.defaultSnooze = 1
                        configManager.defaultFadeDuration = 15
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
                        Text { text: "▲"; anchors.centerIn: parent; color: configManager.themeTextSecondary; font.pixelSize: 10 }
                        MouseArea { id: upMa; anchors.fill: parent; hoverEnabled: true
                            onClicked: sn.current = Math.min(sn.to, sn.current + sn.step) } }
                    Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 14; radius: 4
                        color: dnMa.containsMouse ? Qt.rgba(1,1,1,0.15) : "transparent"
                        Text { text: "▼"; anchors.centerIn: parent; color: configManager.themeTextSecondary; font.pixelSize: 10 }
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

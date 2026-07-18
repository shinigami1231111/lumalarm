import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import GlassAlarm

ColumnLayout {
    id: root
    spacing: 16

    Text {
        text: "Theme"
        color: configManager.themeTextPrimary
        font.pixelSize: 22
        font.bold: true
    }

    Rectangle {
        Layout.fillWidth: true; Layout.preferredHeight: 1
        color: Qt.rgba(1, 1, 1, 0.08)
    }

    RowLayout {
        Layout.fillWidth: true; spacing: 24

        ColumnLayout {
            Layout.fillWidth: true; spacing: 8
            ColorField { label: "Background"; prop: "themeBg" }
            ColorField { label: "Accent"; prop: "themeAccent" }
            ColorField { label: "Text Primary"; prop: "themeTextPrimary" }
            ColorField { label: "Text Secondary"; prop: "themeTextSecondary" }
        }

        ColumnLayout {
            Layout.fillWidth: true; spacing: 8
            ThemeSpinBox { label: "Bg Opacity"; fromVal: 0.1; toVal: 1.0; cfgProp: "themeOpacity"; step: 0.05; decimals: 2 }
        }
    }

    Rectangle {
        Layout.fillWidth: true; Layout.preferredHeight: 1
        color: Qt.rgba(1, 1, 1, 0.08)
    }

    RowLayout {
        Layout.fillWidth: true; spacing: 14
        Text { text: "Show ms in stopwatch"; color: configManager.themeTextSecondary; font.pixelSize: 16; Layout.alignment: Qt.AlignVCenter }
        Item { Layout.fillWidth: true }
        Item { width: 44; height: 24
            Rectangle {
                anchors.fill: parent; radius: 12
                color: configManager.stopwatchShowMs ? Qt.lighter(configManager.themeAccent, 1.4) : Qt.rgba(1, 1, 1, 0.15)
                border.color: configManager.stopwatchShowMs ? configManager.themeAccent : Qt.rgba(1, 1, 1, 0.2)
                border.width: 1
                Behavior on color { ColorAnimation { duration: 200 } }

                Rectangle {
                    x: configManager.stopwatchShowMs ? 22 : 2; y: 2; width: 20; height: 20; radius: 10
                    color: configManager.stopwatchShowMs ? configManager.themeAccent : Qt.rgba(1, 1, 1, 0.5)
                    Behavior on x { NumberAnimation { duration: 200; easing.type: Easing.OutCubic } }
                    Behavior on color { ColorAnimation { duration: 200 } }
                }

                MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: configManager.stopwatchShowMs = !configManager.stopwatchShowMs }
            }
        }
    }

    Item { Layout.fillHeight: true }

    RowLayout {
        Layout.fillWidth: true; Layout.bottomMargin: 4; spacing: 14
        Item { Layout.fillWidth: true }
        GlassButton {
            text: "Reset Defaults"; pixelSize: 14
            baseColor: Qt.rgba(1, 0.3, 0.3, 0.12); hoverColor: Qt.rgba(1, 0.3, 0.3, 0.2)
            onClicked: {
                configManager.themeBg = "#0d0d1a"
                configManager.themeAccent = "#3d7fff"
                configManager.themeTextPrimary = "#ffffff"
                configManager.themeTextSecondary = "#808090"
                configManager.themeOpacity = 0.90
                configManager.stopwatchShowMs = true
            }
        }
    }

    component ColorField: RowLayout {
        id: cf; Layout.fillWidth: true; spacing: 14
        property string label; property string prop

        Text { text: cf.label; color: configManager.themeTextSecondary; font.pixelSize: 16; Layout.preferredWidth: 150 }

        Rectangle {
            id: swatch; Layout.preferredWidth: 28; Layout.preferredHeight: 28; radius: 7
            border.color: Qt.rgba(1, 1, 1, 0.2); border.width: 1
            color: configManager[cf.prop] || "#000000"
            MouseArea {
                anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                onClicked: { picker.currentColor = configManager[cf.prop] || "#000000"; picker.prop = cf.prop; picker.open() }
            }
        }

        TextField {
            id: colorInput; Layout.fillWidth: true; height: 32
            color: configManager.themeTextPrimary; font.pixelSize: 14
            text: configManager[cf.prop] || ""; leftPadding: 10
            background: Rectangle { radius: 7; color: Qt.rgba(1, 1, 1, 0.06); border.color: Qt.rgba(1, 1, 1, 0.1); border.width: 1 }
            onTextChanged: { if (text.length === 7 && text[0] === '#') configManager[cf.prop] = text }
        }

        ColorPickerPopup { id: picker }
    }

    component ThemeSpinBox: RowLayout {
        id: cs; Layout.fillWidth: true; spacing: 14
        property string label; property real fromVal: 0; property real toVal: 1; property string cfgProp; property real step: 1; property int decimals: 0

        Text { text: cs.label; color: configManager.themeTextSecondary; font.pixelSize: 16; Layout.preferredWidth: 150 }

        Rectangle {
            Layout.fillWidth: true; Layout.preferredHeight: 38; radius: 8
            color: Qt.rgba(1, 1, 1, 0.06); border.color: Qt.rgba(1, 1, 1, 0.1); border.width: 1

            RowLayout {
                anchors.fill: parent; anchors.margins: 2; spacing: 2

                Text {
                    Layout.fillWidth: true; Layout.fillHeight: true
                    verticalAlignment: Qt.AlignVCenter; horizontalAlignment: Qt.AlignHCenter
                    color: configManager.themeTextPrimary; font.pixelSize: 16; font.bold: true
                    text: {
                        var v = configManager[cs.cfgProp]
                        if (v === undefined) v = 0
                        return Number(v).toFixed(cs.decimals)
                    }
                }

                ColumnLayout { spacing: 1; Layout.preferredWidth: 22
                    Rectangle {
                        Layout.fillWidth: true; Layout.preferredHeight: 14; radius: 4
                        color: mouseUp.containsMouse ? Qt.rgba(1,1,1,0.15) : "transparent"
                        Text { text: "▲"; anchors.centerIn: parent; color: configManager.themeTextSecondary; font.pixelSize: 10 }
                        MouseArea {
                            id: mouseUp; anchors.fill: parent; hoverEnabled: true
                            onClicked: {
                                var v = (configManager[cs.cfgProp] || 0) + cs.step
                                configManager[cs.cfgProp] = Math.min(cs.toVal, v)
                            }
                        }
                    }
                    Rectangle {
                        Layout.fillWidth: true; Layout.preferredHeight: 14; radius: 4
                        color: mouseDn.containsMouse ? Qt.rgba(1,1,1,0.15) : "transparent"
                        Text { text: "▼"; anchors.centerIn: parent; color: configManager.themeTextSecondary; font.pixelSize: 10 }
                        MouseArea {
                            id: mouseDn; anchors.fill: parent; hoverEnabled: true
                            onClicked: {
                                var v = (configManager[cs.cfgProp] || 0) - cs.step
                                configManager[cs.cfgProp] = Math.max(cs.fromVal, v)
                            }
                        }
                    }
                }
            }
        }
    }

    component ColorPickerPopup: Popup {
        id: popup
        property string prop
        property string currentColor: "#ffffff"

        x: Math.max(0, Math.min(200, popup.parent ? (popup.parent.width - width) / 2 : 0))
        y: popup.parent ? popup.parent.height + 4 : 0
        width: 300; height: 360
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        padding: 0

        background: Rectangle {
            color: Qt.rgba(0.08, 0.08, 0.12, 0.95)
            radius: 12
            border.color: Qt.rgba(1, 1, 1, 0.12)
            border.width: 1
        }

        function hsvToHex(h, s, v) {
            h = ((h % 360) + 360) % 360
            var hi = Math.floor(h / 60)
            var f = h / 60 - hi
            var p = v * (1 - s)
            var q = v * (1 - f * s)
            var t = v * (1 - (1 - f) * s)
            var r = 0, g = 0, b = 0
            if (hi === 0) { r = v; g = t; b = p }
            else if (hi === 1) { r = q; g = v; b = p }
            else if (hi === 2) { r = p; g = v; b = t }
            else if (hi === 3) { r = p; g = q; b = v }
            else if (hi === 4) { r = t; g = p; b = v }
            else { r = v; g = p; b = q }
            var ri = Math.round(r * 255)
            var gi = Math.round(g * 255)
            var bi = Math.round(b * 255)
            return '#' + ri.toString(16).padStart(2, '0') + gi.toString(16).padStart(2, '0') + bi.toString(16).padStart(2, '0')
        }

        function hexToHsv(hex) {
            hex = hex.replace('#', '')
            if (hex.length < 6) hex = hex.padEnd(6, '0')
            var r = parseInt(hex.substring(0, 2), 16) / 255
            var g = parseInt(hex.substring(2, 4), 16) / 255
            var b = parseInt(hex.substring(4, 6), 16) / 255
            var mx = Math.max(r, g, b), mn = Math.min(r, g, b)
            var h = 0, s = 0, v = mx, d = mx - mn
            if (mx !== 0) s = d / mx
            if (mx !== mn) {
                if (mx === r) h = ((g - b) / d + (g < b ? 6 : 0)) * 60
                else if (mx === g) h = ((b - r) / d + 2) * 60
                else h = ((r - g) / d + 4) * 60
            }
            return { h: h, s: s, v: v }
        }

        property var hsv: hexToHsv(currentColor)

        function updateConfig() {
            var hex = hsvToHex(hsv.h, hsv.s, hsv.v)
            if (prop) configManager[prop] = hex
        }

        onCurrentColorChanged: { hsv = hexToHsv(currentColor) }

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 20
            spacing: 14

            Rectangle {
                id: svSquare
                Layout.fillWidth: true
                Layout.preferredHeight: 180
                clip: true

                Rectangle { anchors.fill: parent; color: "white" }

                Rectangle {
                    anchors.fill: parent
                    gradient: Gradient {
                        orientation: Gradient.Horizontal
                        GradientStop { position: 0.0; color: "white" }
                        GradientStop { position: 1.0; color: popup.hsvToHex(popup.hsv.h, 1, 1) }
                    }
                }

                Rectangle {
                    anchors.fill: parent
                    gradient: Gradient {
                        orientation: Gradient.Vertical
                        GradientStop { position: 0.0; color: "transparent" }
                        GradientStop { position: 1.0; color: "black" }
                    }
                }

                MouseArea {
                    anchors.fill: parent
                    onPressed: pickSV(mouse.x, mouse.y)
                    onPositionChanged: { if (pressed) pickSV(mouse.x, mouse.y) }
                    function pickSV(mx, my) {
                        popup.hsv.s = Math.min(1, Math.max(0, mx / svSquare.width))
                        popup.hsv.v = Math.min(1, Math.max(0, 1 - my / svSquare.height))
                        popup.updateConfig()
                    }
                }

                Rectangle {
                    x: popup.hsv.s * parent.width - 5
                    y: (1 - popup.hsv.v) * parent.height - 5
                    width: 10; height: 10; radius: 5
                    border.color: "white"; border.width: 2
                    color: "transparent"
                }
            }

            Rectangle {
                id: hueBar
                Layout.fillWidth: true
                Layout.preferredHeight: 14
                radius: 7
                gradient: Gradient {
                    orientation: Gradient.Horizontal
                    GradientStop { position: 0.00; color: "#ff0000" }
                    GradientStop { position: 0.17; color: "#ffff00" }
                    GradientStop { position: 0.33; color: "#00ff00" }
                    GradientStop { position: 0.50; color: "#00ffff" }
                    GradientStop { position: 0.67; color: "#0000ff" }
                    GradientStop { position: 0.83; color: "#ff00ff" }
                    GradientStop { position: 1.00; color: "#ff0000" }
                }

                MouseArea {
                    anchors.fill: parent
                    onPressed: pickHue(mouse.x)
                    onPositionChanged: { if (pressed) pickHue(mouse.x) }
                    function pickHue(mx) {
                        popup.hsv.h = Math.min(360, Math.max(0, (mx / hueBar.width) * 360))
                        popup.updateConfig()
                    }
                }

                Rectangle {
                    x: (popup.hsv.h / 360) * parent.width - 5
                    y: -1; width: 10; height: 16; radius: 3
                    border.color: "white"; border.width: 2
                    color: "transparent"
                }
            }

            RowLayout {
                Layout.fillWidth: true; spacing: 14
                Rectangle { Layout.preferredWidth: 28; Layout.preferredHeight: 28; radius: 6; border.color: Qt.rgba(1, 1, 1, 0.2); border.width: 1; color: popup.currentColor }
                Rectangle { Layout.preferredWidth: 28; Layout.preferredHeight: 28; radius: 6; border.color: Qt.rgba(1, 1, 1, 0.2); border.width: 1; color: popup.hsvToHex(popup.hsv.h, popup.hsv.s, popup.hsv.v) }
                TextField {
                    Layout.fillWidth: true; height: 32
                    color: configManager.themeTextPrimary; font.pixelSize: 14
                    text: popup.hsvToHex(popup.hsv.h, popup.hsv.s, popup.hsv.v)
                    leftPadding: 10
                    background: Rectangle { radius: 7; color: Qt.rgba(1, 1, 1, 0.08); border.color: Qt.rgba(1, 1, 1, 0.12); border.width: 1 }
                    onTextChanged: {
                        if (text.length === 7 && text[0] === '#') {
                            popup.hsv = popup.hexToHsv(text)
                            if (popup.prop) configManager[popup.prop] = text
                        }
                    }
                }
            }
        }
    }
}

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import GlassAlarm

ComboBox {
    id: cmb

    // Coerce the theme strings to colours once so .r/.g/.b are readable and
    // the hover help follows the active palette instead of a hard-coded grey.
    readonly property color themedBg: configManager.themeBg
    readonly property color themedAccent: configManager.themeAccent

    // Optional one-line-per-item hover help. Empty by default so every other
    // combo keeps its current behaviour; callers with longer explanations
    // (the RTCwake method picker) supply one string per model row.
    property var tooltips: []

    function tipAt(i) {
        return (i >= 0 && i < tooltips.length && tooltips[i]) ? tooltips[i] : ""
    }

    readonly property string currentTooltip: cmb.tipAt(cmb.currentIndex)

    // Explicit ToolTip instances are required for one-off styling; the shared
    // attached ToolTip cannot be restyled outside a custom Qt Quick style.
    ToolTip {
        id: comboTip
        text: cmb.currentTooltip
        visible: cmb.currentTooltip.length > 0 && cmb.hovered
        delay: 300
        timeout: 8000
        padding: 8
        contentItem: Text {
            text: comboTip.text
            color: configManager.themeTextPrimary
            font.pixelSize: 12
            wrapMode: Text.WordWrap
        }
        background: Rectangle {
            color: Qt.rgba(cmb.themedBg.r, cmb.themedBg.g, cmb.themedBg.b, 0.98)
            border.color: cmb.themedAccent
            border.width: 1
            radius: 8
        }
    }

    background: Rectangle {
        radius: 8
        color: Qt.rgba(1, 1, 1, 0.06)
        border.color: Qt.rgba(1, 1, 1, 0.1)
        border.width: 1

        Text {
            text: "▾"
            color: configManager.themeTextSecondary
            font.pixelSize: 14
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            anchors.rightMargin: 8
        }
    }

    contentItem: Text {
        text: cmb.currentText
        color: configManager.themeTextPrimary
        font.pixelSize: 14
        leftPadding: 10
        verticalAlignment: Qt.AlignVCenter
    }

    indicator: Item {}

    delegate: ItemDelegate {
        width: cmb.width
        height: 36
        readonly property string tipText: cmb.tipAt(index)
        ToolTip {
            id: rowTip
            text: tipText
            visible: tipText.length > 0 && hovered
            delay: 300
            timeout: 8000
            padding: 8
            contentItem: Text {
                text: rowTip.text
                color: configManager.themeTextPrimary
                font.pixelSize: 12
                wrapMode: Text.WordWrap
            }
            background: Rectangle {
                color: Qt.rgba(cmb.themedBg.r, cmb.themedBg.g, cmb.themedBg.b, 0.98)
                border.color: cmb.themedAccent
                border.width: 1
                radius: 8
            }
        }
        contentItem: Text {
            text: modelData
            color: cmb.currentIndex === index ? configManager.themeTextPrimary : configManager.themeTextSecondary
            font.pixelSize: 14
            leftPadding: 8
            verticalAlignment: Qt.AlignVCenter
        }
        background: Rectangle {
            color: index === cmb.highlightedIndex ? Qt.rgba(1, 1, 1, 0.1) : "transparent"
            radius: 4
        }
    }

    popup: Popup {
        y: cmb.height + 4
        width: cmb.width
        padding: 4
        background: Rectangle {
            color: Qt.rgba(0.08, 0.08, 0.12, 0.95)
            radius: 10
            border.color: Qt.rgba(1, 1, 1, 0.1)
            border.width: 1
        }
        contentItem: ListView {
            clip: true
            implicitHeight: contentHeight
            model: cmb.delegateModel
            currentIndex: cmb.highlightedIndex
        }
    }
}

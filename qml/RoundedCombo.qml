import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import GlassAlarm

ComboBox {
    id: cmb

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

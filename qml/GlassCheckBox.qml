import QtQuick
import QtQuick.Controls
import GlassAlarm

CheckBox {
    id: root

    property alias labelText: label.text

    contentItem: Text {
        id: label
        color: configManager.themeTextPrimary
        font.pixelSize: 16
        verticalAlignment: Text.AlignVCenter
        leftPadding: 24
    }

    indicator: Rectangle {
        implicitWidth: 20; implicitHeight: 20
        x: 0; y: parent.height / 2 - height / 2
        radius: 5
        color: root.checked ? Qt.rgba(0.2, 0.6, 1, 0.5) : Qt.rgba(1, 1, 1, 0.15)
        border.color: Qt.rgba(1, 1, 1, 0.3); border.width: 1
        Text {
            anchors.centerIn: parent
            text: "\u2713"; color: "#FFFFFF"; font.pixelSize: 16
            visible: root.checked
        }
    }
}

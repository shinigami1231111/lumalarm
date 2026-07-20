import QtQuick
import QtQuick.Controls
import GlassAlarm

Rectangle {
    id: root
    property bool checked: false
    property color onColor: configManager ? configManager.themeAccent : Qt.rgba(0.24, 0.5, 1, 1)
    signal toggled(bool value)

    implicitWidth: 48
    implicitHeight: 28
    radius: 14
    color: root.checked
        ? Qt.lighter(root.onColor, 1.3)
        : Qt.rgba(1, 1, 1, 0.13)
    border.color: root.checked ? root.onColor : Qt.rgba(1, 1, 1, 0.18)
    border.width: 1

    Behavior on color { ColorAnimation { duration: 200 } }
    Behavior on border.color { ColorAnimation { duration: 200 } }

    Rectangle {
        x: root.checked ? parent.width - 24 : 2
        y: 2; width: 22; height: 22; radius: 11
        color: root.checked ? root.onColor : Qt.rgba(1, 1, 1, 0.5)
        Behavior on x { NumberAnimation { duration: 200; easing.type: Easing.OutCubic } }
        Behavior on color { ColorAnimation { duration: 200 } }
    }

    MouseArea {
        anchors.fill: parent
        cursorShape: Qt.PointingHandCursor
        onClicked: { root.checked = !root.checked; root.toggled(root.checked) }
    }
}

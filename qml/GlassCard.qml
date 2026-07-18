import QtQuick
import GlassAlarm

Rectangle {
    id: root

    property color cardColor: Qt.rgba(1, 1, 1, configManager.themeCardOpacity)
    property color borderColor: Qt.rgba(1, 1, 1, 0.08)
    property int cardRadius: 14

    radius: cardRadius
    color: cardColor
    border.color: borderColor
    border.width: 1
}

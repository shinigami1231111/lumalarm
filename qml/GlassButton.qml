import QtQuick
import QtQuick.Controls

Rectangle {
    id: root

    property string text: ""
    property color baseColor: Qt.rgba(1, 1, 1, 0.10)
    property color hoverColor: Qt.rgba(1, 1, 1, 0.18)
    property color pressedColor: Qt.rgba(1, 1, 1, 0.26)
    property color textColor: configManager ? configManager.themeTextPrimary : "#FFFFFF"
    property color borderColor: Qt.rgba(1, 1, 1, 0.10)
    property int pixelSize: 14
    property int buttonRadius: 12
    property bool filled: false
    property color accentColor: configManager ? configManager.themeAccent : Qt.rgba(0.24, 0.5, 1, 1)

    signal clicked()

    width: implicitWidth
    height: implicitHeight
    implicitWidth: label.implicitWidth + 44
    implicitHeight: label.implicitHeight + 26

    radius: root.buttonRadius
    color: mouseArea.containsPress ? pressedColor
          : (mouseArea.containsMouse ? hoverColor
          : (root.filled ? Qt.rgba(root.accentColor.r, root.accentColor.g, root.accentColor.b, 0.22) : baseColor))
    border.color: root.filled ? Qt.rgba(root.accentColor.r, root.accentColor.g, root.accentColor.b, 0.5) : borderColor
    border.width: 1

    Behavior on color { ColorAnimation { duration: 140 } }
    Behavior on border.color { ColorAnimation { duration: 140 } }

    scale: mouseArea.containsPress ? 0.97 : 1.0
    Behavior on scale { NumberAnimation { duration: 90; easing.type: Easing.OutCubic } }

    Text {
        id: label
        anchors.centerIn: parent
        text: root.text
        color: root.filled ? root.accentColor : root.textColor
        font.pixelSize: root.pixelSize
        font.family: "Segoe UI, Helvetica, Arial, sans-serif"
        font.bold: root.filled
    }

    MouseArea {
        id: mouseArea
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: root.clicked()
    }
}

import QtQuick
import QtQuick.Controls

Rectangle {
    id: root

    property string text: ""
    property color baseColor: Qt.rgba(1, 1, 1, 0.15)
    property color hoverColor: Qt.rgba(1, 1, 1, 0.25)
    property color pressedColor: Qt.rgba(1, 1, 1, 0.35)
    property color textColor: "#FFFFFF"
    property color borderColor: Qt.rgba(1, 1, 1, 0.1)
    property int pixelSize: 14
    property int buttonRadius: 12

    signal clicked()

    width: implicitWidth
    height: implicitHeight
    implicitWidth: label.implicitWidth + 44
    implicitHeight: label.implicitHeight + 28

    radius: root.buttonRadius
    color: mouseArea.containsPress ? pressedColor : (mouseArea.containsMouse ? hoverColor : baseColor)
    border.color: root.borderColor
    border.width: 1

    Behavior on color {
        ColorAnimation { duration: 150 }
    }

    Text {
        id: label
        anchors.centerIn: parent
        text: root.text
        color: root.textColor
        font.pixelSize: root.pixelSize
        font.family: "Segoe UI, Helvetica, Arial, sans-serif"
    }

    MouseArea {
        id: mouseArea
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: root.clicked()
    }
}

import QtQuick
import QtQuick.Controls
import GlassAlarm

Row {
    id: root
    property var items: []
    property string current: ""
    signal selected(string mode)

    spacing: 4
    height: 40

    Repeater {
        model: root.items
        Rectangle {
            id: seg
            property bool isActive: root.current === modelData.mode
            width: segRow.implicitWidth + 28; height: 36; radius: 18
            color: isActive ? Qt.rgba(1, 1, 1, 0.12) : "transparent"
            border.color: isActive ? Qt.rgba(1, 1, 1, 0.16) : "transparent"
            border.width: 1
            Behavior on color { ColorAnimation { duration: 150 } }

            Row {
                id: segRow; anchors.centerIn: parent; spacing: 7
                Image {
                    source: "qrc:/GlassAlarm/resources/icons/" + modelData.icon + ".svg"
                    width: 18; height: 18
                    sourceSize.width: 18; sourceSize.height: 18
                    opacity: seg.isActive ? 1.0 : 0.55
                    Behavior on opacity { NumberAnimation { duration: 150 } }
                }
                Text {
                    text: modelData.label
                    color: seg.isActive ? configManager.themeTextPrimary : configManager.themeTextSecondary
                    font.pixelSize: 15
                    font.bold: seg.isActive
                }
            }

            MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: { root.current = modelData.mode; root.selected(modelData.mode) } }
        }
    }
}

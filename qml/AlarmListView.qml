import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import GlassAlarm

ColumnLayout {
    id: root

    property int selectedIndex: -1
    signal editAlarm(int index)
    signal deleteAlarm(int index)
    signal addAlarm()

    spacing: 14

    RowLayout {
        Layout.fillWidth: true
        Layout.margins: 4

        Text {
            text: "Alarms"
            color: "#FFFFFF"
            font.pixelSize: 22
            font.bold: true
            opacity: 0.9
        }

        Item { Layout.fillWidth: true }

        GlassButton {
            text: "+"
            pixelSize: 20
            implicitWidth: 40
            implicitHeight: 40
            radius: 20
            onClicked: root.addAlarm()
        }
    }

    ListView {
        id: alarmListView
        Layout.fillWidth: true
        Layout.fillHeight: true
        Layout.topMargin: 8

        model: alarmManager.alarms
        spacing: 14
        clip: true

        delegate: Item {
            id: delegateRoot
            width: ListView.view.width
            height: 100

            property var alarmData: model.modelData || model

            GlassCard {
                id: cardDelegate
                anchors.fill: parent

                property bool isSelected: index === root.selectedIndex

                cardColor: isSelected ? Qt.rgba(1, 1, 1, 0.12) : Qt.rgba(1, 1, 1, 0.06)
                borderColor: isSelected ? Qt.rgba(1, 1, 1, 0.3) : Qt.rgba(1, 1, 1, 0.1)

                // MouseArea FIRST (bottom of z-order) so buttons above it receive clicks
                MouseArea {
                    anchors.fill: parent
                    onClicked: root.selectedIndex = index
                }

                RowLayout {
                    anchors.fill: parent
                    anchors.margins: 12
                    spacing: 12

                    ColumnLayout {
                        Layout.alignment: Qt.AlignVCenter
                        spacing: 2

                        Text {
                            text: ("00" + delegateRoot.alarmData.hour).slice(-2) + ":" + ("00" + delegateRoot.alarmData.minute).slice(-2)
                            color: "#FFFFFF"
                            font.pixelSize: 32
                            font.bold: true
                            opacity: delegateRoot.alarmData.enabled ? 1.0 : 0.4
                        }

                        Row {
                            spacing: 3
                            visible: !delegateRoot.alarmData.isSnooze

                            Repeater {
                                model: [
                                    {label: "M", active: delegateRoot.alarmData.days[0]},
                                    {label: "T", active: delegateRoot.alarmData.days[1]},
                                    {label: "W", active: delegateRoot.alarmData.days[2]},
                                    {label: "T", active: delegateRoot.alarmData.days[3]},
                                    {label: "F", active: delegateRoot.alarmData.days[4]},
                                    {label: "S", active: delegateRoot.alarmData.days[5]},
                                    {label: "S", active: delegateRoot.alarmData.days[6]}
                                ]

                                Text {
                                    text: modelData.label
                                    color: modelData.active ? "#FFFFFF" : Qt.rgba(1, 1, 1, 0.25)
                                    font.pixelSize: 14
                                    font.bold: modelData.active
                                    opacity: delegateRoot.alarmData.enabled ? 1.0 : 0.4
                                }
                            }
                        }
                    }

                    Item { Layout.fillWidth: true }

                    ColumnLayout {
                        Layout.alignment: Qt.AlignVCenter
                        spacing: 4

                        Text {
                            text: delegateRoot.alarmData.wakeMode !== "none" ? "Wake: " + delegateRoot.alarmData.wakeMode : ""
                            color: Qt.rgba(1, 1, 1, 0.5)
                            font.pixelSize: 14
                        }

                        Item {
                            width: 44; height: 24
                            Rectangle {
                                width: 44; height: 24; radius: 12
                                color: delegateRoot.alarmData.enabled ? Qt.lighter(configManager.themeAccent, 1.4) : Qt.rgba(1, 1, 1, 0.15)
                                border.color: delegateRoot.alarmData.enabled ? configManager.themeAccent : Qt.rgba(1, 1, 1, 0.2)
                                border.width: 1
                                Behavior on color { ColorAnimation { duration: 200 } }

                                Rectangle {
                                    x: delegateRoot.alarmData.enabled ? 22 : 2
                                    y: 2; width: 20; height: 20; radius: 10
                                    color: delegateRoot.alarmData.enabled ? configManager.themeAccent : Qt.rgba(1, 1, 1, 0.5)
                                    Behavior on x { NumberAnimation { duration: 200; easing.type: Easing.OutCubic } }
                                    Behavior on color { ColorAnimation { duration: 200 } }
                                }

                                MouseArea {
                                    anchors.fill: parent
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: {
                                        var alarms = alarmManager.alarms
                                        if (index < 0 || index >= alarms.length) return
                                        var a = alarms[index]
                                        a.enabled = !a.enabled
                                        alarmManager.updateAlarm(index, a)
                                    }
                                }
                            }
                        }
                    }

                    ColumnLayout {
                        Layout.alignment: Qt.AlignVCenter
                        spacing: 4

                        GlassButton {
                            text: "Edit"
                            pixelSize: 11
                            implicitWidth: 50
                            implicitHeight: 28
                            radius: 8
                            onClicked: root.editAlarm(index)
                        }

                        GlassButton {
                            text: "Del"
                            pixelSize: 11
                            implicitWidth: 50
                            implicitHeight: 28
                            radius: 8
                            baseColor: Qt.rgba(1, 0.2, 0.2, 0.15)
                            hoverColor: Qt.rgba(1, 0.2, 0.2, 0.25)
                            onClicked: root.deleteAlarm(index)
                        }
                    }
                }
            }
        }

        ScrollBar.vertical: ScrollBar {
            active: true
            policy: ScrollBar.AsNeeded
        }
    }
}

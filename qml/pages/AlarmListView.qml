import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import GlassAlarm

ColumnLayout {
    id: root

    // Selection follows the alarm itself (its id), not the row position:
    // toggling re-sorts the model, and the highlight must stay on the same
    // alarm — matching the old storage-index behavior of selectedIndex.
    // OQ #2 (resolved 1.0.2): deleting a NON-selected row leaves selectedId
    // untouched (the highlight stays on the same alarm). Deleting the SELECTED
    // row also leaves selectedId holding the removed id — main.qml's
    // onDeleteAlarm only calls removeAlarm, so no row matches and nothing is
    // highlighted until the next row click. Ref: "luma.md, Open Question #2".
    property string selectedId: ""
    signal editAlarm(int index)
    signal deleteAlarm(int index)
    signal addAlarm()

    spacing: 10

    // The alarm list is bound directly to alarmManager.model (an
    // AlarmModel / QAbstractListModel). Sorting — enabled first, then
    // time-of-day — lives in the model now; this view receives fine-grained
    // rowsInserted / rowsRemoved / dataChanged signals instead of the old
    // refreshSortedAlarms() full-array rebuild.

    RowLayout {
        Layout.fillWidth: true
        Layout.margins: 2
        Text { text: "Alarms"; color: configManager.themeTextPrimary; font.pixelSize: 20; font.bold: true }
        Item { Layout.fillWidth: true }
        GlassButton {
            text: "+  New"
            pixelSize: 12
            implicitWidth: 70; implicitHeight: 32
            filled: true
            onClicked: root.addAlarm()
        }
    }

    ListView {
        id: alarmListView
        Layout.fillWidth: true
        Layout.fillHeight: true
        spacing: 8
        clip: true
        model: alarmManager.model

        delegate: Item {
            id: delegateRoot
            width: ListView.view.width
            height: 78

            // Roles of AlarmModel (required-property delegate — replaces the
            // old modelData wrapper objects). Note: the enabled flag is
            // exposed as `alarmEnabled` because `enabled` would collide with
            // Item.enabled and disable the whole row.
            required property int index
            required property string alarmId
            required property int hour
            required property int minute
            required property bool alarmEnabled
            required property string name
            required property var days
            required property string wakeMode
            required property int autoStopDuration
            required property bool enableSound
            required property bool enableCommand
            required property string command
            required property string soundFile

            GlassCard {
                id: cardDelegate
                anchors.fill: parent
                property bool isOff: !alarmEnabled
                cardColor: {
                    if (root.selectedId === alarmId) return Qt.rgba(configManager.themeAccent.r, configManager.themeAccent.g, configManager.themeAccent.b, 0.22)
                    if (isOff) return Qt.rgba(0.4, 0.4, 0.5, 0.06)
                    return Qt.rgba(1,1,1,0.06)
                }
                borderColor: {
                    if (root.selectedId === alarmId) return configManager.themeAccent
                    if (isOff) return Qt.rgba(1,1,1,0.04)
                    return Qt.rgba(1,1,1,0.1)
                }
                opacity: isOff ? 0.5 : 1.0

                MouseArea { anchors.fill: parent; onClicked: root.selectedId = alarmId }

                RowLayout {
                    anchors.fill: parent
                    anchors.margins: 10
                    spacing: 10

                    ColumnLayout {
                        Layout.alignment: Qt.AlignVCenter
                        spacing: 3
                        Layout.fillWidth: true
                        Layout.minimumWidth: 60
                        Layout.maximumWidth: 120

                        Text {
                            text: name && name !== "" ? name : ("00"+hour).slice(-2) + ":" + ("00"+minute).slice(-2)
                            color: alarmEnabled ? configManager.themeTextPrimary : configManager.themeTextSecondary
                            font.pixelSize: name ? 19 : 24
                            font.bold: true
                            opacity: alarmEnabled ? 1.0 : 0.5
                        }

                        Text {
                            visible: name && name !== ""
                            text: ("00"+hour).slice(-2) + ":" + ("00"+minute).slice(-2)
                            color: configManager.themeTextSecondary
                            font.pixelSize: 13
                            opacity: alarmEnabled ? 0.8 : 0.4
                        }

                        Row {
                            spacing: 2
                            Repeater {
                                model: [
                                    {l:"M",a:days[0]},{l:"T",a:days[1]},
                                    {l:"W",a:days[2]},{l:"T",a:days[3]},
                                    {l:"F",a:days[4]},{l:"S",a:days[5]},
                                    {l:"S",a:days[6]}
                                ]
                                Rectangle {
                                    width: 15; height: 15; radius: 4
                                    color: modelData.a ? configManager.themeAccent : "transparent"
                                    border.color: Qt.rgba(1,1,1,0.2); border.width: 1
                                    Text { anchors.centerIn: parent; text: modelData.l; color: modelData.a ? "#FFFFFF" : Qt.rgba(1,1,1,0.3); font.pixelSize: 8; font.bold: true }
                                }
                            }
                        }
                    }

                    ToggleSwitch {
                        Layout.alignment: Qt.AlignVCenter
                        checked: alarmEnabled
                        onToggled: alarmManager.setAlarmEnabled(alarmId, checked)
                    }

                    ColumnLayout {
                        Layout.alignment: Qt.AlignVCenter
                        spacing: 4
                        GlassButton { text:"Edit"; pixelSize:10; implicitWidth:46; implicitHeight:24; radius:6; onClicked: root.editAlarm(index) }
                        GlassButton { text:"Del"; pixelSize:10; implicitWidth:46; implicitHeight:24; radius:6; baseColor: Qt.rgba(1,0.2,0.2,0.15); hoverColor: Qt.rgba(1,0.2,0.2,0.28); onClicked: root.deleteAlarm(index) }
                    }
                }
            }
        }

        ScrollBar.vertical: ScrollBar { active: true; policy: ScrollBar.AsNeeded }
    }
}

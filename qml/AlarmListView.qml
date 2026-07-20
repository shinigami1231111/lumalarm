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

    spacing: 10

    // Sorted view: enabled first, then by time of day. Keeps the real index.
    property var sortedAlarms: {
        var list = []
        var src = alarmManager.alarms
        for (var i = 0; i < src.length; i++)
            list.push({ data: src[i], idx: i })
        list.sort(function(a, b) {
            if (a.data.enabled !== b.data.enabled) return a.data.enabled ? -1 : 1
            var ta = a.data.hour * 60 + a.data.minute
            var tb = b.data.hour * 60 + b.data.minute
            return ta - tb
        })
        return list
    }

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
        model: root.sortedAlarms

        delegate: Item {
            id: delegateRoot
            width: ListView.view.width
            height: 78

            property var alarmData: modelData.data
            property int realIndex: modelData.idx

            GlassCard {
                id: cardDelegate
                anchors.fill: parent
                property bool isOff: !alarmData.enabled
                cardColor: {
                    if (root.selectedIndex === realIndex) return Qt.rgba(configManager.themeAccent.r, configManager.themeAccent.g, configManager.themeAccent.b, 0.22)
                    if (isOff) return Qt.rgba(1,1,1,0.025)
                    return Qt.rgba(1,1,1,0.06)
                }
                borderColor: {
                    if (root.selectedIndex === realIndex) return configManager.themeAccent
                    if (isOff) return Qt.rgba(1,1,1,0.06)
                    return Qt.rgba(1,1,1,0.1)
                }
                opacity: isOff ? 0.6 : 1.0

                MouseArea { anchors.fill: parent; onClicked: root.selectedIndex = realIndex }

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
                            text: alarmData.name && alarmData.name !== "" ? alarmData.name : ("00"+alarmData.hour).slice(-2) + ":" + ("00"+alarmData.minute).slice(-2)
                            color: alarmData.enabled ? configManager.themeTextPrimary : configManager.themeTextSecondary
                            font.pixelSize: alarmData.name ? 19 : 24
                            font.bold: true
                            opacity: alarmData.enabled ? 1.0 : 0.5
                        }

                        Text {
                            visible: alarmData.name && alarmData.name !== ""
                            text: ("00"+alarmData.hour).slice(-2) + ":" + ("00"+alarmData.minute).slice(-2)
                            color: configManager.themeTextSecondary
                            font.pixelSize: 13
                            opacity: alarmData.enabled ? 0.8 : 0.4
                        }

                        Row {
                            spacing: 2
                            visible: !alarmData.isSnooze
                            Repeater {
                                model: [
                                    {l:"M",a:alarmData.days[0]},{l:"T",a:alarmData.days[1]},
                                    {l:"W",a:alarmData.days[2]},{l:"T",a:alarmData.days[3]},
                                    {l:"F",a:alarmData.days[4]},{l:"S",a:alarmData.days[5]},
                                    {l:"S",a:alarmData.days[6]}
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

                    Text {
                        visible: isOff
                        text: "Off"
                        color: Qt.rgba(1,1,1,0.35)
                        font.pixelSize: 11; font.bold: true
                        Layout.alignment: Qt.AlignVCenter
                    }

                    ToggleSwitch {
                        Layout.alignment: Qt.AlignVCenter
                        checked: alarmData.enabled
                        onToggled: function(v) {
                            var a = alarmManager.alarms[realIndex]
                            a.enabled = v
                            alarmManager.updateAlarm(realIndex, a)
                            if (!v && alarmData.id) scheduler.cancelSnooze(alarmData.id)
                        }
                    }

                    ColumnLayout {
                        Layout.alignment: Qt.AlignVCenter
                        spacing: 4
                        GlassButton { text:"Edit"; pixelSize:10; implicitWidth:46; implicitHeight:24; radius:6; onClicked: root.editAlarm(realIndex) }
                        GlassButton { text:"Del"; pixelSize:10; implicitWidth:46; implicitHeight:24; radius:6; baseColor: Qt.rgba(1,0.2,0.2,0.15); hoverColor: Qt.rgba(1,0.2,0.2,0.28); onClicked: { if (alarmData.id) scheduler.cancelSnooze(alarmData.id); root.deleteAlarm(realIndex) } }
                    }
                }
            }
        }

        ScrollBar.vertical: ScrollBar { active: true; policy: ScrollBar.AsNeeded }
    }
}

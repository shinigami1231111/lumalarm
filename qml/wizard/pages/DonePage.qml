import QtQuick
import QtQuick.Layouts
import GlassAlarm

// Page 5 — summary and finish. Finish persists wizard/completed=true; the
// secondary link closes the wizard without writing the flag so it appears
// again on the next launch.
WizardPage {
    id: root
    title: (configManager.emojiLevel, configManager.emoji("🤩", 1)) + qsTr("You are set up")
    subtitle: qsTr("Here is what this machine reported. Nothing else needs configuring for the first alarm.")
    primaryLabel: (configManager.emojiLevel, configManager.emoji("🏁", 2)) + qsTr("Finish")
    primaryEnabled: true
    onPrimaryClicked: wizard.finish()

    function outputDeviceLabel() {
        const wanted = configManager.audioOutputSink
        if (wanted === "")
            return qsTr("Default output device")
        const devices = wizard.outputDevices
        for (let i = 0; i < devices.length; ++i)
            if (devices[i].name === wanted)
                return devices[i].description
        return wanted // device unplugged since it was chosen
    }

    Rectangle {
        Layout.fillWidth: true
        Layout.preferredHeight: summaryColumn.implicitHeight + 24
        radius: 12
        color: Qt.rgba(1, 1, 1, 0.06)
        border.color: Qt.rgba(1, 1, 1, 0.10)
        border.width: 1

        ColumnLayout {
            id: summaryColumn
            anchors.fill: parent
            anchors.margins: 14
            spacing: 10

            RowLayout {
                Layout.fillWidth: true
                spacing: 12
                Text { text: (configManager.emojiLevel, configManager.emoji("🌐", 2)) + qsTr("Language"); color: configManager.themeTextSecondary;
                       font.pixelSize: 13; Layout.preferredWidth: 140 }
                Text {
                    Layout.fillWidth: true
                    text: wizard.language === "en" ? "English"
                          : (wizard.language === "ja" ? "日本語" : qsTr("System default"))
                    color: configManager.themeTextPrimary
                    font.pixelSize: 13
                    horizontalAlignment: Text.AlignRight
                    elide: Text.ElideRight
                }
            }
            RowLayout {
                Layout.fillWidth: true
                spacing: 12
                Text { text: (configManager.emojiLevel, configManager.emoji("⏰", 2)) + qsTr("Wake method"); color: configManager.themeTextSecondary;
                       font.pixelSize: 13; Layout.preferredWidth: 140 }
                Text {
                    Layout.fillWidth: true
                    text: wizard.detectWakeMethod()
                    color: configManager.themeTextPrimary
                    font.pixelSize: 13
                    horizontalAlignment: Text.AlignRight
                    elide: Text.ElideLeft
                }
            }
            RowLayout {
                Layout.fillWidth: true
                spacing: 12
                Text { text: (configManager.emojiLevel, configManager.emoji("🔔", 2)) + qsTr("Notifications"); color: configManager.themeTextSecondary;
                       font.pixelSize: 13; Layout.preferredWidth: 140 }
                Text {
                    Layout.fillWidth: true
                    text: wizard.notificationsAvailable ? qsTr("Available") : qsTr("Not available")
                    color: configManager.themeTextPrimary
                    font.pixelSize: 13
                    horizontalAlignment: Text.AlignRight
                    elide: Text.ElideRight
                }
            }
            RowLayout {
                Layout.fillWidth: true
                spacing: 12
                Text { text: (configManager.emojiLevel, configManager.emoji("🔔", 2)) + qsTr("Alarm Sound"); color: configManager.themeTextSecondary;
                       font.pixelSize: 13; Layout.preferredWidth: 140 }
                Text {
                    Layout.fillWidth: true
                    text: configManager.audioPolicy === "guarantee"
                          ? qsTr("Raise system volume") : qsTr("Never touch system volume")
                    color: configManager.themeTextPrimary
                    font.pixelSize: 13
                    horizontalAlignment: Text.AlignRight
                    elide: Text.ElideRight
                }
            }
            RowLayout {
                Layout.fillWidth: true
                spacing: 12
                Text { text: (configManager.emojiLevel, configManager.emoji("🔉", 2)) + qsTr("Output device"); color: configManager.themeTextSecondary;
                       font.pixelSize: 13; Layout.preferredWidth: 140 }
                Text {
                    Layout.fillWidth: true
                    text: root.outputDeviceLabel()
                    color: configManager.themeTextPrimary
                    font.pixelSize: 13
                    horizontalAlignment: Text.AlignRight
                    elide: Text.ElideRight
                }
            }
        }
    }

    Text {
        Layout.fillWidth: true
        text: qsTr("Run this setup again any time from Settings.")
        color: configManager.themeTextSecondary
        font.pixelSize: 13
        wrapMode: Text.WordWrap
    }

    Text {
        text: (configManager.emojiLevel, configManager.emoji("🔁", 2)) + qsTr("Run again next time")
        color: configManager.themeTextSecondary
        font.pixelSize: 13
        font.underline: true
        MouseArea {
            anchors.fill: parent
            anchors.margins: -6
            cursorShape: Qt.PointingHandCursor
            onClicked: wizard.finishRepeatNextTime()
        }
    }

    Item { Layout.fillHeight: true }
}

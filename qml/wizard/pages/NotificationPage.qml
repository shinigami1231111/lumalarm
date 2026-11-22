import QtQuick
import QtQuick.Layouts
import GlassAlarm

// Page 3 — notifications. Checks whether this session has a notification
// daemon; sending one is optional and never blocks progress.
WizardPage {
    id: root
    title: (configManager.emojiLevel, configManager.emoji("🔔", 2)) + qsTr("Notifications")
    subtitle: qsTr("Lumalarm can notify you on the desktop before an alarm rings.")
    primaryLabel: (configManager.emojiLevel, configManager.emoji("➡️", 2)) + qsTr("Next")
    primaryEnabled: true
    onPrimaryClicked: wizard.next()

    Rectangle {
        Layout.fillWidth: true
        Layout.preferredHeight: noteColumn.implicitHeight + 24
        radius: 12
        color: Qt.rgba(1, 1, 1, 0.06)
        border.color: Qt.rgba(1, 1, 1, 0.10)
        border.width: 1

        ColumnLayout {
            id: noteColumn
            anchors.fill: parent
            anchors.margins: 12
            spacing: 6

            Text {
                text: (configManager.emojiLevel, configManager.emoji("🔔", 2)) + (wizard.notificationsAvailable ? qsTr("Notifications available")
                                                    : qsTr("No notification daemon"))
                color: configManager.themeTextPrimary
                font.pixelSize: 15
                font.bold: true
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
            }
            Text {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                color: configManager.themeTextSecondary
                font.pixelSize: 13
                text: wizard.notificationsAvailable
                    ? qsTr("Alarms can be announced through the desktop notification service.")
                    : qsTr("Desktop notifications will not be shown. Alarms still ring normally.")
            }
        }
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: 12
        visible: wizard.notificationsAvailable

        GlassButton {
            text: (configManager.emojiLevel, configManager.emoji("📤", 2)) + qsTr("Send test notification")
            pixelSize: 13
            onClicked: {
                const ok = wizard.testNotification()
                root.probeResult = (configManager.emojiLevel, configManager.emoji("✅", 2)) + (ok ? qsTr("Test notification sent.")
                                      : qsTr("Could not send the notification."))
            }
        }
        Item { Layout.fillWidth: true }
    }

    property string probeResult: ""

    Text {
        Layout.fillWidth: true
        visible: root.probeResult.length > 0
        text: root.probeResult
        color: configManager.themeTextPrimary
        font.pixelSize: 13
    }

    Item { Layout.fillHeight: true }
}

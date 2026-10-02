import QtQuick
import QtQuick.Layouts
import GlassAlarm

// Page 5 — summary and finish. Finish persists wizard/completed=true; the
// secondary link closes the wizard *without* writing the flag so it appears
// again on the next launch.
WizardPage {
    id: root
    title: "You are set up"
    subtitle: "Here is what this machine reported. Nothing else needs configuring for the first alarm to work."
    primaryLabel: "Finish"
    primaryEnabled: true
    onPrimaryClicked: wizard.finish()

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
                Text { text: "Wake method"; color: configManager.themeTextSecondary; font.pixelSize: 13; Layout.preferredWidth: 140 }
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
                Text { text: "Notifications"; color: configManager.themeTextSecondary; font.pixelSize: 13; Layout.preferredWidth: 140 }
                Text {
                    Layout.fillWidth: true
                    text: wizard.notificationsAvailable ? "Available" : "Not available"
                    color: configManager.themeTextPrimary
                    font.pixelSize: 13
                    horizontalAlignment: Text.AlignRight
                    elide: Text.ElideRight
                }
            }
            RowLayout {
                Layout.fillWidth: true
                spacing: 12
                Text { text: "Default sound"; color: configManager.themeTextSecondary; font.pixelSize: 13; Layout.preferredWidth: 140 }
                Text {
                    Layout.fillWidth: true
                    text: wizard.defaultSound.length > 0 ? wizard.defaultSound : "Silent"
                    color: configManager.themeTextPrimary
                    font.pixelSize: 13
                    horizontalAlignment: Text.AlignRight
                    elide: Text.ElideMiddle
                }
            }
        }
    }

    Text {
        Layout.fillWidth: true
        text: "Run this setup again any time from Settings."
        color: configManager.themeTextSecondary
        font.pixelSize: 13
        wrapMode: Text.WordWrap
    }

    Text {
        text: "Run again next time"
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

import QtQuick
import QtQuick.Layouts
import GlassAlarm

// Page 6 — summary and finish. Finish persists wizard/completed=true; the
// secondary link closes the wizard *without* writing the flag so it appears
// again on the next launch.
WizardPage {
    id: root
    title: qsTr("You are set up")
    subtitle: qsTr("Here is what this machine reported. Nothing else needs configuring for the first alarm to work.")
    primaryLabel: qsTr("Finish")
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
                Text { text: qsTr("Language"); color: configManager.themeTextSecondary; font.pixelSize: 13; Layout.preferredWidth: 140 }
                Text {
                    Layout.fillWidth: true
                    // Same presentation as the selector on page 2: language
                    // names stay in their own script, only "System default"
                    // is translated.
                    text: wizard.language === "en" ? "English"
                          : (wizard.language === "ar" ? "العربية" : qsTr("System default"))
                    color: configManager.themeTextPrimary
                    font.pixelSize: 13
                    horizontalAlignment: Text.AlignRight
                    elide: Text.ElideRight
                }
            }
            RowLayout {
                Layout.fillWidth: true
                spacing: 12
                Text { text: qsTr("Wake method"); color: configManager.themeTextSecondary; font.pixelSize: 13; Layout.preferredWidth: 140 }
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
                Text { text: qsTr("Test alarm"); color: configManager.themeTextSecondary; font.pixelSize: 13; Layout.preferredWidth: 140 }
                Text {
                    Layout.fillWidth: true
                    // Empty means the step was skipped — there is no status
                    // line to report. A probe always leaves one behind.
                    text: wizard.wakeStatus.length > 0 ? wizard.wakeStatus : qsTr("Skipped")
                    color: configManager.themeTextPrimary
                    font.pixelSize: 13
                    horizontalAlignment: Text.AlignRight
                    wrapMode: Text.WordWrap
                }
            }
            RowLayout {
                Layout.fillWidth: true
                spacing: 12
                Text { text: qsTr("Notifications"); color: configManager.themeTextSecondary; font.pixelSize: 13; Layout.preferredWidth: 140 }
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
                Text { text: qsTr("Default sound"); color: configManager.themeTextSecondary; font.pixelSize: 13; Layout.preferredWidth: 140 }
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
        text: qsTr("Run this setup again any time from Settings.")
        color: configManager.themeTextSecondary
        font.pixelSize: 13
        wrapMode: Text.WordWrap
    }

    Text {
        text: qsTr("Run again next time")
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

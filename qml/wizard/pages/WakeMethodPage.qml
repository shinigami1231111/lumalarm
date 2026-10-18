import QtQuick
import QtQuick.Layouts
import GlassAlarm

// Page 2 — wake method. Reports how this machine can wake from sleep and lets
// the user choose the sleep mode. Next is always clickable — no test is forced.
WizardPage {
    id: root
    title: qsTr("Wake method")
    subtitle: qsTr("For an alarm to bring the machine back from sleep, Lumalarm needs a wake source.")
    primaryLabel: qsTr("Next")
    primaryEnabled: true
    onPrimaryClicked: wizard.next()

    Rectangle {
        Layout.fillWidth: true
        Layout.preferredHeight: methodColumn.implicitHeight + 24
        radius: 12
        color: Qt.rgba(1, 1, 1, 0.06)
        border.color: Qt.rgba(1, 1, 1, 0.10)
        border.width: 1

        ColumnLayout {
            id: methodColumn
            anchors.fill: parent
            anchors.margins: 12
            spacing: 6

            Text {
                text: qsTr("Detected")
                color: configManager.themeTextSecondary
                font.pixelSize: 12
            }
            Text {
                text: wizard.detectWakeMethod()
                color: configManager.themeTextPrimary
                font.pixelSize: 15
                font.bold: true
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
            }
            Text {
                text: qsTr("Choose how deeply the machine sleeps before the alarm wakes it.")
                color: configManager.themeTextSecondary
                font.pixelSize: 12
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
            }

            // The same mode selector Settings offers — one value, shared with
            // the helper as `rtcwake -m <mode>`.
            RowLayout {
                Layout.fillWidth: true
                spacing: 14
                Text {
                    text: qsTr("Sleep mode")
                    color: configManager.themeTextPrimary
                    font.pixelSize: 14
                    Layout.preferredWidth: 100
                }
                Item { Layout.fillWidth: true }
                RoundedCombo {
                    model: configManager.sleepModeChoices()
                    currentIndex: Math.max(
                        0, sleepModeCombo.find(configManager.sleepMode))
                    Layout.preferredWidth: 140
                    onActivated: configManager.sleepMode = currentText
                }
            }
        }
    }

    Text {
        Layout.fillWidth: true
        wrapMode: Text.WordWrap
        color: "#ff6b6b"
        font.pixelSize: 13
        font.bold: true
        text: qsTr("Save your work before testing the wake — the machine will sleep.")
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: 12

        GlassButton {
            id: testButton
            text: qsTr("Test wake")
            pixelSize: 13
            enabled: wakeManager.wakeState !== "arming" && wakeManager.wakeState !== "armed"
            onClicked: wizard.testWake()
        }
        GlassButton {
            text: qsTr("Skip this step")
            pixelSize: 13
            onClicked: wizard.skipWakeStep()
        }
        Item { Layout.fillWidth: true }
    }

    Text {
        Layout.fillWidth: true
        visible: wizard.wakeStatus.length > 0
        text: wizard.wakeStatus
        color: configManager.themeTextPrimary
        font.pixelSize: 13
        wrapMode: Text.WordWrap
    }

    Item { Layout.fillHeight: true }
}

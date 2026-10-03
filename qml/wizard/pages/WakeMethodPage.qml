import QtQuick
import QtQuick.Layouts
import GlassAlarm

// Page 3 — wake method. Reports the mechanism already detected by
// WakeManager, offers a non-destructive probe (arm, observe, cancel), and only
// unlocks Next after the probe succeeded or the step was explicitly skipped.
WizardPage {
    id: root
    title: qsTr("Wake method")
    subtitle: qsTr("For an alarm to bring the machine back from sleep, Lumalarm needs a wake source. Here is what this machine offers.")
    primaryLabel: qsTr("Next")
    primaryEnabled: wizard.canGoNext
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
                // WakeManager accepts 60–3600 s for a probe, so 60 s is the
                // shortest non-destructive test on offer.
                text: qsTr("The test arms a wake 60 seconds ahead and cancels it as soon as the result is in. Nothing else is changed.")
                color: configManager.themeTextSecondary
                font.pixelSize: 12
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
            }
        }
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

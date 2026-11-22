import QtQuick
import QtQuick.Layouts
import GlassAlarm

// Page 4 — alarm sound. Mirrors the Settings "Alarm Sound" controls so a
// first-run user can choose the ring device and volume policy without leaving
// the wizard. Every control writes ConfigManager directly, exactly like
// Settings: there is one source of truth for these values, shared with the
// audio controller. Next is never gated — every choice can be changed later.
WizardPage {
    id: root
    title: configManager.emoji("🔔", 2) + qsTr("Alarm Sound")
    subtitle: qsTr("The alarm rings through this device; other apps keep using the system default.")
    primaryLabel: configManager.emoji("➡️", 2) + qsTr("Next")
    primaryEnabled: true
    onPrimaryClicked: wizard.next()

    property var sinkDevices: []
    readonly property var toneNames: configManager.availableTones()

    Component.onCompleted: {
        root.sinkDevices = wizard.outputDevices
        wizard.refreshOutputDevices()
    }
    Connections {
        target: wizard
        function onOutputDevicesChanged() { root.sinkDevices = wizard.outputDevices }
    }

    // OUTPUT DEVICE — index 0 follows the system default sink. The stored
    // value is the node name, so it survives a backend restart.
    RowLayout {
        Layout.fillWidth: true
        spacing: 12

        Text {
            text: configManager.emoji("🔉", 2) + qsTr("Output device")
            color: configManager.themeTextPrimary
            font.pixelSize: 15
            Layout.fillWidth: true
        }

        RoundedCombo {
            id: sinkCombo
            Layout.preferredWidth: 210
            model: {
                const labels = [qsTr("Default output device")]
                for (let i = 0; i < root.sinkDevices.length; ++i)
                    labels.push(root.sinkDevices[i].description)
                return labels
            }
            currentIndex: {
                const wanted = configManager.audioOutputSink
                if (wanted === "")
                    return 0
                for (let i = 0; i < root.sinkDevices.length; ++i)
                    if (root.sinkDevices[i].name === wanted)
                        return i + 1
                return 0 // the chosen device is not present (unplugged)
            }
            onActivated: configManager.audioOutputSink =
                currentIndex === 0 ? "" : root.sinkDevices[currentIndex - 1].name
        }
    }

    // QUICK LISTEN — previews the configured global default tone (or the first
    // tone when none is set), using the same controller call as Settings.
    RowLayout {
        Layout.fillWidth: true
        spacing: 12

        Text {
            text: configManager.emoji("🎵", 2) + qsTr("Alarm Sound")
            color: configManager.themeTextPrimary
            font.pixelSize: 15
            Layout.fillWidth: true
        }

        GlassButton {
            text: configManager.emoji("▶️", 2) + qsTr("Preview")
            pixelSize: 13
            enabled: root.toneNames.length > 0
            onClicked: {
                const wanted = configManager.audioDefaultSound
                wizard.previewSound(wanted !== "" ? wanted : root.toneNames[0])
            }
        }
    }

    Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: Qt.rgba(1,1,1,0.07) }

    // WHEN AN ALARM RINGS — the same two policies Settings offers.
    Text {
        text: configManager.emoji("🔔", 2) + qsTr("When an alarm rings")
        color: configManager.themeTextSecondary
        font.pixelSize: 13
        font.bold: true
        Layout.bottomMargin: -4
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: 8

        GlassButton {
            text: qsTr("Raise system volume")
            filled: configManager.audioPolicy === "guarantee"
            onClicked: configManager.audioPolicy = "guarantee"
        }
        GlassButton {
            text: qsTr("Never touch system volume")
            filled: configManager.audioPolicy === "alarmOnly"
            onClicked: configManager.audioPolicy = "alarmOnly"
        }
        Item { Layout.fillWidth: true }
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: 14
        enabled: configManager.audioPolicy === "guarantee"
        opacity: enabled ? 1.0 : 0.5

        Text {
            text: configManager.emoji("🔈", 2) + qsTr("Minimum volume")
            color: configManager.themeTextPrimary
            font.pixelSize: 15
            Layout.preferredWidth: 180
        }
        Item { Layout.fillWidth: true }
        Slider {
            from: 0; to: 100; stepSize: 5
            value: configManager.audioFloorPercent
            implicitWidth: 200
            onPressedChanged: {
                if (!pressed && value !== configManager.audioFloorPercent)
                    configManager.audioFloorPercent = value
            }
        }
        Text {
            text: Math.round(configManager.audioFloorPercent) + "%"
            color: configManager.themeTextSecondary
            font.pixelSize: 14
            Layout.preferredWidth: 46
            horizontalAlignment: Text.AlignRight
        }
    }

    Text {
        visible: configManager.audioPolicy === "guarantee"
        text: qsTr("The system volume is raised to this level while an alarm rings, then restored exactly.")
        color: configManager.themeTextSecondary
        font.pixelSize: 12
        wrapMode: Text.WordWrap
        Layout.fillWidth: true
    }
    Text {
        visible: configManager.audioPolicy === "alarmOnly"
        text: qsTr("Lumalarm plays its own sound only; your system volume is never changed.")
        color: configManager.themeTextSecondary
        font.pixelSize: 12
        wrapMode: Text.WordWrap
        Layout.fillWidth: true
    }

    Item { Layout.fillHeight: true }
}

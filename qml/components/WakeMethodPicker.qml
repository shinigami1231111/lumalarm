import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import GlassAlarm

// One selector for Settings and the wizard. The mode tokens stay exactly the
// `rtcwake -m <mode>` values; only the labels and descriptions are decorated.
// `no` and `on` remain valid in the security allow-list but are deliberately
// absent here (user, 2026-11-22): a selector is for sleep methods, not for
// troubleshooting switches.
ColumnLayout {
    id: root
    spacing: 5

    property string caption: qsTr("RTCwake method")
    // Callers pass an already-gated emoji so this shared picker never bypasses
    // the Off / Less / More setting (and its live QML dependency).
    property string captionEmoji: ""
    property int comboWidth: 280

    // Order and wording are from the user's 2026-11-22 decision. The separator
    // has no mode key, so selecting it simply restores the current choice.
    readonly property var methods: [
        {
            key: "mem",
            label: qsTr("MEM — Suspend to RAM") + " (" + qsTr("Recommended") + ")",
            detail: qsTr("Keeps your current session in RAM using minimal power. Requires suspend support and RTC wake-up. No automatic login needed.")
        },
        {
            key: "freeze",
            label: qsTr("FREEZE — Lightweight Suspend"),
            detail: qsTr("Freezes running processes without deep sleep. Requires kernel and device support. Uses more power than MEM but may offer better compatibility. No automatic login needed.")
        },
        {
            separator: true,
            label: "────── " + qsTr("Experimental") + " ──────",
            detail: ""
        },
        {
            key: "standby",
            label: qsTr("STANDBY — Hardware Standby (S1)") + " (" + qsTr("Experimental") + ")",
            detail: qsTr("Enters a light hardware standby state. Requires ACPI S1 and RTC wake-up support. Wakes the existing session without automatic login.")
        },
        {
            key: "off",
            label: qsTr("OFF — Complete Shutdown") + " (" + qsTr("Experimental") + ")",
            detail: qsTr("Completely powers off the computer and schedules automatic startup. Requires firmware RTC wake-up, automatic system boot, automatic user login, and alarm app auto-start.")
        },
        {
            key: "disk",
            label: qsTr("DISK — Hibernate to Disk") + " (" + qsTr("Experimental") + ")",
            detail: qsTr("Saves your session to disk and powers off. Requires configured hibernation, sufficient swap space, resume support, and RTC wake-up. Restores the saved session without requiring automatic login.")
        }
    ]

    function indexOfMode(mode) {
        for (let i = 0; i < root.methods.length; ++i)
            if (root.methods[i].key === mode)
                return i
        return 0 // mem — the shipped default
    }

    function sync() {
        combo.currentIndex = root.indexOfMode(configManager.sleepMode)
    }

    function currentDetail() {
        const m = root.methods[combo.currentIndex]
        return m && m.detail ? m.detail : ""
    }

    Component.onCompleted: root.sync()
    Connections {
        target: configManager
        function onConfigChanged() { root.sync() }
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: 14

        Text {
            text: root.captionEmoji + root.caption
            color: configManager.themeTextPrimary
            font.pixelSize: 15
            Layout.preferredWidth: 180
            Layout.alignment: Qt.AlignVCenter
        }
        Item { Layout.fillWidth: true }
        RoundedCombo {
            id: combo
            model: root.methods.map(function(m) { return m.label })
            Layout.preferredWidth: root.comboWidth
            Layout.maximumWidth: root.comboWidth
            onActivated: {
                const m = root.methods[currentIndex]
                if (!m || m.separator) {
                    root.sync()
                    return
                }
                configManager.sleepMode = m.key
                root.sync()
            }
        }
    }

    Text {
        text: root.currentDetail()
        visible: text.length > 0
        color: configManager.themeTextSecondary
        font.pixelSize: 11
        wrapMode: Text.WordWrap
        Layout.fillWidth: true
        Layout.leftMargin: 194
    }
}

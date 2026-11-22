import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import GlassAlarm

// ── CLOSE PROMPT ─────────────────────────────────────────────────────────────
// Opened by AppLifecycle when closeAction is still "ask". This component no
// longer decides or persists anything: it collects a choice and hands it to
// lifecycle.confirmClose(), which is the only place that writes closeAction —
// and it refuses to remember "hide" when there is no tray to come back through.
Rectangle {
    id: root

    visible: false
    anchors.fill: parent
    z: 1200
    color: Qt.rgba(0, 0, 0, 0.45)

    // Idempotent: a second close attempt while the prompt is already up must
    // not stack or reset it.
    function openPrompt() {
        if (visible)
            return
        dontAskAgain.checked = false
        visible = true
    }

    // Clicking the dimmed backdrop cancels — it is never an exit, and it never
    // persists anything. The card is a sibling with higher z, so clicks on it
    // never reach here.
    MouseArea {
        anchors.fill: parent
        onClicked: root.visible = false
    }

    GlassCard {
        z: 1
        anchors.centerIn: parent
        width: Math.min(parent.width - 80, 420)
        // No explicit height — let the card size to its content.
        implicitHeight: col.implicitHeight + 44

        // Esc and the backdrop are both plain cancels.
        Keys.onEscapePressed: root.visible = false

        ColumnLayout {
            id: col
            anchors.fill: parent
            anchors.margins: 22
            spacing: 14

            Text {
                Layout.fillWidth: true
                text: (configManager.emojiLevel, configManager.emoji("🛑", 2)) + qsTr("Close Lumalarm?")
                color: configManager.themeTextPrimary
                font.pixelSize: 17
                font.bold: true
            }

            Text {
                Layout.fillWidth: true
                Layout.bottomMargin: 4
                text: qsTr("Lumalarm can keep running in the tray so your alarms still ring, or it can exit completely.")
                color: Qt.rgba(1, 1, 1, 0.72)
                font.pixelSize: 13
                wrapMode: Text.WordWrap
            }

            Text {
                Layout.fillWidth: true
                Layout.bottomMargin: 4
                text: qsTr("Alarms will not ring while Lumalarm is closed.")
                color: Qt.rgba(1, 1, 1, 0.55)
                font.pixelSize: 13
                wrapMode: Text.WordWrap
            }

            GlassCheckBox {
                id: dontAskAgain
                Layout.fillWidth: true
                labelText: qsTr("Don't ask me again")
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.topMargin: 6
                spacing: 10

                GlassButton {
                    // "Hide to tray" degrades to "Minimize" when there is no tray,
                    // so the label never promises something the app cannot do.
                    text: lifecycle.canHideToTray ? (configManager.emojiLevel, configManager.emoji("📥", 2)) + qsTr("Hide to tray") : (configManager.emojiLevel, configManager.emoji("➖", 2)) + qsTr("Minimize")
                    filled: true
                    onClicked: {
                        root.visible = false
                        lifecycle.confirmClose("hide", dontAskAgain.checked)
                    }
                }

                GlassButton {
                    id: quitBtn
                    text: (configManager.emojiLevel, configManager.emoji("🚪", 2)) + qsTr("Quit")
                    onClicked: {
                        root.visible = false
                        lifecycle.confirmClose("quit", dontAskAgain.checked)
                    }
                }

                GlassButton {
                    text: (configManager.emojiLevel, configManager.emoji("🚫", 2)) + qsTr("Cancel")
                    onClicked: root.visible = false
                }

                Item { Layout.fillWidth: true }
            }
        }
    }
}

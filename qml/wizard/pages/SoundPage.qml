import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import GlassAlarm

// Page 4 — default alarm sound. The list comes straight from the app's own
// tone catalogue (ConfigManager::availableTones), the same one the alarm
// editor offers, so nothing here is hard-coded.
WizardPage {
    id: root
    title: qsTr("Default alarm sound")
    subtitle: qsTr("Pick the tone new alarms start with. You can still change it per alarm in the editor.")
    primaryLabel: qsTr("Next")
    primaryEnabled: wizard.canGoNext
    onPrimaryClicked: wizard.next()

    property var sounds: wizard.availableSounds()
    property string selected: wizard.defaultSound

    Text {
        visible: root.sounds.length === 0
        Layout.fillWidth: true
        wrapMode: Text.WordWrap
        color: configManager.themeTextSecondary
        font.pixelSize: 13
        text: qsTr("No tones were found yet. You can add .wav, .mp3, .ogg or .flac files to the tones folder later and pick them per alarm.")
    }

    Flickable {
        Layout.fillWidth: true
        Layout.fillHeight: true
        clip: true
        contentHeight: toneColumn.implicitHeight
        visible: root.sounds.length > 0

        ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

        ColumnLayout {
            id: toneColumn
            width: parent.width
            spacing: 8

            Repeater {
                model: root.sounds
                delegate: Rectangle {
                    required property var modelData
                    Layout.fillWidth: true
                    Layout.preferredHeight: 46
                    radius: 10
                    color: root.selected === modelData.id
                        ? Qt.rgba(0.24, 0.5, 1, 0.20)
                        : Qt.rgba(1, 1, 1, 0.06)
                    border.color: root.selected === modelData.id
                        ? configManager.themeAccent
                        : Qt.rgba(1, 1, 1, 0.10)
                    border.width: 1

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 12
                        anchors.rightMargin: 8
                        spacing: 10

                        Text {
                            Layout.fillWidth: true
                            text: modelData.label
                            color: configManager.themeTextPrimary
                            font.pixelSize: 14
                            elide: Text.ElideMiddle
                        }
                        GlassButton {
                            text: qsTr("Preview")
                            pixelSize: 12
                            onClicked: wizard.previewSound(modelData.id)
                        }
                        GlassButton {
                            text: root.selected === modelData.id ? qsTr("Selected") : qsTr("Use")
                            pixelSize: 12
                            filled: root.selected !== modelData.id
                            onClicked: {
                                wizard.setDefaultSound(modelData.id)
                                root.selected = modelData.id
                            }
                        }
                    }

                    MouseArea {
                        anchors.fill: parent
                        z: -1
                        onClicked: {
                            wizard.setDefaultSound(modelData.id)
                            root.selected = modelData.id
                        }
                    }
                }
            }
        }
    }

    Item { Layout.fillHeight: true; visible: root.sounds.length === 0 }
}

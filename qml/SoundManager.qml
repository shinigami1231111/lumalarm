import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import GlassAlarm

ColumnLayout {
    id: root
    spacing: 14

    property var toneList: configManager.availableTones()

    function refresh() {
        toneList = configManager.availableTones()
    }

    Text {
        text: "Sound Manager"
        color: "#FFFFFF"
        font.pixelSize: 22
        font.bold: true
    }

    Text {
        text: "Files in " + configManager.tonesDirectory()
        color: Qt.rgba(1,1,1,0.5)
        font.pixelSize: 14
        wrapMode: Text.WordWrap
    }

    Text {
        text: "Restart required for tone selectors in alarm/timer to update"
        color: Qt.rgba(1, 0.6, 0, 0.6)
        font.pixelSize: 12
    }

    ListView {
        Layout.fillWidth: true
        Layout.fillHeight: true
        spacing: 10
        clip: true

        model: toneList

        delegate: Rectangle {
            width: parent.width
            height: 40
            color: Qt.rgba(1,1,1,0.05)
            radius: 8

            RowLayout {
                anchors.fill: parent
                anchors.margins: 8
                spacing: 8

                Text {
                    text: modelData
                    color: "#FFFFFF"
                    font.pixelSize: 16
                    elide: Text.ElideMiddle
                    Layout.fillWidth: true
                }

                GlassButton {
                    text: "Play"
                    pixelSize: 10
                    implicitWidth: 44
                    implicitHeight: 24
                    radius: 6
                    onClicked: audioPlayer.preview(modelData)
                }

                GlassButton {
                    text: "Del"
                    pixelSize: 10
                    implicitWidth: 40
                    implicitHeight: 24
                    radius: 6
                    baseColor: Qt.rgba(1,0.2,0.2,0.15)
                    hoverColor: Qt.rgba(1,0.2,0.2,0.3)
                    onClicked: {
                        configManager.deleteTone(modelData)
                        refresh()
                    }
                }
            }
        }

        ScrollBar.vertical: ScrollBar {
            active: true
            policy: ScrollBar.AsNeeded
        }
    }

    RowLayout {
        Layout.fillWidth: true

        GlassButton {
            text: "Import Sound"
            pixelSize: 12
            onClicked: importDialog.open()
        }

        Item { Layout.fillWidth: true }

        GlassButton {
            text: "Refresh"
            pixelSize: 12
            onClicked: refresh()
        }
    }

    FileDialog {
        id: importDialog
        title: "Import alarm sound"
        nameFilters: ["Audio files (*.wav *.mp3 *.ogg *.flac *.aac)"]
        currentFolder: "file:///home"
        onAccepted: {
            var path = selectedFile.toString()
            if (path.indexOf("file://") === 0)
                path = path.substring(7)
            configManager.copyToTones(path)
            refresh()
        }
    }
}

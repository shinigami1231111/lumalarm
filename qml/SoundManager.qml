import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import GlassAlarm

ColumnLayout {
    id: root
    spacing: 14

    property var toneList: configManager.availableTones()

    Connections {
        target: configManager
        function onConfigChanged() { root.toneList = configManager.availableTones() }
    }
    Connections {
        target: audioPlayer
        function onIsPlayingChanged() { if (!audioPlayer.isPlaying) previewingFile = "" }
    }

    property string previewingFile: ""

    function refresh() { root.toneList = configManager.availableTones() }

    Text {
        text: "Sounds"
        color: configManager.themeTextPrimary
        font.pixelSize: 24
        font.bold: true
    }

    Text {
        text: "Tones folder: " + configManager.tonesDirectory()
        color: configManager.themeTextSecondary
        font.pixelSize: 13
        wrapMode: Text.WordWrap
    }

    ListView {
        Layout.fillWidth: true
        Layout.fillHeight: true
        spacing: 8
        clip: true
        model: toneList

        delegate: Rectangle {
            width: parent.width
            height: 46
            radius: 10
            color: previewingFile === modelData ? Qt.rgba(configManager.themeAccent.r, configManager.themeAccent.g, configManager.themeAccent.b, 0.18)
                  : Qt.rgba(1,1,1,0.05)
            border.color: Qt.rgba(1,1,1,0.08)
            border.width: 1

            RowLayout {
                anchors.fill: parent
                anchors.margins: 10
                spacing: 10

                Text {
                    text: modelData
                    color: configManager.themeTextPrimary
                    font.pixelSize: 15
                    elide: Text.ElideMiddle
                    Layout.fillWidth: true
                }

                GlassButton {
                    text: previewingFile === modelData ? "■ Stop" : "▶ Play"
                    pixelSize: 11
                    implicitWidth: 70
                    implicitHeight: 28
                    radius: 7
                    onClicked: {
                        if (previewingFile === modelData) { audioPlayer.stop(); previewingFile = "" }
                        else { audioPlayer.stop(); audioPlayer.preview(modelData); previewingFile = modelData }
                    }
                }

                GlassButton {
                    text: "Delete"
                    pixelSize: 11
                    implicitWidth: 64
                    implicitHeight: 28
                    radius: 7
                    baseColor: Qt.rgba(1,0.2,0.2,0.15)
                    hoverColor: Qt.rgba(1,0.2,0.2,0.28)
                    onClicked: { configManager.deleteTone(modelData); previewingFile = "" }
                }
            }
        }

        ScrollBar.vertical: ScrollBar { active: true; policy: ScrollBar.AsNeeded }
    }

    RowLayout {
        Layout.fillWidth: true
        GlassButton { text: "Import Sound"; pixelSize: 13; onClicked: importDialog.open() }
        Item { Layout.fillWidth: true }
        GlassButton { text: "Refresh"; pixelSize: 13; onClicked: refresh() }
    }

    FileDialog {
        id: importDialog
        title: "Import alarm sound"
        nameFilters: ["Audio files (*.wav *.mp3 *.ogg *.flac *.aac)"]
        currentFolder: "file:///home"
        onAccepted: {
            var path = selectedFile.toString()
            if (path.indexOf("file://") === 0) path = path.substring(7)
            configManager.copyToTones(path)
        }
    }
}

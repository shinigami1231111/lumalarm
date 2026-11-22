import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import GlassAlarm

ColumnLayout {
    id: root
    spacing: 14

    property var toneList: configManager.availableTones()

    // Feedback for a failed import or delete (Issue #2: never fail silently).
    // Cleared by the timer below and by the next successful operation.
    property string toneErrorText: ""
    // Source url of an import that hit an existing tone, held until the user
    // answers the inline Replace/Cancel prompt — Issue #2's duplicate policy.
    property string pendingReplace: ""
    property string previewingFile: ""

    // themeAccent is exposed as a string; assigning it to a colour property is
    // what makes .r/.g/.b readable (reading them off the string yields
    // undefined and renders the highlight black instead of accent-tinted).
    readonly property color accentColor: configManager.themeAccent

    Connections {
        target: configManager
        function onConfigChanged() { root.toneList = configManager.availableTones() }
    }
    Connections {
        target: audioPlayer
        function onIsPlayingChanged() { if (!audioPlayer.isPlaying) previewingFile = "" }
    }

    Timer { id: toneErrorTimer; interval: 6000; repeat: false; onTriggered: root.toneErrorText = "" }

    function refresh() { root.toneList = configManager.availableTones() }

    // The file dialog hands over a url; ConfigManager converts it through QUrl,
    // so names with spaces, '#', '%', Unicode or Arabic characters all import.
    function requestImport(source) {
        root.toneErrorText = ""
        if (configManager.copyToTones(source) !== "") { root.refresh(); return }
        // Duplicate? Ask instead of guessing — the only duplicate policy here.
        if (configManager.toneError === "exists") { root.pendingReplace = source; return }
        root.toneErrorText = configManager.toneErrorMessage
        toneErrorTimer.restart()
    }

    function confirmReplace() {
        var source = root.pendingReplace
        root.pendingReplace = ""
        root.toneErrorText = ""
        if (configManager.replaceTone(source) !== "") { root.refresh(); return }
        root.toneErrorText = configManager.toneErrorMessage
        toneErrorTimer.restart()
    }

    function cancelReplace() { root.pendingReplace = "" }

    function reportToneFailure() {
        root.toneErrorText = configManager.toneErrorMessage
        toneErrorTimer.restart()
    }

    Text {
        text: (configManager.showEmojis ? "🎵 " : "") + qsTr("Sounds")
        color: configManager.themeTextPrimary
        font.pixelSize: 24
        font.bold: true
    }

    Text {
        text: qsTr("Tones folder: ") + configManager.tonesDirectory
        color: configManager.themeTextSecondary
        font.pixelSize: 13
        wrapMode: Text.WordWrap
        Layout.fillWidth: true
    }

    // Package-installed drop-in folder, when this install has one.
    Text {
        visible: configManager.systemTonesDirectory !== ""
        text: qsTr("Shared tones: ") + configManager.systemTonesDirectory
        color: configManager.themeTextSecondary
        font.pixelSize: 13
        wrapMode: Text.WordWrap
        Layout.fillWidth: true
    }

    Text {
        visible: root.toneErrorText !== ""
        text: root.toneErrorText
        color: "#ff6b6b"
        font.pixelSize: 13
        wrapMode: Text.WordWrap
        Layout.fillWidth: true
    }

    Text {
        visible: root.toneList.length === 0
        Layout.fillWidth: true
        wrapMode: Text.WordWrap
        color: configManager.themeTextSecondary
        font.pixelSize: 13
        text: qsTr("No tones yet. Drop .wav, .mp3, .ogg, .flac or .aac files into the tones folder, or use Import Sound.")
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
            // themeAccent is a string property: reading .r off it yields
            // undefined, which made this highlight render black instead of
            // accent-tinted. Copy it into a colour first.
            color: previewingFile === modelData ? Qt.rgba(root.accentColor.r, root.accentColor.g, root.accentColor.b, 0.18)
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
                    text: previewingFile === modelData ? qsTr("■ Stop") : qsTr("▶ Play")
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
                    text: qsTr("Delete")
                    pixelSize: 11
                    implicitWidth: 64
                    implicitHeight: 28
                    radius: 7
                    baseColor: Qt.rgba(1,0.2,0.2,0.15)
                    hoverColor: Qt.rgba(1,0.2,0.2,0.28)
                    onClicked: {
                        previewingFile = ""
                        if (!configManager.deleteTone(modelData))
                            reportToneFailure()
                    }
                }
            }
        }

        ScrollBar.vertical: ScrollBar { active: true; policy: ScrollBar.AsNeeded }
    }

    RowLayout {
        Layout.fillWidth: true
        visible: root.pendingReplace === ""
        GlassButton { text: qsTr("Import Sound"); pixelSize: 13; onClicked: importDialog.open() }
        Item { Layout.fillWidth: true }
        GlassButton { text: qsTr("Refresh"); pixelSize: 13; onClicked: refresh() }
    }

    // Duplicate confirmation. Inline rather than a modal dialog so the prompt
    // sits exactly where the import controls already are.
    RowLayout {
        Layout.fillWidth: true
        spacing: 8
        visible: root.pendingReplace !== ""

        Text {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            color: configManager.themeTextPrimary
            font.pixelSize: 13
            text: configManager.toneErrorMessage
        }
        GlassButton { text: qsTr("Cancel"); pixelSize: 13; onClicked: cancelReplace() }
        GlassButton { text: qsTr("Replace"); pixelSize: 13; filled: true; onClicked: confirmReplace() }
    }

    FileDialog {
        id: importDialog
        title: qsTr("Import alarm sound")
        nameFilters: [qsTr("Audio files (*.wav *.mp3 *.ogg *.flac *.aac)")]
        currentFolder: "file:///home"
        onAccepted: requestImport(selectedFile)
    }
}

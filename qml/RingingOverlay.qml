import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import QtMultimedia
import GlassAlarm

Rectangle {
    id: root

    property bool ringing: false
    property var alarmData: ({})
    property int alarmIndex: -1
    property string alarmHour: "00"
    property string alarmMinute: "00"
    property string alarmMedia: ""

    signal stopAlarm()

    visible: ringing
    anchors.fill: parent
    color: Qt.rgba(0, 0, 0, 0.85)
    z: 999

    Item {
        anchors.fill: parent
        anchors.margins: 40
        AnimatedImage {
            id: gifPlayer
            anchors.fill: parent
            fillMode: Image.PreserveAspectFit
            playing: root.ringing
            visible: false
        }
        MediaPlayer {
            id: vidPlayer
            videoOutput: videoOut
            loops: MediaPlayer.Infinite
        }
        VideoOutput {
            id: videoOut
            anchors.fill: parent
            visible: false
        }
    }

    onRingingChanged: {
        if (ringing) {
            if (alarmMedia.endsWith(".mp4") || alarmMedia.endsWith(".avi") ||
                alarmMedia.endsWith(".mkv") || alarmMedia.endsWith(".webm"))
                vidPlayer.play()
            else if (alarmMedia.endsWith(".gif"))
                gifPlayer.playing = true
        } else {
            vidPlayer.stop()
            gifPlayer.playing = false
        }
    }

    onAlarmMediaChanged: {
        var isVideo = alarmMedia.endsWith(".mp4") || alarmMedia.endsWith(".avi") ||
                      alarmMedia.endsWith(".mkv") || alarmMedia.endsWith(".webm")
        var isGif = alarmMedia.endsWith(".gif")
        if (isVideo) {
            vidPlayer.source = "file://" + configManager.tonesDirectory + "/" + alarmMedia
            videoOut.visible = true; gifPlayer.visible = false
            if (ringing) vidPlayer.play()
        } else if (isGif) {
            gifPlayer.source = "file://" + configManager.tonesDirectory + "/" + alarmMedia
            gifPlayer.visible = true; videoOut.visible = false
            if (ringing) gifPlayer.playing = true
        } else {
            gifPlayer.visible = false; videoOut.visible = false
        }
    }

    ColumnLayout {
        anchors.centerIn: parent
        spacing: 24
        z: 10

        Text {
            text: "ALARM"
            color: "#FFFFFF"; font.pixelSize: 52; font.bold: true; opacity: 0.9
            Layout.alignment: Qt.AlignHCenter
            style: Text.Raised; styleColor: Qt.rgba(0,0,0,0.5)
        }

        Text {
            visible: Boolean(alarmData) && Boolean(alarmData.name) && alarmData.name !== ""
            text: (Boolean(alarmData) && Boolean(alarmData.name)) ? alarmData.name : ""
            color: Qt.rgba(1, 1, 1, 0.85); font.pixelSize: 26; font.bold: true
            Layout.alignment: Qt.AlignHCenter
            style: Text.Raised; styleColor: Qt.rgba(0,0,0,0.3)
        }

        Text {
            text: ("00" + alarmHour).slice(-2) + ":" + ("00" + alarmMinute).slice(-2)
            color: Qt.rgba(1, 1, 1, 0.6); font.pixelSize: 22
            Layout.alignment: Qt.AlignHCenter
        }

        GlassButton {
            text: "STOP"
            pixelSize: 20; implicitWidth: 140; implicitHeight: 60; radius: 16
            Layout.alignment: Qt.AlignHCenter
            baseColor: Qt.rgba(1, 0.2, 0.2, 0.2)
            hoverColor: Qt.rgba(1, 0.2, 0.2, 0.35)
            pressedColor: Qt.rgba(1, 0.2, 0.2, 0.5)
            onClicked: {
                audioPlayer.stop()
                ringing = false
            }
        }
    }

    SequentialAnimation {
        running: root.ringing
        loops: Animation.Infinite
        NumberAnimation { target: root; property: "opacity"; to: 0.7; duration: 800; easing.type: Easing.InOutQuad }
        NumberAnimation { target: root; property: "opacity"; to: 1.0; duration: 800; easing.type: Easing.InOutQuad }
    }
}

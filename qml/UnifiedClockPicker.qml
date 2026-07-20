import QtQuick
import QtQuick.Controls

// Style 3 — single analog clock with draggable hour/minute grips + AM/PM.
// Based on /home/dmk/Desktop/code for singel clock.txt (UnifiedClockPicker).
Item {
    id: root
    width: 260; height: 260
    property int hour: 7          // 0..23 (24h)
    property int minute: 0        // 0..59
    property bool isPM: false
    signal timeChanged(int hour, int minute)

    readonly property color cText: configManager.themeTextPrimary
    readonly property color cMuted: configManager.themeTextSecondary
    readonly property color cAccent: configManager.themeAccent
    readonly property color faceColor: Qt.rgba(0, 0, 0, 0.30)
    readonly property color tickColor: Qt.rgba(1, 1, 1, 0.30)

    readonly property real cx: width / 2
    readonly property real cy: height / 2

    readonly property real hourLen: 70
    readonly property real minuteLen: 98

    function emitTime() {
        root.isPM = root.hour >= 12
        root.timeChanged(root.hour, root.minute)
    }

    // Clock face
    Rectangle {
        anchors.centerIn: parent
        width: 260; height: 260; radius: 130
        color: root.faceColor
        border.color: Qt.rgba(1, 1, 1, 0.12); border.width: 1
    }

    Repeater {
        model: 60
        Rectangle {
            property real a: (index * 6) * Math.PI / 180
            property bool major: index % 5 === 0
            width: major ? 2 : 1; height: major ? 10 : 6; radius: 1
            color: major ? root.tickColor : Qt.rgba(1, 1, 1, 0.18)
            x: root.cx - width / 2 + (root.width / 2 - 12) * Math.sin(a)
            y: root.cy - height / 2 - (root.width / 2 - 12) * Math.cos(a)
        }
    }

    Repeater {
        model: ["12","1","2","3","4","5","6","7","8","9","10","11"]
        Text {
            property real a: (index * 30) * Math.PI / 180
            text: modelData
            font.pixelSize: 16; font.weight: Font.Medium
            color: root.cMuted
            width: 24; height: 20
            horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
            x: root.cx - width / 2 + (root.width / 2 - 40) * Math.sin(a)
            y: root.cy - height / 2 - (root.width / 2 - 40) * Math.cos(a)
        }
    }

    Rectangle {
        id: hourHand
        width: 6; height: root.hourLen; radius: 3
        color: root.cText; antialiasing: true
        x: root.cx - width / 2; y: root.cy - height
        transformOrigin: Item.Bottom
        rotation: (root.hour % 12) * 30 + root.minute * 0.5
        z: 2
    }
    Rectangle {
        id: minuteHand
        width: 4; height: root.minuteLen; radius: 2
        color: root.cAccent; antialiasing: true
        x: root.cx - width / 2; y: root.cy - height
        transformOrigin: Item.Bottom
        rotation: root.minute * 6
        z: 3
    }
    Rectangle {
        width: 10; height: 10; radius: 5; color: root.cText
        x: root.cx - width / 2; y: root.cy - height / 2
        z: 4
    }

    // Hour grip (sits at the hour-hand tip)
    Item {
        id: hourGrip
        width: 30; height: 30
        x: root.cx + root.hourLen * Math.sin(hourHand.rotation * Math.PI / 180) - width / 2
        y: root.cy - root.hourLen * Math.cos(hourHand.rotation * Math.PI / 180) - height / 2
        z: 5
        Rectangle {
            anchors.fill: parent; radius: width / 2
            color: root.cAccent; opacity: 0.35
        }
        MouseArea {
            anchors.fill: parent
            onPressed: parent.update(mouse.x, mouse.y)
            onPositionChanged: { if (pressed) parent.update(mouse.x, mouse.y) }
        }
        function update(px, py) {
            var pt = mapToItem(root, px, py)
            var ang = Math.atan2(pt.x - root.cx, root.cy - pt.y) * 180 / Math.PI
            if (ang < 0) ang += 360
            var nh = Math.round(ang / 30) % 12
            var isPM = root.hour >= 12
            var newH = isPM ? nh + 12 : nh
            if (newH !== root.hour) { root.hour = newH; emitTime() }
        }
    }
    // Minute grip (sits at the minute-hand tip)
    Item {
        id: minuteGrip
        width: 30; height: 30
        x: root.cx + root.minuteLen * Math.sin(minuteHand.rotation * Math.PI / 180) - width / 2
        y: root.cy - root.minuteLen * Math.cos(minuteHand.rotation * Math.PI / 180) - height / 2
        z: 5
        Rectangle {
            anchors.fill: parent; radius: width / 2
            color: root.cAccent; opacity: 0.35
        }
        MouseArea {
            anchors.fill: parent
            onPressed: parent.update(mouse.x, mouse.y)
            onPositionChanged: { if (pressed) parent.update(mouse.x, mouse.y) }
        }
        function update(px, py) {
            var pt = mapToItem(root, px, py)
            var ang = Math.atan2(pt.x - root.cx, root.cy - pt.y) * 180 / Math.PI
            if (ang < 0) ang += 360
            var nm = Math.round(ang / 6) % 60
            if (nm !== root.minute) { root.minute = nm; emitTime() }
        }
    }
}

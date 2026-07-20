import QtQuick
import QtQuick.Controls

// Style 2 — two separate clock dials (hour + minute), each with a draggable hand.
// Mirrors /home/dmk/Desktop/2/dual_clock_time_picker.html.
Item {
    id: root
    width: 420; height: 220
    property int hour: 7          // 0..23 (24h)
    property int minute: 0        // 0..59
    signal timeChanged(int hour, int minute)

    readonly property color cText: configManager.themeTextPrimary
    readonly property color cMuted: configManager.themeTextSecondary
    readonly property color cAccent: configManager.themeAccent
    readonly property color faceColor: Qt.rgba(0, 0, 0, 0.30)
    readonly property color tickColor: Qt.rgba(1, 1, 1, 0.35)

    function emitTime() { root.timeChanged(root.hour, root.minute) }

    // ---- Reusable dial ----
    component Dial: Item {
        id: dial
        width: 180; height: 180
        property int steps: 12
        property int majorCount: 12
        property var labels: []
        property int value: 0
        property color accent: root.cAccent
        signal valueModified(int v)

        readonly property real c: width / 2

        Rectangle {
            anchors.fill: parent
            radius: width / 2
            color: root.faceColor
            border.color: Qt.rgba(1, 1, 1, 0.12); border.width: 1
        }
        Repeater {
            model: dial.steps
            Rectangle {
                property real a: (index * (360 / dial.steps)) * Math.PI / 180
                property bool major: index % (dial.steps / dial.majorCount) === 0
                width: major ? 2 : 1; height: major ? 10 : 6
                color: major ? root.tickColor : Qt.rgba(1, 1, 1, 0.18)
                x: dial.c - width / 2 + (dial.width / 2 - 12) * Math.sin(a)
                y: dial.c - height / 2 - (dial.width / 2 - 12) * Math.cos(a)
            }
        }
        Repeater {
            model: dial.labels
            Text {
                property real a: (index * (360 / dial.labels.length)) * Math.PI / 180
                text: modelData
                font.pixelSize: 14; font.weight: Font.Medium
                color: root.cMuted
                x: dial.c - width / 2 + (dial.width / 2 - 34) * Math.sin(a)
                y: dial.c - height / 2 - (dial.width / 2 - 34) * Math.cos(a)
            }
        }
        Rectangle {
            id: hand
            width: 4; height: dial.width / 2 - 30
            radius: 2; color: dial.accent; antialiasing: true
            x: dial.c - width / 2
            y: dial.c - height
            transformOrigin: Item.Bottom
            rotation: (dial.value % dial.steps) * (360 / dial.steps)
        }
        Rectangle {
            width: 10; height: 10; radius: 5; color: dial.accent
            x: dial.c - width / 2; y: dial.c - height / 2
        }
        MouseArea {
            anchors.fill: parent
            onPressed: update(mouse.x, mouse.y)
            onPositionChanged: { if (pressed) update(mouse.x, mouse.y) }
        }
        function update(px, py) {
            var ang = Math.atan2(px - dial.c, dial.c - py) * 180 / Math.PI
            if (ang < 0) ang += 360
            var nv = Math.round(ang / (360 / steps)) % steps
            if (nv !== value) { value = nv; valueModified(value) }
        }
    }

    Row {
        anchors.centerIn: parent
        spacing: 28
        Column {
            spacing: 8; Dial {
                id: hourDial; steps: 12; majorCount: 12
                labels: ["12","1","2","3","4","5","6","7","8","9","10","11"]
                value: root.hour % 12 === 0 ? 12 : root.hour % 12
                onValueModified: {
                    var isPM = root.hour >= 12
                    root.hour = isPM ? v + 12 : v
                    emitTime()
                }
            }
            Text { text: String(hourDial.value === 0 ? 12 : hourDial.value).padStart(2,'0'); font.pixelSize: 22; font.weight: Font.Medium; color: cText; anchors.horizontalCenter: parent.horizontalCenter }
        }
        Column {
            spacing: 8; Dial {
                id: minuteDial; steps: 60; majorCount: 12
                labels: ["00","05","10","15","20","25","30","35","40","45","50","55"]
                value: root.minute
                onValueModified: { root.minute = v; emitTime() }
            }
            Text { text: String(minuteDial.value).padStart(2,'0'); font.pixelSize: 22; font.weight: Font.Medium; color: cText; anchors.horizontalCenter: parent.horizontalCenter }
        }
    }
}

import QtQuick
import QtQuick.Controls

// Style 1 — three scrollable wheels (hour / minute / AM-PM).
// Mirrors /home/dmk/Desktop/1/wheel_time_picker.html.
Item {
    id: root
    width: 280; height: 260
    property int hour: 7          // 0..23 (24h)
    property int minute: 0        // 0..59
    signal timeChanged(int hour, int minute)

    readonly property color cText: configManager.themeTextPrimary
    readonly property color cMuted: configManager.themeTextSecondary
    readonly property color cAccent: configManager.themeAccent

    function emitTime() { root.timeChanged(root.hour, root.minute) }

    Row {
        anchors.centerIn: parent
        spacing: 6

        Tumbler {
            id: hourTumbler
            width: 70; height: 240
            model: 12
            currentIndex: (root.hour % 12) - 1
            delegate: Text {
                text: String(modelData + 1).padStart(2, '0')
                font.pixelSize: Tumbler.displacement === 0 ? 22 : 18
                font.weight: Tumbler.displacement === 0 ? Font.Medium : Font.Normal
                opacity: Math.max(0.3, 1 - Math.abs(Tumbler.displacement) * 0.4)
                color: Tumbler.displacement === 0 ? cText : cMuted
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                width: parent.width; height: parent.height
            }
            onCurrentIndexChanged: {
                var base = currentIndex + 1
                var isPM = root.hour >= 12
                root.hour = isPM ? base + 12 : base
                emitTime()
            }
        }

        Text { text: ":"; font.pixelSize: 30; font.weight: Font.Medium; color: cText; anchors.verticalCenter: parent.verticalCenter }

        Tumbler {
            id: minuteTumbler
            width: 70; height: 240
            model: 60
            currentIndex: root.minute
            delegate: Text {
                text: String(modelData).padStart(2, '0')
                font.pixelSize: Tumbler.displacement === 0 ? 22 : 18
                font.weight: Tumbler.displacement === 0 ? Font.Medium : Font.Normal
                opacity: Math.max(0.3, 1 - Math.abs(Tumbler.displacement) * 0.4)
                color: Tumbler.displacement === 0 ? cText : cMuted
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                width: parent.width; height: parent.height
            }
            onCurrentIndexChanged: { root.minute = currentIndex; emitTime() }
        }

        Tumbler {
            id: periodTumbler
            width: 60; height: 240
            model: ["AM", "PM"]
            currentIndex: root.hour >= 12 ? 1 : 0
            delegate: Text {
                text: modelData
                font.pixelSize: Tumbler.displacement === 0 ? 20 : 17
                font.weight: Tumbler.displacement === 0 ? Font.Medium : Font.Normal
                opacity: Math.max(0.3, 1 - Math.abs(Tumbler.displacement) * 0.4)
                color: Tumbler.displacement === 0 ? cText : cMuted
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                width: parent.width; height: parent.height
            }
            onCurrentIndexChanged: {
                var isPM = currentIndex === 1
                var h12 = ((root.hour % 12) === 0 ? 12 : (root.hour % 12))
                root.hour = isPM ? h12 + 12 : h12
                emitTime()
            }
        }
    }
}

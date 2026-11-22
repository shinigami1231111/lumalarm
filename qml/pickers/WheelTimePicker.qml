import QtQuick
import QtQuick.Controls

// Style 1 — three scrollable wheels (hour / minute / AM-PM).
// Modelled on the original wheel_time_picker.html prototype.
Item {
    id: root
    width: 280; height: 260
    property int hour: 7          // 0..23 (internal, regardless of display mode)
    property int minute: 0        // 0..59
    signal timeChanged(int hour, int minute)

    // Settings > Time format: 24-hour shows 00-23 and hides the AM/PM wheel;
    // 12-hour keeps the original 1-12 + AM/PM behavior.
    readonly property bool is24h: configManager.timeFormat !== "12h"

    readonly property color cText: configManager.themeTextPrimary
    readonly property color cMuted: configManager.themeTextSecondary
    readonly property color cAccent: configManager.themeAccent

    // ── Quirk fix (v1.0.3) ───────────────────────────────────────────────────
    // Old behavior: each Tumbler bound `currentIndex` to a JS expression over
    // hour/minute, and `onCurrentIndexChanged` wrote hour/minute back
    // imperatively. Two failures resulted:
    //   1. ANY imperative write destroys QML bindings. The write-back destroyed
    //      the `currentIndex:` bindings on first interaction, and destroyed the
    //      `hour: editHour` / `minute: editMinute` instance bindings coming
    //      from AlarmEditor on the very first index change. From then on
    //      external updates (loadAlarm) could not move the wheels — the
    //      displayed value desynced from the bound model value.
    //   2. The 12-hour math was not a bijection: `(hour % 12) - 1` yields -1
    //      for hours 0/12, `base + 12` yields invalid hour 24 at the "12"
    //      slot, and the AM/PM handler flipped 0 <-> 12. Fast flicks cross
    //      many indices per gesture and hit those boundary cases mid-flick,
    //      where the write-back fought the in-flight position update, so the
    //      wheels settled showing one value while the model held another.
    //      Slow single-steps never interleaved that way and looked fine.
    // Fix: this component never writes hour/minute (it only reports
    // timeChanged — AlarmEditor owns the values and pushes them back through
    // the hour:/minute: bindings, which now survive forever). External changes
    // reach the tumblers through _syncTumblers(), guarded so programmatic
    // updates cannot re-enter the report path. hourToIndex()/indexToHour()
    // are exact inverses over 0..23, so every user step round-trips to itself
    // — nothing left to fight, fast or slow.
    // ─────────────────────────────────────────────────────────────────────────

    property bool _ready: false
    property bool _syncing: false

    // 0..23 -> tumbler index: direct in 24h mode, 12-hour slot map otherwise.
    function hourToIndex(h) {
        return root.is24h ? Math.max(0, Math.min(23, h)) : ((h % 12) + 11) % 12
    }

    // Exact inverse of hourToIndex(): index (+ AM/PM in 12h mode) -> 0..23.
    function indexToHour(idx, isPM) {
        if (root.is24h)
            return Math.max(0, Math.min(23, idx))
        var h12 = idx + 1            // 1..12, "12" for idx 11
        return isPM ? (h12 % 12) + 12 : h12 % 12
    }

    function _syncTumblers() {
        if (!_ready || _syncing) return
        _syncing = true
        hourTumbler.currentIndex = hourToIndex(root.hour)
        minuteTumbler.currentIndex = root.minute
        periodTumbler.currentIndex = root.hour >= 12 ? 1 : 0
        _syncing = false
    }
    onHourChanged: _syncTumblers()
    onMinuteChanged: _syncTumblers()
    Connections {
        target: configManager
        function onConfigChanged() { _syncTumblers() }
    }
    Component.onCompleted: { _ready = true; _syncTumblers() }

    Row {
        anchors.centerIn: parent
        spacing: 6

        Tumbler {
            id: hourTumbler
            width: 70; height: 240
            model: root.is24h ? 24 : 12
            delegate: Text {
                text: root.is24h ? String(modelData).padStart(2, '0')
                                 : String(modelData + 1).padStart(2, '0')
                font.pixelSize: Tumbler.displacement === 0 ? 22 : 18
                font.weight: Tumbler.displacement === 0 ? Font.Medium : Font.Normal
                opacity: Math.max(0.3, 1 - Math.abs(Tumbler.displacement) * 0.4)
                color: Tumbler.displacement === 0 ? cText : cMuted
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                width: parent.width; height: parent.height
            }
            onCurrentIndexChanged: {
                if (root._syncing) return    // programmatic sync — not user input
                root.timeChanged(indexToHour(currentIndex, root.hour >= 12), root.minute)
            }
        }

        Text { text: ":"; font.pixelSize: 30; font.weight: Font.Medium; color: cText; anchors.verticalCenter: parent.verticalCenter }

        Tumbler {
            id: minuteTumbler
            width: 70; height: 240
            model: 60
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
            onCurrentIndexChanged: {
                if (root._syncing) return    // programmatic sync — not user input
                root.timeChanged(root.hour, currentIndex)
            }
        }

        Tumbler {
            id: periodTumbler
            visible: !root.is24h
            width: 60; height: 240
            model: [qsTr("AM"), qsTr("PM")]
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
                if (root._syncing) return    // programmatic sync — not user input
                root.timeChanged(indexToHour(hourToIndex(root.hour), currentIndex === 1), root.minute)
            }
        }
    }
}

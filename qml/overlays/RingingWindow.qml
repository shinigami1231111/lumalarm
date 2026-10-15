import QtQuick
import QtQuick.Window
import QtQuick.Controls
import GlassAlarm

// ── ALARM WINDOW FOR THE HIDDEN CASE (Phase 4) ──────────────────────────────
// The ringing overlay is an Item inside the MAIN window. With that window
// hidden to tray, alarmUiMode "alarmOnly" had no UI to show at all — the alarm
// rang into a window nobody could see.
//
// This Window is created once alongside the main window and is never
// destroyed; it is shown and hidden purely by the binding below, so there is no
// imperative visibility code anywhere (I1). It only exists on screen while an
// alarm is actually ringing AND the main window is not up AND the user chose
// "alarmOnly".
//
// It reads the ringing state from main.qml's `root`, which is the single source
// of truth shared with the in-window overlay — one stop clears both.
Window {
    id: ringWin

    // `root` resolves to main.qml: this file is instantiated exactly there, so
    // the creation context's ids are in scope.
    visible: root.ringing
             && !lifecycle.windowShown
             && configManager.alarmUiMode === "alarmOnly"

    width: 420
    height: 300
    minimumWidth: 320
    minimumHeight: 240
    title: qsTr("Alarm")
    color: "transparent"

    // Dialog so it does not appear in the taskbar as a second app; stays on top
    // so it is not buried under other windows while an alarm is ringing.
    flags: Qt.Dialog | Qt.WindowStaysOnTopHint | Qt.FramelessWindowHint

    // Same component, same Snooze/Stop actions as the in-window overlay. No
    // native window handles are cached here, so nothing needs re-applying on
    // show (I7) — this window only hosts QML.
    RingingOverlay {
        anchors.fill: parent
        ringing: root.ringing
        alarmData: root.ringAlarm
        alarmIndex: root.ringIndex
        alarmHour: root.ringHour
        alarmMinute: root.ringMinute
        alarmMedia: root.ringMedia
        onStopAlarm: root.ringing = false
    }
}

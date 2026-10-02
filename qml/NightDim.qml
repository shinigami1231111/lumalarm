import QtQuick

// Night mode overlay: a full-window, non-interactive black veil that dims the
// app shortly before an alarm. It deliberately does not touch the system
// backlight — no root, no compositor-specific API.
//
// Opacity animates over a short transition so the dimming is not a hard cut,
// and the item never takes input, so it cannot swallow clicks meant for the UI
// underneath while the alarm is about to ring.
Item {
    id: root

    anchors.fill: parent
    visible: nightMode.active
    // Never intercept input: the UI below must stay usable.
    enabled: false

    Rectangle {
        id: veil
        anchors.fill: parent
        color: "black"

        // Animate so entering/leaving the window is a smooth fade.
        opacity: nightMode.dimOpacity
        Behavior on opacity {
            NumberAnimation { duration: 180; easing.type: Easing.InOutQuad }
        }

        // Re-read the target when the setting changes so the value is always
        // current even when the overlay is created already visible.
        onVisibleChanged: if (visible) opacity = nightMode.dimOpacity
    }

    // Safety net: if nightMode is unavailable (e.g. an older build), stay hidden.
    Component.onCompleted: {
        if (typeof nightMode === "undefined") {
            console.warn("NightDim: nightMode context property missing — overlay disabled")
            root.visible = false
        }
    }
}

import QtQuick
import QtQuick.Controls

// Native (QML) recreation of the analog_time_picker.html design.
// Drag the grip at the tip of either hand to set the time. Motion is smooth:
// while dragging the hands follow the pointer 1:1, and on release / external
// changes they animate (eased) to the target angle.
Item {
    id: root

    property int hour: 7     // 0..23 (24h)
    property int minute: 0   // 0..59

    implicitWidth: 260
    implicitHeight: 260

    // Internal smooth angles (degrees). These are what we paint from.
    property real hourAngle: (hour % 12) * 30 + minute * 0.5
    property real minuteAngle: minute * 6

    // Repaint whenever the smooth angles change (drag or animation).
    onHourAngleChanged: canvas.requestPaint()
    onMinuteAngleChanged: canvas.requestPaint()

    // When hour/minute change from the outside (text fields, load), retarget.
    onHourChanged: root.retarget()
    onMinuteChanged: root.retarget()

    function retarget() {
        if (face.dragging) return
        hourAnim.to = (hour % 12) * 30 + minute * 0.5
        minuteAnim.to = minute * 6
        hourAnim.start()
        minuteAnim.start()
    }

    Rectangle {
        id: face
        anchors.centerIn: parent
        width: 260; height: 260; radius: 130
        color: Qt.rgba(0, 0, 0, 0.35)
        border.color: Qt.rgba(1, 1, 1, 0.2)
        border.width: 1

        property bool dragging: false
        property string dragMode: "none"

        Canvas {
            id: canvas
            anchors.fill: parent
            antialiasing: true
            onPaint: {
                var ctx = getContext("2d")
                var cx = 130, cy = 130, r = 118
                ctx.clearRect(0, 0, width, height)

                // Numerals
                ctx.fillStyle = Qt.rgba(1, 1, 1, 0.6)
                ctx.font = "500 16px sans-serif"
                ctx.textAlign = "center"
                ctx.textBaseline = "middle"
                var nums = [12,1,2,3,4,5,6,7,8,9,10,11]
                for (var i = 0; i < 12; i++) {
                    var rad = (i * 30) * Math.PI / 180
                    var nx = cx + 86 * Math.sin(rad)
                    var ny = cy - 86 * Math.cos(rad)
                    ctx.fillText(nums[i], nx, ny)
                }

                // Ticks
                for (i = 0; i < 60; i++) {
                    var a = (i * 6) * Math.PI / 180
                    var isMajor = (i % 5 === 0)
                    var r1 = isMajor ? 100 : 108
                    var r2 = 114
                    var x1 = cx + r1 * Math.sin(a), y1 = cy - r1 * Math.cos(a)
                    var x2 = cx + r2 * Math.sin(a), y2 = cy - r2 * Math.cos(a)
                    ctx.beginPath()
                    ctx.moveTo(x1, y1); ctx.lineTo(x2, y2)
                    ctx.strokeStyle = isMajor ? Qt.rgba(1,1,1,0.5) : Qt.rgba(1,1,1,0.2)
                    ctx.lineWidth = isMajor ? 2 : 1
                    ctx.stroke()
                }

                // Hour hand
                var hRad = root.hourAngle * Math.PI / 180
                var hx = cx + 52 * Math.sin(hRad), hy = cy - 52 * Math.cos(hRad)
                ctx.beginPath(); ctx.moveTo(cx, cy); ctx.lineTo(hx, hy)
                ctx.strokeStyle = parent.dragMode === "hour" ? "#4DA6FF" : "#FFFFFF"
                ctx.lineWidth = 6; ctx.lineCap = "round"; ctx.stroke()

                // Minute hand
                var mRad = root.minuteAngle * Math.PI / 180
                var mx = cx + 84 * Math.sin(mRad), my = cy - 84 * Math.cos(mRad)
                ctx.beginPath(); ctx.moveTo(cx, cy); ctx.lineTo(mx, my)
                ctx.strokeStyle = parent.dragMode === "minute" ? "#4DA6FF" : Qt.rgba(0.3, 0.8, 1, 0.9)
                ctx.lineWidth = 4; ctx.lineCap = "round"; ctx.stroke()

                // Center dot
                ctx.beginPath(); ctx.arc(cx, cy, 5, 0, Math.PI * 2)
                ctx.fillStyle = "#FFFFFF"; ctx.fill()

                // Grips at the hand tips
                function grip(gx, gy, active, col) {
                    ctx.beginPath(); ctx.arc(gx, gy, 11, 0, Math.PI * 2)
                    ctx.fillStyle = active ? col : Qt.rgba(1, 1, 1, 0.05)
                    ctx.fill()
                    ctx.lineWidth = 2
                    ctx.strokeStyle = active ? "#FFFFFF" : Qt.rgba(1, 1, 1, 0.5)
                    ctx.stroke()
                }
                grip(hx, hy, parent.dragMode === "hour", "#4DA6FF")
                grip(mx, my, parent.dragMode === "minute", Qt.rgba(0.3, 0.8, 1, 0.9))
            }
        }

        // Smooth angle animations (eased). These drive the canvas repaint.
        NumberAnimation {
            id: hourAnim
            target: root; property: "hourAngle"
            duration: 180; easing.type: Easing.OutCubic
            onRunningChanged: canvas.requestPaint()
        }
        NumberAnimation {
            id: minuteAnim
            target: root; property: "minuteAngle"
            duration: 180; easing.type: Easing.OutCubic
            onRunningChanged: canvas.requestPaint()
        }

        function angleFromEvent(mx, my) {
            var cx = 130, cy = 130
            var angle = Math.atan2(mx - cx, cy - my) * 180 / Math.PI
            if (angle < 0) angle += 360
            return angle
        }

        function distFromCenter(mx, my) {
            return Math.sqrt((mx - 130) * (mx - 130) + (my - 130) * (my - 130))
        }

        function pickGrip(mx, my) {
            var cx = 130, cy = 130
            // Prefer the hand tip the pointer is nearest (within grab radius).
            var hRad = root.hourAngle * Math.PI / 180
            var hx = cx + 52 * Math.sin(hRad), hy = cy - 52 * Math.cos(hRad)
            var mRad = root.minuteAngle * Math.PI / 180
            var mx2 = cx + 84 * Math.sin(mRad), my2 = cy - 84 * Math.cos(mRad)
            var dH = Math.hypot(mx - hx, my - hy)
            var dM = Math.hypot(mx - mx2, my - my2)
            if (Math.min(dH, dM) <= 24) return dH <= dM ? "hour" : "minute"
            // Otherwise zone: inner half = hour, outer half = minute.
            var dist = distFromCenter(mx, my)
            if (dist < 118 * 0.5) return "hour"
            return "minute"
        }

        function endDrag() {
            parent.dragging = false
            parent.dragMode = "none"
            root.retarget()
        }

        function applyAngle(mode, mx, my) {
            var angle = angleFromEvent(mx, my)
            if (mode === "hour") {
                var h = Math.round(angle / 30) % 12
                var isPM = root.hour >= 12
                root.hour = isPM ? h + 12 : h
                root.hourAngle = (root.hour % 12) * 30 + root.minute * 0.5
            } else {
                root.minute = Math.round(angle / 6) % 60
                root.minuteAngle = root.minute * 6
            }
        }

        MouseArea {
            anchors.fill: parent
            onPressed: {
                parent.dragMode = parent.pickGrip(mouseX, mouseY)
                parent.dragging = true
                parent.applyAngle(parent.dragMode, mouseX, mouseY)
            }
            onPositionChanged: {
                if (!parent.dragging) return
                parent.applyAngle(parent.dragMode, mouseX, mouseY)
            }
            onReleased: {
                parent.endDrag()
            }
            onCanceled: {
                parent.endDrag()
            }
        }
    }
}

import QtQuick

// Transparent edge/corner strip for a frameless Window. No chrome is drawn —
// only the cursor changes and the compositor gets a system resize gesture.
Rectangle {
    id: resizeHandle

    property int edge: 0
    property var target: null
    property int cursor: Qt.SizeHorCursor

    // Qt 6.8+ exposes startSystemResize on Window. Older Qt 6.5 builds keep a
    // manual geometry fallback, which is also useful if a platform declines
    // the system gesture.
    property bool systemResize: target !== null && typeof target.startSystemResize === "function"
    property bool manualActive: false
    property point startGlobal: Qt.point(0, 0)
    property real startX: 0
    property real startY: 0
    property real startW: 0
    property real startH: 0

    // Resize edges are physical screen edges, even when the UI mirrors.
    LayoutMirroring.enabled: false
    LayoutMirroring.childrenInherit: false

    color: "transparent"
    z: 10

    function beginManual(mouse) {
        startGlobal = area.mapToGlobal(Qt.point(mouse.x, mouse.y))
        startX = target.x
        startY = target.y
        startW = target.width
        startH = target.height
        manualActive = true
    }

    function applyManual(mouse) {
        const p = area.mapToGlobal(Qt.point(mouse.x, mouse.y))
        let dx = p.x - startGlobal.x
        let dy = p.y - startGlobal.y
        let x = startX
        let y = startY
        let w = startW
        let h = startH

        if (edge & Qt.LeftEdge) {
            dx = Math.min(dx, startW - target.minimumWidth)
            x = startX + dx
            w = startW - dx
        }
        if (edge & Qt.RightEdge)
            w = Math.max(target.minimumWidth, startW + dx)
        if (edge & Qt.TopEdge) {
            dy = Math.min(dy, startH - target.minimumHeight)
            y = startY + dy
            h = startH - dy
        }
        if (edge & Qt.BottomEdge)
            h = Math.max(target.minimumHeight, startH + dy)

        target.x = x
        target.y = y
        target.width = w
        target.height = h
    }

    MouseArea {
        id: area
        anchors.fill: parent
        acceptedButtons: Qt.LeftButton
        hoverEnabled: true
        cursorShape: resizeHandle.cursor

        onPressed: function(mouse) {
            if (resizeHandle.systemResize) {
                resizeHandle.target.startSystemResize(resizeHandle.edge)
                return
            }
            if (target)
                beginManual(mouse)
        }
        onPositionChanged: function(mouse) {
            if (resizeHandle.manualActive)
                applyManual(mouse)
        }
        onReleased: resizeHandle.manualActive = false
    }
}

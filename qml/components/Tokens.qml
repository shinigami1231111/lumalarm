pragma Singleton
import QtQuick

// Dark Glass design tokens.
// Single source of truth for colour, radius, motion and spacing so no alpha,
// radius or duration is ever hand-written in a component file.
// Nothing in the app reads Tokens yet — this section only creates the
// foundation. Existing components keep their current values, so the app must
// look identical after this section.

QtObject {
    // ── colour ──
    readonly property color accent: "#3a86ff"
    readonly property color accentHover: Qt.rgba(0.23, 0.53, 1.0, 0.14)
    readonly property color accentPress: Qt.rgba(0.23, 0.53, 1.0, 0.28)
    readonly property color accentText: "#ffffff"

    readonly property color textPrimary: "#ffffff"
    readonly property color textSecondary: Qt.rgba(1, 1, 1, 0.7)

    // Base opacity of the glass background (0..1).
    readonly property real baseAlpha: 0.05
    // Corner radius of the root window and cards.
    readonly property real windowRadius: 22

    // ── alpha helpers ──
    // Call these instead of hand-writing an alpha, e.g. Tokens.accentA(0.18)
    function accentA(a) { return Qt.rgba(0.23, 0.53, 1.0, a) }
    function whiteA(a)  { return Qt.rgba(1, 1, 1, a) }
    function blackA(a)  { return Qt.rgba(0, 0, 0, a) }

    // ── radius scale ──
    readonly property real radiusXs: 4
    readonly property real radiusS: 10
    readonly property real radiusM: 22
    readonly property real radiusL: 32
    readonly property real radiusXl: 40

    // ── 4 px spacing grid ──
    readonly property real space4: 4
    readonly property real space8: 8
    readonly property real space12: 12
    readonly property real space16: 16
    readonly property real space20: 20

    // ── motion durations ──
    readonly property int motionFast: 150
    readonly property int motionNormal: 240
    readonly property int motionSlow: 350

    // ── one shared bezier curve ──
    readonly property var ease: Qt.bezierCurve(0.25, 0.1, 0.25, 1.0)
}

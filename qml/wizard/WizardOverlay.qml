import QtQuick
import QtQuick.Layouts
import GlassAlarm

// First-run wizard overlay (STG-5.5). Mounted at z:1000 in main.qml so it
// covers everything — including the ringing overlay (z:950) and the night
// dim veil (z:900) — while the setup is open.
Item {
    id: root
    anchors.fill: parent
    z: 1000
    visible: !wizard.wizardCompleted

    // Scrim: matches the app's glass language and, being a real item over the
    // whole window, also stops clicks reaching the UI underneath.
    Rectangle {
        anchors.fill: parent
        color: Qt.rgba(0, 0, 0, 0.55)
        Behavior on color { ColorAnimation { duration: 160 } }
    }

    Rectangle {
        id: card
        width: Math.min(parent.width - 64, 640)
        height: Math.min(parent.height - 80, 540)
        anchors.centerIn: parent
        radius: 20
        color: Qt.rgba(1, 1, 1, 0.10)
        border.color: Qt.rgba(1, 1, 1, 0.18)
        border.width: 1

        // Cross-fade between steps: the stack dips and returns, <=200 ms total.
        StackLayout {
            id: stack
            anchors.fill: parent
            anchors.margins: 28
            currentIndex: wizard.currentStep
            opacity: 1
            Behavior on opacity { NumberAnimation { duration: 100; easing.type: Easing.InOutQuad } }

            LanguagePage { }
            WakeMethodPage { }
            NotificationPage { }
            DonePage { }
        }

        Timer {
            id: fadeBack
            interval: 100
            repeat: false
            onTriggered: stack.opacity = 1
        }

        Connections {
            target: wizard
            function onCurrentStepChanged() {
                if (!root.visible) return
                stack.opacity = 0
                fadeBack.restart()
            }
        }
    }

    // First frame: focus the primary button of the page that is showing.
    onVisibleChanged: {
        if (visible && stack.currentIndex !== wizard.currentStep)
            stack.currentIndex = wizard.currentStep
    }
}

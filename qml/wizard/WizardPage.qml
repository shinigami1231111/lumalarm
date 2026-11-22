import QtQuick
import QtQuick.Layouts
import GlassAlarm

// Shared chrome for every wizard page: title, subtitle, content slot and the
// footer (Back · Skip setup · primary action). Pages only describe their
// content and which label the primary button carries, so the six steps stay
// visually identical without duplicating layout code.
Rectangle {
    id: page
    color: "transparent"

    property string title: ""
    property string subtitle: ""
    // Content slot — pages drop their own children straight into the column.
    default property alias content: contentColumn.data

    property string primaryLabel: (configManager.emojiLevel, configManager.emoji("➡️", 2)) + qsTr("Next")
    property bool primaryEnabled: wizard.canGoNext
    signal primaryClicked()

    ColumnLayout {
        id: headerColumn
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        spacing: 6

        Text {
            text: page.title
            color: configManager.themeAccent
            font.pixelSize: 22
            font.bold: true
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
        }
        Text {
            text: page.subtitle
            color: configManager.themeTextSecondary
            font.pixelSize: 14
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
        }
    }

    ColumnLayout {
        id: contentColumn
        anchors.top: headerColumn.bottom
        anchors.topMargin: 20
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: footerRow.top
        anchors.bottomMargin: 20
        spacing: 12
    }

    RowLayout {
        id: footerRow
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        spacing: 12

        GlassButton {
            text: (configManager.emojiLevel, configManager.emoji("🔙", 2)) + qsTr("Back")
            pixelSize: 13
            visible: wizard.canGoBack
            enabled: wizard.canGoBack
            onClicked: wizard.back()
        }

        // Skip is offered on every page: it marks the setup as done (so it
        // never auto-shows again) and discards whatever was chosen so far.
        Text {
            id: skipLink
            text: (configManager.emojiLevel, configManager.emoji("⏭️", 2)) + qsTr("Skip setup")
            color: configManager.themeTextSecondary
            font.pixelSize: 13
            font.underline: true
            Layout.leftMargin: 6
            MouseArea {
                anchors.fill: parent
                anchors.margins: -6
                cursorShape: Qt.PointingHandCursor
                onClicked: wizard.skip()
            }
        }

        Item { Layout.fillWidth: true }

        GlassButton {
            id: primaryButton
            text: page.primaryLabel
            pixelSize: 14
            filled: true
            enabled: page.primaryEnabled
            onClicked: page.primaryClicked()
        }
    }

    // Focus lands on the primary button whenever this page becomes the
    // current one, so Enter/Space walks the wizard forward.
    onVisibleChanged: {
        if (visible)
            Qt.callLater(function() { primaryButton.forceActiveFocus() })
    }
}

import QtQuick
import QtQuick.Layouts
import GlassAlarm

WizardPage {
    id: root
    title: qsTr("Welcome to Lumalarm")
    subtitle: qsTr("This short setup picks your language and checks the three things an alarm clock depends on: which sound new alarms use, how this machine wakes up, and whether desktop notifications are available. It takes about a minute.")
    primaryLabel: qsTr("Get started")
    primaryEnabled: true
    onPrimaryClicked: wizard.next()

    Text {
        Layout.fillWidth: true
        wrapMode: Text.WordWrap
        color: configManager.themeTextPrimary
        font.pixelSize: 14
        text: qsTr("You can run this setup again at any time from Settings, and skip any step along the way.")
    }

    Item { Layout.fillHeight: true }
}

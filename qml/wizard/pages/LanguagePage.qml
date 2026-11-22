import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import GlassAlarm

// Page 1 — language. First thing the user sees: pick the interface language.
// The same selector Settings offers, pointed at the same `ui/language` key, so
// the STG-5.2 mechanism is reused wholesale: main.cpp swaps the catalog live
// while this page is still on screen.
WizardPage {
    id: root
    title: (configManager.emojiLevel, configManager.emoji("🌐", 2)) + qsTr("Language")
    subtitle: qsTr("Choose the language Lumalarm uses. It takes effect right away.")
    primaryLabel: qsTr("Next")
    primaryEnabled: true
    onPrimaryClicked: wizard.next()

    // Language codes stored in settings. Display names stay in their own
    // script — only "System default" is translated.
    readonly property var languageCodes: ["", "en", "ja"]

    function languageIndexFor(code) {
        return code === "en" ? 1 : (code === "ja" ? 2 : 0)
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: 14

        Text {
            text: (configManager.emojiLevel, configManager.emoji("🌍", 2)) + qsTr("Interface language")
            color: configManager.themeTextPrimary
            font.pixelSize: 15
            Layout.fillWidth: true
        }

        RoundedCombo {
            id: languageCombo
            model: [qsTr("System default"), "English", "日本語"]
            Layout.preferredWidth: 200

            function syncToLanguage() {
                currentIndex = root.languageIndexFor(wizard.language)
            }
            Component.onCompleted: syncToLanguage()
            onModelChanged: syncToLanguage()
            onActivated: wizard.language = root.languageCodes[currentIndex]
        }
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: 14

        Text {
            text: (configManager.emojiLevel, configManager.emoji("🎭", 2)) + qsTr("Show emojis")
            color: configManager.themeTextPrimary
            font.pixelSize: 14
        }

        RoundedCombo {
            model: [qsTr("Off"), qsTr("Less"), qsTr("More")]
            currentIndex: wizard.emojiLevel
            Layout.preferredWidth: 110
            onActivated: wizard.emojiLevel = currentIndex
        }
    }

    Text {
        Layout.fillWidth: true
        wrapMode: Text.WordWrap
        color: configManager.themeTextSecondary
        font.pixelSize: 13
        text: qsTr("“System default” follows your desktop language.")
    }

    Item { Layout.fillHeight: true }
}

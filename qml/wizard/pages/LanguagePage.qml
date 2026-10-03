import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import GlassAlarm

// Page 2 — language. This is the same selector Settings offers, pointed at the
// same single `ui/language` key, so picking a language reuses the STG-5.2
// mechanism wholesale: main.cpp swaps the QTranslator, calls
// engine.retranslate() and the layout mirrors for Arabic — live, while this
// page is still on screen.
WizardPage {
    id: root
    title: qsTr("Language")
    subtitle: qsTr("Choose the language Lumalarm should use. It takes effect right away and can be changed again at any time in Settings.")
    primaryLabel: qsTr("Next")
    primaryEnabled: true
    onPrimaryClicked: wizard.next()

    // The three supported entries, in the order Settings lists them. Language
    // names stay in their own script on purpose — a label the reader cannot
    // read defeats the purpose — so only "System default" is translatable.
    readonly property var languageCodes: ["", "en", "ar"]

    function languageIndexFor(code) {
        return code === "en" ? 1 : (code === "ar" ? 2 : 0)
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: 14

        Text {
            text: qsTr("Interface language")
            color: configManager.themeTextPrimary
            font.pixelSize: 15
            Layout.fillWidth: true
        }

        RoundedCombo {
            id: languageCombo
            model: [qsTr("System default"), "English", "العربية"]
            Layout.preferredWidth: 200

            // No declarative currentIndex binding: retranslate() rebuilds the
            // model above, and a ComboBox that has just been handed a new model
            // is free to drop the index it was showing. Re-asserting the stored
            // choice right after the rebuild is the one rule that survives both
            // paths — and it is re-applied on the very click that changes the
            // language, too.
            function syncToLanguage() {
                currentIndex = root.languageIndexFor(wizard.language)
            }
            Component.onCompleted: syncToLanguage()
            onModelChanged: syncToLanguage()
            onActivated: wizard.language = root.languageCodes[currentIndex]
        }
    }

    Text {
        Layout.fillWidth: true
        wrapMode: Text.WordWrap
        color: configManager.themeTextSecondary
        font.pixelSize: 13
        text: qsTr("Choosing Arabic mirrors the whole window to right-to-left as soon as it is selected.")
    }

    Text {
        Layout.fillWidth: true
        wrapMode: Text.WordWrap
        color: configManager.themeTextSecondary
        font.pixelSize: 13
        text: qsTr("“System default” follows the language your desktop is running in.")
    }

    Item { Layout.fillHeight: true }
}

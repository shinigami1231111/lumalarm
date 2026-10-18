import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import GlassAlarm

// Page 1 — language. First thing the user sees: pick the interface language,
// then choose whether Arabic mirrors the whole window and its buttons. The
// same selector Settings offers, pointed at the same `ui/language` key, so the
// STG-5.2 mechanism is reused wholesale: main.cpp swaps the catalog and the
// layout mirrors live while this page is still on screen.
WizardPage {
    id: root
    title: qsTr("Language")
    subtitle: qsTr("Choose the language Lumalarm uses. It takes effect right away.")
    primaryLabel: qsTr("Next")
    primaryEnabled: true
    onPrimaryClicked: wizard.next()

    // Language codes stored in settings. Display names stay in their own
    // script — only "System default" is translated.
    readonly property var languageCodes: ["", "en", "ar", "ja"]

    function languageIndexFor(code) {
        return code === "en" ? 1 : (code === "ar" ? 2 : (code === "ja" ? 3 : 0))
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
            model: [qsTr("System default"), "English", "العربية", "日本語"]
            Layout.preferredWidth: 200

            function syncToLanguage() {
                currentIndex = root.languageIndexFor(wizard.language)
            }
            Component.onCompleted: syncToLanguage()
            onModelChanged: syncToLanguage()
            onActivated: wizard.language = root.languageCodes[currentIndex]
        }
    }

    // Mirror toggles — only offered for Arabic.
    property bool isArabic: wizard.language === "ar"
    RowLayout {
        Layout.fillWidth: true
        visible: isArabic
        spacing: 14

        Text {
            text: qsTr("Mirror the app right-to-left")
            color: configManager.themeTextPrimary
            font.pixelSize: 14
        }

        ToggleSwitch {
            checked: configManager.mirrorApp
            onCheckedChanged: configManager.mirrorApp = checked
        }
    }

    RowLayout {
        Layout.fillWidth: true
        visible: isArabic
        spacing: 14

        Text {
            text: qsTr("Mirror window buttons (quit · minimize · maximize)")
            color: configManager.themeTextPrimary
            font.pixelSize: 14
        }

        ToggleSwitch {
            checked: configManager.mirrorWindowButtons
            onCheckedChanged: configManager.mirrorWindowButtons = checked
        }
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: 14

        Text {
            text: qsTr("Show emojis")
            color: configManager.themeTextPrimary
            font.pixelSize: 14
        }

        ToggleSwitch {
            checked: configManager.showEmojis
            onCheckedChanged: configManager.showEmojis = checked
        }
    }

    Text {
        Layout.fillWidth: true
        wrapMode: Text.WordWrap
        color: configManager.themeTextSecondary
        font.pixelSize: 13
        text: isArabic
            ? qsTr("Arabic mirrors the window right-to-left when the toggles above are on.")
            : qsTr("“System default” follows your desktop language.")
    }

    Item { Layout.fillHeight: true }
}

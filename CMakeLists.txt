cmake_minimum_required(VERSION 3.21)
project(Lumalarm VERSION 1.0.5 LANGUAGES CXX)

# C++17 (Qt 6's required language level) and AUTORCC are set explicitly on
# purpose: qt_standard_project_setup() only defaults CMAKE_AUTOMOC/CMAKE_AUTOUIC
# to TRUE (see Qt6CoreMacros.cmake). It does NOT set CMAKE_AUTORCC or the C++
# standard, so these must stay to keep behavior unchanged.
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Multimedia Qml Quick QuickControls2 Widgets DBus)

qt_standard_project_setup(REQUIRES 6.5)

# Optional KDE blur-behind support (KWin). Enabled automatically when
# KWindowSystem is found; can be forced off with -DLUMALARM_ENABLE_KDE_BLUR=OFF.
option(LUMALARM_ENABLE_KDE_BLUR "Enable KDE/KWin native blur-behind (requires KWindowSystem)" ON)
if(LUMALARM_ENABLE_KDE_BLUR)
    find_package(KF6WindowSystem)
    if(KF6WindowSystem_FOUND)
        message(STATUS "Lumalarm: KDE blur-behind support ENABLED (KF6WindowSystem found)")
        add_definitions(-DLUMALARM_ENABLE_KDE_BLUR)
    else()
        message(WARNING "LUMALARM_ENABLE_KDE_BLUR is ON but KF6WindowSystem was not found. "
                        "Building WITHOUT KDE blur-behind support (transparency still works; "
                        "KWin blur must be done via other means).")
        set(LUMALARM_ENABLE_KDE_BLUR OFF)
    endif()
else()
    message(STATUS "Lumalarm: KDE blur-behind support DISABLED (LUMALARM_ENABLE_KDE_BLUR=OFF)")
endif()

set(SOURCES
    src/main.cpp
    src/managers/AlarmManager.cpp
    src/models/AlarmModel.cpp
    src/audio/AlarmAudioController.cpp
    src/audio/SystemMixer.cpp
    src/audio/MixerBackend.cpp
    src/audio/PactlMixer.cpp
    src/audio/WpctlMixer.cpp
    src/audio/AmixerMixer.cpp
    src/audio/NullMixer.cpp
    src/audio/VolumePolicy.cpp
    src/audio/RestoreJournal.cpp
    src/audio/SoundResolver.cpp
    src/audio/AlarmPlayer.cpp
    src/audio/MprisPause.cpp
    src/wake/Scheduler.cpp
    src/wake/SchedulerState.cpp
    src/wake/WakeManager.cpp
    src/managers/ConfigManager.cpp
    src/managers/ThemeManager.cpp
    src/ui/BlurManager.cpp
    src/ui/Notifier.cpp
    src/ui/TrayController.cpp
    src/app/AppLifecycle.cpp
    src/app/SingleInstance.cpp
    src/ui/NightMode.cpp
    src/ui/WizardController.cpp
)

set(HEADERS
    src/models/Alarm.h
    src/managers/AlarmManager.h
    src/models/AlarmModel.h
    src/audio/AlarmAudioController.h
    src/audio/SystemMixer.h
    src/audio/MixerBackend.h
    src/audio/PactlMixer.h
    src/audio/WpctlMixer.h
    src/audio/AmixerMixer.h
    src/audio/NullMixer.h
    src/audio/VolumePolicy.h
    src/audio/RestoreJournal.h
    src/audio/SoundResolver.h
    src/audio/AlarmPlayer.h
    src/audio/MprisPause.h
    src/wake/Scheduler.h
    src/wake/SchedulerState.h
    src/wake/WakeManager.h
    src/managers/ConfigManager.h
    src/managers/TonePaths.h
    src/managers/ThemeManager.h
    src/ui/BlurManager.h
    src/ui/Notifier.h
    src/ui/TrayController.h
    src/app/AppLifecycle.h
    src/app/SingleInstance.h
    src/ui/NightMode.h
    src/ui/WizardController.h
)

set(QML_FILES
    qml/main.qml
    qml/NightDim.qml
    qml/wizard/WizardOverlay.qml
    qml/wizard/WizardPage.qml
    qml/wizard/pages/LanguagePage.qml
    qml/wizard/pages/WakeMethodPage.qml
    qml/wizard/pages/NotificationPage.qml
    qml/wizard/pages/AlarmSoundPage.qml
    qml/wizard/pages/DonePage.qml
    qml/components/GlassButton.qml
    qml/components/GlassCard.qml
    qml/components/Tokens.qml
    qml/pages/AlarmListView.qml
    qml/pages/AlarmEditor.qml
    qml/overlays/RingingOverlay.qml
    qml/overlays/SoundManager.qml
    qml/overlays/CloseDialog.qml
    qml/overlays/RingingWindow.qml
    qml/pages/TimerPage.qml
    qml/pages/StopwatchPage.qml
    qml/pages/SettingsPage.qml
    qml/components/RoundedCombo.qml
    qml/components/WakeMethodPicker.qml
    qml/components/GlassCheckBox.qml
    qml/pickers/ClockFace.qml
    qml/components/ToggleSwitch.qml
    qml/components/SegmentedControl.qml
    qml/components/WindowResizeHandle.qml
    qml/pickers/WheelTimePicker.qml
)

qt6_add_executable(lumalarm ${SOURCES} ${HEADERS})

# Sources are grouped into modules under src/ (models/managers/audio/wake/ui);
# all project includes are rooted at src/ (e.g. #include "wake/Scheduler.h").
target_include_directories(lumalarm PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

qt6_add_qml_module(lumalarm
    URI GlassAlarm
    VERSION 1.0
    QML_FILES ${QML_FILES}
    RESOURCES
        resources/lumalarm.qrc
        resources/icons/alarms.svg
        resources/icons/timer.svg
        resources/icons/stopwatch.svg
        resources/icons/sounds.svg
        resources/icons/settings.svg
)

target_link_libraries(lumalarm PRIVATE
    Qt6::Core
    Qt6::Multimedia
    Qt6::Qml
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::Widgets
    Qt6::DBus
)

if(LUMALARM_ENABLE_KDE_BLUR)
    target_link_libraries(lumalarm PRIVATE KF6::WindowSystem)
endif()

# ── Translations (STG-5.2) ───────────────────────────────────────────────────
# Qt Linguist is a BUILD-time tool only: lrelease compiles translations/*.ts to
# .qm and qt_add_translations embeds them in the binary under /i18n. It is
# deliberately not REQUIRED — a machine (or a CI image) without lupdate/lrelease
# still configures, builds and runs, simply falling back to the English source
# strings, which is exactly what QTranslator does when no catalog matches.
find_package(Qt6 QUIET COMPONENTS LinguistTools)
if(Qt6LinguistTools_FOUND)
    message(STATUS "Lumalarm: Qt Linguist found — translations enabled")
    # SOURCE_TARGETS limits lupdate to the application, so the test binaries do
    # not leak strings into the catalogs. The .ts files are committed and only
    # refreshed by the update_translations target, never by a normal build, so
    # a build cannot dirty the working tree.
    qt_add_translations(lumalarm
        TS_FILES
            ${CMAKE_CURRENT_SOURCE_DIR}/translations/lumalarm_en.ts
            ${CMAKE_CURRENT_SOURCE_DIR}/translations/lumalarm_ar.ts
            ${CMAKE_CURRENT_SOURCE_DIR}/translations/lumalarm_ja.ts
        SOURCE_TARGETS lumalarm
        RESOURCE_PREFIX "/i18n"
    )
else()
    message(WARNING "Lumalarm: Qt6 LinguistTools not found — building WITHOUT "
                    "compiled translations; the UI stays in English.")
endif()

# ── Wake helper: privileged D-Bus system service (STG-1.1) ───────────────────
# Replaces the sudoers NOPASSWD rtcwake rule; see helper/lumalarm-wake-helper.cpp
qt_add_executable(lumalarm-wake-helper
    helper/lumalarm-wake-helper.cpp
    src/wake/WakeCaps.cpp
)
target_link_libraries(lumalarm-wake-helper PRIVATE Qt6::Core Qt6::DBus)
# The helper shares the STG-1 boundary/polkit/timer contract with the app so
# the two can never enforce different rules (see src/wake/WakeSecurity.h).
target_include_directories(lumalarm-wake-helper PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

# ── Tests ─────────────────────────────────────────────────────────────────────
include(CTest)
if(BUILD_TESTING)
    find_package(Qt6 REQUIRED COMPONENTS Test)
    qt6_add_executable(tst_schedulerlogic
        tests/tst_schedulerlogic.cpp
        src/managers/AlarmManager.cpp
        src/models/AlarmModel.cpp
        src/wake/Scheduler.cpp
        src/wake/SchedulerState.cpp
        src/wake/WakeManager.cpp
    )
    target_include_directories(tst_schedulerlogic PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(tst_schedulerlogic PRIVATE Qt6::Core Qt6::Test Qt6::DBus)
    add_test(NAME tst_schedulerlogic COMMAND tst_schedulerlogic)

    # Test-alarm surface of WakeManager (armTestWake/cancelTestWake). Range
    # validation always runs; the arming cases skip when the privileged D-Bus
    # helper is not present on the test machine.
    qt6_add_executable(tst_wakemanager
        tests/tst_wakemanager.cpp
        src/wake/WakeManager.cpp
    )
    target_include_directories(tst_wakemanager PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(tst_wakemanager PRIVATE Qt6::Core Qt6::Test Qt6::DBus)
    add_test(NAME tst_wakemanager COMMAND tst_wakemanager)

    # ── STG-1 security regression tests ───────────────────────────────────
    # Deterministic coverage of the shared validation boundary, the polkit
    # argument list and the packaging contract (no sudoers, required D-Bus /
    # polkit / systemd files), plus the live helper checks — bounds, replace
    # semantics, restart rediscovery and non-root denial — which QSKIP when
    # the system bus, systemd and an installed helper are not available.
    qt6_add_executable(tst_stg1security
        tests/tst_stg1security.cpp
        src/wake/WakeManager.cpp
    )
    target_include_directories(tst_stg1security PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(tst_stg1security PRIVATE Qt6::Core Qt6::Test Qt6::DBus)
    # The packaging/install assertions read the real source and build trees.
    target_compile_definitions(tst_stg1security PRIVATE
        STG1_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
        STG1_BINARY_DIR="${CMAKE_BINARY_DIR}"
    )
    add_test(NAME tst_stg1security COMMAND tst_stg1security)

    # ── WAKE-1 §A: capability detection ───────────────────────────────────
    # The reader parses fixture sys-trees, never the live machine, so the two
    # no-assumption rules — `mem` is not assumed to be S3, rtc0 is not assumed
    # to be wake-capable — are asserted as data on every run.
    qt6_add_executable(tst_wakecaps
        tests/tst_wakecaps.cpp
        src/wake/WakeCaps.cpp
    )
    target_include_directories(tst_wakecaps PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(tst_wakecaps PRIVATE Qt6::Core Qt6::Test)
    add_test(NAME tst_wakecaps COMMAND tst_wakecaps)

    # Notifier (src/ui/): construction and the disabled / no-bus paths. The
    # live-daemon path is verified manually with busctl --user monitor.
    qt6_add_executable(tst_notifier
        tests/tst_notifier.cpp
        src/ui/Notifier.cpp
    )
    target_include_directories(tst_notifier PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(tst_notifier PRIVATE Qt6::Core Qt6::Test Qt6::DBus)
    add_test(NAME tst_notifier COMMAND tst_notifier)

    # TrayController (src/ui/): construction, usability and tooltip text on a
    # headless box (offscreen platform, no system tray -> icon stays unusable,
    # no crash).
    qt6_add_executable(tst_trayicon
        tests/tst_trayicon.cpp
        src/ui/TrayController.cpp
        src/wake/Scheduler.cpp
        src/wake/SchedulerState.cpp
        src/wake/WakeManager.cpp
        src/managers/AlarmManager.cpp
        src/models/AlarmModel.cpp   # AlarmManager delegates storage to the model
    )
    target_include_directories(tst_trayicon PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(tst_trayicon PRIVATE Qt6::Core Qt6::Test Qt6::DBus Qt6::Widgets)
    add_test(NAME tst_trayicon COMMAND tst_trayicon)

    # SystemMixer — parsers, backends, async runner, fake mixer
    qt6_add_executable(tst_systemmixer
        tests/tst_systemmixer.cpp
        src/audio/SystemMixer.cpp
        src/audio/MixerBackend.cpp
        src/audio/PactlMixer.cpp
        src/audio/WpctlMixer.cpp
        src/audio/AmixerMixer.cpp
        src/audio/NullMixer.cpp
        src/audio/FakeMixer.cpp
    )
    target_include_directories(tst_systemmixer PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(tst_systemmixer PRIVATE Qt6::Core Qt6::Test)
    add_test(NAME tst_systemmixer COMMAND tst_systemmixer)

    # VolumePolicy — pure decision logic: policy kinds, balance-preserving
    # floor raise with the hard 100 % ceiling, the 3 % tolerance and the
    # user-override comparison. No mixer, no files.
    qt6_add_executable(tst_volumepolicy
        tests/tst_volumepolicy.cpp
        src/audio/VolumePolicy.cpp
    )
    target_include_directories(tst_volumepolicy PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(tst_volumepolicy PRIVATE Qt6::Core Qt6::Test)
    add_test(NAME tst_volumepolicy COMMAND tst_volumepolicy)

    # RestoreJournal — write-ahead file contract (atomic, 0600, versioned,
    # corrupt => delete) plus compare-and-set crash recovery on FakeMixer.
    qt6_add_executable(tst_restorejournal
        tests/tst_restorejournal.cpp
        src/audio/RestoreJournal.cpp
        src/audio/VolumePolicy.cpp
        src/audio/SystemMixer.cpp
        src/audio/MixerBackend.cpp
        src/audio/PactlMixer.cpp
        src/audio/WpctlMixer.cpp
        src/audio/AmixerMixer.cpp
        src/audio/NullMixer.cpp
        src/audio/FakeMixer.cpp
    )
    target_include_directories(tst_restorejournal PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(tst_restorejournal PRIVATE Qt6::Core Qt6::Test)
    add_test(NAME tst_restorejournal COMMAND tst_restorejournal)

    # SoundResolver — the resolution chain per-alarm -> global default ->
    # first tone -> empty, against injected tone dirs.
    qt6_add_executable(tst_soundresolver
        tests/tst_soundresolver.cpp
        src/audio/SoundResolver.cpp
    )
    target_include_directories(tst_soundresolver PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(tst_soundresolver PRIVATE Qt6::Core Qt6::Test)
    add_test(NAME tst_soundresolver COMMAND tst_soundresolver)

    # AlarmPlayer — media seam (no real audio device), stream-volume clamp
    # and the mandatory stream verification/forcing on FakeMixer.
    qt6_add_executable(tst_alarmplayer
        tests/tst_alarmplayer.cpp
        src/audio/AlarmPlayer.cpp
        src/audio/VolumePolicy.cpp
        src/audio/SystemMixer.cpp
        src/audio/MixerBackend.cpp
        src/audio/PactlMixer.cpp
        src/audio/WpctlMixer.cpp
        src/audio/AmixerMixer.cpp
        src/audio/NullMixer.cpp
        src/audio/FakeMixer.cpp
    )
    target_include_directories(tst_alarmplayer PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(tst_alarmplayer PRIVATE Qt6::Core Qt6::Test Qt6::Multimedia)
    add_test(NAME tst_alarmplayer COMMAND tst_alarmplayer)

    # MprisPause — the pure bus-name filter and the no-bus safety contract
    # (tests never pause a real player).
    qt6_add_executable(tst_mprispause
        tests/tst_mprispause.cpp
        src/audio/MprisPause.cpp
    )
    target_include_directories(tst_mprispause PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(tst_mprispause PRIVATE Qt6::Core Qt6::Test Qt6::DBus)
    add_test(NAME tst_mprispause COMMAND tst_mprispause)

    # AlarmAudioController — the ring flow end to end on FakeMixer: journal
    # written BEFORE the sink moves, restore exactly, alarmOnly never touches
    # the sink, user override means no restore, quit bounded, startup recovery.
    # FakeMpris ensures no real player is ever paused by a test run.
    qt6_add_executable(tst_audiocontroller
        tests/tst_audiocontroller.cpp
        src/audio/AlarmAudioController.cpp
        src/audio/AlarmPlayer.cpp
        src/audio/VolumePolicy.cpp
        src/audio/RestoreJournal.cpp
        src/audio/SoundResolver.cpp
        src/audio/MprisPause.cpp
        src/audio/SystemMixer.cpp
        src/audio/MixerBackend.cpp
        src/audio/PactlMixer.cpp
        src/audio/WpctlMixer.cpp
        src/audio/AmixerMixer.cpp
        src/audio/NullMixer.cpp
        src/audio/FakeMixer.cpp
    )
    target_include_directories(tst_audiocontroller PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(tst_audiocontroller PRIVATE Qt6::Core Qt6::Test Qt6::Multimedia Qt6::DBus)
    add_test(NAME tst_audiocontroller COMMAND tst_audiocontroller)

    # NightMode (src/ui/): edge-trigger semantics, settings round-trip and
    # clamping. Runs headless (GUILESS) — no QML involved.
    qt6_add_executable(tst_nightmode
        tests/tst_nightmode.cpp
        src/ui/NightMode.cpp
        src/managers/ConfigManager.cpp
        src/managers/ThemeManager.cpp   # ConfigManager owns a ThemeManager
        src/wake/Scheduler.cpp
        src/wake/SchedulerState.cpp
        src/wake/WakeManager.cpp
        src/managers/AlarmManager.cpp
        src/models/AlarmModel.cpp
    )
    target_include_directories(tst_nightmode PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(tst_nightmode PRIVATE Qt6::Core Qt6::Test Qt6::DBus Qt6::Gui)
    add_test(NAME tst_nightmode COMMAND tst_nightmode)

    # WizardController (src/ui/): step machine, guards, persisted flag and the
    # probe helpers. WakeManager is real; the privileged arming half of
    # testWake() is substituted through the armProbeWake() seam, and the
    # notification probe is compared against an independent bus check.
    # AlarmManager/AlarmModel are linked so the "a wizard run never adds or
    # rewrites an alarm" contract is asserted against the real store.
    qt6_add_executable(tst_wizard
        tests/tst_wizard.cpp
        src/ui/WizardController.cpp
        src/managers/ConfigManager.cpp
        src/managers/ThemeManager.cpp   # ConfigManager owns a ThemeManager
        src/wake/WakeManager.cpp
        # Production controller (no start(): no backend probe, no recovery,
        # no CLI). The wizard only calls preview() on it.
        src/audio/AlarmAudioController.cpp
        src/audio/AlarmPlayer.cpp
        src/audio/VolumePolicy.cpp
        src/audio/RestoreJournal.cpp
        src/audio/SoundResolver.cpp
        src/audio/MprisPause.cpp
        src/audio/SystemMixer.cpp
        src/audio/MixerBackend.cpp
        src/audio/PactlMixer.cpp
        src/audio/WpctlMixer.cpp
        src/audio/AmixerMixer.cpp
        src/audio/NullMixer.cpp
        src/managers/AlarmManager.cpp
        src/models/AlarmModel.cpp
    )
    target_include_directories(tst_wizard PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(tst_wizard PRIVATE Qt6::Core Qt6::Test Qt6::DBus Qt6::Gui Qt6::Multimedia)
    add_test(NAME tst_wizard COMMAND tst_wizard)

    # AlarmModel unit tests (QAbstractListModel refactor of the old
    # QVariantList alarm exposure).
    qt6_add_executable(tst_alarmmodel
        tests/tst_alarmmodel.cpp
        src/models/AlarmModel.cpp
        src/managers/AlarmManager.cpp   # provides Alarm::toVariantMap()/fromVariantMap()
    )
    target_include_directories(tst_alarmmodel PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(tst_alarmmodel PRIVATE Qt6::Core Qt6::Test)
    add_test(NAME tst_alarmmodel COMMAND tst_alarmmodel)

    # selectedId selection-behavior regression tests (luma.md Open Question #2,
    # resolved 1.0.2 — QML selection rules mirrored in a minimal C++ harness).
    qt6_add_executable(tst_selectionbehavior
        tests/tst_selectionbehavior.cpp
        src/models/AlarmModel.cpp
        src/managers/AlarmManager.cpp   # provides Alarm::toVariantMap()/fromVariantMap()
    )
    target_include_directories(tst_selectionbehavior PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(tst_selectionbehavior PRIVATE Qt6::Core Qt6::Test)
    add_test(NAME tst_selectionbehavior COMMAND tst_selectionbehavior)

    # ── Per-module unit tests (STG-3.5 / Phase D) ──────────────────────────
    # Construction + one observable behavior per previously untested module.
    # ConfigManager/ThemeManager tests enable QStandardPaths test mode so they
    # never touch the real ~/.config/lumalarm.
    # (tst_audioplayer removed with AudioPlayer — Issue #1; its surface is
    # covered by tst_alarmplayer + tst_audiocontroller.)
    qt6_add_executable(tst_configmanager
        tests/tst_configmanager.cpp
        src/managers/ConfigManager.cpp
        src/managers/ThemeManager.cpp   # ConfigManager owns a ThemeManager
    )
    target_include_directories(tst_configmanager PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(tst_configmanager PRIVATE Qt6::Core Qt6::Test Qt6::Gui)
    add_test(NAME tst_configmanager COMMAND tst_configmanager)

    qt6_add_executable(tst_thememanager
        tests/tst_thememanager.cpp
        src/managers/ThemeManager.cpp
    )
    target_include_directories(tst_thememanager PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(tst_thememanager PRIVATE Qt6::Core Qt6::Test Qt6::Gui)
    add_test(NAME tst_thememanager COMMAND tst_thememanager)

    qt6_add_executable(tst_blurmanager
        tests/tst_blurmanager.cpp
        src/ui/BlurManager.cpp
    )
    target_include_directories(tst_blurmanager PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(tst_blurmanager PRIVATE Qt6::Core Qt6::Test Qt6::Quick)
    if(LUMALARM_ENABLE_KDE_BLUR)
        target_link_libraries(tst_blurmanager PRIVATE KF6::WindowSystem)   # same guard as the app
    endif()
    add_test(NAME tst_blurmanager COMMAND tst_blurmanager)

    # ── Smoke test: v1.0.1 resource layout (QTP0001) + Qt version checks ──────
    # Embed the module's QML/icon resources into the test binary using the app
    # target's REAL QT_QML_MODULE_RESOURCE_PREFIX. This keeps the test in sync
    # with the shipped binary: if the app's resource prefix ever regresses, the
    # expected qrc:/qt/qml/GlassAlarm/... paths disappear and the test fails.
    get_target_property(LUMALARM_QML_RESOURCE_PREFIX lumalarm QT_QML_MODULE_RESOURCE_PREFIX)
    qt6_add_executable(tst_qmlresources
        tests/tst_qmlresources.cpp
    )
    target_include_directories(tst_qmlresources PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    qt6_add_resources(tst_qmlresources tst_qmlresources_res
        PREFIX "${LUMALARM_QML_RESOURCE_PREFIX}"
        BASE "${CMAKE_CURRENT_SOURCE_DIR}"
        FILES
            ${QML_FILES}
            resources/icons/alarms.svg
            resources/icons/timer.svg
            resources/icons/stopwatch.svg
            resources/icons/sounds.svg
            resources/icons/settings.svg
    )
    target_link_libraries(tst_qmlresources PRIVATE Qt6::Core Qt6::Test)
    add_test(NAME tst_qmlresources COMMAND tst_qmlresources)

    # ── i18n (STG-5.2) ──────────────────────────────────────────────────────
    # Embeds the same generated .qm files the app ships, so the resource path
    # main.cpp loads (":/i18n/<name>.qm") is asserted against the real
    # artifact instead of a copy. Guarded exactly like the app: without Qt
    # Linguist there is no catalog to embed and the dependent cases report a
    # skip rather than a false failure.
    qt6_add_executable(tst_i18n
        tests/tst_i18n.cpp
        src/managers/ConfigManager.cpp
        src/managers/ThemeManager.cpp   # ConfigManager owns a ThemeManager
    )
    target_include_directories(tst_i18n PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_compile_definitions(tst_i18n
        PRIVATE "LUMALARM_SOURCE_DIR=\"${CMAKE_CURRENT_SOURCE_DIR}\"")
    if(Qt6LinguistTools_FOUND)
        set_source_files_properties(
            ${CMAKE_CURRENT_BINARY_DIR}/lumalarm_ar.qm
            ${CMAKE_CURRENT_BINARY_DIR}/lumalarm_en.qm
            ${CMAKE_CURRENT_BINARY_DIR}/lumalarm_ja.qm
            PROPERTIES GENERATED TRUE)
        qt6_add_resources(tst_i18n tst_i18n_res
            PREFIX "/i18n"
            BASE "${CMAKE_CURRENT_BINARY_DIR}"
            FILES
                ${CMAKE_CURRENT_BINARY_DIR}/lumalarm_ar.qm
                ${CMAKE_CURRENT_BINARY_DIR}/lumalarm_en.qm
                ${CMAKE_CURRENT_BINARY_DIR}/lumalarm_ja.qm
        )
    endif()
    target_link_libraries(tst_i18n PRIVATE Qt6::Core Qt6::Test Qt6::Qml Qt6::Gui)
    add_test(NAME tst_i18n COMMAND tst_i18n)
endif()

install(TARGETS lumalarm RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES ${QML_FILES} DESTINATION ${CMAKE_INSTALL_DATADIR}/lumalarm/qml)
install(FILES resources/lumalarm.qrc DESTINATION ${CMAKE_INSTALL_DATADIR}/lumalarm)
install(FILES resources/lumalarm.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
install(FILES resources/icon.svg DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps RENAME lumalarm.svg)
install(FILES resources/icon-256.png DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/256x256/apps RENAME lumalarm.png)
install(FILES resources/icon-512.png DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/512x512/apps RENAME lumalarm.png)

# ── Shared tone folder (Issue #2) ────────────────────────────────────────────
# The Arch package has always created /usr/share/lumalarm/tones and the app now
# scans every XDG data location for a lumalarm/tones directory, so a plain
# `cmake --install` has to build the same layout or the two installation paths
# drift apart again. Nothing is placed in it here: packagers and admins drop
# read-only tones there, users import into ~/.config/lumalarm/tones.
# Bracket syntax keeps $ENV{DESTDIR} and ${CMAKE_INSTALL_PREFIX} for install
# time, where --prefix and DESTDIR are actually known.
install(CODE [[
    file(MAKE_DIRECTORY "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/share/lumalarm/tones")
]])

# ── Wake helper install rules (STG-1.1) ──────────────────────────────────────
install(TARGETS lumalarm-wake-helper RUNTIME DESTINATION /usr/lib/lumalarm)
install(FILES data/org.lumalarm.Helper.service DESTINATION /usr/share/dbus-1/system-services)
install(FILES data/org.lumalarm.schedule.policy DESTINATION /usr/share/polkit-1/actions)
install(FILES data/lumalarm-wake-helper.service DESTINATION /usr/lib/systemd/system)
install(FILES data/org.lumalarm.Helper.xml DESTINATION /usr/share/lumalarm)
# Bus policy punch: the system bus default denies own/method_call, so without
# this file the helper cannot register its name and no client can reach it
# (proven empirically: "Request to own name refused by policy").
install(FILES data/org.lumalarm.Helper.conf DESTINATION /usr/share/dbus-1/system.d)

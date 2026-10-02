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
    src/audio/AudioPlayer.cpp
    src/wake/Scheduler.cpp
    src/wake/SchedulerState.cpp
    src/wake/WakeManager.cpp
    src/managers/ConfigManager.cpp
    src/managers/ThemeManager.cpp
    src/ui/BlurManager.cpp
    src/ui/Notifier.cpp
    src/ui/TrayIcon.cpp
    src/ui/NightMode.cpp
)

set(HEADERS
    src/models/Alarm.h
    src/managers/AlarmManager.h
    src/models/AlarmModel.h
    src/audio/AudioPlayer.h
    src/wake/Scheduler.h
    src/wake/SchedulerState.h
    src/wake/WakeManager.h
    src/managers/ConfigManager.h
    src/managers/ThemeManager.h
    src/ui/BlurManager.h
    src/ui/Notifier.h
    src/ui/TrayIcon.h
    src/ui/NightMode.h
)

set(QML_FILES
    qml/main.qml
    qml/NightDim.qml
    qml/components/GlassButton.qml
    qml/components/GlassCard.qml
    qml/pages/AlarmListView.qml
    qml/pages/AlarmEditor.qml
    qml/overlays/RingingOverlay.qml
    qml/overlays/SoundManager.qml
    qml/pages/TimerPage.qml
    qml/pages/StopwatchPage.qml
    qml/pages/SettingsPage.qml
    qml/components/RoundedCombo.qml
    qml/components/GlassCheckBox.qml
    qml/pickers/ClockFace.qml
    qml/components/ToggleSwitch.qml
    qml/components/SegmentedControl.qml
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

# ── Wake helper: privileged D-Bus system service (STG-1.1) ───────────────────
# Replaces the sudoers NOPASSWD rtcwake rule; see helper/lumalarm-wake-helper.cpp
qt_add_executable(lumalarm-wake-helper
    helper/lumalarm-wake-helper.cpp
)
target_link_libraries(lumalarm-wake-helper PRIVATE Qt6::Core Qt6::DBus)

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

    # Notifier (src/ui/): construction and the disabled / no-bus paths. The
    # live-daemon path is verified manually with busctl --user monitor.
    qt6_add_executable(tst_notifier
        tests/tst_notifier.cpp
        src/ui/Notifier.cpp
    )
    target_include_directories(tst_notifier PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(tst_notifier PRIVATE Qt6::Core Qt6::Test Qt6::DBus)
    add_test(NAME tst_notifier COMMAND tst_notifier)

    # TrayIcon (src/ui/): construction and tooltip text on a headless box
    # (offscreen platform, no system tray -> icon disabled, no crash).
    qt6_add_executable(tst_trayicon
        tests/tst_trayicon.cpp
        src/ui/TrayIcon.cpp
        src/wake/Scheduler.cpp
        src/wake/SchedulerState.cpp
        src/wake/WakeManager.cpp
        src/managers/AlarmManager.cpp
        src/models/AlarmModel.cpp   # AlarmManager delegates storage to the model
    )
    target_include_directories(tst_trayicon PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(tst_trayicon PRIVATE Qt6::Core Qt6::Test Qt6::DBus Qt6::Widgets)
    add_test(NAME tst_trayicon COMMAND tst_trayicon)

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
    qt6_add_executable(tst_audioplayer
        tests/tst_audioplayer.cpp
        src/audio/AudioPlayer.cpp
    )
    target_include_directories(tst_audioplayer PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(tst_audioplayer PRIVATE Qt6::Core Qt6::Test Qt6::Multimedia)
    add_test(NAME tst_audioplayer COMMAND tst_audioplayer)

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
endif()

install(TARGETS lumalarm RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES ${QML_FILES} DESTINATION ${CMAKE_INSTALL_DATADIR}/lumalarm/qml)
install(FILES resources/lumalarm.qrc DESTINATION ${CMAKE_INSTALL_DATADIR}/lumalarm)
install(FILES resources/lumalarm.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
install(FILES resources/icon.svg DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps RENAME lumalarm.svg)
install(FILES resources/icon-256.png DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/256x256/apps RENAME lumalarm.png)
install(FILES resources/icon-512.png DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/512x512/apps RENAME lumalarm.png)

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

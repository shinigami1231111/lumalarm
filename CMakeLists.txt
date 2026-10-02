cmake_minimum_required(VERSION 3.21)
project(Lumalarm VERSION 1.0.2 LANGUAGES CXX)

# C++17 (Qt 6's required language level) and AUTORCC are set explicitly on
# purpose: qt_standard_project_setup() only defaults CMAKE_AUTOMOC/CMAKE_AUTOUIC
# to TRUE (see Qt6CoreMacros.cmake). It does NOT set CMAKE_AUTORCC or the C++
# standard, so these must stay to keep behavior unchanged.
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Multimedia Qml Quick QuickControls2 Widgets)

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
    src/AlarmManager.cpp
    src/AlarmModel.cpp
    src/AudioPlayer.cpp
    src/Scheduler.cpp
    src/SchedulerState.cpp
    src/WakeManager.cpp
    src/ConfigManager.cpp
    src/ThemeManager.cpp
    src/BlurManager.cpp
)

set(HEADERS
    src/Alarm.h
    src/AlarmManager.h
    src/AlarmModel.h
    src/AudioPlayer.h
    src/Scheduler.h
    src/SchedulerState.h
    src/WakeManager.h
    src/ConfigManager.h
    src/ThemeManager.h
    src/BlurManager.h
)

set(QML_FILES
    qml/main.qml
    qml/GlassButton.qml
    qml/GlassCard.qml
    qml/AlarmListView.qml
    qml/AlarmEditor.qml
    qml/RingingOverlay.qml
    qml/SoundManager.qml
    qml/TimerPage.qml
    qml/StopwatchPage.qml
    qml/SettingsPage.qml
    qml/RoundedCombo.qml
    qml/GlassCheckBox.qml
    qml/ClockFace.qml
    qml/ToggleSwitch.qml
    qml/SegmentedControl.qml
    qml/WheelTimePicker.qml
)

qt6_add_executable(lumalarm ${SOURCES} ${HEADERS})

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
)

if(LUMALARM_ENABLE_KDE_BLUR)
    target_link_libraries(lumalarm PRIVATE KF6::WindowSystem)
endif()

# ── Tests ─────────────────────────────────────────────────────────────────────
include(CTest)
if(BUILD_TESTING)
    find_package(Qt6 REQUIRED COMPONENTS Test)
    qt6_add_executable(tst_schedulerlogic
        tests/tst_schedulerlogic.cpp
        src/AlarmManager.cpp
        src/AlarmModel.cpp
        src/Scheduler.cpp
        src/SchedulerState.cpp
        src/WakeManager.cpp
    )
    target_include_directories(tst_schedulerlogic PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(tst_schedulerlogic PRIVATE Qt6::Core Qt6::Test)
    add_test(NAME tst_schedulerlogic COMMAND tst_schedulerlogic)

    # AlarmModel unit tests (QAbstractListModel refactor of the old
    # QVariantList alarm exposure).
    qt6_add_executable(tst_alarmmodel
        tests/tst_alarmmodel.cpp
        src/AlarmModel.cpp
        src/AlarmManager.cpp   # provides Alarm::toVariantMap()/fromVariantMap()
    )
    target_include_directories(tst_alarmmodel PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(tst_alarmmodel PRIVATE Qt6::Core Qt6::Test)
    add_test(NAME tst_alarmmodel COMMAND tst_alarmmodel)

    # selectedId selection-behavior regression tests (luma.md Open Question #2,
    # resolved 1.0.2 — QML selection rules mirrored in a minimal C++ harness).
    qt6_add_executable(tst_selectionbehavior
        tests/tst_selectionbehavior.cpp
        src/AlarmModel.cpp
        src/AlarmManager.cpp   # provides Alarm::toVariantMap()/fromVariantMap()
    )
    target_include_directories(tst_selectionbehavior PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(tst_selectionbehavior PRIVATE Qt6::Core Qt6::Test)
    add_test(NAME tst_selectionbehavior COMMAND tst_selectionbehavior)

    # ── Smoke test: v1.0.1 resource layout (QTP0001) + Qt version checks ──────
    # Embed the module's QML/icon resources into the test binary using the app
    # target's REAL QT_QML_MODULE_RESOURCE_PREFIX. This keeps the test in sync
    # with the shipped binary: if the app's resource prefix ever regresses, the
    # expected qrc:/qt/qml/GlassAlarm/... paths disappear and the test fails.
    get_target_property(LUMALARM_QML_RESOURCE_PREFIX lumalarm QT_QML_MODULE_RESOURCE_PREFIX)
    qt6_add_executable(tst_qmlresources
        tests/tst_qmlresources.cpp
    )
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

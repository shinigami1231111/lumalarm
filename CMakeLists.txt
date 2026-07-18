cmake_minimum_required(VERSION 3.16)
project(Lumalarm VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Multimedia Qml Quick QuickControls2)

set(SOURCES
    src/main.cpp
    src/AlarmManager.cpp
    src/AudioPlayer.cpp
    src/Scheduler.cpp
    src/WakeManager.cpp
    src/ConfigManager.cpp
)

set(HEADERS
    src/AlarmManager.h
    src/AudioPlayer.h
    src/Scheduler.h
    src/WakeManager.h
    src/ConfigManager.h
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
)

qt6_add_executable(lumalarm ${SOURCES} ${HEADERS})

qt6_add_qml_module(lumalarm
    URI GlassAlarm
    VERSION 1.0
    QML_FILES ${QML_FILES}
)

target_link_libraries(lumalarm PRIVATE
    Qt6::Core
    Qt6::Multimedia
    Qt6::Qml
    Qt6::Quick
    Qt6::QuickControls2
)

install(TARGETS lumalarm RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES ${QML_FILES} DESTINATION ${CMAKE_INSTALL_DATADIR}/lumalarm/qml)
install(FILES resources/lumalarm.qrc DESTINATION ${CMAKE_INSTALL_DATADIR}/lumalarm)
install(FILES resources/lumalarm.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
install(FILES resources/icon.svg DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps RENAME lumalarm.svg)
install(FILES resources/icon-256.png DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/256x256/apps RENAME lumalarm.png)
install(FILES resources/icon-512.png DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/512x512/apps RENAME lumalarm.png)

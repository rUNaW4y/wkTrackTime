cmake_minimum_required(VERSION 3.17)
project(wkTrackTime)

set(CMAKE_CXX_STANDARD 20)

set(WORM_TRACKER_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../wkWormsTracker")

add_library(hacklib STATIC
        "${WORM_TRACKER_ROOT}/lib/hacklib/ExeFile.cpp"
        "${WORM_TRACKER_ROOT}/lib/hacklib/ExeFile_WIN32.cpp"
        "${WORM_TRACKER_ROOT}/lib/hacklib/PatternScanner.cpp"
        "${WORM_TRACKER_ROOT}/lib/hacklib/Memory_WIN32.cpp")

link_directories("${WORM_TRACKER_ROOT}/lib")
include_directories("${WORM_TRACKER_ROOT}/include" "${WORM_TRACKER_ROOT}/lib/hacklib")

configure_file(
        resource.rc.in
        resource.rc
)
add_definitions(-DLIBRARY_EXPORTS)

add_library(wkTrackTime SHARED
        src/dllmain.cpp
        src/Chat.cpp src/Chat.h
        src/Config.cpp src/Config.h
        src/Hooks.cpp src/Hooks.h
        src/W2App.cpp src/W2App.h
        src/Utils.cpp src/Utils.h
        src/Debugf.h
        src/Constants.h
        src/GameTextRenderer.cpp src/GameTextRenderer.h
        src/Diagnostics.cpp src/Diagnostics.h
        src/TrackTimeOverlay.cpp src/TrackTimeOverlay.h
        src/entities/CList.h
        src/entities/CTask.h
        src/entities/CGameTask.h
        src/entities/CTaskTeam.cpp src/entities/CTaskTeam.h
        src/entities/CTaskTurnGame.h
        src/entities/CTaskWorm.h
        ${CMAKE_CURRENT_BINARY_DIR}/resource.rc
        )

set_target_properties(wkTrackTime PROPERTIES OUTPUT_NAME "wkTrackTime")
set_property(TARGET wkTrackTime hacklib PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

target_link_libraries(wkTrackTime hacklib capstone Polyhook_2 version)

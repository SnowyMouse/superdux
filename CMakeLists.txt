cmake_minimum_required(VERSION 3.10)
project("kamonegi"
    VERSION 0.1.0
    LANGUAGES C CXX
)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(SDL2 REQUIRED)
find_package(Python3 COMPONENTS Interpreter REQUIRED)

# We only support Qt5 if not on Windows as it's unnecessary for Windows to have Qt 5 (todo: remove Qt5 support when KDE drops it)
if(NOT WIN32)
    find_package(Qt5 COMPONENTS Widgets)
endif()

if(WIN32)
    set(CMAKE_CXX_STANDARD_LIBRARIES "${CMAKE_CXX_STANDARD_LIBRARIES} -lsetupapi -lssp")
    set(CMAKE_C_STANDARD_LIBRARIES "${CMAKE_C_STANDARD_LIBRARIES} -lsetupapi -lssp")
    set(GETLINE_IF_NEEDED "src/getline.cpp")
endif()

find_package(Qt6 COMPONENTS Widgets)

include("sameboy.cmake")

set(KAMONEGI_FILES
    src/kamonegi.qrc
    src/kamonegi.rc
    src/debugger.cpp
    src/debugger_break_and_trace_results_dialog.cpp
    src/debugger_disassembler.cpp
    src/edit_advanced_game_boy_model_dialog.cpp
    src/edit_controls_dialog.cpp
    src/edit_speed_control_settings_dialog.cpp
    src/game_window.cpp
    src/input_device.cpp
    src/main.cpp
    src/vram_viewer.cpp

    ${GETLINE_IF_NEEDED}
)

# These are shared regardless of Qt version
add_library(kamonegi-shared-br STATIC
    src/built_in_boot_rom.c
    src/gb_proxy.c
    src/game_instance.cpp
    ${BOOT_ROMS_HEADER}
)
target_link_libraries(kamonegi-shared-br sameboy-core)

target_include_directories(kamonegi-shared-br
    PRIVATE "${SAMEBOY_SOURCE_DIR}"
    PRIVATE "${CMAKE_CURRENT_BINARY_DIR}"
)

option(BUILD_QT5_VERSION "Build the Qt5 version" ${Qt5_FOUND})
option(BUILD_QT6_VERSION "Build the Qt6 version" ${Qt6_FOUND})

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# And lastly our executable
macro(make_kamonegi target_name qt_widgets)
    add_executable(${target_name}
        ${KAMONEGI_FILES}
    )

    target_include_directories(${target_name}
        PRIVATE "${SAMEBOY_SOURCE_DIR}"
        PRIVATE "${CMAKE_CURRENT_BINARY_DIR}"
    )

    target_compile_definitions(${target_name}
        PRIVATE SAMEBOY_SOURCE_HASH="${SAMEBOY_SOURCE_HASH}"
    )

    target_link_libraries(${target_name} kamonegi-shared-br ${SDL2_LIBRARIES} pthread ${qt_widgets})
endmacro()

if(${BUILD_QT5_VERSION})
    if(WIN32)
        message(FATAL_ERROR "Qt 5 on Windows is not supported for this project")
    endif()

    make_kamonegi(kamonegi-qt5 Qt5::Widgets)
    set_target_properties(kamonegi-qt5
        PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/qt5"
        OUTPUT_NAME "kamonegi"
    )
    set(INSTALL_TARGET "kamonegi-qt5")
endif()
if(${BUILD_QT6_VERSION})
    make_kamonegi(kamonegi-qt6 Qt6::Widgets)
    set_target_properties(kamonegi-qt6
        PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/qt6"
        OUTPUT_NAME "kamonegi"
    )
    set(INSTALL_TARGET "kamonegi-qt6")
endif()

if((NOT ${BUILD_QT5_VERSION}) AND (NOT ${BUILD_QT6_VERSION}))
    message(fatal_error "Must build either Qt5 or Qt6 version")
endif()

# Install the version built for the latest Qt version specified
install(
    TARGETS ${INSTALL_TARGET}
    RUNTIME DESTINATION bin
)

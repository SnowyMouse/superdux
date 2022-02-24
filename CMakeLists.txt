cmake_minimum_required(VERSION 3.10)
project("superdux"
    VERSION 0.1.0
    LANGUAGES C CXX
)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(SDL2 REQUIRED)
find_package(Python3 COMPONENTS Interpreter REQUIRED)

option(BUILD_QT5_VERSION "Build for Qt5 (deprecated)")

# Add capitalization for Windows builds
if(WIN32)
    set(SUPERDUX_EXE_NAME "SuperDUX")
# Linux and anything else should not have capitalization as capital letters are a massive pain to type in bash
else()
    set(SUPERDUX_EXE_NAME "superdux")
endif()

# By default we do Qt6
if(NOT ${BUILD_QT5_VERSION})
    find_package(Qt6 COMPONENTS Widgets REQUIRED)
    set(qt_widgets Qt6::Widgets)

# Qt5?
else()
    message(WARNING "Qt5 support will be dropped in a future version. Do not report bugs.")
    find_package(Qt5 COMPONENTS Widgets REQUIRED)
    set(qt_widgets Qt5::Widgets)
endif()

if(WIN32)
    set(CMAKE_CXX_STANDARD_LIBRARIES "${CMAKE_CXX_STANDARD_LIBRARIES} -lsetupapi -lssp")
    set(CMAKE_C_STANDARD_LIBRARIES "${CMAKE_C_STANDARD_LIBRARIES} -lsetupapi -lssp")
    set(GETLINE_IF_NEEDED "src/getline.cpp")
endif()

include("sameboy.cmake")

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

add_executable(superdux
    src/superdux.qrc
    src/superdux.rc
    src/debugger.cpp
    src/debugger_break_and_trace_results_dialog.cpp
    src/debugger_disassembler.cpp
    src/edit_advanced_game_boy_model_dialog.cpp
    src/edit_controls_dialog.cpp
    src/edit_speed_control_settings_dialog.cpp
    src/game_window.cpp
    src/input_device.cpp
    src/main.cpp
    src/printer.cpp
    src/vram_viewer.cpp
    src/settings.cpp

    src/built_in_boot_rom.c
    src/gb_proxy.c
    src/game_instance.cpp
    ${BOOT_ROMS_HEADER}

    ${GETLINE_IF_NEEDED}
)
target_include_directories(superdux
    PRIVATE "${SAMEBOY_SOURCE_DIR}"
    PRIVATE "${CMAKE_CURRENT_BINARY_DIR}"
)
target_compile_definitions(superdux
    PRIVATE SAMEBOY_SOURCE_HASH="${SAMEBOY_SOURCE_HASH}"
)
target_link_libraries(superdux sameboy-core ${SDL2_LIBRARIES} pthread ${qt_widgets})
set_target_properties(superdux PROPERTIES OUTPUT_NAME "${SUPERDUX_EXE_NAME}")

# Install the version built for the latest Qt version specified
install(
    TARGETS superdux
    RUNTIME DESTINATION bin
)

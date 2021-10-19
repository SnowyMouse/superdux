cmake_minimum_required(VERSION 3.10)
project("sameboy-dx"
    VERSION 0.1.0
    LANGUAGES C CXX
)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(SDL2 REQUIRED)
find_package(Python3 COMPONENTS Interpreter REQUIRED)

find_package(Qt5 COMPONENTS Widgets)
find_package(Qt6 COMPONENTS Widgets)

include("sameboy.cmake")

set(SDX_FILES
    src/edit_advanced_game_boy_model_dialog.cpp
    src/edit_controls_dialog.cpp
    src/edit_speed_control_settings_dialog.cpp
    src/game_debugger.cpp
    src/game_disassembler.cpp
    src/game_instance.cpp
    src/game_window.cpp
    src/gb_proxy.c
    src/input_device.cpp
    src/main.cpp
    src/vram_viewer.cpp
)

option(BUILD_QT5_VERSION "Build the Qt5 version" ${Qt5_FOUND})
option(BUILD_QT6_VERSION "Build the Qt6 version" ${Qt6_FOUND})

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# And lastly our executable
macro(make_sameboy_dx target_name qt_widgets)
    add_executable(${target_name}
        ${SDX_FILES}
        ${BOOT_ROMS_HEADER}
    )

    target_include_directories(${target_name}
        PRIVATE "${SAMEBOY_SOURCE_DIR}"
        PRIVATE "${CMAKE_CURRENT_BINARY_DIR}"
    )

    target_compile_definitions(${target_name}
        PRIVATE SAMEBOY_SOURCE_HASH="${SAMEBOY_SOURCE_HASH}"
    )

    target_link_libraries(${target_name} sameboy-core ${SDL2_LIBRARIES} pthread ${qt_widgets})
endmacro()

if(${BUILD_QT5_VERSION})
    make_sameboy_dx(sameboy-dx-qt5 Qt5::Widgets)
    set_target_properties(sameboy-dx-qt5
        PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/qt5"
        OUTPUT_NAME "sameboy-dx"
    )
endif()
if(${BUILD_QT6_VERSION})
    make_sameboy_dx(sameboy-dx-qt6 Qt6::Widgets)
    set_target_properties(sameboy-dx-qt6
        PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/qt6"
        OUTPUT_NAME "sameboy-dx"
    )
endif()

if((NOT ${BUILD_QT5_VERSION}) AND (NOT ${BUILD_QT6_VERSION}))
    message(fatal_error "Must build either Qt5 or Qt6 version")
endif()

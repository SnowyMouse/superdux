cmake_minimum_required(VERSION 3.10)
project("super-sameboy"
    LANGUAGES C CXX
)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt5 COMPONENTS Widgets Multimedia Gamepad REQUIRED)
find_package(Python3 COMPONENTS Interpreter)

include("sameboy.cmake")

file(GLOB_RECURSE SUPER_FILES
    "src/*.cpp"
    "src/*.c"
)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# And lastly our executable
add_executable(super-sameboy
    ${SUPER_FILES}
    ${BOOT_ROMS_HEADER}
)

target_include_directories(super-sameboy
    PRIVATE "${SAMEBOY_DIR}"
    PRIVATE "${CMAKE_CURRENT_BINARY_DIR}"
)

string(TOLOWER "${CMAKE_BUILD_TYPE}" CMAKE_BUILD_TYPE_LOWER)
if(CMAKE_BUILD_TYPE_LOWER MATCHES "debug")
    target_compile_definitions(super-sameboy
        PRIVATE DEBUG
    )
endif()

target_link_libraries(super-sameboy sameboy-core ${SDL2_LIBRARIES} pthread Qt5::Widgets Qt5::Multimedia Qt5::Gamepad)



# Test struct size - struct size for GB seems different on C vs C++ on GCC (not
# clang). This is fine provided the struct is large enough on C++ vs C, but if
# it's smaller, then we risk SameBoy accessing data outside of the allocated
# data.
add_executable(gb-struct-size-test
    test/gb_struct_size/main.cpp
    test/gb_struct_size/test.cpp
    test/gb_struct_size/test.c
)

add_custom_target(gb-struct-size-do ALL gb-struct-size-test
    COMMENT "Test if GB struct is large enough on C++"
)

target_include_directories(gb-struct-size-test
    PRIVATE "${SAMEBOY_DIR}"
)

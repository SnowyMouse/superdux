cmake_minimum_required(VERSION 3.10)
project("gamechu"
    LANGUAGES C CXX
)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt5 COMPONENTS Widgets REQUIRED)
find_package(Python3 COMPONENTS Interpreter)

include("sameboy.cmake")

file(GLOB_RECURSE QT_FILES
    "src/*.cpp"
)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# And lastly our executable
add_executable(gamechu
    ${QT_FILES}
    ${BOOT_ROMS_HEADER}
)

target_include_directories(gamechu PRIVATE "${SAMEBOY_DIR}" PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
target_link_libraries(gamechu sameboy-core ${SDL2_LIBRARIES} pthread Qt5::Widgets)

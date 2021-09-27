cmake_minimum_required(VERSION 3.10)
project("gamechu"
    LANGUAGES C CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt5 COMPONENTS Widgets REQUIRED)
find_package(SDL2 REQUIRED)
find_package(OpenGL REQUIRED)

set(SAMEBOY_DIR "${CMAKE_CURRENT_SOURCE_DIR}/SameBoy")

# Include all of the SameBoy core files
file(GLOB_RECURSE CORE_FILES
    "${SAMEBOY_DIR}/Core/*.c"
)

file(GLOB_RECURSE QT_FILES
    "src/*.cpp"
)

add_library(sameboy-core STATIC
    ${CORE_FILES}
)

add_executable(gamechu
    ${QT_FILES}
)

target_include_directories(gamechu PRIVATE "${SAMEBOY_DIR}")

target_link_libraries(gamechu sameboy-core ${SDL2_LIBRARIES} pthread Qt5::Widgets)

# Get the version
file(READ "${SAMEBOY_DIR}/version.mk" GB_VERSION_TEXT)
string(REGEX MATCH "([0-9]+\.)+[0-9]+" GB_VERSION "${GB_VERSION_TEXT}")

target_compile_definitions(sameboy-core
    PRIVATE GB_VERSION="${GB_VERSION}"
    PRIVATE GB_INTERNAL
)

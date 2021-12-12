# Include all of the SameBoy files
set(SAMEBOY_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/SameBoy" CACHE STRING "Path to the SameBoy source directory")
file(GLOB_RECURSE CORE_FILES
    "${SAMEBOY_SOURCE_DIR}/Core/*.c"
)
list(SORT CORE_FILES)

# Hash every .c file in SameBoy
set(CF_HASHES "")
foreach(CF ${CORE_FILES})
    file(SHA256 "${CF}" CF_HASH)
    set(CF_HASHES "${CF_HASHES}${CF_HASH}")
endforeach()
string(SHA256 SAMEBOY_SOURCE_HASH "${CF_HASHES}")

add_library(sameboy-core STATIC
    ${CORE_FILES}
)

if((CMAKE_CXX_COMPILER_ID STREQUAL "Clang") OR (CMAKE_CXX_COMPILER_ID STREQUAL "GNU"))
    target_compile_options(sameboy-core PRIVATE -Wno-multichar -Wno-attributes)
endif()

# Parse the version
file(READ "${SAMEBOY_SOURCE_DIR}/version.mk" GB_VERSION_TEXT)
string(REGEX MATCH "([0-9]+\.)+[0-9]+" GB_VERSION "${GB_VERSION_TEXT}")

target_compile_definitions(sameboy-core
    PRIVATE GB_VERSION="${GB_VERSION}"
    PRIVATE GB_INTERNAL
    PRIVATE _GNU_SOURCE
)

# Build the boot ROMs
set(BOOT_ROMS
    "agb_boot"
    "cgb_boot"
    "cgb_boot_fast"
    "dmg_boot"
    "sgb_boot"
    "sgb2_boot"
)
add_executable(pb12
    "${SAMEBOY_SOURCE_DIR}/BootROMs/pb12.c"
)
add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/SameBoyLogo.2bpp"
    COMMAND rgbgfx -h -u -o "${CMAKE_CURRENT_BINARY_DIR}/SameBoyLogo.2bpp" "${SAMEBOY_SOURCE_DIR}/BootROMs/SameBoyLogo.png"
)
add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/SameBoyLogo.pb12"
    COMMAND "${CMAKE_CURRENT_BINARY_DIR}/pb12${CMAKE_EXECUTABLE_SUFFIX}" < "${CMAKE_CURRENT_BINARY_DIR}/SameBoyLogo.2bpp" > "${CMAKE_CURRENT_BINARY_DIR}/SameBoyLogo.pb12"
    DEPENDS pb12 "${CMAKE_CURRENT_BINARY_DIR}/SameBoyLogo.2bpp"
)

option(USE_CUSTOM_BOOT_ROMS "Use custom boot ROMs in the current directory instead of SameBoy's included boot ROMs" NO)

set(BOOT_ROMS_BIN)
set(BOOT_ROMS_HEADER)

foreach(ROM ${BOOT_ROMS})
    set(ROM_BIN "${CMAKE_CURRENT_BINARY_DIR}/${ROM}.bin")
    set(ROM_H "${CMAKE_CURRENT_BINARY_DIR}/${ROM}.h")
    set(ROM_ASM_DEP "${SAMEBOY_SOURCE_DIR}/BootROMs/${ROM}.asm")

    if(USE_CUSTOM_BOOT_ROMS)
        # Check to see if the bin exists
        if(NOT EXISTS "${ROM_BIN}")
            message(SEND_ERROR "Can't find '${ROM_BIN}'")
        endif()
    else()
        # ROMs that depend on other sources
        if(("${ROM}" EQUAL "agb_boot") OR ("${ROM}" EQUAL "cgb_boot_fast"))
            set(ROM_ASM_DEP
                ${ROM_ASM_DEP}
                "${SAMEBOY_SOURCE_DIR}/BootROMs/cgb_boot.asm"
            )
        endif()
        if("${ROM}" EQUAL "sgb2_boot")
            set(ROM_ASM_DEP
                ${ROM_ASM_DEP}
                "${SAMEBOY_SOURCE_DIR}/BootROMs/sgb_boot.asm"
            )
        endif()
    endif()
    
    list(APPEND BOOT_ROMS_BIN "${CMAKE_CURRENT_BINARY_DIR}/${ROM}.bin")
    list(APPEND BOOT_ROMS_HEADER "${CMAKE_CURRENT_BINARY_DIR}/${ROM}.h")
    
    if(USE_CUSTOM_BOOT_ROMS)
        # Just convert to a header
        if(EXISTS "${ROM_BIN}")
            add_custom_command(OUTPUT "${ROM_H}"
                COMMAND "Python3::Interpreter" "${CMAKE_CURRENT_SOURCE_DIR}/bin_to_c_header.py" "${ROM}" "${ROM_BIN}" "${ROM_H}"
                DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/bin_to_c_header.py" "${ROM_BIN}"
            )
        endif()
    else()
        # Resize to this size if needed
        if(("${ROM}" MATCHES "agb.*") OR ("${ROM}" MATCHES "cgb.*"))
            set(ROM_SIZE 2304)
        else()
            set(ROM_SIZE 256)
        endif()

        # Compile, resize, and convert
        add_custom_command(OUTPUT "${ROM_H}"
            COMMAND rgbasm -i "${SAMEBOY_SOURCE_DIR}/BootROMs/" -o "${CMAKE_CURRENT_BINARY_DIR}/${ROM}.o" "${SAMEBOY_SOURCE_DIR}/BootROMs/${ROM}.asm"
            COMMAND rgblink -x -o "${ROM_BIN}" "${CMAKE_CURRENT_BINARY_DIR}/${ROM}.o"

            COMMAND "Python3::Interpreter" "${CMAKE_CURRENT_SOURCE_DIR}/append.py" "${ROM}" "${ROM_BIN}" "${ROM_SIZE}"
            COMMAND "Python3::Interpreter" "${CMAKE_CURRENT_SOURCE_DIR}/bin_to_c_header.py" "${ROM}" "${ROM_BIN}" "${ROM_H}"

            DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/SameBoyLogo.pb12" "${ROM_ASM_DEP}" "${CMAKE_CURRENT_SOURCE_DIR}/bin_to_c_header.py" "${CMAKE_CURRENT_SOURCE_DIR}/append.py"
            BYPRODUCTS "${CMAKE_CURRENT_BINARY_DIR}/${ROM}.o" "${ROM_BIN}"
        )
    endif()
endforeach()

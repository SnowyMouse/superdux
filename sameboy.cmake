# Include all of the SameBoy files
set(SAMEBOY_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/SameBoy" CACHE STRING "Path to the SameBoy source directory")
file(GLOB_RECURSE CORE_FILES
    "${SAMEBOY_SOURCE_DIR}/Core/*.c"
)

add_library(sameboy-core STATIC
    ${CORE_FILES}
)

# Parse the version
file(READ "${SAMEBOY_SOURCE_DIR}/version.mk" GB_VERSION_TEXT)
string(REGEX MATCH "([0-9]+\.)+[0-9]+" GB_VERSION "${GB_VERSION_TEXT}")

target_compile_definitions(sameboy-core
    PRIVATE GB_VERSION="${GB_VERSION}"
    PRIVATE GB_INTERNAL
)

# Build the boot ROMs
set(BOOT_ROMS
    "agb_boot"
    "cgb_boot"
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
    COMMAND pb12 < "${CMAKE_CURRENT_BINARY_DIR}/SameBoyLogo.2bpp" > "${CMAKE_CURRENT_BINARY_DIR}/SameBoyLogo.pb12"
    DEPENDS pb12 "${CMAKE_CURRENT_BINARY_DIR}/SameBoyLogo.2bpp"
)
set(BOOT_ROMS_BIN)
set(BOOT_ROMS_HEADER)

foreach(ROM ${BOOT_ROMS})
    set(ROM_BIN "${CMAKE_CURRENT_BINARY_DIR}/${ROM}.bin")
    set(ROM_H "${CMAKE_CURRENT_BINARY_DIR}/${ROM}.h")
    set(ROM_ASM_DEP "${SAMEBOY_SOURCE_DIR}/BootROMs/${ROM}.asm")
    
    # ROMs that depend on other sources
    if("${ROM}" EQUAL "agb_boot")
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
    
    list(APPEND BOOT_ROMS_BIN "${CMAKE_CURRENT_BINARY_DIR}/${ROM}.bin")
    list(APPEND BOOT_ROMS_HEADER "${CMAKE_CURRENT_BINARY_DIR}/${ROM}.h")
    
    # Resize to this size if needed
    if(("${ROM}" MATCHES "agb.*") OR ("${ROM}" MATCHES "cgb.*"))
        set(ROM_SIZE 2304)
    else()
        set(ROM_SIZE 256)
    endif()
    
    add_custom_command(OUTPUT "${ROM_H}"
        COMMAND rgbasm -i "${SAMEBOY_SOURCE_DIR}/BootROMs/" -o "${CMAKE_CURRENT_BINARY_DIR}/${ROM}.o" "${SAMEBOY_SOURCE_DIR}/BootROMs/${ROM}.asm"
        COMMAND rgblink -x -o "${ROM_BIN}" "${CMAKE_CURRENT_BINARY_DIR}/${ROM}.o"
        
        COMMAND Python3::Interpreter "${CMAKE_CURRENT_SOURCE_DIR}/append.py" "${ROM}" "${ROM_BIN}" "${ROM_SIZE}"
        COMMAND Python3::Interpreter "${CMAKE_CURRENT_SOURCE_DIR}/bin_to_c_header.py" "${ROM}" "${ROM_BIN}" "${ROM_H}"
        
        DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/SameBoyLogo.pb12" ${ROM_ASM_DEP} "${CMAKE_CURRENT_SOURCE_DIR}/bin_to_c_header.py" "${CMAKE_CURRENT_SOURCE_DIR}/append.py"
        BYPRODUCTS "${CMAKE_CURRENT_BINARY_DIR}/${ROM}.o" "${ROM_BIN}"
    )
endforeach()

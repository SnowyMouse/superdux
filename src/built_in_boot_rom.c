#include "built_in_boot_rom.h"

#include <agb_boot.h>
#include <sgb_boot.h>
#include <cgb_boot.h>
#include <dmg_boot.h>
#include <sgb2_boot.h>
#include <cgb_boot_fast.h>

const uint8_t *built_in_dmg_boot_room(size_t *size) {
    *size = sizeof(dmg_boot);
    return dmg_boot;
}
const uint8_t *built_in_cgb_boot_room(size_t *size) {
    *size = sizeof(cgb_boot);
    return cgb_boot;
}
const uint8_t *built_in_fast_cgb_boot_room(size_t *size) {
    *size = sizeof(cgb_boot_fast);
    return cgb_boot_fast;
}
const uint8_t *built_in_agb_boot_room(size_t *size) {
    *size = sizeof(agb_boot);
    return agb_boot;
}
const uint8_t *built_in_sgb_boot_room(size_t *size) {
    *size = sizeof(sgb_boot);
    return sgb_boot;
}
const uint8_t *built_in_sgb2_boot_room(size_t *size) {
    *size = sizeof(sgb2_boot);
    return sgb2_boot;
}

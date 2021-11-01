#ifndef BUILT_IN_BOOT_ROM_H
#define BUILT_IN_BOOT_ROM_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>

const uint8_t *built_in_dmg_boot_room(size_t *size);
const uint8_t *built_in_cgb_boot_room(size_t *size);
const uint8_t *built_in_fast_cgb_boot_room(size_t *size);
const uint8_t *built_in_agb_boot_room(size_t *size);
const uint8_t *built_in_sgb_boot_room(size_t *size);
const uint8_t *built_in_sgb2_boot_room(size_t *size);

#ifdef __cplusplus
}
#endif

#endif

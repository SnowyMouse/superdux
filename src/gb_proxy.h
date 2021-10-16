#ifndef GB_PROXY_H
#define GB_PROXY_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#include <Core/gb.h>

typedef enum {
    SM83_REG_A,
    SM83_REG_B,
    SM83_REG_C,
    SM83_REG_D,
    SM83_REG_E,
    SM83_REG_F,
    SM83_REG_AF,
    SM83_REG_BC,
    SM83_REG_DE,
    SM83_REG_HL,
    SM83_REG_PC,
    SM83_REG_SP,
} sm83_register;

uint16_t get_gb_register(const struct GB_gameboy_s *gb, sm83_register r);
void set_gb_register(struct GB_gameboy_s *gb, sm83_register r, uint16_t v);

uint32_t get_gb_backtrace_size(const struct GB_gameboy_s *gb);
uint16_t get_gb_backtrace_address(const struct GB_gameboy_s *gb, uint32_t bt);

uint32_t get_gb_breakpoint_size(const struct GB_gameboy_s *gb);
uint16_t get_gb_breakpoint_address(const struct GB_gameboy_s *gb, uint32_t bt);

const uint32_t *get_gb_palette(const struct GB_gameboy_s *gb, GB_palette_type_t palette_type, unsigned char palette_index);

typedef enum {
    TILESET_INFO_NONE = 0,
    TILESET_INFO_OAM,
    TILESET_INFO_BACKGROUND,
    TILESET_INFO_WINDOW
} tileset_object_info_tile_type;

typedef struct {
    uint16_t tile_address;
    uint16_t tile_index;
    uint8_t tile_bank;

    uint8_t accessed_type;
    uint8_t accessed_tile_index;
    uint8_t accessed_tile_palette_index;
} tileset_object_info_tile;

typedef struct {
    tileset_object_info_tile tiles[384*2];
} tileset_object_info;

void get_tileset_object_info(struct GB_gameboy_s *gb, tileset_object_info *tileset_info);

#ifdef __cplusplus
}
#endif
    
#endif

#ifndef GB_PROXY_H
#define GB_PROXY_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#include <Core/gb.h>

typedef enum {
    // 8-bit registers
    SM83_REG_A,
    SM83_REG_B,
    SM83_REG_C,
    SM83_REG_D,
    SM83_REG_E,
    SM83_REG_F, // there is technically no F register, but here's a way to access it separately anyway
    SM83_REG_H,
    SM83_REG_L,

    // 16-bit combined registers
    SM83_REG_AF,
    SM83_REG_BC,
    SM83_REG_DE,
    SM83_REG_HL,

    // Stack pointer
    SM83_REG_SP,

    // PC (current instruction pointer)
    SM83_REG_PC,
} sm83_register;

// Get a value in a register
uint16_t get_gb_register(const struct GB_gameboy_s *gb, sm83_register r);

// Set a value in a reguster
void set_gb_register(struct GB_gameboy_s *gb, sm83_register r, uint16_t v);

// Get the # of backtraces
uint32_t get_gb_backtrace_size(const struct GB_gameboy_s *gb);

// Then get their addresses
uint16_t get_gb_backtrace_address(const struct GB_gameboy_s *gb, uint32_t bt);

// Get the # of breakpoints
uint32_t get_gb_breakpoint_size(const struct GB_gameboy_s *gb);

// Then get their addresses
uint16_t get_gb_breakpoint_address(const struct GB_gameboy_s *gb, uint32_t bt);

const uint32_t *get_gb_palette(const struct GB_gameboy_s *gb, GB_palette_type_t palette_type, unsigned char palette_index);

typedef enum {
    // No access made
    TILESET_INFO_NONE = 0,

    // OAM
    TILESET_INFO_OAM,

    // Background
    TILESET_INFO_BACKGROUND,

    // Window (uses background palette)
    TILESET_INFO_WINDOW
} tileset_object_info_tile_type;

typedef struct {
    // Address in VRAM
    uint16_t tile_address;

    // Index in the tileset
    uint16_t tile_index;

    // Tileset bank used (applies only to GameBoy Color games)
    uint8_t tile_bank;

    // Did we access it? If so, refer to tileset_object_info_tile_type
    uint8_t accessed_type;

    // If we accessed it, what's the index used to access it? (applies mainly to background/window. otherwise it's the same as tile_index for OAM)
    uint8_t accessed_tile_index;

    // If we accessed it, palette used for this tile.
    uint8_t accessed_tile_palette_index;
} tileset_object_info_tile;

typedef struct {
    tileset_object_info_tile tiles[384*2];
} tileset_object_info;

// Get the tileset info (used for getting information on each tile in the tileset)
void get_tileset_object_info(struct GB_gameboy_s *gb, tileset_object_info *tileset_info);

#ifdef __cplusplus
}
#endif
    
#endif

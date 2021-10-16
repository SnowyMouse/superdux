#define GB_INTERNAL // I solumnly swear I am up to no good
#include "gb_proxy.h"

// from SameBoy's debugger.c (struct is not exposed - will need updated if sameboy gets updated)
struct GB_breakpoint_s {
    union {
        struct {
        uint16_t addr;
        uint16_t bank; /* -1 = any bank*/
        };
        uint32_t key; /* For sorting and comparing */
    };
    char *condition;
    bool is_jump_to;
};

static inline uint8_t *get_8_bit_gb_register_address(struct GB_gameboy_s *gb, sm83_register r) {
    switch(r) {
        case SM83_REG_A:
            return &gb->a;
        case SM83_REG_B:
            return &gb->b;
        case SM83_REG_C:
            return &gb->c;
        case SM83_REG_D:
            return &gb->d;
        case SM83_REG_E:
            return &gb->e;
        case SM83_REG_F:
            return &gb->f;
        case SM83_REG_H:
            return &gb->h;
        case SM83_REG_L:
            return &gb->l;
        default:
            return NULL;
    }
}
static inline uint16_t *get_16_bit_gb_register_address(struct GB_gameboy_s *gb, sm83_register r) {
    switch(r) {
        case SM83_REG_HL:
            return &gb->hl;
        case SM83_REG_PC:
            return &gb->pc;
        case SM83_REG_SP:
            return &gb->sp;
        case SM83_REG_BC:
            return &gb->bc;
        case SM83_REG_AF:
            return &gb->af;
        case SM83_REG_DE:
            return &gb->de;
        default:
            return NULL;
    }
}

static inline uint8_t get_8_bit_gb_register(const struct GB_gameboy_s *gb, sm83_register r) {
    return *get_8_bit_gb_register_address((struct GB_gameboy_s *)gb, r);
}
static inline uint16_t get_16_bit_gb_register(const struct GB_gameboy_s *gb, sm83_register r) {
    return *get_16_bit_gb_register_address((struct GB_gameboy_s *)gb, r);
}

static void set_8_bit_gb_register(struct GB_gameboy_s *gb, sm83_register r, uint8_t v) {
    *get_8_bit_gb_register_address(gb, r) = v;
}
static void set_16_bit_gb_register(struct GB_gameboy_s *gb, sm83_register r, uint16_t v) {
    *get_16_bit_gb_register_address(gb, r) = v;
}

uint16_t get_gb_register(const struct GB_gameboy_s *gb, sm83_register r) {
    switch(r) {
        case SM83_REG_A:
        case SM83_REG_B:
        case SM83_REG_C:
        case SM83_REG_D:
        case SM83_REG_E:
        case SM83_REG_F:
        case SM83_REG_H:
        case SM83_REG_L:
            return get_8_bit_gb_register(gb, r);
        case SM83_REG_HL:
        case SM83_REG_PC:
        case SM83_REG_SP:
        case SM83_REG_AF:
        case SM83_REG_BC:
        case SM83_REG_DE:
            return get_16_bit_gb_register(gb, r);
        default:
            abort();
    }
}

void set_gb_register(struct GB_gameboy_s *gb, sm83_register r, uint16_t v) {
    switch(r) {
        case SM83_REG_A:
        case SM83_REG_B:
        case SM83_REG_C:
        case SM83_REG_D:
        case SM83_REG_E:
        case SM83_REG_F:
        case SM83_REG_H:
        case SM83_REG_L:
            return set_8_bit_gb_register(gb, r, v);
        case SM83_REG_HL:
        case SM83_REG_PC:
        case SM83_REG_SP:
        case SM83_REG_AF:
        case SM83_REG_BC:
        case SM83_REG_DE:
            return set_16_bit_gb_register(gb, r, v);
        default:
            abort();
    }
}

uint32_t get_gb_backtrace_size(const struct GB_gameboy_s *gb) {
    return gb->backtrace_size;
}
uint16_t get_gb_backtrace_address(const struct GB_gameboy_s *gb, uint32_t bt) {
    return gb->backtrace_returns[bt].addr;
}

uint32_t get_gb_breakpoint_size(const struct GB_gameboy_s *gb) {
    return gb->n_breakpoints;
}
uint16_t get_gb_breakpoint_address(const struct GB_gameboy_s *gb, uint32_t bt) {
    return gb->breakpoints[bt].addr;
}

static const uint32_t PALETTE_NONE[4] = { 0xFFFFFFFF, 0xFFAAAAAA, 0xFF555555, 0xFF000000 };
static const uint32_t PALETTE_FLIPPED[4] = { 0xFF000000, 0xFF555555, 0xFFAAAAAA, 0xFFFFFFFF };

const uint32_t *get_gb_palette(const struct GB_gameboy_s *gb, GB_palette_type_t palette_type, unsigned char palette_index) {
    const uint32_t *palette;

    switch(palette_type) {
        case GB_PALETTE_BACKGROUND:
            palette = gb->background_palettes_rgb;
            break;
        case GB_PALETTE_OAM:
            palette = gb->sprite_palettes_rgb;
            break;
        default:
        case GB_PALETTE_NONE:
        case GB_PALETTE_AUTO:
            return PALETTE_NONE;
    }

    return palette + 4 * (palette_index % 8);
}

static const uint32_t TILESET_WIDTH = 256;
static const uint32_t TILESET_HEIGHT = 192;
static const uint32_t TILESET_BLOCK_LENGTH = 8;

static const uint32_t TILESET_BLOCK_WIDTH = TILESET_WIDTH / TILESET_BLOCK_LENGTH;
static const uint32_t TILESET_BLOCK_HEIGHT = TILESET_HEIGHT / TILESET_BLOCK_LENGTH;

static const uint32_t TILESET_PAGE_BLOCK_WIDTH = TILESET_BLOCK_WIDTH / 2;
static const uint32_t TILESET_PAGE_BLOCK_COUNT = TILESET_PAGE_BLOCK_WIDTH * TILESET_BLOCK_HEIGHT;

// much of this information was from the pandocs (https://gbdev.io/pandocs/OAM.html)
void get_tileset_object_info(struct GB_gameboy_s *gb, tileset_object_info *tileset_info) {
    // Zero-intitialize tileset_info
    memset(tileset_info, 0, sizeof(*tileset_info));

    // Get the OAM data
    uint8_t *oam_data = GB_get_direct_access(gb, GB_DIRECT_ACCESS_OAM, NULL, NULL);
    uint8_t lcdc = GB_read_memory(gb, 0xFF40);
    uint16_t sprite_height = (lcdc & 0b100) ? 16 : 8;

    uint16_t bank = 0;
    size_t size = 0;

    // Background tile data
    uint8_t *tile_9800 = GB_get_direct_access(gb, GB_DIRECT_ACCESS_VRAM, &size, &bank) + 0x1800;
    uint8_t *tile_9C00 = tile_9800 + 0x400;

    bool sprites_enabled = (lcdc & 0b10);
    bool bg_window_enabled = gb->cgb_mode || (lcdc & 0b1);
    bool window_enabled = (lcdc & 0b100000) && bg_window_enabled;
    uint8_t window_x = GB_read_memory(gb, 0xFF4B), window_y = GB_read_memory(gb, 0xFF4A);

    uint8_t *background = (lcdc & 0b1000) ? tile_9C00 : tile_9800;
    uint8_t *background_attributes = background + 0x2000;

    uint8_t *window = (lcdc & 0b1000000) ? tile_9C00 : tile_9800;
    uint8_t *window_attributes = window + 0x2000;

    bool background_window_8800 = !(lcdc & 0b10000);

    for(uint32_t y = 0; y < TILESET_BLOCK_HEIGHT; y++) {
        for(uint32_t x = 0; x < TILESET_BLOCK_WIDTH; x++) {
            uint16_t tile_number = 0;

            uint16_t tileset_number = 0;
            uint16_t virtual_x = x;

            if(x >= TILESET_PAGE_BLOCK_WIDTH) {
                tileset_number = 1;
                virtual_x -= TILESET_PAGE_BLOCK_WIDTH;
            }

            // Get the tile number
            tile_number = virtual_x + (y * TILESET_PAGE_BLOCK_WIDTH);

            // Set these
            tileset_object_info_tile *block_info = tileset_info->tiles + x + y * TILESET_BLOCK_WIDTH;
            block_info->tile_index = tile_number;
            block_info->tile_bank = tileset_number;
            block_info->tile_address = 0x8000 + tile_number * 0x10;

            // If we already accessed it, continue
            if(block_info->accessed_type != TILESET_INFO_NONE) {
                continue;
            }

            // Check if a sprite uses this tile
            if(sprites_enabled) {
                for(uint8_t i = 0; i < 40; i++) {
                    uint8_t *object = oam_data + i * 4;

                    uint8_t flags = object[3];

                    uint16_t o_tileset_number = gb->cgb_mode ? (
                                                                    (flags & 0b1000) >> 3
                                                               ) : 0; // DMG is always tileset 0

                    uint16_t oam_tile = object[2];
                    if(sprite_height == 16) {
                        oam_tile = oam_tile & 0xFE; // from pandocs
                    }

                    // Is this the right tile?
                    if(oam_tile != tile_number || o_tileset_number != tileset_number) {
                        continue;
                    }

                    // Are we offscreen?
                    uint8_t oam_x = object[1];
                    uint8_t oam_y = object[0];

                    if(oam_x == 0 || oam_x >= 168) {
                        continue;
                    }
                    if(oam_y + sprite_height <= 16 || oam_y >= 160) {
                        continue;
                    }

                    // If in CGB mode, it's the lower 3 bits. Otherwise, it's the 5th bit
                    block_info->accessed_type = TILESET_INFO_OAM;
                    block_info->accessed_tile_palette_index = gb->cgb_mode ? (flags & 0b111) : ((flags & 0b10000) >> 4);
                    block_info->accessed_tile_index = oam_tile;
                    block_info->accessed_user_index = i;

                    // Do the next tile if 16 height sprite
                    if(sprite_height == 16) {
                        tileset_object_info_tile *next = block_info + 1;
                        next->accessed_type = TILESET_INFO_OAM;
                        next->accessed_tile_palette_index = gb->cgb_mode ? (flags & 0b111) : ((flags & 0b10000) >> 4);
                        next->accessed_tile_index = oam_tile;
                        next->accessed_user_index = i;
                    }

                    // We're done here
                    goto spaghetti_done_with_this_block;
                }
            }

            #define READ_BG_WINDOW(x, y, tile_data, tile_data_attributes, access_type) { \
                uint8_t accessed_tile_index = tile_data[x + y * 32]; \
                uint16_t tile = accessed_tile_index; \
                if(background_window_8800) { \
                    if(tile < 128) { \
                        tile += 0x100; \
                    } \
                } \
             \
                uint8_t bw_tileset_number, tile_palette; \
                if(gb->cgb_mode) { \
                    uint8_t tile_attributes = tile_data_attributes[x + y * 32]; \
                    bw_tileset_number = (tile_attributes & 0b1000) >> 3; \
                    tile_palette = tile_attributes & 0b111; \
                } \
                else { \
                    bw_tileset_number = 0; \
                    tile_palette = 0; \
                } \
             \
                if(tile != tile_number || tileset_number != bw_tileset_number) { \
                    continue; \
                } \
             \
                block_info->accessed_type = access_type; \
                block_info->accessed_tile_index = tile; \
                block_info->accessed_tile_palette_index = tile_palette; \
                goto spaghetti_done_with_this_block; \
            }

            // Next, check if a background tile uses this
            if(bg_window_enabled) {
                // First, window
                if(window_enabled && window_x <= 166 && window_y <= 143) {
                    for(uint8_t wy = 0; wy < (32 - window_y / 8); wy++) {
                        for(uint8_t wx = 0; wx < (32 - window_x / 8); wx++) {
                            READ_BG_WINDOW(wx, wy, window, window_attributes, TILESET_INFO_WINDOW)
                        }
                    }
                }

                // Next, background
                for(uint8_t by = 0; by < 32; by++) {
                    for(uint8_t bx = 0; bx < 32; bx++) {
                        READ_BG_WINDOW(bx, by, background, background_attributes, TILESET_INFO_BACKGROUND)
                    }
                }
            }


            spaghetti_done_with_this_block:
            continue;
        }
    }
}

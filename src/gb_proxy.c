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
static const uint32_t PALETTE_ZERO[4] = {0,0,0,0};

const uint32_t *get_gb_palette(struct GB_gameboy_s *gb, GB_palette_type_t palette_type, unsigned char palette_index) {
    if(!GB_is_cgb(gb)) {
        if(palette_type == GB_PALETTE_BACKGROUND && palette_index > 0) {
            return PALETTE_ZERO;
        }
        else if(palette_type == GB_PALETTE_OAM && palette_index > 1) {
            return PALETTE_ZERO;
        }
    }

    const uint32_t *palette;

    switch(palette_type) {
        case GB_PALETTE_BACKGROUND:
            palette = gb->background_palettes_rgb;
            break;
        case GB_PALETTE_OAM:
            palette = gb->object_palettes_rgb;
            break;
        default:
        case GB_PALETTE_NONE:
        case GB_PALETTE_AUTO:
            return PALETTE_NONE;
    }

    return palette + 4 * (palette_index % 8);
}

void skip_sgb_intro_animation(struct GB_gameboy_s *gb) {
    gb->sgb->intro_animation = 1000;
}

#include "gb_proxy.h"

#define GB_INTERNAL // I solumnly swear I am up to no good
#include <Core/gb.h>

// from SameBoy's debugger.c (struct is not exposed)
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

static inline uint8_t *get_8_bit_gb_register_address(struct GB_gameboy_s *gb, gbz80_register r) {
    switch(r) {
        case GBZ80_REG_A:
            return &gb->a;
        case GBZ80_REG_B:
            return &gb->b;
        case GBZ80_REG_C:
            return &gb->c;
        case GBZ80_REG_D:
            return &gb->d;
        case GBZ80_REG_E:
            return &gb->e;
        case GBZ80_REG_F:
            return &gb->f;
        default:
            return NULL;
    }
}
static inline uint16_t *get_16_bit_gb_register_address(struct GB_gameboy_s *gb, gbz80_register r) {
    switch(r) {
        case GBZ80_REG_HL:
            return &gb->hl;
        case GBZ80_REG_PC:
            return &gb->pc;
        default:
            return NULL;
    }
}

uint8_t get_8_bit_gb_register(struct GB_gameboy_s *gb, gbz80_register r) {
    return *get_8_bit_gb_register_address(gb, r);
}
uint16_t get_16_bit_gb_register(struct GB_gameboy_s *gb, gbz80_register r) {
    return *get_16_bit_gb_register_address(gb, r);
}

void set_8_bit_gb_register(struct GB_gameboy_s *gb, gbz80_register r, uint8_t v) {
    *get_8_bit_gb_register_address(gb, r) = v;
}
void set_16_bit_gb_register(struct GB_gameboy_s *gb, gbz80_register r, uint16_t v) {
    *get_16_bit_gb_register_address(gb, r) = v;
}

uint32_t get_gb_backtrace_size(struct GB_gameboy_s *gb) {
    return gb->backtrace_size;
}
uint16_t get_gb_backtrace_address(struct GB_gameboy_s *gb, uint32_t bt) {
    return gb->backtrace_returns[bt].addr;
}

uint32_t get_gb_breakpoint_size(struct GB_gameboy_s *gb) {
    return gb->n_breakpoints;
}
uint16_t get_gb_breakpoint_address(struct GB_gameboy_s *gb, uint32_t bt) {
    return gb->breakpoints[bt].addr;
}

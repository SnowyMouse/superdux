#include "gb_proxy.h"
#include <Core/gb.h>

typedef struct GB_gameboy_internal_s INTERNAL;

static inline uint8_t *get_8_bit_gb_register_address(struct GB_gameboy_s *gb, gbz80_register r) {
    INTERNAL *gb_int = (INTERNAL *)(gb);
    
    switch(r) {
        case GBZ80_REG_A:
            return &gb_int->a;
        case GBZ80_REG_B:
            return &gb_int->b;
        case GBZ80_REG_C:
            return &gb_int->c;
        case GBZ80_REG_D:
            return &gb_int->d;
        case GBZ80_REG_E:
            return &gb_int->e;
        case GBZ80_REG_F:
            return &gb_int->f;
        default:
            return NULL;
    }
}
static inline uint16_t *get_16_bit_gb_register_address(struct GB_gameboy_s *gb, gbz80_register r) {
    INTERNAL *gb_int = (INTERNAL *)(gb);
    
    switch(r) {
        case GBZ80_REG_HL:
            return &gb_int->hl;
        case GBZ80_REG_PC:
            return &gb_int->pc;
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
    return ((INTERNAL *)(gb))->backtrace_size;
}
uint16_t get_gb_backtrace_address(struct GB_gameboy_s *gb, uint32_t bt) {
    return ((INTERNAL *)(gb))->backtrace_returns[bt].addr;
}

#ifndef GB_PROXY_H
#define GB_PROXY_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct GB_gameboy_s;

typedef enum {
    GBZ80_REG_A,
    GBZ80_REG_B,
    GBZ80_REG_C,
    GBZ80_REG_D,
    GBZ80_REG_E,
    GBZ80_REG_F,
    GBZ80_REG_HL,
    GBZ80_REG_PC,
} gbz80_register;

uint8_t get_8_bit_gb_register(struct GB_gameboy_s *gb, gbz80_register r);
uint16_t get_16_bit_gb_register(struct GB_gameboy_s *gb, gbz80_register r);

void set_8_bit_gb_register(struct GB_gameboy_s *gb, gbz80_register r, uint8_t v);
void set_16_bit_gb_register(struct GB_gameboy_s *gb, gbz80_register r, uint16_t v);

uint32_t get_gb_backtrace_size(struct GB_gameboy_s *gb);
uint16_t get_gb_backtrace_address(struct GB_gameboy_s *gb, uint32_t bt);

uint32_t get_gb_breakpoint_size(struct GB_gameboy_s *gb);
uint16_t get_gb_breakpoint_address(struct GB_gameboy_s *gb, uint32_t bt);

#ifdef __cplusplus
}
#endif
    
#endif

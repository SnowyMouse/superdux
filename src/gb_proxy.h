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
    GBZ80_REG_SP,
} gbz80_register;

uint16_t get_gb_register(const struct GB_gameboy_s *gb, gbz80_register r);
void set_gb_register(struct GB_gameboy_s *gb, gbz80_register r, uint16_t v);

uint32_t get_gb_backtrace_size(const struct GB_gameboy_s *gb);
uint16_t get_gb_backtrace_address(const struct GB_gameboy_s *gb, uint32_t bt);

uint32_t get_gb_breakpoint_size(const struct GB_gameboy_s *gb);
uint16_t get_gb_breakpoint_address(const struct GB_gameboy_s *gb, uint32_t bt);

#ifdef __cplusplus
}
#endif
    
#endif

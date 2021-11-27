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
} sm83_register_t;

// Get a value in a register
uint16_t get_gb_register(const struct GB_gameboy_s *gb, sm83_register_t r);

// Set a value in a reguster
void set_gb_register(struct GB_gameboy_s *gb, sm83_register_t r, uint16_t v);

// Get the # of backtraces
uint32_t get_gb_backtrace_size(const struct GB_gameboy_s *gb);

// Then get their addresses
uint16_t get_gb_backtrace_address(const struct GB_gameboy_s *gb, uint32_t bt);

// Get the # of breakpoints
uint32_t get_gb_breakpoint_size(const struct GB_gameboy_s *gb);

// Then get their addresses
uint16_t get_gb_breakpoint_address(const struct GB_gameboy_s *gb, uint32_t bt);

// Get a pointer to the palette
const uint32_t *get_gb_palette(struct GB_gameboy_s *gb, GB_palette_type_t palette_type, unsigned char palette_index);

#ifdef __cplusplus
}
#endif
    
#endif

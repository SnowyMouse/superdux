#ifndef GB_PROXY_H
#define GB_PROXY_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#include <Core/gb.h>

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

// Skip the SGB intro animation
void skip_sgb_intro_animation(struct GB_gameboy_s *gb);

#ifdef __cplusplus
}
#endif
    
#endif

extern "C" {
#include <Core/gb.h>
}

#include <cstdint>

extern "C" std::size_t get_gb_struct_size_cxx() {
    return sizeof(GB_gameboy_s);
}

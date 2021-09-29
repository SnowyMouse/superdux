#include <cstdlib>
#include <cstdio>

extern "C" {
    std::size_t get_gb_struct_size_cxx();
    std::size_t get_gb_struct_size_c();
}


int main() {
    if(get_gb_struct_size_c() != get_gb_struct_size_cxx()) {
        std::fprintf(stderr, "Struct size is different!\n");
        std::fprintf(stderr, "C (0x%08zX bytes) vs C++ (0x%08zX bytes)\n", get_gb_struct_size_c(), get_gb_struct_size_cxx());
        return EXIT_FAILURE;
    }
    else {
        return EXIT_SUCCESS;
    }
}

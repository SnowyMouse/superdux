#include <cstdlib>
#include <cstdio>

extern "C" {
    std::size_t get_gb_struct_size_cxx();
    std::size_t get_gb_struct_size_c();
}


int main() {
    auto c = get_gb_struct_size_c();
    auto cxx = get_gb_struct_size_cxx();
    
    if(cxx >= c) {
        return EXIT_SUCCESS;
    }
    
    std::fprintf(stderr, "Difference found: C (0x%08zX bytes) vs C++ (0x%08zX bytes)\n", get_gb_struct_size_c(), get_gb_struct_size_cxx());
    
    if(cxx > c) {
        std::fprintf(stderr, "The CXX struct is larger, so it is most likely still fine.\n");
        return EXIT_SUCCESS;
    }
    else {
        std::fprintf(stderr, "The CXX struct is smaller which will not work.\n");
        return EXIT_FAILURE;
    }
}

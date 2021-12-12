#include <cstdio>
#include <string>
#include <cstring>

extern "C" ssize_t getline(char **lineptr, size_t *n, FILE *stream) {
    *n = 0;

    std::string str;
    char c;

    while(true) {
        if(std::fread(&c, sizeof(char), 1, stream) != 1) {
            return -1;
        }
        str += c;
        *n = *n + 1;

        if(c == '\n') {
            break;
        }
    }

    free(*lineptr);
    *lineptr = reinterpret_cast<char *>(std::calloc(str.size() + 1, 1));
    std::memcpy(*lineptr, str.c_str(), str.size());

    return *n;
}

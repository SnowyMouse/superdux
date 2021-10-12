#include <QApplication>
#include <filesystem>
#include "game_window.hpp"
#include <SDL2/SDL.h>

int main(int argc, char **argv) {
    if(argc > 2) {
        std::printf("Usage: %s [path-to-rom]\n", argv[0]);
        return EXIT_FAILURE;
    }
    else if(argc == 2 && !std::filesystem::exists(argv[1])) {
        std::fprintf(stderr, "Error: No file exists at %s\n", argv[1]);
        return EXIT_FAILURE;
    }
    
    SDL_Init(SDL_INIT_AUDIO | SDL_INIT_EVENTS | SDL_INIT_JOYSTICK);
    
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName("sameboy-dx");
    QCoreApplication::setOrganizationDomain("sameboy-dx");
    QCoreApplication::setApplicationName("sameboy-dx");
    
    GameWindow window;
    
    QSettings settings;
    
    window.show();
    
    if(argc == 2) {
        window.load_rom(argv[1]);
    }
    
    int r = app.exec();
    
    SDL_Quit();
    
    return r;
}

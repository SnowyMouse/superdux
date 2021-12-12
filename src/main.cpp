#include <QApplication>
#include <filesystem>
#include "game_window.hpp"
#include <SDL2/SDL.h>
#include <QIcon>

#ifdef _WIN32
#include <QtPlugin>
Q_IMPORT_PLUGIN(QWindowsIntegrationPlugin)
Q_IMPORT_PLUGIN(QWindowsVistaStylePlugin)
#endif

int main(int argc, char **argv) {
    if(argc > 2) {
        std::printf("Usage: %s [path-to-rom]\n", argv[0]);
        return EXIT_FAILURE;
    }
    else if(argc == 2 && !std::filesystem::exists(argv[1])) {
        std::fprintf(stderr, "Error: No file exists at %s\n", argv[1]);
        return EXIT_FAILURE;
    }
    
    SDL_Init(SDL_INIT_AUDIO | SDL_INIT_EVENTS | SDL_INIT_GAMECONTROLLER | SDL_INIT_JOYSTICK);
    
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName("superdux");
    QCoreApplication::setOrganizationDomain("superdux");
    QCoreApplication::setApplicationName("superdux");

    app.setWindowIcon(QIcon(":icon/superdux.ico"));

    int r;

    // Braces here because InputDeviceGamepad destructor will cause a segfault if SDL_Quit is called before it is called
    {
        GameWindow window;
        window.show();

        if(argc == 2) {
            window.load_rom(argv[1]);
        }

        r = app.exec();
    }
    
    SDL_Quit();
    
    return r;
}

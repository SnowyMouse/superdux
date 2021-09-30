#include <QApplication>
#include <filesystem>
#include "game_window.hpp"

int main(int argc, char **argv) {
    if(argc > 2) {
        std::printf("Usage: %s [path-to-rom]\n", argv[0]);
        return EXIT_FAILURE;
    }
    else if(argc == 2 && !std::filesystem::exists(argv[1])) {
        std::fprintf(stderr, "Error: No file exists at %s\n", argv[1]);
        return EXIT_FAILURE;
    }
    
    auto *app = new QApplication(argc, argv);
    
    QCoreApplication::setOrganizationName("super-sameboy");
    QCoreApplication::setOrganizationDomain("super-sameboy");
    QCoreApplication::setApplicationName("super-sameboy");
    
    GameWindow window;
    
    QSettings settings;
    
    window.show();
    
    if(argc == 2) {
        window.load_rom(argv[1]);
    }
    
    return app->exec();
}

#include <QApplication>
#include "game_window.hpp"

int main(int argc, char **argv) {
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

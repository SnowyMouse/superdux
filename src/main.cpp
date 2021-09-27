#include <QApplication>
#include "game_window.hpp"

int main(int argc, char **argv) {
    auto *app = new QApplication(argc, argv);
    
    GameWindow window;
    
    window.show();
    
    if(argc == 2) {
        window.load_rom(argv[1]);
    }
    
    return app->exec();
}
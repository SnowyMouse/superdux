#include "game_debugger.hpp"
#include "game_window.hpp"
#include "game_disassembler.hpp"

#include <QHeaderView>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QHBoxLayout>
#include <QMenuBar>
#include <QTimer>

GameDebugger::GameDebugger() {
    QMenuBar *bar = new QMenuBar(this);
    this->setMenuBar(bar);
    
    auto *central_widget = new QWidget(this);
    
    auto *layout = new QHBoxLayout(central_widget);
    layout->addWidget((this->disassembler = new GameDisassembler(this)));
    
    
    this->setCentralWidget(central_widget);
    this->setMinimumHeight(600);
    this->setMinimumWidth(800);
    
    this->setWindowTitle("Debugger");
}

void GameDebugger::set_gameboy(GB_gameboy_s *gb) {
    this->gameboy = gb;
    GB_set_log_callback(this->gameboy, log_callback);
}

void GameDebugger::refresh_view() {
    // If we aren't visible, go away
    if(!this->isVisible()) {
        return;
    }
    
    this->disassembler->refresh_view();
}

GameDebugger::~GameDebugger() {}

GameDebugger *GameDebugger::resolve_debugger(GB_gameboy_s *gb) noexcept {
    return reinterpret_cast<GameWindow *>(GB_get_user_data(gb))->debugger_window;
}

void GameDebugger::log_callback(GB_gameboy_s *gb, const char *text, GB_log_attributes) {
    auto *debugger_window = resolve_debugger(gb);
    if(debugger_window->retain_logs > 0) {
        debugger_window->retained_logs += text;
        return;
    }
    
    std::printf(text); // todo: implement a console
}

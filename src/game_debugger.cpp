#include "game_debugger.hpp"
#include "game_window.hpp"
#include "game_disassembler.hpp"

#include <QHeaderView>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QCoreApplication>
#include <QHBoxLayout>
#include <QMenuBar>
#include <QToolBar>
#include <QTimer>

GameDebugger::GameDebugger() {
    QToolBar *bar = new QToolBar(this);
    bar->setMovable(false);
    this->addToolBar(bar);
    
    this->break_button = bar->addAction("Break");
    this->break_button->setEnabled(true);
    connect(this->break_button, &QAction::triggered, this, &GameDebugger::break_now);
    
    this->continue_button = bar->addAction("Continue");
    this->continue_button->setEnabled(false);
    connect(this->continue_button, &QAction::triggered, this, &GameDebugger::continue_break);
    
    auto *central_widget = new QWidget(this);
    auto *layout = new QHBoxLayout(central_widget);
    layout->addWidget((this->disassembler = new GameDisassembler(this)));
    
    this->setCentralWidget(central_widget);
    this->setMinimumHeight(600);
    this->setMinimumWidth(800);
    
    this->setWindowTitle("Debugger");
}

void GameDebugger::break_now() {
    GB_debugger_break(this->gameboy);
}

void GameDebugger::continue_break() {
    char *cmd = nullptr;
    asprintf(&cmd, "continue");
    GB_debugger_execute_command(this->gameboy, cmd);
    this->debug_breakpoint_pause = false;
}

char *GameDebugger::input_callback(GB_gameboy_s *gb) noexcept {
    // TODO: don't do this messy thing
    auto *debugger = resolve_debugger(gb);
    auto &pause = debugger->debug_breakpoint_pause;
    pause = true;
    debugger->break_button->setEnabled(false);
    
    // Figure out the address
    std::uint16_t first_address;
    debugger->disassembler->disassemble_at_address(std::nullopt, 5, first_address);
    
    while(pause) {
        QCoreApplication::processEvents();
        debugger->disassembler->current_address = first_address;
        debugger->refresh_view();
        debugger->continue_button->setEnabled(true);
    }
    
    debugger->break_button->setEnabled(true);
    debugger->continue_button->setEnabled(false);
    
    return nullptr;
}

void GameDebugger::set_gameboy(GB_gameboy_s *gb) {
    this->gameboy = gb;
    GB_set_log_callback(this->gameboy, log_callback);
    GB_set_input_callback(this->gameboy, input_callback);
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
    
    std::printf("%s", text); // todo: implement a console
}

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
    connect(this->break_button, &QAction::triggered, this, &GameDebugger::action_break);
    
    this->continue_button = bar->addAction("Continue");
    this->continue_button->setEnabled(false);
    connect(this->continue_button, &QAction::triggered, this, &GameDebugger::action_continue);
    
    this->step_button = bar->addAction("Step");
    this->step_button->setEnabled(false);
    connect(this->step_button, &QAction::triggered, this, &GameDebugger::action_step);
    
    this->step_over_button = bar->addAction("Step over");
    this->step_over_button->setEnabled(false);
    connect(this->step_over_button, &QAction::triggered, this, &GameDebugger::action_step_over);
    
    this->finish_fn_button = bar->addAction("Finish function");
    this->finish_fn_button->setEnabled(false);
    connect(this->finish_fn_button, &QAction::triggered, this, &GameDebugger::action_finish);
    
    auto *central_widget = new QWidget(this);
    auto *layout = new QHBoxLayout(central_widget);
    layout->addWidget((this->disassembler = new GameDisassembler(this)));
    
    this->setCentralWidget(central_widget);
    this->setMinimumHeight(600);
    this->setMinimumWidth(800);
    
    this->setWindowTitle("Debugger");
}

void GameDebugger::action_break() {
    GB_debugger_break(this->gameboy);
}

void GameDebugger::action_continue() {
    this->continue_break("continue");
}

void GameDebugger::action_step() {
    this->continue_break("step");
}

void GameDebugger::action_step_over() {
    this->continue_break("next");
}

void GameDebugger::action_finish() {
    this->continue_break("finish");
}

void GameDebugger::continue_break(const char *command_to_execute) {
    this->debug_breakpoint_pause = false;
    this->command_to_execute_on_unbreak = command_to_execute ? command_to_execute : "continue";
}

char *GameDebugger::input_callback(GB_gameboy_s *gb) noexcept {
    // TODO: don't do this messy thing
    auto *debugger = resolve_debugger(gb);
    auto &pause = debugger->debug_breakpoint_pause;
    pause = true;
    debugger->break_button->setEnabled(false);
    
    // Figure out the address
    debugger->disassembler->set_address_to_current_breakpoint();
    
    // Enable these
    debugger->continue_button->setEnabled(true);
    debugger->step_button->setEnabled(true);
    debugger->step_over_button->setEnabled(true);
    debugger->finish_fn_button->setEnabled(true);
    
    while(pause) {
        QCoreApplication::processEvents();
        debugger->refresh_view();
    }
    
    debugger->break_button->setEnabled(true);
    debugger->continue_button->setEnabled(false);
    debugger->step_button->setEnabled(false);
    debugger->step_over_button->setEnabled(false);
    debugger->finish_fn_button->setEnabled(false);
    
    // If the command we want to execute is empty, simply continue
    if(debugger->command_to_execute_on_unbreak.empty()) {
        return nullptr;
    }
    
    // Otherwise, execute it
    char *cmd;
    asprintf(&cmd, "%s", debugger->command_to_execute_on_unbreak.c_str());
    debugger->command_to_execute_on_unbreak.clear();
    return cmd;
}

void GameDebugger::execute_debugger_command(const char *command) {
    char *cmd = nullptr;
    asprintf(&cmd, "%s", command);
    GB_debugger_execute_command(this->gameboy, cmd);
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

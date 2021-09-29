#define GB_INTERNAL // Required to directly access registers since SameBoy does not expose these presently

#include "game_debugger.hpp"
#include "game_window.hpp"
#include "game_disassembler.hpp"

#include <QLineEdit>
#include <QHeaderView>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QCoreApplication>
#include <QHBoxLayout>
#include <QMenuBar>
#include <QToolBar>
#include <QTimer>
#include <QFontDatabase>
#include <QGridLayout>
#include <QMessageBox>
#include <QLabel>
#include <QGroupBox>

GameDebugger::GameDebugger() {
    // Set the preferred font for the debugger
    this->table_font = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    this->table_font.setPixelSize(14);
    
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
    
    
    
    this->right_view = new QWidget(this);
    auto *right_view_layout = new QVBoxLayout(this->right_view);
    this->right_view->setLayout(right_view_layout);
    
    // Add registers
    this->register_view = new QGroupBox(this->right_view);
    this->register_view->setVisible(false);
    reinterpret_cast<QGroupBox *>(this->register_view)->setTitle("Registers");
    this->register_view->setMinimumWidth(200);
    this->register_view->setMaximumWidth(200);
    right_view_layout->setContentsMargins(0,0,0,0);
    
    
    auto *register_layout = new QGridLayout();
    int register_row = 0;
    
    #define ADD_REGISTER_FIELD(nameA, fieldA, nameB, fieldB) register_layout->addWidget(new QLabel(nameA), register_row, 0); \
                                                             this->fieldA = new QLineEdit(this->register_view); \
                                                             register_layout->addWidget(this->fieldA, register_row, 1); \
                                                             register_layout->addWidget(new QLabel(nameB), register_row, 2); \
                                                             this->fieldB = new QLineEdit(this->register_view); \
                                                             register_layout->addWidget(this->fieldB, register_row, 3); \
                                                             connect(this->fieldA, &QLineEdit::textChanged, this, &GameDebugger::action_update_registers);\
                                                             connect(this->fieldB, &QLineEdit::textChanged, this, &GameDebugger::action_update_registers);\
                                                             register_row++;

    ADD_REGISTER_FIELD("A", register_a, "F", register_f);
    ADD_REGISTER_FIELD("B", register_b, "C", register_c);
    ADD_REGISTER_FIELD("D", register_d, "E", register_e);
    ADD_REGISTER_FIELD("HL", register_hl, "PC", register_pc);
    
    // Disable editing PC value
    this->register_pc->setDisabled(true);
    
    #undef ADD_REGISTER_FIELD
    
    this->register_view->setLayout(register_layout);
    this->register_view->setSizePolicy(QSizePolicy::Policy::Fixed, QSizePolicy::Policy::Fixed);
    right_view_layout->addWidget(this->register_view);
    right_view_layout->addWidget(new QWidget());
    layout->addWidget(this->right_view);
    
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
    debugger->register_view->setVisible(true);
    
    // Enable these
    debugger->continue_button->setEnabled(true);
    debugger->step_button->setEnabled(true);
    debugger->step_over_button->setEnabled(true);
    debugger->finish_fn_button->setEnabled(true);
    
    while(pause) {
        QCoreApplication::processEvents();
        debugger->refresh_view();
    }
    
    debugger->register_view->setVisible(false);
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
    this->refresh_registers();
}

void GameDebugger::refresh_registers() {
    if(this->register_view->isVisible()) {
        #define PROCESS_REGISTER_FIELD(name, field) this->field->blockSignals(true); \
                                                    this->field->setText(QString("$") + QString::number(this->gameboy->name, 16)); \
                                                    this->field->blockSignals(false);

        PROCESS_REGISTER_FIELD(a, register_a);
        PROCESS_REGISTER_FIELD(b, register_b);
        PROCESS_REGISTER_FIELD(c, register_c);
        PROCESS_REGISTER_FIELD(d, register_d);
        PROCESS_REGISTER_FIELD(e, register_e);
        PROCESS_REGISTER_FIELD(f, register_f);
        PROCESS_REGISTER_FIELD(hl, register_hl);
        PROCESS_REGISTER_FIELD(pc, register_pc);
        
        #undef PROCESS_REGISTER_FIELD
    }
}

void GameDebugger::action_update_registers() noexcept {
    if(!this->debug_breakpoint_pause || this->register_view->isHidden()) {
        return;
    }
    
    #define PROCESS_REGISTER_FIELD(name, field) {\
        auto value = this->evaluate_expression(this->field->text().toUtf8().data());\
        if(value.has_value()) {\
            this->gameboy->name = *value;\
        }\
    }

    PROCESS_REGISTER_FIELD(a, register_a);
    PROCESS_REGISTER_FIELD(b, register_b);
    PROCESS_REGISTER_FIELD(c, register_c);
    PROCESS_REGISTER_FIELD(d, register_d);
    PROCESS_REGISTER_FIELD(e, register_e);
    PROCESS_REGISTER_FIELD(f, register_f);
    PROCESS_REGISTER_FIELD(hl, register_hl);
    //PROCESS_REGISTER_FIELD(pc, register_pc); // changing this is very bad
    
    #undef PROCESS_REGISTER_FIELD
}

std::optional<std::uint16_t> GameDebugger::evaluate_expression(const char *expression) {
    std::optional<std::uint16_t> result;
    std::uint16_t result_maybe;
    
    this->push_retain_logs();
    
    if(GB_debugger_evaluate(this->gameboy, expression, &result_maybe, nullptr)) {
        auto &logs = this->retained_logs;
        if(!logs.empty()) {
            QMessageBox(QMessageBox::Icon::Critical, "Error evaluating expression", logs.c_str()).exec();
            logs.clear();
        }
        else {
            QMessageBox(QMessageBox::Icon::Critical, "Error evaluating expression", QString("Could not evaulate expression ") + expression).exec();
        }
    }
    else {
        result = result_maybe;
    }
    this->pop_retain_logs();
    
    return result;
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

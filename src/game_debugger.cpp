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
#include <QScrollBar>
#include <QMouseEvent>

#include "gb_proxy.h"

class GameDebuggerTable : public QTableWidget {
public:
    GameDebuggerTable(QWidget *parent, GameDebugger *window) : QTableWidget(parent), window(window) {}
    
    // Double click = goto address in debugger
    void mouseDoubleClickEvent(QMouseEvent *event) override {
        auto *item = this->itemAt(event->pos());
        if(item) {
            int r = item->data(Qt::UserRole).toInt();
            auto bt_size = get_gb_backtrace_size(this->window->gameboy);
            
            // Go there
            if(r >= 0 && r <= bt_size) {
                std::uint16_t addr;
                
                // Topmost should just be the current instruction since that's what is shown
                if(r == 0) {
                    addr = get_16_bit_gb_register(this->window->gameboy, gbz80_register::GBZ80_REG_PC);
                }
                else {
                    addr = get_gb_backtrace_address(this->window->gameboy, bt_size - r);
                }
                
                this->window->disassembler->go_to(addr);
            }
        }
    }
    
private:
    GameDebugger *window;
};

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
    
    bar->addSeparator();
    
    this->clear_breakpoints_button = bar->addAction("Clear breakpoints");
    connect(this->clear_breakpoints_button, &QAction::triggered, this, &GameDebugger::action_clear_breakpoints);
    
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
    auto *register_view = new QGroupBox(this->right_view);
    reinterpret_cast<QGroupBox *>(register_view)->setTitle("Registers");
    right_view_layout->setContentsMargins(0,0,0,0);
    
    
    auto *register_layout = new QGridLayout();
    int register_row = 0;
    
    #define ADD_REGISTER_FIELD(nameA, fieldA, nameB, fieldB) register_layout->addWidget(new QLabel(nameA), register_row, 0); \
                                                             this->fieldA = new QLineEdit(register_view); \
                                                             register_layout->addWidget(this->fieldA, register_row, 1); \
                                                             register_layout->addWidget(new QLabel(nameB), register_row, 2); \
                                                             this->fieldB = new QLineEdit(register_view); \
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
    
    register_view->setLayout(register_layout);
    register_view->setSizePolicy(QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Fixed);
    right_view_layout->addWidget(register_view);
    
    // Backtrace
    auto *backtrace_frame = new QGroupBox(this->right_view);
    backtrace_frame->setTitle("Backtrace");
    auto *backtrace_layout = new QVBoxLayout();
    this->backtrace = new GameDebuggerTable(this->right_view, this);
    this->format_table(this->backtrace);
    this->backtrace->setColumnCount(1);
    this->backtrace->setTextElideMode(Qt::ElideNone);
    backtrace_layout->addWidget(this->backtrace);
    backtrace_frame->setLayout(backtrace_layout);
    right_view_layout->addWidget(backtrace_frame);
    
    // Done
    layout->addWidget(this->right_view);
    this->right_view->setMaximumWidth(300);
    this->right_view->setMinimumWidth(300);
    this->right_view->setEnabled(false);
    
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
    debugger->right_view->setEnabled(true);
    
    // Enable these
    debugger->continue_button->setEnabled(true);
    debugger->step_button->setEnabled(true);
    debugger->step_over_button->setEnabled(true);
    debugger->finish_fn_button->setEnabled(true);
    
    while(pause) {
        QCoreApplication::processEvents();
        debugger->refresh_view();
    }
    
    debugger->right_view->setEnabled(false);
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
    this->clear_breakpoints_button->setEnabled(get_gb_breakpoint_size(this->gameboy) > 0);
    
    if(!this->debug_breakpoint_pause) {
        #define PROCESS_REGISTER_FIELD(name, size, field, fmt) {\
            char str[8]; \
            std::snprintf(str, sizeof(str), fmt, get_##size##_bit_gb_register(this->gameboy, gbz80_register::GBZ80_REG_##name)); \
            this->field->blockSignals(true); \
            this->field->setText(str); \
            this->field->blockSignals(false); \
        }

        PROCESS_REGISTER_FIELD(A, 8, register_a, "$%02x");
        PROCESS_REGISTER_FIELD(B, 8, register_b, "$%02x");
        PROCESS_REGISTER_FIELD(C, 8, register_c, "$%02x");
        PROCESS_REGISTER_FIELD(D, 8, register_d, "$%02x");
        PROCESS_REGISTER_FIELD(E, 8, register_e, "$%02x");
        PROCESS_REGISTER_FIELD(F, 8, register_f, "$%02x");
        PROCESS_REGISTER_FIELD(HL, 16, register_hl, "$%04x");
        PROCESS_REGISTER_FIELD(PC, 16, register_pc, "$%04x");
        
        #undef PROCESS_REGISTER_FIELD
        
        this->push_retain_logs();
        this->execute_debugger_command("backtrace");
        auto logs = QString::fromStdString(this->retained_logs).split('\n');
        this->retained_logs.clear();
        this->pop_retain_logs();
        
        this->backtrace->setRowCount(logs.length());
        int row = 0;
        for(auto &l : logs) {
            auto l_trimmed = l.trimmed();
            if(l_trimmed.isEmpty()) {
                continue;
            }
            auto *item = new QTableWidgetItem(l_trimmed);
            item->setToolTip(l_trimmed);
            item->setFlags(item->flags() & ~Qt::ItemIsEditable);
            this->backtrace->setItem(row, 0, item);
            item->setData(Qt::UserRole, row);
            row++;
        }
        this->backtrace->setRowCount(row);
    }
}

void GameDebugger::action_clear_breakpoints() noexcept {
    this->execute_debugger_command("delete");
}

void GameDebugger::action_update_registers() noexcept {
    if(!this->debug_breakpoint_pause || this->right_view->isHidden()) {
        return;
    }
    
    #define PROCESS_REGISTER_FIELD(name, size, field) {\
        auto value = this->evaluate_expression(this->field->text().toUtf8().data());\
        if(value.has_value()) {\
            set_##size##_bit_gb_register(this->gameboy, gbz80_register::GBZ80_REG_##name, *value);\
        }\
    }

    PROCESS_REGISTER_FIELD(A, 8, register_a);
    PROCESS_REGISTER_FIELD(B, 8, register_b);
    PROCESS_REGISTER_FIELD(C, 8, register_c);
    PROCESS_REGISTER_FIELD(D, 8, register_d);
    PROCESS_REGISTER_FIELD(E, 8, register_e);
    PROCESS_REGISTER_FIELD(F, 8, register_f);
    PROCESS_REGISTER_FIELD(HL, 16, register_hl);
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

void GameDebugger::format_table(QTableWidget *widget) {
    widget->horizontalHeader()->setStretchLastSection(true);
    widget->horizontalHeader()->hide();
    widget->horizontalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    widget->verticalHeader()->setMaximumSectionSize(this->table_font.pixelSize() + 4);
    widget->verticalHeader()->setMinimumSectionSize(this->table_font.pixelSize() + 4);
    widget->verticalHeader()->setDefaultSectionSize(this->table_font.pixelSize() + 4);
    widget->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    widget->verticalHeader()->hide();
    widget->setSelectionMode(QAbstractItemView::SingleSelection);
    widget->verticalScrollBar()->hide();
    widget->setAlternatingRowColors(true);
    widget->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    widget->setShowGrid(false);
    widget->setFont(this->table_font);
}

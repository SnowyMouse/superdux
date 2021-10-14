#include "game_debugger.hpp"
#include "game_window.hpp"
#include "game_disassembler.hpp"

#include <QLineEdit>
#include <QHeaderView>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QCoreApplication>
#include <QCheckBox>
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

class GameDebugger::GameDebuggerTable : public QTableWidget {
public:
    GameDebuggerTable(QWidget *parent, GameDebugger *window) : QTableWidget(parent), window(window) {}
    
    // Double click = goto address in debugger
    void mouseDoubleClickEvent(QMouseEvent *event) override {
        auto *item = this->itemAt(event->pos());
        if(item) {
            unsigned int r = item->data(Qt::UserRole).toUInt();
            this->window->disassembler->go_to(r);
        }
    }
    
private:
    GameDebugger *window;
};

GameDebugger::GameDebugger(GameWindow *window) : game_window(window) {
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
    
    this->step_button = bar->addAction("Step Into");
    this->step_button->setEnabled(false);
    connect(this->step_button, &QAction::triggered, this, &GameDebugger::action_step);
    
    this->step_over_button = bar->addAction("Step Over");
    this->step_over_button->setEnabled(false);
    connect(this->step_over_button, &QAction::triggered, this, &GameDebugger::action_step_over);
    
    this->finish_fn_button = bar->addAction("Finish Function");
    this->finish_fn_button->setEnabled(false);
    connect(this->finish_fn_button, &QAction::triggered, this, &GameDebugger::action_finish);
    
    bar->addSeparator();
    
    this->clear_breakpoints_button = bar->addAction("Clear Breakpoints");
    this->clear_breakpoints_button->setEnabled(false);
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
    right_view_layout->setContentsMargins(0,0,0,0);
    
    // Add registers
    auto *register_view = new QGroupBox(this->right_view);
    reinterpret_cast<QGroupBox *>(register_view)->setTitle("Registers");
    auto *register_view_layout = new QVBoxLayout(register_view);
    register_view->setLayout(register_view_layout);
    
    auto *register_widget = new QWidget(register_view);
    auto *register_widget_layout = new QGridLayout(register_widget);
    register_widget->setLayout(register_widget_layout);
    register_widget_layout->setContentsMargins(0,0,0,0);
    int register_row = 0;
    
    #define ADD_REGISTER_FIELD(nameA, fieldA, nameB, fieldB) register_widget_layout->addWidget(new QLabel(nameA, register_widget), register_row, 0); \
                                                             this->fieldA = new QLineEdit(register_widget); \
                                                             register_widget_layout->addWidget(this->fieldA, register_row, 1); \
                                                             register_widget_layout->addWidget(new QLabel(nameB, register_widget), register_row, 2); \
                                                             this->fieldB = new QLineEdit(register_widget); \
                                                             register_widget_layout->addWidget(this->fieldB, register_row, 3); \
                                                             connect(this->fieldA, &QLineEdit::textChanged, this, &GameDebugger::action_update_registers);\
                                                             connect(this->fieldB, &QLineEdit::textChanged, this, &GameDebugger::action_update_registers);\
                                                             register_row++;

    ADD_REGISTER_FIELD("AF", register_af, "HL", register_hl);
    ADD_REGISTER_FIELD("BC", register_bc, "SP", register_sp);
    ADD_REGISTER_FIELD("DE", register_de, "PC", register_pc);

    #undef ADD_REGISTER_FIELD

    register_view_layout->addWidget(register_widget);
    this->register_pc->setReadOnly(true); // prevent editing PC

    auto *flag_widget = new QWidget(register_view);
    auto *flag_layout = new QHBoxLayout(flag_widget);
    flag_layout->setContentsMargins(0,0,0,0);

    #define ADD_FLAG_CHECKBOX(field, text) this->field = new QCheckBox(flag_widget); \
                                           this->field->setText(text); \
                                           flag_layout->addWidget(this->field); \
                                           this->field->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed); \
                                           connect(this->field, &QCheckBox::stateChanged, this, &GameDebugger::action_register_flag_state_changed);

    ADD_FLAG_CHECKBOX(flag_carry, "C");
    ADD_FLAG_CHECKBOX(flag_half_carry, "H");
    ADD_FLAG_CHECKBOX(flag_subtract, "N");
    ADD_FLAG_CHECKBOX(flag_zero, "Z");

    flag_widget->setLayout(flag_layout);
    register_view_layout->addWidget(flag_widget);


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
    this->get_instance().break_immediately();
}

void GameDebugger::action_continue() {
    this->get_instance().unbreak("continue");
    this->set_known_breakpoint(false);
}

void GameDebugger::action_step() {
    this->get_instance().unbreak("step");
    this->set_known_breakpoint(false);
}

void GameDebugger::action_step_over() {
    this->get_instance().unbreak("next");
    this->set_known_breakpoint(false);
}

void GameDebugger::action_finish() {
    this->get_instance().unbreak("finish");
    this->set_known_breakpoint(false);
}

void GameDebugger::set_known_breakpoint(bool known_breakpoint) {
    if(this->known_breakpoint != known_breakpoint) {
        this->known_breakpoint = known_breakpoint;
        this->right_view->setEnabled(known_breakpoint);
        this->break_button->setEnabled(!known_breakpoint);
        this->continue_button->setEnabled(known_breakpoint);
        this->step_button->setEnabled(known_breakpoint);
        this->step_over_button->setEnabled(known_breakpoint);
        this->finish_fn_button->setEnabled(known_breakpoint);
        
        if(known_breakpoint) {
            this->disassembler->go_to(this->get_instance().get_register_value(sm83_register::SM83_REG_PC));
        }
    }
}

void GameDebugger::refresh_flags() {
    auto f = this->get_instance().get_register_value(sm83_register::SM83_REG_F);

    #define PROCESS_FLAG_FIELD(field, flag) {\
        this->field->blockSignals(true); \
        this->field->setChecked(f & flag); \
        this->field->blockSignals(false); \
    }

    PROCESS_FLAG_FIELD(flag_carry, GB_CARRY_FLAG);
    PROCESS_FLAG_FIELD(flag_half_carry, GB_HALF_CARRY_FLAG);
    PROCESS_FLAG_FIELD(flag_zero, GB_ZERO_FLAG);
    PROCESS_FLAG_FIELD(flag_subtract, GB_SUBTRACT_FLAG);

    #undef PROCESS_FLAG_FIELD
}

void GameDebugger::refresh_registers() {
    auto &instance = this->get_instance();

    #define PROCESS_REGISTER_FIELD(name, field, fmt) {\
        char str[8]; \
        std::snprintf(str, sizeof(str), fmt, instance.get_register_value(sm83_register::SM83_REG_##name)); \
        this->field->blockSignals(true); \
        this->field->setText(str); \
        this->field->blockSignals(false); \
    }

    PROCESS_REGISTER_FIELD(AF, register_af, "$%04x");
    PROCESS_REGISTER_FIELD(BC, register_bc, "$%04x");
    PROCESS_REGISTER_FIELD(DE, register_de, "$%04x");
    PROCESS_REGISTER_FIELD(HL, register_hl, "$%04x");
    PROCESS_REGISTER_FIELD(SP, register_sp, "$%04x");
    PROCESS_REGISTER_FIELD(PC, register_pc, "$%04x");

    #undef PROCESS_REGISTER_FIELD
}

void GameDebugger::refresh_view() {
    // If we aren't visible, go away
    if(!this->isVisible()) {
        return;
    }
    
    auto &instance = this->get_instance();
    bool bp_pause = instance.is_paused_from_breakpoint();
    
    this->disassembler->refresh_view();
    this->breakpoints_copy = instance.get_breakpoints();
    this->clear_breakpoints_button->setEnabled(this->breakpoints_copy.size() > 0);
    this->backtrace_copy = instance.get_backtrace();
    
    if(!bp_pause || this->known_breakpoint != bp_pause) {
        this->refresh_registers();
        this->refresh_flags();
    
        int row = 0;
        this->backtrace->setRowCount(this->backtrace_copy.size());
        for(auto &l : this->backtrace_copy) {
            auto l_trimmed = QString::fromStdString(l.first).trimmed();
            if(l_trimmed.isEmpty()) {
                continue;
            }
            auto *item = new QTableWidgetItem(l_trimmed);
            item->setToolTip(l_trimmed);
            item->setFlags(item->flags() & ~Qt::ItemIsEditable);
            this->backtrace->setItem(row, 0, item);
            item->setData(Qt::UserRole, l.second);
            row++;
        }

        if(bp_pause) {
            auto bnt = instance.get_break_and_trace_results();
            if(!bnt.empty()) {
                instance.clear_break_and_trace_results();

                std::vector<ProcessedBNTResult> results;

                // Strip address pointer, newlines, and comment from instruction
                for(auto &b : bnt) {
                    auto &r = results.emplace_back();
                    static_cast<GameInstance::BreakAndTraceResult &>(r) = b;

                    auto d = QString(b.disassembly.c_str()).split("\n");
                    auto d_second_to_last = d.at(d.size() - 2);
                    d_second_to_last.replace("->", "");
                    d_second_to_last = d_second_to_last.mid(d_second_to_last.indexOf(":") + 1);
                    d_second_to_last = d_second_to_last.mid(0, d_second_to_last.indexOf(" ;")).trimmed();
                    r.instruction = d_second_to_last.toStdString();

                    auto comma_index = d_second_to_last.indexOf(",");

                    bool is_call = !r.step_over && d_second_to_last.startsWith("CALL "); // ignore calls if stepping over
                    bool is_call_conditional = is_call && comma_index > 0;

                    bool is_ret = d_second_to_last.startsWith("RET"); // never ignore returning
                    bool is_ret_conditional = is_ret && d_second_to_last.startsWith("RET ");

                    if(is_call || is_ret) {
                        if(is_call_conditional || is_ret_conditional) {
                            QString condition;
                            if(is_call) {
                                condition = d_second_to_last.mid(0, comma_index).replace("CALL ", "").toLower();
                            }
                            else if(is_ret) {
                                condition = d_second_to_last.replace("RET ", "").toLower();
                            }

                            auto condition_met = (condition == "z" && r.zero) &&
                                                 (condition == "nz" && !r.zero) &&
                                                 (condition == "c" && r.carry) &&
                                                 (condition == "nc" && !r.carry);
                            if(!condition_met) {
                                is_call = false;
                                is_ret = false;
                            }
                        }

                        if(is_call) {
                            r.direction = 1;
                        }
                        else if(is_ret) {
                            r.direction = -1;
                        }
                    }
                }

                // Turn it into a 2D thing
                ProcessedBNTResultNode::directory_t top_directory;
                std::vector<ProcessedBNTResultNode::directory_t *> trace = { &top_directory };

                for(auto &r : results) {
                    auto *current_directory = trace[trace.size() - 1];
                    auto &d = current_directory->emplace_back();
                    d.result = r;

                    if(r.direction == 1) {
                        trace.emplace_back(&d.children);
                    }
                    else if(r.direction == -1) {
                        trace.erase(trace.begin() + (trace.size() - 1));

                        // Make a new top directory if we returned to a higher level than we breakpoint
                        if(trace.size() == 0) {
                            top_directory = { top_directory };
                            trace = { &top_directory };
                        }
                    }
                }

                auto go_through_directory = [](const ProcessedBNTResultNode::directory_t &directory, auto &go_through_directory_r, std::size_t depth = 0) -> void {
                    for(auto &d : directory) {
                        for(std::size_t z = 0; z < depth; z++) {
                            std::printf("    ");
                        }
                        std::printf("($%04x) %s\n", d.result.pc, d.result.instruction.c_str());
                        go_through_directory_r(d.children, go_through_directory_r, depth + 1);
                    }
                };

                go_through_directory(top_directory, go_through_directory);
            }
        }
    }
    
    this->set_known_breakpoint(bp_pause);
}

void GameDebugger::action_clear_breakpoints() noexcept {
    this->get_instance().remove_all_breakpoints();
}

void GameDebugger::action_update_registers() noexcept {
    auto &instance = this->get_instance();
    
    if(!instance.is_paused_from_breakpoint() || this->right_view->isHidden()) {
        return;
    }

    #define PROCESS_REGISTER_FIELD(name, field) {\
        auto value = instance.evaluate_expression(this->field->text().toUtf8().data());\
        if(value.has_value()) {\
            instance.set_register_value(sm83_register::SM83_REG_##name, *value);\
        }\
    }

    PROCESS_REGISTER_FIELD(AF, register_af);
    PROCESS_REGISTER_FIELD(BC, register_bc);
    PROCESS_REGISTER_FIELD(DE, register_de);
    PROCESS_REGISTER_FIELD(HL, register_hl);
    PROCESS_REGISTER_FIELD(SP, register_sp);
    // PROCESS_REGISTER_FIELD(PC, register_pc);
    
    #undef PROCESS_REGISTER_FIELD

    this->refresh_flags();
}

void GameDebugger::closeEvent(QCloseEvent *) {
    this->disassembler->clear();
    this->backtrace->clear();
}

GameDebugger::~GameDebugger() {}

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

void GameDebugger::action_register_flag_state_changed(int) noexcept {
    auto &instance = this->get_instance();
    std::uint8_t f = instance.get_register_value(sm83_register::SM83_REG_F);
    f = f & ~(GB_CARRY_FLAG | GB_HALF_CARRY_FLAG | GB_ZERO_FLAG | GB_SUBTRACT_FLAG);

    #define SET_FLAG_IF_CHECKED(variable, flag) if(this->variable->isChecked()) { f = f | flag; }

    SET_FLAG_IF_CHECKED(flag_carry, GB_CARRY_FLAG);
    SET_FLAG_IF_CHECKED(flag_half_carry, GB_HALF_CARRY_FLAG);
    SET_FLAG_IF_CHECKED(flag_zero, GB_ZERO_FLAG);
    SET_FLAG_IF_CHECKED(flag_subtract, GB_SUBTRACT_FLAG);

    #undef SET_FLAG_IF_CHECKED
    instance.set_register_value(sm83_register::SM83_REG_F, f);

    this->refresh_registers();
}

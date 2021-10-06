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
            this->disassembler->go_to(this->get_instance().get_register_value(gbz80_register::GBZ80_REG_PC));
        }
    }
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
        #define PROCESS_REGISTER_FIELD(name, field, fmt) {\
            char str[8]; \
            std::snprintf(str, sizeof(str), fmt, instance.get_register_value(gbz80_register::GBZ80_REG_##name)); \
            this->field->blockSignals(true); \
            this->field->setText(str); \
            this->field->blockSignals(false); \
        }

        PROCESS_REGISTER_FIELD(A, register_a, "$%02x");
        PROCESS_REGISTER_FIELD(B, register_b, "$%02x");
        PROCESS_REGISTER_FIELD(C, register_c, "$%02x");
        PROCESS_REGISTER_FIELD(D, register_d, "$%02x");
        PROCESS_REGISTER_FIELD(E, register_e, "$%02x");
        PROCESS_REGISTER_FIELD(F, register_f, "$%02x");
        PROCESS_REGISTER_FIELD(HL, register_hl, "$%04x");
        PROCESS_REGISTER_FIELD(PC, register_pc, "$%04x");
        
        #undef PROCESS_REGISTER_FIELD
    
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
    }
    
    this->set_known_breakpoint(bp_pause);
}

void GameDebugger::action_clear_breakpoints() noexcept {
    this->get_instance().execute_command("delete");
}

void GameDebugger::action_update_registers() noexcept {
    auto &instance = this->get_instance();
    
    if(!instance.is_paused_from_breakpoint() || this->right_view->isHidden()) {
        return;
    }
    
    #define PROCESS_REGISTER_FIELD(name, field) {\
        auto value = instance.evaluate_expression(this->field->text().toUtf8().data());\
        if(value.has_value()) {\
            instance.set_register_value(gbz80_register::GBZ80_REG_##name, *value);\
        }\
    }

    PROCESS_REGISTER_FIELD(A, register_a);
    PROCESS_REGISTER_FIELD(B, register_b);
    PROCESS_REGISTER_FIELD(C, register_c);
    PROCESS_REGISTER_FIELD(D, register_d);
    PROCESS_REGISTER_FIELD(E, register_e);
    PROCESS_REGISTER_FIELD(F, register_f);
    PROCESS_REGISTER_FIELD(HL, register_hl);
    //PROCESS_REGISTER_FIELD(pc, register_pc); // changing this is very bad
    
    #undef PROCESS_REGISTER_FIELD
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

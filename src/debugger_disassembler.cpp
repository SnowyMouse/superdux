#include <QMenu>
#include <QAction>
#include <QInputDialog>
#include <QMessageBox>
#include <QWheelEvent>
#include <QHeaderView>
#include <QScrollBar>
#include <QKeyEvent>
#include <QApplication>
#include <QGridLayout>
#include <QPalette>
#include <QCheckBox>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>

#include "gb_proxy.h"
#include "debugger_disassembler.hpp"
#include "debugger.hpp"

static std::optional<std::uint16_t> evaluate_address_with_error_message(GameInstance &gb, const char *expression) {
    auto rval = gb.evaluate_expression(expression);
    if(!rval.has_value()) {
        char error_message[512];
        std::snprintf(error_message, sizeof(error_message), "An invalid expression `%s` was given. Check your input and try again.", expression);
        QMessageBox(QMessageBox::Icon::Critical, "Invalid Expression", error_message, QMessageBox::StandardButton::Ok).exec();
    }
    return rval;
}

DebuggerDisassembler::DebuggerDisassembler(Debugger *parent) : QTableWidget(parent), debugger(parent) {
    this->setColumnCount(1);
    this->debugger->format_table(this);
    this->setSelectionMode(QAbstractItemView::NoSelection);
    this->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(this, &DebuggerDisassembler::customContextMenuRequested, this, &DebuggerDisassembler::show_context_menu);
    history.reserve(256);
    
    auto palette = QApplication::palette();
    
    this->text_default_color = palette.color(QPalette::ColorRole::Text);
    this->bg_default_color = palette.color(QPalette::ColorRole::Base);
    
    this->text_highlight_color = palette.color(QPalette::ColorRole::HighlightedText);
    this->bg_highlight_color = palette.color(QPalette::ColorRole::Highlight);

    this->setMinimumHeight(400);
    this->setMinimumWidth(400);
}

DebuggerDisassembler::~DebuggerDisassembler() {}

void DebuggerDisassembler::go_to(std::uint16_t where) {
    this->history.emplace_back(this->current_address);
    this->clearSelection();
    this->current_address = where;
    this->refresh_view();
}

void DebuggerDisassembler::go_back() {
    auto where = this->history.size() - 1;
    this->current_address = this->history[where];
    this->clearSelection();
    this->history.resize(where);
    this->refresh_view();
}

void DebuggerDisassembler::follow_address() {
    auto where = evaluate_address_with_error_message(this->debugger->get_instance(), this->last_disassembly->follow_address.toUtf8().data());
    if(where.has_value()) {
        this->go_to(*where);
    }
}

void DebuggerDisassembler::set_address_to_current_breakpoint() {
    this->current_address = this->debugger->get_instance().get_register_value(GameInstance::SM83Register::SM83_REG_PC);
}

void DebuggerDisassembler::add_breakpoint() {
    this->debugger->get_instance().break_at(*this->last_disassembly->address);
}

void DebuggerDisassembler::add_break_and_trace_breakpoint() {
    QDialog dialog;
    dialog.setWindowTitle("Break and Trace");
    dialog.setFixedWidth(500);

    // Set up the UI
    auto *layout = new QVBoxLayout(&dialog);
    dialog.setLayout(layout);

    auto *input_grid_w = new QWidget(&dialog);
    auto *input_grid = new QGridLayout(input_grid_w);
    input_grid->setContentsMargins(0,0,0,0);
    input_grid_w->setLayout(input_grid);

    char address_test[6];
    std::snprintf(address_test, sizeof(address_test), "$%04x", *this->last_disassembly->address);

    auto *address = new QLineEdit(input_grid_w);
    address->setText(address_test);

    input_grid->addWidget(new QLabel("Address:", input_grid_w), 0, 0);
    input_grid->addWidget(address, 0, 1);

    input_grid->addWidget(new QLabel("Count:", input_grid_w), 1, 0);
    auto *amt = new QLineEdit(input_grid_w);
    amt->setText("50");
    input_grid->addWidget(amt, 1, 1);

    input_grid->addWidget(new QLabel("Step Over:", input_grid_w), 2, 0);
    auto *step_over = new QCheckBox(input_grid_w);
    step_over->setMinimumHeight(amt->sizeHint().height());
    input_grid->addWidget(step_over, 2, 1);

    input_grid->addWidget(new QLabel("Break When Done:", input_grid_w), 3, 0);
    auto *break_when_done = new QCheckBox(input_grid_w);
    break_when_done->setMinimumHeight(amt->sizeHint().height());
    input_grid->addWidget(break_when_done, 3, 1);

    layout->addWidget(input_grid_w);

    auto *ok_button_row = new QWidget(&dialog);
    layout->addWidget(ok_button_row);
    auto *ok_button_row_l = new QHBoxLayout(ok_button_row);
    ok_button_row_l->setContentsMargins(0,0,0,0);
    ok_button_row->setLayout(ok_button_row_l);
    ok_button_row_l->addStretch(1);

    auto *ok_button = new QPushButton("OK", ok_button_row);
    ok_button_row_l->addWidget(ok_button);

    // Select all the text in address before we begin
    address->selectAll();

    // Pressing enter on any the controls will accept the dialog
    connect(ok_button, &QPushButton::clicked, &dialog, &QDialog::accept);
    connect(address, &QLineEdit::returnPressed, &dialog, &QDialog::accept);
    connect(amt, &QLineEdit::returnPressed, &dialog, &QDialog::accept);

    // Now... let's get down to business. Keep asking for something until the user enters something valid or gives up.
    while(true) {
        if(dialog.exec() == QDialog::Accepted) {
            auto address_maybe = evaluate_address_with_error_message(this->debugger->get_instance(), address->text().toUtf8().data());
            auto count_maybe = evaluate_address_with_error_message(this->debugger->get_instance(), amt->text().toUtf8().data());

            if(!address_maybe.has_value() || !count_maybe.has_value()) {
                continue;
            }

            this->debugger->get_instance().break_and_trace_at(*address_maybe, *count_maybe, step_over->isChecked(), break_when_done->isChecked());
        }
        break;
    }
}

void DebuggerDisassembler::delete_breakpoint() {
    this->debugger->get_instance().remove_breakpoint(*this->last_disassembly->address);
}

void DebuggerDisassembler::show_context_menu(const QPoint &point) {
    this->last_disassembly = std::nullopt;
    
    // Make a right-click menu
    QMenu menu;
    auto *jump_to_option = menu.addAction("Jump to address...");
    connect(jump_to_option, &QAction::triggered, this, &DebuggerDisassembler::jump_to_address_window);
    
    // If we can go back/forward, add those options
    if(this->history.size()) {
        char text_format[64];
        std::uint16_t back = this->history[this->history.size() - 1];
        std::snprintf(text_format, sizeof(text_format), "Go back to $%04x", back);
        auto *go_back_option = menu.addAction(text_format);
        connect(go_back_option, &QAction::triggered, this, &DebuggerDisassembler::go_back);
    }
    
    // Get the item at the point
    auto *item = this->itemAt(point);
    if(item) {
        auto index = item->data(Qt::UserRole).toUInt();
        if(index <= this->disassembly.size()) {
            this->last_disassembly = this->disassembly[index];
            
            // Add a follow option if possible
            auto &follow_address = this->last_disassembly->follow_address;
            if(follow_address.length() > 0) {
                auto *jump_to_option = menu.addAction(QString("Follow to ") + follow_address);
                connect(jump_to_option, &QAction::triggered, this, &DebuggerDisassembler::follow_address);
            }
            
            menu.addSeparator();

            auto &address = this->last_disassembly->address;
            if(address.has_value()) {
                char breakpoint_text[512];
                bool create = !address_is_breakpoint(*address);
                
                std::snprintf(breakpoint_text, sizeof(breakpoint_text), "%s breakpoint at $%04X", create ? "Set" : "Unset", *address);
                auto *set_breakpoint = menu.addAction(breakpoint_text);
                connect(set_breakpoint, &QAction::triggered, this, create ? &DebuggerDisassembler::add_breakpoint : &DebuggerDisassembler::delete_breakpoint);

                if(create) {
                    std::snprintf(breakpoint_text, sizeof(breakpoint_text), "Break-and-trace at $%04X", *address);
                    auto *set_bnt_breakpoint = menu.addAction(breakpoint_text);
                    connect(set_bnt_breakpoint, &QAction::triggered, this, &DebuggerDisassembler::add_break_and_trace_breakpoint);
                }
            }
        }
    }
    
    menu.exec(this->mapToGlobal(point));
}

bool DebuggerDisassembler::address_is_breakpoint(std::uint16_t address) {
    for(auto &i : this->debugger->get_breakpoints()) {
        if(i == address) {
            return true;
        }
    }
    
    return false;
}

void DebuggerDisassembler::jump_to_address_window() {
    QInputDialog dialog;
    dialog.setLabelText("Enter an address or expression to go to...");
    dialog.setWindowTitle("Enter an expression");
    
    // Ask for the address
    if(this->last_disassembly.has_value()) {
        if(this->last_disassembly->address.has_value()) {
            char text_value[6];
            std::snprintf(text_value, sizeof(text_value), "$%04x", *this->last_disassembly->address);
            dialog.setTextValue(text_value);
        }
        else if(this->last_disassembly->is_marker) {
            dialog.setTextValue(this->last_disassembly->instruction);
        }
    }
    
    // Go to the address
    if(dialog.exec() == QInputDialog::Accepted) {
        auto where_to = evaluate_address_with_error_message(this->debugger->get_instance(), dialog.textValue().toUtf8().data());
        if(where_to.has_value()) {
            this->go_to(*where_to);
        }
    }
}

void DebuggerDisassembler::wheelEvent(QWheelEvent *event) {
    int d = event->angleDelta().y();
    if(d > 0) {
        this->current_address -= std::min(static_cast<std::uint16_t>(1), this->current_address);
    }
    else if(d < 0) {
        this->current_address = this->next_address_medium;
    }
    
    this->refresh_view();
    event->ignore();
}

void DebuggerDisassembler::keyPressEvent(QKeyEvent *event) {
    switch(event->key()) {
        case Qt::Key::Key_PageDown:
            this->current_address = next_address_far;
            break;
        case Qt::Key::Key_PageUp:
            this->current_address -= std::min(static_cast<std::uint16_t>(10), this->current_address);
            break;
        case Qt::Key::Key_Up:
        case Qt::Key::Key_Left:
            this->current_address -= std::min(static_cast<std::uint16_t>(1), this->current_address);
            break;
        case Qt::Key::Key_Right:
            this->current_address = std::min(static_cast<std::uint32_t>(65535), static_cast<std::uint32_t>(this->current_address + 1));
            break;
        case Qt::Key::Key_Down:
            this->current_address = next_address_short;
            break;
        default:
            return;
    }
    
    this->refresh_view();
    event->ignore();
}

void DebuggerDisassembler::refresh_view() {
    // Clear the table if we changed addresses
    if(this->current_address != this->last_address) {
        this->last_address = this->current_address;
    }
    
    // Disassemble based on the number of rows that are visible (plus one in case there is a partial row on the bottom)
    auto query_rows = static_cast<std::uint8_t>(std::min(255, this->height() / this->debugger->get_table_font().pixelSize() + 1));
    this->setRowCount(query_rows);
    this->disassembly = this->disassemble_at_address(this->current_address, query_rows);
    this->next_address_short = this->current_address;
    this->next_address_medium = this->current_address;
    this->next_address_far = this->current_address;
    
    // Calculate how far to scroll down if we scroll down
    int next_addresses_found = 0;
    for(auto &i : this->disassembly) {
        if(i.address.has_value() && *i.address > this->current_address) {
            next_addresses_found++;
            
            if(next_addresses_found <= 1) {
                this->next_address_short = *i.address;
            }
            if(next_addresses_found <= 2) {
                this->next_address_medium = *i.address;
            }
            if(next_addresses_found <= 10) {
                this->next_address_far = *i.address;
            }
        }
    }
    
    // If anything has a breakpoint set, highlight it
    auto disassembly_count = this->disassembly.size();
    for(std::size_t row = 0; row < disassembly_count && row < query_rows; row++) {
        auto &d = this->disassembly[row];
        bool is_breakpoint = d.address.has_value() ? this->address_is_breakpoint(*d.address) : false;
        
        auto *item = new QTableWidgetItem(d.raw_result);
        item->setData(Qt::UserRole, static_cast<unsigned int>(row));
        item->setFlags(item->flags() & ~Qt::ItemIsEditable);
        
        if(is_breakpoint) {
            item->setForeground(this->text_highlight_color);
            item->setBackground(this->bg_highlight_color);
        }
        else {
            item->setForeground(this->text_default_color);
            item->setBackground(this->bg_default_color);
        }
        
        this->setItem(row, 0, item);
    }
}

std::vector<DebuggerDisassembler::Disassembly> DebuggerDisassembler::disassemble_at_address(std::uint16_t address, std::uint8_t count) {
    // Tell sameboy to disassemble at the address and capture its output.
    // Doing it this way is horrible. Let's do it anyway.
    QStringList lines;
    
    lines = QString::fromStdString(this->debugger->get_instance().disassemble_address(address, count)).split("\n");
    
    std::vector<Disassembly> returned_instructions;
    for(auto &l : lines) {
        int colon_offset = l.indexOf(':');
        if(colon_offset == -1) {
            continue;
        }
        
        auto &instruction = returned_instructions.emplace_back();
        
        if(l[0] == ' ') {
            instruction.address = l.mid(4, 4).toUInt(nullptr, 16);
            instruction.current_location = l[0] == '-';
            
            int semicolon_offset = l.indexOf(';', 8);
            if(semicolon_offset >= 0) {
                instruction.comment = l.mid(semicolon_offset).trimmed();
                instruction.instruction = l.mid(colon_offset + 1, semicolon_offset - colon_offset - 1).trimmed();
            }
            else {
                instruction.instruction = l.mid(colon_offset + 1).trimmed();
            }
            
            // Handle following
            auto add_follow = [&instruction](const QString &prefix, bool use_after_comma) {
                auto &instruction_str = instruction.instruction;
                if(instruction_str.startsWith(prefix)) {
                    int start = prefix.length();
                    int ending = instruction_str.indexOf(',');
                    if(ending != -1) {
                        if(use_after_comma) {
                            start = ending + 1;
                            ending = -1;
                        }
                        else {
                            ending = instruction_str.length() - ending - prefix.length();
                        }
                    }
                    instruction.follow_address = instruction.instruction.mid(start, ending).trimmed();
                    return true;
                }
                return false;
            };
            
            add_follow("CALL ", true) || add_follow("RST ", false) || add_follow("JP ", true) || add_follow("JR ", true);
        }
        else {
            instruction.is_marker = true;
            instruction.instruction = l.mid(0, colon_offset);
        }
        
        instruction.raw_result = l;
    }
    
    return returned_instructions;
}

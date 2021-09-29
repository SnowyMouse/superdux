#include <QMenu>
#include <QAction>
#include <QInputDialog>
#include <QMessageBox>
#include <QWheelEvent>
#include <QHeaderView>
#include <QScrollBar>
#include <QKeyEvent>
#include <QFontDatabase>

#include "game_disassembler.hpp"
#include "game_debugger.hpp"

GameDisassembler::GameDisassembler(GameDebugger *parent) : QTableWidget(parent), debugger(parent) {
    this->text_font = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    this->text_font.setPixelSize(14);
    
    this->text_font_bold = this->text_font;
    this->text_font_bold.setBold(true);
    this->text_font_bold.setWeight(700);
    
    this->setColumnCount(1);
    this->horizontalHeader()->setStretchLastSection(true);
    this->horizontalHeader()->hide();
    this->horizontalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    this->verticalHeader()->setMaximumSectionSize(this->text_font.pixelSize() + 4);
    this->verticalHeader()->setMinimumSectionSize(this->text_font.pixelSize() + 4);
    this->verticalHeader()->setDefaultSectionSize(this->text_font.pixelSize() + 4);
    this->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    this->verticalHeader()->hide();
    this->setSelectionMode(QAbstractItemView::SingleSelection);
    this->verticalScrollBar()->hide();
    this->setAlternatingRowColors(true);
    this->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    this->setShowGrid(false);
    this->setFont(this->text_font);
    
    this->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(this, &GameDisassembler::customContextMenuRequested, this, &GameDisassembler::show_context_menu);
    history.reserve(256);
}

GameDisassembler::~GameDisassembler() {}

std::optional<std::uint16_t> GameDisassembler::evaluate_expression(const char *expression) {
    std::optional<std::uint16_t> result;
    std::uint16_t result_maybe;
    
    this->debugger->push_retain_logs();
    
    if(GB_debugger_evaluate(this->debugger->gameboy, expression, &result_maybe, nullptr)) {
        auto &logs = this->debugger->retained_logs;
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
    this->debugger->pop_retain_logs();
    
    return result;
}

void GameDisassembler::go_to(std::uint16_t where) {
    this->history.emplace_back(this->current_address);
    this->clearSelection();
    this->current_address = where;
    this->refresh_view();
}

void GameDisassembler::go_back() {
    auto where = this->history.size() - 1;
    this->current_address = this->history[where];
    this->clearSelection();
    this->history.resize(where);
    this->refresh_view();
}

void GameDisassembler::follow_address() {
    auto where = this->evaluate_expression(this->last_disassembly->follow_address.toUtf8().data());
    if(where.has_value()) {
        this->go_to(*where);
    }
}

void GameDisassembler::set_address_to_current_breakpoint() {
    std::uint16_t first_address;
    if(this->disassemble_at_address(std::nullopt, 5, first_address).size() > 0) {
        this->current_address = first_address;
    }
}

void GameDisassembler::add_breakpoint() {
    char command[512];
    std::snprintf(command, sizeof(command), "breakpoint $%04X", *this->last_disassembly->address);
    GB_debugger_execute_command(this->debugger->gameboy, command);
}

void GameDisassembler::show_context_menu(const QPoint &point) {
    this->last_disassembly = std::nullopt;
    
    // Make a right-click menu
    QMenu menu;
    auto *jump_to_option = menu.addAction("Jump to address...");
    connect(jump_to_option, &QAction::triggered, this, &GameDisassembler::jump_to_address_window);
    
    // If we can go back/forward, add those options
    if(this->history.size()) {
        char text_format[64];
        std::uint16_t back = this->history[this->history.size() - 1];
        std::snprintf(text_format, sizeof(text_format), "Go back to $%04x", back);
        auto *go_back_option = menu.addAction(text_format);
        connect(go_back_option, &QAction::triggered, this, &GameDisassembler::go_back);
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
                connect(jump_to_option, &QAction::triggered, this, &GameDisassembler::follow_address);
            }
            
            menu.addSeparator();
            
            // TODO: add a delete breakpoint maybe?
            auto &address = this->last_disassembly->address;
            if(address.has_value()) {
                char breakpoint_text[512];
                std::snprintf(breakpoint_text, sizeof(breakpoint_text), "Create breakpoint at $%04X", *address);
                auto *create_breakpoint = menu.addAction(breakpoint_text);
                connect(create_breakpoint, &QAction::triggered, this, &GameDisassembler::add_breakpoint);
            }
        }
    }
    
    menu.exec(this->mapToGlobal(point));
}

void GameDisassembler::jump_to_address_window() {
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
        auto where_to = this->evaluate_expression(dialog.textValue().toUtf8().data());
        if(where_to.has_value()) {
            this->go_to(*where_to);
        }
    }
}

void GameDisassembler::wheelEvent(QWheelEvent *event) {
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

void GameDisassembler::keyPressEvent(QKeyEvent *event) {
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

void GameDisassembler::refresh_view() {
    // Clear the table if we changed addresses
    if(this->current_address != this->last_address) {
        this->last_address = this->current_address;
        this->clear();
    }
    
    auto query_rows = static_cast<std::uint8_t>(std::min(255, this->height() / this->text_font.pixelSize() + 1));
    this->setRowCount(query_rows);
    std::uint16_t first_address;
    this->disassembly = this->disassemble_at_address(this->current_address, query_rows, first_address);
    this->next_address_short = this->current_address;
    this->next_address_medium = this->current_address;
    this->next_address_far = this->current_address;
    
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
    
    auto disassembly_count = this->disassembly.size();
    for(std::size_t row = 0; row < disassembly_count; row++) {
        auto &d = this->disassembly[row];
        auto *item = new QTableWidgetItem(d.raw_result);
        if(d.is_marker) {
            item->setFont(this->text_font_bold);
        }
        else {
            item->setFont(this->text_font);
        }
        item->setData(Qt::UserRole, static_cast<unsigned int>(row));
        item->setFlags(item->flags() & ~Qt::ItemIsEditable);
        this->setItem(row, 0, item);
    }
    this->setRowCount(disassembly.size());
}

std::vector<GameDisassembler::Disassembly> GameDisassembler::disassemble_at_address(std::optional<std::uint16_t> address, std::uint8_t count, std::uint16_t &first_address) {
    if(!this->debugger->gameboy) {
        return {};
    }
    
    // Tell sameboy to disassemble at the address and capture its output.
    // Doing it this way is horrible. Let's do it anyway.
    this->debugger->push_retain_logs();
    
    if(address.has_value()) {
        GB_cpu_disassemble(this->debugger->gameboy, *address, count);
    }
    else {
        char *cmd = nullptr;
        asprintf(&cmd, "disassemble");
        GB_debugger_execute_command(this->debugger->gameboy, cmd);
    }
        
    auto lines = QString::fromStdString(this->debugger->retained_logs).split("\n");
    this->debugger->retained_logs.clear();
    
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
            
            add_follow("CALL ", false) || add_follow("RST ", false) || add_follow("JP ", true) || add_follow("JR ", true);
        }
        else {
            instruction.is_marker = true;
            instruction.instruction = l.mid(0, colon_offset);
        }
        
        instruction.raw_result = l;
    }
    
    for(auto &f : returned_instructions) {
        if(f.address.has_value()) {
            first_address = *f.address;
            break;
        }
    }
    
    this->debugger->pop_retain_logs();
    
    return returned_instructions;
}

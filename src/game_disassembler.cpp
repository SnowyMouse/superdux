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
    this->setColumnCount(1);
    this->horizontalHeader()->setStretchLastSection(true);
    this->horizontalHeader()->hide();
    this->verticalHeader()->hide();
    this->setSelectionMode(QAbstractItemView::SingleSelection);
    this->verticalScrollBar()->hide();
    this->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto font = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    this->setFont(font);
    
    this->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(this, &GameDisassembler::customContextMenuRequested, this, &GameDisassembler::show_context_menu);
    history.reserve(256);
}

GameDisassembler::~GameDisassembler() {}

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
        }
    }
    
    menu.exec(this->mapToGlobal(point));
}

void GameDisassembler::jump_to_address_window() {
    QInputDialog dialog;
    dialog.setLabelText("Enter an address to go to...");
    
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
    
    if(dialog.exec() == QInputDialog::Accepted) {
        auto where_to = dialog.textValue();
        std::uint16_t result;
        std::uint16_t bank;
        
        this->debugger->retain_logs = true;
        if(GB_debugger_evaluate(this->debugger->gameboy, where_to.toUtf8().data(), &result, nullptr)) {
            auto &logs = this->debugger->retained_logs;
            if(!logs.empty()) {
                QMessageBox qmb(QMessageBox::Icon::Critical, "Error", logs.c_str());
                logs.clear();
            }
            else {
                QMessageBox qmb(QMessageBox::Icon::Critical, "Error", QString("Could not go to address ") + where_to);
            }
        }
        else {
            this->go_to(result);
        }
        this->debugger->retain_logs = false;
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
    
    this->setRowCount(127);
    this->disassembly = this->disassemble_at_address(this->current_address, this->rowCount());
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
        item->setData(Qt::UserRole, static_cast<unsigned int>(row));
        item->setFlags(item->flags() & ~Qt::ItemIsEditable);
        this->setItem(row, 0, item);
    }
    this->setRowCount(disassembly.size());
}

std::vector<GameDisassembler::Disassembly> GameDisassembler::disassemble_at_address(std::uint16_t address, std::uint8_t count) {
    if(!this->debugger->gameboy) {
        return {};
    }
    
    // Tell sameboy to disassemble at the address and capture its output.
    // Doing it this way is horrible. Let's do it anyway.
    this->debugger->retain_logs = true;
    GB_cpu_disassemble(this->debugger->gameboy, address, count);
    auto lines = QString::fromStdString(this->debugger->retained_logs).split("\n");
    this->debugger->retained_logs.clear();
    
    std::vector<Disassembly> returned_instructions;
    for(auto &l : lines) {
        int colon_offset = l.indexOf(':');
        if(colon_offset == -1) {
            continue;
        }
        
        auto &instruction = returned_instructions.emplace_back();
        if(l[0] == ' ' || l[0] == '-') {
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
        }
        else {
            instruction.is_marker = true;
            instruction.instruction = l.mid(0, colon_offset);
        }
        
        instruction.raw_result = l;
    }
    
    this->debugger->retain_logs = false;
    
    return returned_instructions;
}
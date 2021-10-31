#include "debugger_break_and_trace_results_dialog.hpp"
#include "game_disassembler.hpp"

#include <QVBoxLayout>
#include <QLabel>
#include <QFontDatabase>
#include <QTreeWidget>
#include <QTreeWidgetItem>

GameDebugger::BreakAndTraceResultsDialog::BreakAndTraceResultsDialog(QWidget *parent, GameDebugger *window, ProcessedBNTResultNode::directory_t &&results) : QDialog(parent), window(window), results(results) {
    this->setWindowTitle("Break and Trace Results");

    auto *layout = new QVBoxLayout(this);
    auto *inner_widget = new QWidget(this);

    // Make our inner widget (shows register information and stuff)
    auto *inner_layout = new QHBoxLayout(inner_widget);
    inner_layout->setContentsMargins(0,0,0,0);

    // Make the tree view
    auto fixed_font = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    auto *table_view = new QTreeWidget(inner_widget);
    table_view->setAnimated(false);
    table_view->setAlternatingRowColors(true);
    table_view->setHeaderHidden(true);
    auto add_directory = [&table_view, &fixed_font](const ProcessedBNTResultNode::directory_t &dir, QTreeWidgetItem *parent, auto &add_directory) -> void {
        for(auto &i : dir) {
            char text[256];
            std::snprintf(text, sizeof(text), "$%04x - %s", i.result.pc, i.result.instruction.c_str());

            // Make our widget item
            QTreeWidgetItem *new_item;
            if(parent) {
                new_item = new QTreeWidgetItem(parent);
                parent->addChild(new_item);
            }
            else {
                new_item = new QTreeWidgetItem(table_view);
                table_view->addTopLevelItem(new_item);
            }
            new_item->setText(0, text);
            new_item->setFont(0, fixed_font);
            new_item->setData(0, Qt::UserRole, QVariant::fromValue(reinterpret_cast<std::uintptr_t>(&i)));

            add_directory(i.children, new_item, add_directory);
        }
    };
    connect(table_view, &QTreeWidget::itemDoubleClicked, this, &BreakAndTraceResultsDialog::double_clicked_item);
    connect(table_view, &QTreeWidget::currentItemChanged, this, &BreakAndTraceResultsDialog::show_info_for_register);

    // Populate it and add it
    table_view->setMinimumWidth(500);
    table_view->setMinimumHeight(400);
    add_directory(this->results, nullptr, add_directory);
    inner_layout->addWidget(table_view);

    // Add register info here
    this->register_info = new QLabel(inner_widget);
    this->register_info->setFont(fixed_font);
    this->register_info->setAlignment(Qt::AlignmentFlag::AlignTop | Qt::AlignmentFlag::AlignLeft);
    this->show_info_for_register(nullptr, nullptr);
    inner_layout->addWidget(this->register_info);

    // Add the thing
    inner_widget->setLayout(inner_layout);
    layout->addWidget(inner_widget);

    this->setLayout(layout);
}

void GameDebugger::BreakAndTraceResultsDialog::double_clicked_item(QTreeWidgetItem *item, int column) {
    if(item) {
        this->window->disassembler->go_to(reinterpret_cast<const GameDebugger::ProcessedBNTResultNode *>(item->data(0, Qt::UserRole).value<std::uintptr_t>())->result.pc);
    }
}

void GameDebugger::BreakAndTraceResultsDialog::show_info_for_register(QTreeWidgetItem *current, QTreeWidgetItem *) {
    std::uint16_t af = 0, hl = 0, bc = 0, sp = 0, de = 0, pc = 0;

    if(current) {
        auto &data = reinterpret_cast<const GameDebugger::ProcessedBNTResultNode *>(current->data(0, Qt::UserRole).value<std::uintptr_t>())->result;
        af = data.a << 8 | data.f;
        hl = data.hl;
        bc = data.b << 8 | data.c;
        sp = data.sp;
        de = data.d << 8 | data.e;
        pc = data.pc;
    }

    // Read the flags (binary AND)
    char flags[5] = "____";

    if(af & GB_CARRY_FLAG) {
        flags[0] = 'C';
    }

    if(af & GB_HALF_CARRY_FLAG) {
        flags[1] = 'H';
    }

    if(af & GB_SUBTRACT_FLAG) {
        flags[2] = 'N';
    }

    if(af & GB_ZERO_FLAG) {
        flags[3] = 'Z';
    }

    // Show the text
    char text[512];
    std::snprintf(text, sizeof(text), "Registers:\n\nAF: $%04x, HL: $%04x,\nBC: $%04x, SP: $%04x,\nDE: $%04x, PC: $%04x\n\nFlags: %s", af, hl, bc, sp, de, pc, flags);
    this->register_info->setText(text);
}

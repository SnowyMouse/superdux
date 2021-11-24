#include "debugger_break_and_trace_results_dialog.hpp"
#include "debugger_disassembler.hpp"

#include <QVBoxLayout>
#include <QLabel>
#include <QFontDatabase>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QPushButton>
#include <QFileDialog>
#include <QMessageBox>

Debugger::BreakAndTraceResultsDialog::BreakAndTraceResultsDialog(QWidget *parent, Debugger *window, ProcessedBNTResultNode::directory_t &&results) : QDialog(parent), window(window), results(results) {
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
    auto *right_widget = new QWidget(inner_widget);
    auto *right_layout = new QVBoxLayout(right_widget);
    right_layout->setContentsMargins(0,0,0,0);
    right_widget->setLayout(right_layout);

    this->register_info = new QLabel(right_widget);
    this->register_info->setFont(fixed_font);
    this->register_info->setAlignment(Qt::AlignmentFlag::AlignTop | Qt::AlignmentFlag::AlignLeft);
    this->show_info_for_register(nullptr, nullptr);
    right_layout->addWidget(this->register_info);

    auto *export_results_button = new QPushButton("Export to CSV...", right_widget);
    connect(export_results_button, &QPushButton::clicked, this, &BreakAndTraceResultsDialog::export_results);
    right_layout->addWidget(export_results_button);
    inner_layout->addWidget(right_widget);

    // Add the thing
    inner_widget->setLayout(inner_layout);
    layout->addWidget(inner_widget);

    this->setLayout(layout);
}

void Debugger::BreakAndTraceResultsDialog::double_clicked_item(QTreeWidgetItem *item, int column) {
    if(item) {
        this->window->disassembler->go_to(reinterpret_cast<const Debugger::ProcessedBNTResultNode *>(item->data(0, Qt::UserRole).value<std::uintptr_t>())->result.pc);
    }
}

void Debugger::BreakAndTraceResultsDialog::show_info_for_register(QTreeWidgetItem *current, QTreeWidgetItem *) {
    std::uint16_t af = 0, hl = 0, bc = 0, sp = 0, de = 0, pc = 0;

    if(current) {
        auto &data = reinterpret_cast<const Debugger::ProcessedBNTResultNode *>(current->data(0, Qt::UserRole).value<std::uintptr_t>())->result;
        af = data.a << 8 | data.f;
        hl = data.h << 8 | data.l;
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

void Debugger::BreakAndTraceResultsDialog::export_results() {
    QFileDialog file_dialog;
    file_dialog.setFileMode(QFileDialog::FileMode::AnyFile);
    file_dialog.setNameFilters(QStringList { "Comma-Separated Values (*.csv)" });
    file_dialog.setWindowTitle("Save a CSV");
    file_dialog.setDefaultSuffix(".csv");
    file_dialog.setAcceptMode(QFileDialog::AcceptSave);

    if(file_dialog.exec() == QFileDialog::Accepted) {
        auto output = std::filesystem::path(file_dialog.selectedFiles()[0].toStdString());

        // Open for writing
        FILE *f = std::fopen(output.string().c_str(), "w");
        if(!f) {
            QMessageBox(QMessageBox::Icon::Critical, "Failed to save", QString("Failed to open ") + output.string().c_str() + " for writing.").exec();
            return;
        }

        // Print the header
        std::fprintf(f, "depth,instruction,af,bc,de,hl,sp,pc,carry,halfcarry,subtract,zero\n");

        // Let's do this
        auto export_directory = [&f](const ProcessedBNTResultNode::directory_t &directory, int depth, auto &export_directory) -> void {
            auto directory_size = directory.size();
            auto d = directory.begin();
            for(std::size_t i = 0; i < directory_size; i++, d++) {
                auto af = d->result.a << 8 | d->result.f;
                auto bc = d->result.b << 8 | d->result.c;
                auto de = d->result.d << 8 | d->result.e;
                auto hl = d->result.h << 8 | d->result.l;

                std::fprintf(f, "%i,\"%s\",$%04x,$%04x,$%04x,$%04x,$%04x,$%04x,%i,%i,%i,%i\n",
                                depth,
                                d->result.instruction.c_str(),
                                af,
                                bc,
                                de,
                                hl,
                                d->result.sp,
                                d->result.pc,
                                (d->result.f & GB_CARRY_FLAG) != 0,
                                (d->result.f & GB_HALF_CARRY_FLAG) != 0,
                                (d->result.f & GB_SUBTRACT_FLAG) != 0,
                                (d->result.f & GB_ZERO_FLAG) != 0);

                if(!d->children.empty()) {
                    export_directory(d->children, depth + 1, export_directory);
                }
            }
        };
        export_directory(this->results, 0, export_directory);

        // Done!
        std::fclose(f);
    }
}

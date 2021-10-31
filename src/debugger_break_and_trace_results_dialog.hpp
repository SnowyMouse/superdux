#ifndef BREAK_AND_TRACE_RESULTS_DIALOG_HPP
#define BREAK_AND_TRACE_RESULTS_DIALOG_HPP

#include <QDialog>

#include "game_debugger.hpp"

class QTreeWidgetItem;
class QLabel;

class GameDebugger::BreakAndTraceResultsDialog : public QDialog {
public:
    BreakAndTraceResultsDialog(QWidget *parent, GameDebugger *window, ProcessedBNTResultNode::directory_t &&results);

private:
    ProcessedBNTResultNode::directory_t results;
    QLabel *register_info;
    GameDebugger *window;

    void double_clicked_item(QTreeWidgetItem *item, int column);
    void show_info_for_register(QTreeWidgetItem *current, QTreeWidgetItem *);
};

#endif

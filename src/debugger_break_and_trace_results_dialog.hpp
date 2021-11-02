#ifndef BREAK_AND_TRACE_RESULTS_DIALOG_HPP
#define BREAK_AND_TRACE_RESULTS_DIALOG_HPP

#include <QDialog>

#include "debugger.hpp"

class QTreeWidgetItem;
class QLabel;

class Debugger::BreakAndTraceResultsDialog : public QDialog {
public:
    BreakAndTraceResultsDialog(QWidget *parent, Debugger *window, ProcessedBNTResultNode::directory_t &&results);

private:
    ProcessedBNTResultNode::directory_t results;
    QLabel *register_info;
    Debugger *window;

    void double_clicked_item(QTreeWidgetItem *item, int column);
    void show_info_for_register(QTreeWidgetItem *current, QTreeWidgetItem *);
};

#endif

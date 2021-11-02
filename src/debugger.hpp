#ifndef DEBUGGER_HPP
#define DEBUGGER_HPP

extern "C" {
#include <Core/gb.h>
}

#include <QMainWindow>

#include <vector>
#include <optional>

#include "game_window.hpp"

class DebuggerDisassembler;
class QLineEdit;
class GameWindow;
class QTableWidget;
class QCheckBox;

class Debugger : public QMainWindow {
    Q_OBJECT
    
public:
    struct ProcessedBNTResult : GameInstance::BreakAndTraceResult {
        std::string instruction;

        char direction = 0; // -1 = return; 1 = call; 0 = next instruction
    };

    struct ProcessedBNTResultNode {
        using directory_t = std::list<ProcessedBNTResultNode>;

        ProcessedBNTResult result;
        directory_t children;
    };

    Debugger(GameWindow *window);
    ~Debugger() override;
    
    GameInstance &get_instance() noexcept {
        return this->game_window->get_instance();
    }
    
    /** Format the table for use with the debugger */
    void format_table(QTableWidget *widget);
    
    /** Get the font used for tables */
    const QFont &get_table_font() const noexcept {
        return this->table_font;
    }
    
    /** Get all breakpoints */
    const std::vector<std::uint16_t> get_breakpoints() {
        return this->breakpoints_copy;
    }
    
    /** Refresh the information in view */
    void refresh_view();
private:
    DebuggerDisassembler *disassembler;
    
    class BacktraceTable;
    class BreakAndTraceResultsDialog;
    
    // Copy of breakpoints and backtrace
    std::vector<std::pair<std::string, std::uint16_t>> backtrace_copy;
    std::vector<std::uint16_t> breakpoints_copy;
    
    // Did we check if breakpoint
    bool known_breakpoint = false;
    void set_known_breakpoint(bool known_breakpoint);
    
    // Set the preferred font for the debugger
    QFont table_font;
    
    void action_break();
    void action_continue();
    void action_step();
    void action_step_over();
    void action_finish();
    void action_clear_breakpoints() noexcept;
    void action_update_registers() noexcept;
    void action_register_flag_state_changed(int) noexcept;

    void refresh_registers();
    void refresh_flags();
    
    QWidget *right_view;
    
    QAction *break_button;
    QAction *continue_button;
    QAction *step_button;
    QAction *step_over_button;
    QAction *finish_fn_button;
    QAction *clear_breakpoints_button;
    
    QLineEdit *register_af, *register_bc, *register_de, *register_hl, *register_sp, *register_pc;
    QCheckBox *flag_carry, *flag_half_carry, *flag_subtract, *flag_zero;
    BacktraceTable *backtrace;
    GameWindow *game_window;
    
    static void log_callback(GB_gameboy_s *, const char *, GB_log_attributes);
    void closeEvent(QCloseEvent *) override;
    
    
};

#endif

#ifndef SB_QT_DEBUGGER_HPP
#define SB_QT_DEBUGGER_HPP

extern "C" {
#include <Core/gb.h>
}

#include <vector>
#include <QTableWidget>
#include <QMainWindow>
#include <optional>

#include "game_window.hpp"

class GameDisassembler;
class QLineEdit;
class GameWindow;

class GameDebugger : public QMainWindow {
    friend GameWindow;
    
    Q_OBJECT
    
public:
    GameDebugger(GameWindow *window);
    ~GameDebugger() override;
    
    GB_gameboy_s *get_gameboy() noexcept {
        return this->game_window->get_gameboy();
    }
    
    /** Add 1 to retaining logs (if != 0, logs are not printed to console but stored in a buffer */
    void push_retain_logs() { this->retain_logs++; }
    
    /** Subtract 1 from retaining logs (if != 0, logs are not printed to console but stored in a buffer */
    void pop_retain_logs() { this->retain_logs--; }
    
    /** Return logs and clear */
    std::string get_and_clear_retained_logs() {
        auto result = std::move(this->retained_logs);
        this->retained_logs = {};
        return result;
    }
    
    /** Format the table for use with the debugger */
    void format_table(QTableWidget *widget);
    
    /** Evaluate the expression, returning an address if successful */
    std::optional<std::uint16_t> evaluate_expression(const char *expression);
    
    /** Execute the debugger command, returning the result */
    std::string execute_debugger_command(const char *command);
    
    /** Get the font used for tables */
    const QFont &get_table_font() const noexcept {
        return this->table_font;
    }
private:
    GameDisassembler *disassembler;
    
    class GameDebuggerTable;
    
    // Set the preferred font for the debugger
    QFont table_font;
    int retain_logs = 0;
    std::string retained_logs;
    static GameDebugger *resolve_debugger(GB_gameboy_s *gb) noexcept;
    static char *input_callback(GB_gameboy_s *) noexcept;
    bool debug_breakpoint_pause = false;
    
    void continue_break(const char *command_to_execute = nullptr);
    void action_break();
    void action_continue();
    void action_step();
    void action_step_over();
    void action_finish();
    void action_clear_breakpoints() noexcept;
    void action_update_registers() noexcept;
    
    std::string command_to_execute_on_unbreak;
    
    QWidget *right_view;
    
    QAction *break_button;
    QAction *continue_button;
    QAction *step_button;
    QAction *step_over_button;
    QAction *finish_fn_button;
    QAction *clear_breakpoints_button;
    
    QLineEdit *register_a, *register_b, *register_c, *register_d, *register_e, *register_f, *register_hl, *register_pc;
    QTableWidget *backtrace;
    
    
    GameWindow *game_window;
    
    static void log_callback(GB_gameboy_s *, const char *, GB_log_attributes);
    void refresh_view();
    
    
};

#endif

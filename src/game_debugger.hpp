#ifndef SB_QT_DEBUGGER_HPP
#define SB_QT_DEBUGGER_HPP

extern "C" {
#include <Core/gb.h>
}

#include <vector>
#include <QTableWidget>
#include <QMainWindow>
#include <optional>

class GameDisassembler;
class GameDebuggerTable;
class GameWindow;
class QLineEdit;

class GameDebugger : public QMainWindow {
    friend GameDisassembler;
    friend GameDebuggerTable;
    friend GameWindow;
    
    Q_OBJECT
    
public:
    GameDebugger(GameWindow *window);
    ~GameDebugger() override;
    
    void set_gameboy(GB_gameboy_s *);
    
private:
    GameDisassembler *disassembler;
    
    // Set the preferred font for the debugger
    QFont table_font;
    
    GB_gameboy_s *gameboy = nullptr;
    void push_retain_logs() { this->retain_logs++; }
    void pop_retain_logs() { this->retain_logs--; }
    int retain_logs = 0;
    std::string retained_logs;
    static GameDebugger *resolve_debugger(GB_gameboy_s *gb) noexcept;
    static char *input_callback(GB_gameboy_s *) noexcept;
    bool debug_breakpoint_pause = false;
    
    void execute_debugger_command(const char *command);
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
    
    std::optional<std::uint16_t> evaluate_expression(const char *expression);
    
    GameWindow *game_window;
    
    static void log_callback(GB_gameboy_s *, const char *, GB_log_attributes);
    void refresh_view();
    
    void format_table(QTableWidget *widget);
    
};

#endif

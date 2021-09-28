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
class GameWindow;

class GameDebugger : public QMainWindow {
    friend GameDisassembler;
    friend GameWindow;
    
    Q_OBJECT
    
public:
    GameDebugger();
    ~GameDebugger() override;
    
    void set_gameboy(GB_gameboy_s *);
    
private:
    GameDisassembler *disassembler;
    
    void push_retain_logs() { this->retain_logs++; }
    void pop_retain_logs() { this->retain_logs--; }
    int retain_logs = 0;
    std::string retained_logs;
    GB_gameboy_s *gameboy = nullptr;
    static GameDebugger *resolve_debugger(GB_gameboy_s *gb) noexcept;
    static char *input_callback(GB_gameboy_s *) noexcept;
    bool debug_breakpoint_pause = false;
    
    void break_now();
    void continue_break();
    
    QAction *break_button;
    QAction *continue_button;
    
    static void log_callback(GB_gameboy_s *, const char *, GB_log_attributes);
    void refresh_view();
    
};

#endif

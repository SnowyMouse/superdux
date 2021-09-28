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
    
    bool retain_logs = false;
    std::string retained_logs;
    GB_gameboy_s *gameboy = nullptr;
    static GameDebugger *resolve_debugger(GB_gameboy_s *gb) noexcept;
    
    static void log_callback(GB_gameboy_s *, const char *, GB_log_attributes);
    void refresh_view();
    
};

#endif

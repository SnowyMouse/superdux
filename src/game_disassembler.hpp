#ifndef SB_QT_DISASSEMBLER
#define SB_QT_DISASSEMBLER

#include <QTableWidget>
#include <optional>
#include <QFont>

class GameDebugger;

class GameDisassembler : public QTableWidget {
    Q_OBJECT
public:
    GameDisassembler(GameDebugger *parent);
    ~GameDisassembler() override;
    
    // Used for navigation
    
    // If last_address is not equal to current_address, clear the table when refresh_view() is called
    std::uint16_t last_address = 0;
    
    // Address to inspect
    std::uint16_t current_address = 0x150;
    
    // Next address(es) depending on if we use arrow keys, page down, or scroll wheel
    std::uint16_t next_address_short = 0;
    std::uint16_t next_address_medium = 0;
    std::uint16_t next_address_far = 0;
    
    struct Disassembly {
        std::optional<std::uint16_t> address;
        QString follow_address;
        QString raw_result;
        QString instruction;
        QString comment;
        bool current_location;
        bool is_marker;
    };
    std::vector<Disassembly> disassembly;
    
    std::optional<Disassembly> last_disassembly;
    std::vector<std::uint16_t> history;
    
    void go_to(std::uint16_t where);
    void go_back();
    void show_context_menu(const QPoint &point);
    void jump_to_address_window();
    void follow_address();
    void add_breakpoint();
    void delete_breakpoint();
    void refresh_view();
    bool address_is_breakpoint(std::uint16_t address);
    void set_address_to_current_breakpoint();
    
    void wheelEvent(QWheelEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

    std::vector<Disassembly> disassemble_at_address(std::uint16_t address, std::uint8_t count);
    
    QColor text_default_color;
    QColor text_highlight_color;
    
    QColor bg_default_color;
    QColor bg_highlight_color;
    
private:
    GameDebugger *debugger;
};

#endif

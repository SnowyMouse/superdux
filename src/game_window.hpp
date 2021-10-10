#ifndef SB_QT_GAME_WINDOW_HPP
#define SB_QT_GAME_WINDOW_HPP

extern "C" {
#include <Core/gb.h>
}

#include <SDL2/SDL.h>
#include <QMainWindow>
#include <QImage>
#include <QPixmap>
#include <QGraphicsView>
#include <QGraphicsScene>
#include <QIODevice>
#include <vector>
#include <chrono>
#include <string>
#include <optional>
#include <QStringList>
#include <QSettings>
#include <QTimer>

#include <thread>

#include "input_device.hpp"

#include "game_instance.hpp"

class QGamepad;
class GameDebugger;
class EditAdvancedGameBoyModelDialog;

class GameWindow : public QMainWindow {
    Q_OBJECT
    
    friend GameDebugger;
    friend EditAdvancedGameBoyModelDialog;
    
public:
    GameWindow();
    
    void load_rom(const char *rom_path) noexcept;
    
    const QSettings &settings();
    static std::vector<std::unique_ptr<InputDevice>> get_all_devices();
    
    ~GameWindow();
    
    GameInstance &get_instance() noexcept {
        return *this->instance;
    }
    
private:
    using clock = std::chrono::steady_clock;
    
    std::unique_ptr<GameInstance> instance;
    std::thread instance_thread;

    // Game thread timer
    QTimer game_thread_timer;
    
    // Gameboy itself
    QAction *open_roms_action;
    QMenu *gameboy_model_menu;
    std::vector<QAction *> gb_model_actions;
    
    // Audio settings
    unsigned int sample_count = 0;
    unsigned int sample_rate = 1024; // using 1024 as the default instead of 0 is deliberate - SDL sometimes defaults to 4096 which has terrible audio delay
    std::vector<QAction *> channel_count_options;
    
    // Emulation
    QAction *pause_action;
    void game_loop();
    std::vector<QAction *> rtc_mode_options;
    GB_rtc_mode_t rtc_mode = GB_rtc_mode_t::GB_RTC_MODE_ACCURATE;
    
    // Pause if menu is open?
    bool menu_open = false;
    bool pause_on_menu = false;
    
    // Video
    int scaling = 2;
    std::vector<QAction *> scaling_options;
    std::vector<QAction *> pixel_buffer_options;
    bool vblank = false;
    QPixmap pixel_buffer_pixmap;
    QGraphicsPixmapItem *pixel_buffer_pixmap_item = nullptr;
    std::vector<std::uint32_t> pixel_buffer;
    QGraphicsView *pixel_buffer_view;
    QGraphicsScene *pixel_buffer_scene = nullptr;
    QGraphicsTextItem *fps_text = nullptr;
    GB_color_correction_mode_t color_correction_mode = GB_color_correction_mode_t::GB_COLOR_CORRECTION_DISABLED;
    std::vector<QAction *> color_correction_mode_options;
    void set_pixel_view_scaling(int scaling);
    void redraw_pixel_buffer();
    
    // For showing FPS
    bool show_fps = false;
    float last_fps = -1.0;
    
    void show_status_text(const char *text);
    QGraphicsTextItem *status_text = nullptr;
    clock::time_point status_text_deletion;
    std::vector<QAction *> volume_options;
    void show_new_volume_text();
    
    // Input
    bool disable_input = false; // used when configuring settings
    void keyPressEvent(QKeyEvent *) override;
    void keyReleaseEvent(QKeyEvent *) override;
    void handle_keyboard_key(QKeyEvent *event, bool press);
    std::vector<std::unique_ptr<InputDevice>> devices;

    // Revisions
    enum GameBoyType {
        GameBoyGB,
        GameBoyGBC,
        GameBoyGBA,
        GameBoySGB,
        GameBoySGB2,
        GameBoy_END
    };
    GameBoyType gb_type = GameBoyType::GameBoyGB;

    GB_model_t gb_rev = GB_model_t::GB_MODEL_DMG_B;
    GB_model_t gbc_rev = GB_model_t::GB_MODEL_CGB_C;
    GB_model_t gba_rev = GB_model_t::GB_MODEL_AGB;
    GB_model_t sgb_rev = GB_model_t::GB_MODEL_SGB_NTSC;
    GB_model_t sgb2_rev = GB_model_t::GB_MODEL_SGB2;

    GB_model_t model_for_type(GameBoyType type) const noexcept;
    const std::optional<std::filesystem::path> &boot_rom_for_type(GameBoyType type) const noexcept;
    bool use_fast_boot_rom_for_type(GameBoyType type) const noexcept;

    bool gbc_fast_boot_rom = false;
    std::optional<std::filesystem::path> gb_boot_rom_path, gbc_boot_rom_path, gba_boot_rom_path, sgb_boot_rom_path, sgb2_boot_rom_path;
    bool sgb_crop_borders = false, sgb2_crop_borders = false;
    bool use_crop_borders_for_type(GameBoyType type) const noexcept;

    
    void reload_devices();
    
    // Save path
    std::filesystem::path save_path;
    bool exit_without_save = false;
    QAction *exit_without_saving;
    bool save_if_loaded() noexcept;
    
    // Debugging
    QAction *show_debugger;
    GameDebugger *debugger_window;
    
    // Recent ROMs
    QStringList recent_roms;
    QMenu *recent_roms_menu;
    
    // Reset button
    QAction *reset_rom_action;
    
    // Make a shadow
    void make_shadow(QGraphicsTextItem *object);
    
    // Handle input
    void handle_device_input(InputDevice::InputType type, double input);
    
    // Used for preventing loading different ROMs while debugging
    void set_loading_other_roms_enabled(bool enabled) noexcept;
    
    void update_recent_roms_list();
    void closeEvent(QCloseEvent *) override;
    
    void increment_volume(int amount);
    
private slots:
    void action_set_scaling() noexcept;
    void action_toggle_showing_fps() noexcept;
    void action_toggle_pause() noexcept;
    void action_open_rom() noexcept;
    void action_open_recent_rom();
    void action_reset() noexcept;
    void action_edit_controls() noexcept;
    void action_set_buffer_mode() noexcept;
    void action_set_rtc_mode() noexcept;
    void action_set_color_correction_mode() noexcept;
    void action_show_advanced_model_options() noexcept;
    
    void action_toggle_audio() noexcept;
    void action_set_volume();
    void action_add_volume();
    void action_set_channel_count() noexcept;
    
    void action_showing_menu() noexcept;
    void action_hiding_menu() noexcept;
    
    void action_set_model() noexcept;
    void action_save_sram() noexcept;
    void action_quit_without_saving() noexcept;
};

#endif

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
#include <QTimer>

#include <thread>

#include "input_device.hpp"

#include "game_instance.hpp"

class Printer;
class Debugger;
class EditAdvancedGameBoyModelDialog;
class EditSpeedControlSettingsDialog;
class VRAMViewer;

class GameWindow : public QMainWindow {
    Q_OBJECT
    
    friend Debugger;
    friend EditAdvancedGameBoyModelDialog;
    friend EditSpeedControlSettingsDialog;
    
public:
    enum ScalingFilter {
        SCALING_FILTER_NEAREST = 0,
        SCALING_FILTER_BILINEAR
    };

    GameWindow();
    
    void load_rom(const char *rom_path) noexcept;

    std::vector<std::shared_ptr<InputDevice>> get_all_devices();

    void disconnect_serial();
    
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
    QAction *save_sram_now;
    QMenu *gameboy_model_menu;
    std::vector<QAction *> gb_model_actions;

    bool integrity_check_corrupt = true;
    bool integrity_check_compatible = true;
    
    // Audio settings
    unsigned int sample_count = 1024; // using 1024 as the default instead of 0 is deliberate - SDL sometimes defaults to 4096 which has terrible audio delay
    unsigned int sample_rate = 0;
    std::vector<QAction *> channel_count_options;
    GB_highpass_mode_t highpass_filter_mode = GB_highpass_mode_t::GB_HIGHPASS_ACCURATE;
    std::vector<QAction *> highpass_filter_mode_options;

    // Printer
    QAction *show_printer;
    Printer *printer_window;
    
    // Emulation
    QAction *pause_action;
    void game_loop();
    std::vector<QAction *> rtc_mode_options;
    GB_rtc_mode_t rtc_mode = GB_rtc_mode_t::GB_RTC_MODE_ACCURATE;
    double rewind_length = 30.0;
    double rewind_speed = 1.0;
    double max_turbo = 4.0;
    double max_slowmo = 0.25;

    // Set whether or not these are enabled
    bool turbo_enabled = false;
    bool slowmo_enabled = false;
    bool rewind_enabled = false;

    // Rumble
    std::vector<QAction *> rumble_mode_options;
    GB_rumble_mode_t rumble_mode = GB_rumble_mode_t::GB_RUMBLE_CARTRIDGE_ONLY;
    
    // Pause if menu is open?
    bool menu_open = false;
    bool pause_on_menu = false;
    
    // Video
    int scaling = 2;
    std::vector<QAction *> scaling_options;
    std::vector<QAction *> pixel_buffer_options;
    std::vector<QAction *> scaling_filter_options;
    ScalingFilter scaling_filter = ScalingFilter::SCALING_FILTER_NEAREST;
    bool vblank = false;
    QPixmap pixel_buffer_pixmap;
    QGraphicsPixmapItem *pixel_buffer_pixmap_item = nullptr;
    std::vector<std::uint32_t> pixel_buffer;
    QGraphicsView *pixel_buffer_view;
    QGraphicsScene *pixel_buffer_scene = nullptr;
    QGraphicsTextItem *fps_text = nullptr;
    GB_color_correction_mode_t color_correction_mode = GB_color_correction_mode_t::GB_COLOR_CORRECTION_EMULATE_HARDWARE;
    std::vector<QAction *> color_correction_mode_options;
    void set_pixel_view_scaling(int scaling);
    void redraw_pixel_buffer();
    
    // For showing FPS
    bool show_fps = false;
    float last_fps = -1.0;
    
    void show_status_text(const char *text);
    QGraphicsTextItem *status_text = nullptr;
    clock::time_point status_text_deletion;
    bool status_text_hidden = false;
    std::vector<QAction *> volume_options;
    void show_new_volume_text();
    
    // Input
    bool disable_input = false; // used when configuring settings
    void keyPressEvent(QKeyEvent *) override;
    void keyReleaseEvent(QKeyEvent *) override;
    void handle_keyboard_key(QKeyEvent *event, bool press);
    std::vector<std::shared_ptr<InputDevice>> devices;

    // Revisions
    enum GameBoyType {
        GameBoyGB,
        GameBoyGBC,
        GameBoyGBA,
        GameBoySGB,
        GameBoySGB2,
        GameBoy_END
    };
    GameBoyType gb_type = GameBoyType::GameBoyGBC;

    GB_model_t gb_rev = GB_model_t::GB_MODEL_DMG_B;
    GB_model_t gbc_rev = GB_model_t::GB_MODEL_CGB_E;
    GB_model_t gba_rev = GB_model_t::GB_MODEL_AGB;
    GB_model_t sgb_rev = GB_model_t::GB_MODEL_SGB_NTSC;
    GB_model_t sgb2_rev = GB_model_t::GB_MODEL_SGB2;

    GB_model_t model_for_type(GameBoyType type) const noexcept;
    const std::optional<std::filesystem::path> &boot_rom_for_type(GameBoyType type) const noexcept;
    bool use_fast_boot_rom_for_type(GameBoyType type) const noexcept;

    bool gbc_fast_boot_rom = false;
    bool sgb_skip_intro = false;
    bool sgb2_skip_intro = false;
    std::optional<std::filesystem::path> gb_boot_rom_path, gbc_boot_rom_path, gba_boot_rom_path, sgb_boot_rom_path, sgb2_boot_rom_path;
    bool gb_allow_custom_boot_rom = false, gbc_allow_custom_boot_rom = false, gba_allow_custom_boot_rom = false, sgb_allow_custom_boot_rom = false, sgb2_allow_custom_boot_rom = false;
    bool gb_border = false, gbc_border = false, gba_border = false, sgb_border = true, sgb2_border = true;
    bool use_border_for_type(GameBoyType type) const noexcept;

    // Save states
    std::vector<QAction *> create_save_state_actions;
    std::vector<QAction *> load_save_state_actions;
    std::vector<std::vector<std::uint8_t>> temporary_save_states;
    unsigned int next_temporary_save_state = 0;
    unsigned int temporary_save_state_buffer_length = 10;
    bool load_save_state(const std::filesystem::path &path);
    std::filesystem::path get_save_state_path(int index) const;
    QMenu *save_state_menu;
    
    void reload_devices();
    InputDeviceGamepad *last_used_input_device = nullptr;
    
    // Save path
    std::filesystem::path save_path;
    bool exit_without_save = false;
    QAction *exit_without_saving;
    bool save_if_loaded() noexcept;
    
    // Debugging
    QAction *show_debugger;
    Debugger *debugger_window;

    // VRAM viewing
    QAction *show_vram_viewer;
    VRAMViewer *vram_viewer_window;
    
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

    void handle_joypad_event(const SDL_ControllerButtonEvent &event) noexcept;
    void handle_joypad_event(const SDL_ControllerAxisEvent &event) noexcept;
    
    void update_recent_roms_list();
    void closeEvent(QCloseEvent *) override;
    
    void increment_volume(int amount);

    // Can we make save states?
    bool save_states_allowed() noexcept;

    // -rewind_speed = rewind
    double rewind_multiplier = 1.0F;
    
    // >1.0F = turbo
    double turbo_multiplier = 1.0F;

    // <1.0F = slow motion
    double slowmo_multiplier = 1.0F;

    // Base speed
    double base_multiplier = 1.0F;

    // Update the emulation speed
    void update_emulation_speed();
    void reset_emulation_speed();

private slots:
    void action_set_scaling() noexcept;
    void action_set_scale_filter() noexcept;
    void action_toggle_showing_fps() noexcept;
    void action_toggle_pause() noexcept;
    void action_open_rom() noexcept;
    void action_open_recent_rom();
    void action_reset() noexcept;
    void action_set_buffer_mode() noexcept;
    void action_set_rtc_mode() noexcept;
    void action_set_color_correction_mode() noexcept;
    void action_show_advanced_model_options() noexcept;
    void action_set_highpass_filter_mode() noexcept;
    void action_set_rumble_mode() noexcept;
    void action_toggle_hide_status_text() noexcept;
    void action_edit_controls();
    void action_edit_speed_control();

    void action_clear_all_roms() noexcept;
    void action_clear_missing_roms() noexcept;

    void action_create_save_state();
    void action_load_save_state();
    void action_import_save_state();

    void action_revert_save_state();
    void action_unrevert_save_state();
    
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

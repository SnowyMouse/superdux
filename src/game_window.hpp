#ifndef SB_QT_GAME_WINDOW_HPP
#define SB_QT_GAME_WINDOW_HPP

extern "C" {
#include <Core/gb.h>
}

#include <QMainWindow>
#include <QImage>
#include <QPixmap>
#include <QGraphicsView>
#include <QGraphicsScene>
#include <QIODevice>
#include <QAudio>
#include <vector>
#include <chrono>
#include <string>
#include <QStringList>
#include <QSettings>

#include "game_debugger.hpp"

class QAudioOutput;
class QIODevice;
class QGamepad;

class GameWindow : public QMainWindow {
    friend GameDebugger;
    
    Q_OBJECT
    
public:
    GameWindow();
    
    void load_rom(const char *rom_path) noexcept;
    
    const QSettings &settings();
    
    ~GameWindow();
    
private:
    using clock = std::chrono::steady_clock;
    
    // Gameboy itself
    GB_gameboy_s gameboy = {};
    GB_model_t gb_model = GB_model_t::GB_MODEL_CGB_C;
    void initialize_gameboy(GB_model_t model) noexcept;
    QAction *open_roms_action;
    QMenu *gameboy_model_menu;
    std::vector<QAction *> gb_model_actions;
    
    // Audio
    bool muted = false;
    bool mono = false;
    char volume = 100;
    std::vector<QAction *> channel_count_options;
    
    // Emulation
    QAction *pause_action;
    bool paused = false;
    bool rom_loaded = false;
    void game_loop();
    
    // Pause if menu is open?
    bool menu_open = false;
    bool pause_on_menu = false;
    
    // Video
    int scaling = 2;
    std::vector<QAction *> scaling_options;
    bool vblank = false;
    QPixmap pixel_buffer_pixmap;
    QGraphicsPixmapItem *pixel_buffer_pixmap_item = nullptr;
    QImage pixel_buffer;
    QGraphicsView *pixel_buffer_view;
    QGraphicsScene *pixel_buffer_scene = nullptr;
    QGraphicsTextItem *fps_text = nullptr;
    static void on_vblank(GB_gameboy_s *);
    void set_pixel_view_scaling(int scaling);
    void redraw_pixel_buffer();
    
    // For showing FPS
    bool show_fps = false;
    clock::time_point last_frame_time;
    float fps_denominator; // fps time
    int fps_numerator; // fps count
    void calculate_frame_rate() noexcept;
    void reset_fps_counter(bool update_text) noexcept;
    
    // Audio
    QAudioOutput *audio_output;
    QIODevice *audio_device;
    static void on_sample(GB_gameboy_s *, GB_sample_t *);
    std::vector<std::int16_t> sample_buffer;
    void play_audio_buffer();
    
    void show_status_text(const char *text);
    QGraphicsTextItem *status_text = nullptr;
    clock::time_point status_text_deletion;
    std::vector<QAction *> volume_options;
    void show_new_volume_text();
    
    // Input
    QGamepad *gamepad = nullptr;
    void keyPressEvent(QKeyEvent *) override;
    void keyReleaseEvent(QKeyEvent *) override;
    void handle_keyboard_key(QKeyEvent *event, bool press);
    
    // Save path
    std::string save_path;
    bool exit_without_save = false;
    QAction *exit_without_saving;
    bool save_if_loaded() noexcept;
    
    // Debugging
    GameDebugger *debugger_window;
    
    // Recent ROMs
    QStringList recent_roms;
    QMenu *recent_roms_menu;
    
    // Reset button
    QAction *reset_rom_action;
    
    // Make a shadow
    void make_shadow(QGraphicsTextItem *object);
    
    // Used for preventing loading different ROMs while debugging
    void set_loading_other_roms_enabled(bool enabled) noexcept;
    
    void update_recent_roms_list();
    void closeEvent(QCloseEvent *) override;
    
private slots:
    void action_set_scaling() noexcept;
    void action_toggle_showing_fps() noexcept;
    void action_toggle_pause() noexcept;
    void action_toggle_pause_in_menu() noexcept;
    void action_open_rom() noexcept;
    void action_open_recent_rom();
    void action_reset() noexcept;
    
    void action_toggle_audio() noexcept;
    void action_set_volume();
    void action_add_volume();
    void action_set_channel_count() noexcept;
    
    void action_gamepads_changed();
    
    void action_gamepad_a(bool) noexcept;
    void action_gamepad_b(bool) noexcept;
    void action_gamepad_start(bool) noexcept;
    void action_gamepad_select(bool) noexcept;
    void action_gamepad_up(bool) noexcept;
    void action_gamepad_down(bool) noexcept;
    void action_gamepad_left(bool) noexcept;
    void action_gamepad_right(bool) noexcept;
    void action_gamepad_axis_x(double) noexcept;
    void action_gamepad_axis_y(double) noexcept;
    
    void action_showing_menu() noexcept;
    void action_hiding_menu() noexcept;
    
    void action_set_model() noexcept;
    void action_save_sram() noexcept;
    void action_quit_without_saving() noexcept;
};

#endif

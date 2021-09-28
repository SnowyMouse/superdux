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
    
    ~GameWindow();
    
private:
    using clock = std::chrono::steady_clock;
    
    // Gameboy itself
    GB_gameboy_s gameboy;
    void initialize_gameboy(GB_model_t model) noexcept;
    
    // Audio
    bool muted = false;
    bool mono = false;
    char volume = 100;
    std::vector<QAction *> channel_count_options;
    
    // Emulation
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
    QGraphicsPixmapItem *pixel_buffer_pixmap_item;
    QImage pixel_buffer;
    QGraphicsView *pixel_buffer_view;
    QGraphicsScene *pixel_buffer_scene;
    QGraphicsTextItem *fps_text = nullptr;
    QGraphicsTextItem *fps_text_shadow = nullptr;
    static void on_vblank(GB_gameboy_s *);
    void set_pixel_view_scaling(int scaling);
    void redraw_pixel_buffer();
    
    // For showing FPS
    bool show_fps = false;
    clock::time_point last_frame_time;
    float fps_denominator; // fps time
    int fps_numerator; // fps count
    void calculate_frame_rate() noexcept;
    
    // Audio
    QAudioOutput *audio_output;
    QIODevice *audio_device;
    static void on_sample(GB_gameboy_s *, GB_sample_t *);
    std::vector<std::int16_t> sample_buffer;
    void play_audio_buffer();
    
    void show_status_text(const char *text);
    QGraphicsTextItem *status_text = nullptr;
    QGraphicsTextItem *status_text_shadow = nullptr;
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
    void save_if_loaded() noexcept;
    
    // Debugging
    GameDebugger *debugger_window;
    
    void closeEvent(QCloseEvent *) override;
    
private slots:
    void action_set_scaling() noexcept;
    void action_toggle_showing_fps() noexcept;
    void action_toggle_pause() noexcept;
    void action_toggle_pause_in_menu() noexcept;
    void action_open_rom() noexcept;
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
    
    void action_save_battery() noexcept;
};

#endif

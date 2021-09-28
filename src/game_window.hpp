#ifndef SB_QT_GAME_WINDOW_CPP
#define SB_QT_GAME_WINDOW_CPP

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

class QAudioOutput;
class QIODevice;
class QGamepad;

class GameWindow : public QMainWindow {
    Q_OBJECT
    
public:
    GameWindow();
    
    void load_rom(const char *rom_path) noexcept;
    
private:
    using clock = std::chrono::steady_clock;
    
    // Gameboy itself
    GB_gameboy_s gameboy;
    void initialize_gameboy(GB_model_t model) noexcept;
    
    // Audio
    bool muted = false;
    char volume = 100;
    
    // Emulation
    bool paused = false;
    bool rom_loaded = false;
    void game_loop();
    
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
    
    QGamepad *gamepad = nullptr;
    
    void show_new_volume_text();
    
private slots:
    void action_set_scaling() noexcept;
    void action_toggle_showing_fps() noexcept;
    void action_toggle_pause() noexcept;
    void action_open_rom() noexcept;
    void action_reset() noexcept;
    void action_toggle_audio() noexcept;
    
    void action_set_volume();
    void action_add_volume();
    
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
    
};

#endif

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
#include <vector>
#include <chrono>

class GameWindow : public QMainWindow {
    Q_OBJECT
    
public:
    GameWindow();
    
    void load_rom(const char *rom_path) noexcept;
    
private:
    using clock = std::chrono::steady_clock;
    
    GB_gameboy_s gameboy;
    bool paused = false;
    bool rom_loaded = false;
    
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
    
    void set_pixel_view_scaling(int scaling);
    void update_pixel_buffer_dimensions();
    void redraw_pixel_buffer();
    
    void game_loop();
    
    static void on_vblank(GB_gameboy_s *);
    
    // For showing FPS
    bool show_fps = false;
    clock::time_point last_frame_time;
    float fps_denominator; // fps time
    int fps_numerator; // fps count
    void calculate_frame_rate() noexcept;
    
    void initialize_gameboy(GB_model_t model) noexcept;
    
private slots:
    void action_set_scaling() noexcept;
    void action_toggle_showing_fps() noexcept;
    void action_toggle_pause() noexcept;
    void action_open_rom() noexcept;
    void action_reset() noexcept;
};

#endif

#include "game_window.hpp"
#include <QHBoxLayout>
#include <QTimer>
#include <QMenuBar>
#include <QGraphicsTextItem>
#include <QFontDatabase>
#include <QFileDialog>
#include <cstring>
#include <chrono>

static void load_boot_rom(GB_gameboy_t *gb, GB_boot_rom_t type) {
    // from sameboy
    static constexpr const char *const names[] = {
        [GB_BOOT_ROM_DMG0] = "dmg0_boot.bin",
        [GB_BOOT_ROM_DMG] = "dmg_boot.bin",
        [GB_BOOT_ROM_MGB] = "mgb_boot.bin",
        [GB_BOOT_ROM_SGB] = "sgb_boot.bin",
        [GB_BOOT_ROM_SGB2] = "sgb2_boot.bin",
        [GB_BOOT_ROM_CGB0] = "cgb0_boot.bin",
        [GB_BOOT_ROM_CGB] = "cgb_boot.bin",
        [GB_BOOT_ROM_AGB] = "agb_boot.bin",
    };
    
    GB_load_boot_rom(gb, names[type]);
}

static std::uint32_t rgb_encode(GB_gameboy_t *gb, uint8_t r, uint8_t g, uint8_t b) {
    return 0xFF000000 | (r << 16) | (g << 8) | (b << 0);
}

#define GET_ICON(what) QIcon::fromTheme(QStringLiteral(what))

GameWindow::GameWindow() {
    QMenuBar *bar = new QMenuBar(this);
    this->setMenuBar(bar);
    
    // File menu
    auto *file_menu = bar->addMenu("File");
    auto *open = file_menu->addAction("Open ROM...");
    open->setShortcut(QKeySequence::Open);
    open->setIcon(GET_ICON("document-open"));
    connect(open, &QAction::triggered, this, &GameWindow::action_open_rom);
    
    // Emulation menu
    auto *emulation_menu = bar->addMenu("Emulation");
    auto *pause = emulation_menu->addAction("Pause");
    connect(pause, &QAction::triggered, this, &GameWindow::action_toggle_pause);
    pause->setIcon(GET_ICON("media-playback-pause"));
    pause->setCheckable(true);
    auto *reset = emulation_menu->addAction("Reset");
    connect(reset, &QAction::triggered, this, &GameWindow::action_reset);
    reset->setIcon(GET_ICON("view-refresh"));
    
    // View menu
    auto *view_menu = bar->addMenu("View");

    // Add scaling options
    auto *scaling = view_menu->addMenu("Scaling");
    for(int i = 1; i <= 8; i *= 2) {
        char text[4];
        std::snprintf(text, sizeof(text), "%ix", i);
        auto *action = new QAction(text, scaling);
        action->setData(i);
        scaling->addAction(action);
        connect(action, &QAction::triggered, this, &GameWindow::action_set_scaling);
        action->setCheckable(true);
        action->setChecked(i == this->scaling);
        
        scaling_options.emplace_back(action);
    }
    
    auto *toggle_fps = view_menu->addAction("Show FPS");
    connect(toggle_fps, &QAction::triggered, this, &GameWindow::action_toggle_showing_fps);
    toggle_fps->setCheckable(true);
    
    auto *central_widget = new QWidget(this);
    auto *layout = new QHBoxLayout(central_widget);
    
    this->pixel_buffer_view = new QGraphicsView(central_widget);
    this->pixel_buffer_scene = new QGraphicsScene(central_widget);
    this->pixel_buffer_pixmap_item = this->pixel_buffer_scene->addPixmap(this->pixel_buffer_pixmap);
    this->pixel_buffer_view->setScene(this->pixel_buffer_scene);
    
    // Instantiate the game boy
    this->initialize_gameboy(GB_model_t::GB_MODEL_CGB_C);

    // Set our pixel buffer parameters
    this->pixel_buffer_view->setFrameStyle(0);
    this->pixel_buffer_view->setHorizontalScrollBarPolicy(Qt::ScrollBarPolicy::ScrollBarAlwaysOff);
    this->pixel_buffer_view->setVerticalScrollBarPolicy(Qt::ScrollBarPolicy::ScrollBarAlwaysOff);
    this->pixel_buffer_view->setSizePolicy(QSizePolicy::Policy::Fixed, QSizePolicy::Policy::Fixed);
    
    layout->addWidget(this->pixel_buffer_view);
    layout->setMargin(0);
    central_widget->setLayout(layout);
    this->setCentralWidget(central_widget);
    
    this->redraw_pixel_buffer();
    
    QTimer *timer = new QTimer(this);
    timer->callOnTimeout(this, &GameWindow::game_loop);
    timer->start();
}

void GameWindow::load_rom(const char *rom_path) noexcept {
    this->rom_loaded = true;
    GB_load_rom(&this->gameboy, rom_path);
    GB_reset(&this->gameboy);
}

void GameWindow::redraw_pixel_buffer() {
    // Update the pixmap
    this->pixel_buffer_pixmap.convertFromImage(this->pixel_buffer);
    pixel_buffer_pixmap_item->setPixmap(this->pixel_buffer_pixmap);
}

void GameWindow::set_pixel_view_scaling(int scaling) {
    this->scaling = scaling;
    this->pixel_buffer_view->setMinimumSize(this->pixel_buffer.width() * this->scaling,this->pixel_buffer.height() * this->scaling);
    this->pixel_buffer_view->setMaximumSize(this->pixel_buffer.width() * this->scaling,this->pixel_buffer.height() * this->scaling);
    
    this->pixel_buffer_view->setTransform(QTransform::fromScale(scaling, scaling));
    this->redraw_pixel_buffer();
    
    // Go through all scaling options. Uncheck/check whatever applies.
    for(auto *option : this->scaling_options) {
        option->setChecked(option->data().toInt() == scaling);
    }
}

void GameWindow::calculate_frame_rate() noexcept {
    if(this->show_fps) {
        auto now = clock::now();
        
        this->fps_denominator += std::chrono::duration_cast<std::chrono::microseconds>(now - this->last_frame_time).count() / 1000000.0;
        this->last_frame_time = now;
        
        // Once we reach critical mass (30 frames), show the frame rate
        if(this->fps_numerator++ > 30) {
            float fps_shown = this->fps_numerator / this->fps_denominator;
            this->fps_numerator = 0;
            this->fps_denominator = 0;
            
            char fps_text_str[64];
            std::snprintf(fps_text_str, sizeof(fps_text_str), "FPS: %.01f", fps_shown);
            QString fps_text_qstr = fps_text_str;
            this->fps_text_shadow->setPlainText(fps_text_str);
            this->fps_text->setPlainText(fps_text_str);
        }
    }
}

void GameWindow::on_vblank(GB_gameboy_s *gb) {
    auto *window = reinterpret_cast<GameWindow *>(GB_get_user_data(gb));
    window->vblank = true;
}

void GameWindow::game_loop() {
    if(!this->rom_loaded || this->paused) {
        return;
    }
    
    // Run until vblank occurs or we take waaaay too long (e.g. 50 ms)
    auto timeout = clock::now() + std::chrono::milliseconds(50);
    
    while(!this->vblank && clock::now() < timeout) {
        GB_run(&this->gameboy);
    }
    
    // Clear the vblank flag and redraw
    this->vblank = false;
    this->redraw_pixel_buffer();
    this->calculate_frame_rate();
}

void GameWindow::action_set_scaling() noexcept {
    // Uses the user data from the sender to get scaling
    auto *action = qobject_cast<QAction *>(sender());
        
    int rescale = action->data().toInt(); 
    this->set_pixel_view_scaling(action->data().toInt());
}

void GameWindow::action_toggle_showing_fps() noexcept {
    this->show_fps = !this->show_fps;
    
    // If showing frame rate, create text objects and initialize the FPS counter
    if(this->show_fps) {
        this->fps_numerator = 0;
        this->fps_denominator = 0;
        this->last_frame_time = clock::now();
        
        this->fps_text_shadow = this->pixel_buffer_scene->addText("FPS: --", QFontDatabase::systemFont(QFontDatabase::FixedFont));
        fps_text_shadow->setDefaultTextColor(QColor::fromRgb(0,0,0));
        fps_text_shadow->setPos(1, 1);
        
        this->fps_text = this->pixel_buffer_scene->addText("FPS: --", QFontDatabase::systemFont(QFontDatabase::FixedFont));
        fps_text->setDefaultTextColor(QColor::fromRgb(255,255,0));
        fps_text->setPos(0, 0);
    }
    else {
        delete this->fps_text_shadow;
        delete this->fps_text;
        
        this->fps_text_shadow = nullptr;
        this->fps_text = nullptr;
    }
}

void GameWindow::action_toggle_pause() noexcept {
    this->paused = !this->paused;
}

void GameWindow::action_open_rom() noexcept {
    QFileDialog qfd;
    qfd.setNameFilters(QStringList { "Game Boy ROM (*.gb)", "Game Boy Color ROM (*.gbc)" });
    
    if(qfd.exec() == QDialog::DialogCode::Accepted) {
        this->load_rom(qfd.selectedFiles()[0].toUtf8().data());
    }
}

void GameWindow::action_reset() noexcept {
    GB_reset(&this->gameboy);
}

void GameWindow::initialize_gameboy(GB_model_t model) noexcept {
    std::memset(&this->gameboy, 0, sizeof(this->gameboy));
    GB_init(&this->gameboy, model);
    GB_set_user_data(&this->gameboy, this);
    GB_set_boot_rom_load_callback(&this->gameboy, load_boot_rom);
    GB_set_rgb_encode_callback(&this->gameboy, rgb_encode);
    GB_set_vblank_callback(&this->gameboy, GameWindow::on_vblank);
    
    // Update/clear our pixel buffer
    int width = GB_get_screen_width(&this->gameboy), height = GB_get_screen_height(&this->gameboy);
    this->pixel_buffer = QImage(width, height, QImage::Format::Format_ARGB32);
    this->pixel_buffer.fill(0);
    GB_set_pixels_output(&this->gameboy, reinterpret_cast<std::uint32_t *>(this->pixel_buffer.bits()));
    this->set_pixel_view_scaling(this->scaling);
}

#include "game_window.hpp"
#include "game_debugger.hpp"
#include "edit_controls_dialog.hpp"

#include <QHBoxLayout>
#include <QTimer>
#include <QMenuBar>
#include <QGraphicsTextItem>
#include <filesystem>
#include <QFontDatabase>
#include <QFileDialog>
#include <cstring>
#include <QGamepad>
#include <QKeyEvent>
#include <QGamepadManager>
#include <QApplication>
#include <QGraphicsDropShadowEffect>
#include <chrono>
#include <QMessageBox>
#include <QMimeData>
#include <QComboBox>
#include <QDialog>
#include <QLineEdit>

#include <QLabel>

#include "input_device.hpp"

#define SETTINGS_VOLUME "volume"
#define SETTINGS_SCALE "scale"
#define SETTINGS_SHOW_FPS "show_fps"
#define SETTINGS_MONO "mono"
#define SETTINGS_MUTE "mute"
#define SETTINGS_RECENT_ROMS "recent_roms"
#define SETTINGS_GB_MODEL "gb_model"
#define SETTINGS_SAMPLE_BUFFER_SIZE "sample_buffer_size"
#define SETTINGS_SAMPLE_RATE "sample_rate"
#define SETTINGS_BUFFER_MODE "buffer_mode"

#ifdef DEBUG
#define print_debug_message(...) std::printf("Debug: " __VA_ARGS__)
#else
#define print_debug_message(...) (void(0))
#endif

class GamePixelBufferView : public QGraphicsView {
public:
    GamePixelBufferView(QWidget *parent, GameWindow *window) : QGraphicsView(parent), window(window) {
        this->setAcceptDrops(true);
    }
    void keyPressEvent(QKeyEvent *event) override {
        event->ignore();
    }
    
    // Make sure the extension is valid
    template<typename T> static std::optional<std::filesystem::path> validate_event(T *event) {
        auto *d = event->mimeData();
        if(d->hasUrls()) {
            auto urls = d->urls();
            if(urls.length() == 1) {
                auto path = std::filesystem::path(urls[0].path().toStdString());
                if(path.extension() == ".gb" || path.extension() == ".gbc" || path.extension() == ".isx") {
                    return path;
                }
            }
        }
        return std::nullopt;
    }
    
    // Handle drag-and-drop
    void dragEnterEvent(QDragEnterEvent *event) override {
        if(validate_event(event).has_value()) {
            event->accept();
        }
    }
    void dragMoveEvent(QDragMoveEvent *event) override {
        if(validate_event(event).has_value()) {
            event->accept();
        }
    }
    void dropEvent(QDropEvent *event) override {
        auto path = validate_event(event);
        if(path.has_value()) {
            this->window->load_rom(path->string().c_str());
        }
    }
    
    GameWindow *window;
};

#define GET_ICON(what) QIcon::fromTheme(QStringLiteral(what))

GameWindow::GameWindow() {
    // Load settings
    QSettings settings;
    this->scaling = settings.value(SETTINGS_SCALE, this->scaling).toInt();
    this->show_fps = settings.value(SETTINGS_SHOW_FPS, this->show_fps).toBool();
    this->gb_model = static_cast<decltype(this->gb_model)>(settings.value(SETTINGS_GB_MODEL, static_cast<int>(this->gb_model)).toInt());
    this->recent_roms = settings.value(SETTINGS_RECENT_ROMS).toStringList();
    
    // Instantiate the gameboy
    this->instance = std::make_unique<GameInstance>(this->gb_model);
    this->instance->set_pixel_buffering_mode(static_cast<GameInstance::PixelBufferMode>(settings.value(SETTINGS_BUFFER_MODE, instance->get_pixel_buffering_mode()).toInt()));
    
    // Set window title and enable drag-n-dropping files
    this->setAcceptDrops(true);
    this->setWindowTitle("SameBoy DX");
    
    // Start setting up the menu bar
    QMenuBar *bar = new QMenuBar(this);
    this->setMenuBar(bar);
    
    // File menu
    auto *file_menu = bar->addMenu("File");
    connect(file_menu, &QMenu::aboutToShow, this, &GameWindow::action_showing_menu);
    connect(file_menu, &QMenu::aboutToHide, this, &GameWindow::action_hiding_menu);
    
    this->open_roms_action = file_menu->addAction("Open ROM...");
    this->open_roms_action->setShortcut(QKeySequence::Open);
    this->open_roms_action->setIcon(GET_ICON("document-open"));
    connect(this->open_roms_action, &QAction::triggered, this, &GameWindow::action_open_rom);
    
    this->recent_roms_menu = file_menu->addMenu("Recent ROMs");
    this->update_recent_roms_list();
    
    auto *save = file_menu->addAction("Save SRAM to Disk");
    save->setShortcut(QKeySequence::Save);
    save->setIcon(GET_ICON("document-save"));
    connect(save, &QAction::triggered, this, &GameWindow::action_save_sram);
    
    file_menu->addSeparator();
    
    this->exit_without_saving = file_menu->addAction("Quit Without Saving");
    this->exit_without_saving->setIcon(GET_ICON("application-exit"));
    connect(this->exit_without_saving, &QAction::triggered, this, &GameWindow::action_quit_without_saving);
    this->exit_without_saving->setEnabled(false);
    
    auto *quit = file_menu->addAction("Quit");
    quit->setShortcut(QKeySequence::Quit);
    quit->setIcon(GET_ICON("application-exit"));
    connect(quit, &QAction::triggered, this, &GameWindow::close);
    
    // Settings menu
    auto *edit_menu = bar->addMenu("Settings");
    connect(edit_menu, &QMenu::aboutToShow, this, &GameWindow::action_showing_menu);
    connect(edit_menu, &QMenu::aboutToHide, this, &GameWindow::action_hiding_menu);
    
    this->gameboy_model_menu = edit_menu->addMenu("Game Boy Model");
    std::pair<const char *, GB_model_t> models[] = {
        {"Game Boy", GB_model_t::GB_MODEL_DMG_B},
        {"Game Boy Color", GB_model_t::GB_MODEL_CGB_C},
        {"Game Boy Advance (GBC mode)", GB_model_t::GB_MODEL_AGB},
        {"Super Game Boy", GB_model_t::GB_MODEL_SGB},
        {"Super Game Boy 2", GB_model_t::GB_MODEL_SGB2},
    };
    for(auto &m : models) {
        auto *action = this->gameboy_model_menu->addAction(m.first);
        action->setData(static_cast<int>(m.second));
        action->setCheckable(true);
        action->setChecked(m.second == this->gb_model);
        connect(action, &QAction::triggered, this, &GameWindow::action_set_model);
        this->gb_model_actions.emplace_back(action);
    }

    edit_menu->addSeparator();
    
    // Volume list (increase/decrease and set volumes from 0 to 100)
    auto *volume = edit_menu->addMenu("Volume");
    
    auto *mute = volume->addAction("Mute");
    connect(mute, &QAction::triggered, this, &GameWindow::action_toggle_audio);
    mute->setIcon(GET_ICON("audio-volume-muted"));
    mute->setCheckable(true);
    bool muted = settings.value(SETTINGS_MUTE, false).toBool();
    mute->setChecked(muted);
    
    auto *raise_volume = volume->addAction("Increase Volume");
    raise_volume->setIcon(GET_ICON("audio-volume-high"));
    raise_volume->setShortcut(static_cast<int>(Qt::CTRL) + static_cast<int>(Qt::Key_Up));
    raise_volume->setData(10);
    connect(raise_volume, &QAction::triggered, this, &GameWindow::action_add_volume);
    
    auto *reduce_volume = volume->addAction("Decrease Volume");
    reduce_volume->setIcon(GET_ICON("audio-volume-low"));
    reduce_volume->setShortcut(static_cast<int>(Qt::CTRL) + static_cast<int>(Qt::Key_Down));
    reduce_volume->setData(-10);
    connect(reduce_volume, &QAction::triggered, this, &GameWindow::action_add_volume);

    this->instance->set_mono_forced(settings.value(SETTINGS_MONO, this->instance->is_mono_forced()).toBool());
    this->instance->set_volume(settings.value(SETTINGS_VOLUME, this->instance->get_volume()).toInt());
    
    volume->addSeparator();
    for(int i = 100; i >= 0; i-=10) {
        char text[5];
        std::snprintf(text, sizeof(text), "%i%%", i);
        auto *action = volume->addAction(text);
        action->setData(i);
        connect(action, &QAction::triggered, this, &GameWindow::action_set_volume);
        action->setCheckable(true);
        action->setChecked(i == this->instance->get_volume());
        this->volume_options.emplace_back(action);
    }
    
    // Channel count
    auto *channel_count = edit_menu->addMenu("Channel Count");
    auto *stereo = channel_count->addAction("Stereo");
    stereo->setData(2);
    stereo->setCheckable(true);
    stereo->setChecked(!this->instance->is_mono_forced());
    connect(stereo, &QAction::triggered, this, &GameWindow::action_set_channel_count);
    
    auto *mono = channel_count->addAction("Mono");
    mono->setData(1);
    mono->setCheckable(true);
    mono->setChecked(this->instance->is_mono_forced());
    connect(mono, &QAction::triggered, this, &GameWindow::action_set_channel_count);
    this->channel_count_options = { mono, stereo };
    
    edit_menu->addSeparator();
    
    // Add scaling options
    auto *scaling = edit_menu->addMenu("Render Scaling");
    for(int i = 8; i >= 1; i--) {
        char text[4];
        std::snprintf(text, sizeof(text), "%ix", i);
        auto *action = scaling->addAction(text);
        action->setData(i);
        connect(action, &QAction::triggered, this, &GameWindow::action_set_scaling);
        action->setCheckable(true);
        action->setChecked(i == this->scaling);
        this->scaling_options.emplace_back(action);
    }

    // Buffer modes
    auto *buffer_modes = edit_menu->addMenu("Pixel Buffer Mode");
    std::pair<const char *, GameInstance::PixelBufferMode> buffers[] = {
        {"Single Buffer", GameInstance::PixelBufferMode::PixelBufferSingle},
        {"Double Buffer", GameInstance::PixelBufferMode::PixelBufferDouble},
        {"Double Buffer + Interframe Blending", GameInstance::PixelBufferMode::PixelBufferDoubleBlend},
    };
    for(auto &i : buffers) {
        auto *action = buffer_modes->addAction(i.first);
        action->setData(i.second);
        connect(action, &QAction::triggered, this, &GameWindow::action_set_buffer_mode);
        action->setCheckable(true);
        action->setChecked(i.second == this->instance->get_pixel_buffering_mode());
        this->pixel_buffer_options.emplace_back(action);
    }
    
    edit_menu->addSeparator();
    
    // Add controls options
    auto *controls = edit_menu->addAction("Controls");
    connect(controls, &QAction::triggered, this, &GameWindow::action_edit_controls);
    
    
    // Emulation menu
    auto *emulation_menu = bar->addMenu("Emulation");
    connect(emulation_menu, &QMenu::aboutToShow, this, &GameWindow::action_showing_menu);
    connect(emulation_menu, &QMenu::aboutToHide, this, &GameWindow::action_hiding_menu);
    
    this->reset_rom_action = emulation_menu->addAction("Reset");
    connect(this->reset_rom_action, &QAction::triggered, this, &GameWindow::action_reset);
    this->reset_rom_action->setIcon(GET_ICON("view-refresh"));
    this->reset_rom_action->setEnabled(false);
    
    emulation_menu->addSeparator();
    
    // Pause options
    this->pause_action = emulation_menu->addAction("Pause");
    connect(this->pause_action, &QAction::triggered, this, &GameWindow::action_toggle_pause);
    this->pause_action->setIcon(GET_ICON("media-playback-pause"));
    this->pause_action->setCheckable(true);
    this->pause_action->setChecked(false);
    
    // Video menu
    auto *view_menu = bar->addMenu("View");
    connect(view_menu, &QMenu::aboutToShow, this, &GameWindow::action_showing_menu);
    connect(view_menu, &QMenu::aboutToHide, this, &GameWindow::action_hiding_menu);
    
    auto *toggle_fps = view_menu->addAction("Show FPS");
    connect(toggle_fps, &QAction::triggered, this, &GameWindow::action_toggle_showing_fps);
    toggle_fps->setCheckable(true);
    toggle_fps->setShortcut(static_cast<int>(Qt::Key_F3));
    
    // Here's the layout
    auto *central_widget = new QWidget(this);
    auto *layout = new QHBoxLayout(central_widget);
    
    // Set our pixel buffer parameters
    this->pixel_buffer_view = new GamePixelBufferView(central_widget, this);
    this->pixel_buffer_view->setFrameStyle(0);
    this->pixel_buffer_view->setHorizontalScrollBarPolicy(Qt::ScrollBarPolicy::ScrollBarAlwaysOff);
    this->pixel_buffer_view->setVerticalScrollBarPolicy(Qt::ScrollBarPolicy::ScrollBarAlwaysOff);
    this->pixel_buffer_view->setSizePolicy(QSizePolicy::Policy::Fixed, QSizePolicy::Policy::Fixed);
    this->set_pixel_view_scaling(this->scaling);
    
    // Create the debugger now that everything else is set up
    this->debugger_window = new GameDebugger(this);
    this->show_debugger = view_menu->addAction("Show Debugger");
    connect(this->show_debugger, &QAction::triggered, this->debugger_window, &GameDebugger::show);
    this->show_debugger->setEnabled(false);

    // If showing FPS, trigger it
    if(this->show_fps) {
        this->show_fps = false;
        this->action_toggle_showing_fps();
        toggle_fps->setChecked(true);
    }
    
    layout->addWidget(this->pixel_buffer_view);
    layout->setContentsMargins(0,0,0,0);
    central_widget->setLayout(layout);
    this->setCentralWidget(central_widget);

    // Audio
    this->sample_rate = 48000;
    this->instance->set_up_sdl_audio(this->sample_rate, settings.value(SETTINGS_SAMPLE_BUFFER_SIZE, this->sample_count).toUInt());
    this->instance->set_audio_enabled(!muted);
    
    // Detect gamepads changing
    connect(QGamepadManager::instance(), &QGamepadManager::connectedGamepadsChanged, this, &GameWindow::reload_devices);
    this->reload_devices();
    
    // Fire game_loop repeatedly
    this->game_thread_timer.callOnTimeout(this, &GameWindow::game_loop);
}

void GameWindow::action_set_volume() {
    // Uses the user data from the sender to get volume
    auto *action = qobject_cast<QAction *>(sender());
    int volume = action->data().toInt(); 
    this->instance->set_volume(volume);
    this->show_new_volume_text();
}

void GameWindow::action_set_channel_count() noexcept {
    // Uses the user data from the sender to get volume
    auto *action = qobject_cast<QAction *>(sender());
    int channel_count = action->data().toInt(); 
    this->instance->set_mono_forced(channel_count == 1);
    
    for(auto &i : this->channel_count_options) {
        i->setChecked(i->data().toInt() == channel_count);
    }
}

void GameWindow::increment_volume(int amount) {
    this->instance->set_volume(this->instance->get_volume() + amount);
    this->show_new_volume_text();
}

void GameWindow::action_add_volume() {
    // Uses the user data from the sender to get volume delta
    auto *action = qobject_cast<QAction *>(sender());
    this->increment_volume(action->data().toInt());
}

void GameWindow::show_new_volume_text() {
    char volume_text[256];
    int new_volume = this->instance->get_volume();
    std::snprintf(volume_text, sizeof(volume_text), "Volume: %i%%", new_volume);
    
    this->show_status_text(volume_text);
    
    for(auto *i : this->volume_options) {
        i->setChecked(new_volume == i->data().toInt());
    }
}

void GameWindow::load_rom(const char *rom_path) noexcept {
    if(!std::filesystem::exists(rom_path)) {
        this->show_status_text("Error: ROM not found");
        print_debug_message("Could not find %s\n", rom_path);
        return;
    }
    
    this->save_if_loaded();
    this->reset_rom_action->setEnabled(true);
    this->exit_without_saving->setEnabled(true);

    // Remove the ROM from the list (if present) so we can re-place it at the top of the list
    this->recent_roms.removeAll(rom_path);
    this->recent_roms.push_front(rom_path);
    this->recent_roms = this->recent_roms.mid(0, 5);    
    this->update_recent_roms_list();
    
    // Make path
    auto path = std::filesystem::path(rom_path);
    this->save_path = std::filesystem::path(path).replace_extension(".sav");
    
    // Reset
    int r;

    if(path.extension() == ".isx") {
        r = this->instance->load_isx(path, save_path, std::filesystem::path(path).replace_extension(".sym"));
    }
    else {
        r = this->instance->load_rom(path, save_path, std::filesystem::path(path).replace_extension(".sym"));
    }

    // Set timer and UI
    this->show_debugger->setEnabled(true);
    this->game_thread_timer.start();
    
    // Start thread
    if(r == 0 && !instance_thread.joinable()) {
        instance_thread = std::thread(GameInstance::start_game_loop, this->instance.get());
    }
}

void GameWindow::update_recent_roms_list() {
    // Regenerate the ROM menu
    this->recent_roms_menu->clear();
    if(this->recent_roms.empty()) {
        this->recent_roms_menu->addAction("No recent ROMs")->setDisabled(true);
    }
    else {
        for(auto &i : this->recent_roms) {
            auto *recent = this->recent_roms_menu->addAction(i);
            recent->setData(i);
            connect(recent, &QAction::triggered, this, &GameWindow::action_open_recent_rom);
        }
    }
}

void GameWindow::action_open_recent_rom() {
    auto *action = qobject_cast<QAction *>(sender());
    this->load_rom(action->data().toString().toUtf8().data());
}

void GameWindow::redraw_pixel_buffer() {
    std::uint32_t width, height;
    this->instance->get_dimensions(width, height);
    this->pixel_buffer.resize(width * height, 0xFFFFFFFF);
    this->instance->read_pixel_buffer(this->pixel_buffer.data(), this->pixel_buffer.size());
    
    this->pixel_buffer_pixmap.convertFromImage(QImage(reinterpret_cast<const uchar *>(this->pixel_buffer.data()), width, height, QImage::Format::Format_ARGB32));
    pixel_buffer_pixmap_item->setPixmap(this->pixel_buffer_pixmap);
    
    // Handle status text fade
    if(this->status_text) {
        auto now = clock::now();
        
        // Delete status text if time expired
        if(now > this->status_text_deletion) {
            delete this->status_text;
            this->status_text = nullptr;
        }
        
        // Otherwise fade out in last 500 ms
        else {
            auto ms_left = std::chrono::duration_cast<std::chrono::milliseconds>(this->status_text_deletion - now).count();
            static constexpr const double fade_ms = 500.0;
            if(ms_left < fade_ms) {
                float opacity = ms_left / fade_ms;
                this->status_text->setOpacity(opacity);
                qobject_cast<QGraphicsDropShadowEffect *>(this->status_text->graphicsEffect())->setColor(QColor::fromRgbF(0,0,0,opacity * opacity));
            }
        }
    }
    
    // Show frame rate
    if(this->fps_text) {
        float fps = this->instance->get_frame_rate();
        if(this->last_fps != fps) {
            char fps_text_str[64];
            if(fps == 0.0) {
                std::snprintf(fps_text_str, sizeof(fps_text_str), "FPS: --");
            }
            else {
                std::snprintf(fps_text_str, sizeof(fps_text_str), "FPS: %.01f", fps);
            }
            this->fps_text->setPlainText(fps_text_str);
            this->last_fps = fps;
        }
    }
}

void GameWindow::set_pixel_view_scaling(int scaling) {
    // Reinitialize the scene
    this->pixel_buffer_pixmap = {};
    auto *new_scene = new QGraphicsScene(this->pixel_buffer_view);
    auto *new_pixmap = new_scene->addPixmap(this->pixel_buffer_pixmap);
    if(this->pixel_buffer_scene) {
        delete this->pixel_buffer_pixmap_item;
        auto items = this->pixel_buffer_scene->items();
        for(auto &i : items) {
            new_scene->addItem(i);
        }
    }
    this->pixel_buffer_pixmap_item = new_pixmap;
    this->pixel_buffer_scene = new_scene;
    this->pixel_buffer_view->setScene(this->pixel_buffer_scene);
    
    this->scaling = scaling;
    
    std::uint32_t width, height;
    this->instance->get_dimensions(width, height);
    this->pixel_buffer_view->setMinimumSize(width * this->scaling, height * this->scaling);
    this->pixel_buffer_view->setMaximumSize(width * this->scaling, height * this->scaling);
    this->pixel_buffer_view->setTransform(QTransform::fromScale(scaling, scaling));
    this->make_shadow(this->fps_text);
    this->make_shadow(this->status_text);
    this->redraw_pixel_buffer();
    
    this->setFixedSize(this->pixel_buffer_view->maximumWidth(), this->pixel_buffer_view->maximumHeight() + this->menuBar()->height());
    
    // Go through all scaling options. Uncheck/check whatever applies.
    for(auto *option : this->scaling_options) {
        option->setChecked(option->data().toInt() == scaling);
    }
}

void GameWindow::game_loop() {
    this->redraw_pixel_buffer();
    this->debugger_window->refresh_view();

    // If we're paused, we don't need to fire as often since not as much information is being changed
    if(this->instance->is_paused()) {
        this->game_thread_timer.setInterval(50);
    }

    // Otherwise, we should fire quickly
    else {
        this->game_thread_timer.setInterval(5);
    }
}

void GameWindow::action_set_scaling() noexcept {
    // Uses the user data from the sender to get scaling
    auto *action = qobject_cast<QAction *>(sender());
    this->set_pixel_view_scaling(action->data().toInt());
}


void GameWindow::make_shadow(QGraphicsTextItem *object) {
    if(object == nullptr) {
        return;
    }
    
    auto *effect = new QGraphicsDropShadowEffect(object);
    effect->setColor(QColor::fromRgb(0,0,0));
    effect->setXOffset(std::max(this->scaling / 2, 1));
    effect->setYOffset(std::max(this->scaling / 2, 1));
    effect->setBlurRadius(0);
    object->setGraphicsEffect(effect);
}

void GameWindow::action_toggle_showing_fps() noexcept {
    this->show_fps = !this->show_fps;
    
    // If showing frame rate, create text objects and initialize the FPS counter
    if(this->show_fps) {
        auto font = QFontDatabase::systemFont(QFontDatabase::FixedFont);
        font.setPixelSize(9);
        
        this->fps_text = this->pixel_buffer_scene->addText("", font);
        fps_text->setDefaultTextColor(QColor::fromRgb(255,255,0));
        fps_text->setPos(0, 0);
        this->make_shadow(this->fps_text);
        this->last_fps = -1.0;
    }
    else {
        delete this->fps_text;
        this->fps_text = nullptr;
    }
}

void GameWindow::action_toggle_pause() noexcept {
    this->instance->set_paused_manually(!this->instance->is_paused_manually());
}

void GameWindow::action_open_rom() noexcept {
    QFileDialog qfd;
    qfd.setNameFilters(QStringList { "Game Boy ROM (*.gb)", "Game Boy Color ROM (*.gbc)", "ISX Binary (*.isx)" });
    
    if(qfd.exec() == QDialog::DialogCode::Accepted) {
        this->load_rom(qfd.selectedFiles().at(0).toUtf8().data());
    }
}

void GameWindow::action_reset() noexcept {
    this->instance->reset();
}

void GameWindow::action_toggle_audio() noexcept {
    auto muted = !this->instance->is_audio_enabled();
    muted = !muted;

    this->instance->set_audio_enabled(!muted);
    
    if(muted) {
        this->show_status_text("Muted");
    }
    else {
        this->show_status_text("Unmuted");
    }
}

void GameWindow::show_status_text(const char *text) {
    if(this->status_text) {
        delete this->status_text;
    }
    
    auto font = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    font.setPixelSize(9);
    
    this->status_text = this->pixel_buffer_scene->addText(text, font);
    status_text->setDefaultTextColor(QColor::fromRgb(255,255,0));
    status_text->setPos(0, 12);
    this->make_shadow(this->status_text);
    
    this->status_text_deletion = clock::now() + std::chrono::seconds(3);
}

void GameWindow::handle_keyboard_key(QKeyEvent *event, bool press) {
    for(auto &i : this->devices) {
        auto *dev = dynamic_cast<InputDeviceKeyboard *>(i.get());
        if(dev) {
            dev->handle_key_event(event, press);
        }
    }
    
    event->ignore();
}

void GameWindow::keyPressEvent(QKeyEvent *event) {
    if(event->isAutoRepeat()) {
        return;
    }
    handle_keyboard_key(event, true);
}

void GameWindow::keyReleaseEvent(QKeyEvent *event) {
    if(event->isAutoRepeat()) {
        return;
    }
    handle_keyboard_key(event, false);
}

void GameWindow::action_showing_menu() noexcept {
    this->menu_open = true;
}
void GameWindow::action_hiding_menu() noexcept {
    this->menu_open = false;
}

bool GameWindow::save_if_loaded() noexcept {
    if(this->instance->is_rom_loaded()) {
        if(this->instance->save_sram(this->save_path) == 0) {
            print_debug_message("Saved cartridge RAM to %s\n", save_path.string().c_str());
            return true;
        }
        else {
            print_debug_message("Failed to save %s\n", save_path.string().c_str());
            return false;
        }
    }
    else {
        print_debug_message("Save cancelled since no ROM was loaded\n");
        return false;
    }
}
    
void GameWindow::action_save_sram() noexcept {
    // Initiate saving the SRAM
    auto filename = this->save_path.filename().string();
    if(!this->save_if_loaded()) {
        if(this->instance->is_rom_loaded()) {
            char message[256];
            std::snprintf(message, sizeof(message), "Failed to save %s", filename.c_str());
            this->show_status_text(message);
        }
        else {
            this->show_status_text("Can't save - no ROM loaded");
        }
    }
    else {
        this->show_status_text("SRAM saved");
    }
}

void GameWindow::action_set_model() noexcept {
    // Uses the user data from the sender to get model
    auto *action = qobject_cast<QAction *>(sender());
    this->gb_model = static_cast<decltype(this->gb_model)>(action->data().toInt());
    this->instance->set_model(this->gb_model);
    
    for(auto &i : this->gb_model_actions) {
        i->setChecked(i->data().toInt() == this->gb_model);
    }
    
    this->set_pixel_view_scaling(this->scaling);
}

void GameWindow::action_quit_without_saving() noexcept {
    QMessageBox qmb(QMessageBox::Icon::Question, "Are you sure?", "This will close the emulator without saving your SRAM.\n\nAny save data that has not been saved to disk will be lost.", QMessageBox::Cancel | QMessageBox::Ok);
    qmb.setDefaultButton(QMessageBox::Cancel);
    
    if(qmb.exec() == QMessageBox::Ok) {
        this->exit_without_save = true;
        this->close();
    }
}

void GameWindow::set_loading_other_roms_enabled(bool enabled) noexcept {
    this->recent_roms_menu->setEnabled(enabled);
    this->reset_rom_action->setEnabled(enabled);
    this->open_roms_action->setEnabled(enabled);
    this->gameboy_model_menu->setEnabled(enabled);
    this->pause_action->setEnabled(enabled);
}

GameWindow::~GameWindow() {
    if(this->instance_thread.joinable()) {
        this->instance->end_game_loop();
        this->instance_thread.join();
    }
}

void GameWindow::closeEvent(QCloseEvent *) {
    if(!this->exit_without_save) {
        this->save_if_loaded();
    }
    
    QSettings settings;
    settings.setValue(SETTINGS_VOLUME, this->instance->get_volume());
    settings.setValue(SETTINGS_SCALE, this->scaling);
    settings.setValue(SETTINGS_SHOW_FPS, this->show_fps);
    settings.setValue(SETTINGS_MONO, this->instance->is_mono_forced());
    settings.setValue(SETTINGS_MUTE, !this->instance->is_audio_enabled());
    settings.setValue(SETTINGS_RECENT_ROMS, this->recent_roms);
    settings.setValue(SETTINGS_GB_MODEL, static_cast<int>(this->gb_model));
    settings.setValue(SETTINGS_SAMPLE_BUFFER_SIZE, this->sample_count);
    settings.setValue(SETTINGS_SAMPLE_RATE, this->sample_rate);
    settings.setValue(SETTINGS_BUFFER_MODE, instance->get_pixel_buffering_mode());
    
    QApplication::quit();
}

void GameWindow::action_edit_controls() noexcept {
    EditControlsDialog d;
    this->disable_input = true;
    d.exec();
    this->disable_input = false;
    this->reload_devices();
}

std::vector<std::unique_ptr<InputDevice>> GameWindow::get_all_devices() {
    std::vector<std::unique_ptr<InputDevice>> devices;
    devices.emplace_back(std::make_unique<InputDeviceKeyboard>());
    for(auto &i : QGamepadManager::instance()->connectedGamepads()) {
        devices.emplace_back(std::make_unique<InputDeviceGamepad>(i));
    }
    return devices;
}

void GameWindow::reload_devices() {
    this->devices.clear();
    this->devices = this->get_all_devices();
    print_debug_message("Loading %zu devices\n", this->devices.size());
    for(auto &d : this->devices) {
        connect(d.get(), &InputDevice::input, this, &GameWindow::handle_device_input);
    }
}

void GameWindow::handle_device_input(InputDevice::InputType type, double input) {
    if(this->disable_input) {
        return;
    }
    
    bool boolean_input = input >= 0.5;
    
    switch(type) {
        case InputDevice::Input_A:
            this->instance->set_button_state(GB_key_t::GB_KEY_A, boolean_input);
            break;
        case InputDevice::Input_B:
            this->instance->set_button_state(GB_key_t::GB_KEY_B, boolean_input);
            break;
        case InputDevice::Input_Start:
            this->instance->set_button_state(GB_key_t::GB_KEY_START, boolean_input);
            break;
        case InputDevice::Input_Select:
            this->instance->set_button_state(GB_key_t::GB_KEY_SELECT, boolean_input);
            break;
        case InputDevice::Input_Up:
            this->instance->set_button_state(GB_key_t::GB_KEY_UP, boolean_input);
            break;
        case InputDevice::Input_Down:
            this->instance->set_button_state(GB_key_t::GB_KEY_DOWN, boolean_input);
            break;
        case InputDevice::Input_Left:
            this->instance->set_button_state(GB_key_t::GB_KEY_LEFT, boolean_input);
            break;
        case InputDevice::Input_Right:
            this->instance->set_button_state(GB_key_t::GB_KEY_RIGHT, boolean_input);
            break;
        case InputDevice::Input_Turbo:
            if(input > 0.1) {
                this->instance->set_speed_multiplier(1.0 + 3.0 * ((input - 0.1) / 0.9)); // TODO: allow you to set the maximum turbo
            }
            else {
                this->instance->set_speed_multiplier(1.0);
            }
            break;
        case InputDevice::Input_Slowmo:
            if(input > 0.1) {
                this->instance->set_speed_multiplier(1.0 / (1.0 + 3.0 * ((input - 0.1) / 0.9))); // TODO: allow you to set the maximum slowmotion
            }
            else {
                this->instance->set_speed_multiplier(1.0);
            }
            break;
        case InputDevice::Input_VolumeDown:
            if(boolean_input) {
                this->increment_volume(-5);
            }
            break;
        case InputDevice::Input_VolumeUp:
            if(boolean_input) {
                this->increment_volume(5);
            }
            break;
        default: break;
    }
}

void GameWindow::action_set_buffer_mode() noexcept {
    auto *action = qobject_cast<QAction *>(sender());
    auto mode = static_cast<GameInstance::PixelBufferMode>(action->data().toInt());
    this->instance->set_pixel_buffering_mode(mode);

    for(auto &i : this->pixel_buffer_options) {
        i->setChecked(i->data().toInt() == mode);
    }
}

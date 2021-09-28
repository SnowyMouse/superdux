#include "game_window.hpp"
#include <QHBoxLayout>
#include <QTimer>
#include <QMenuBar>
#include <QGraphicsTextItem>
#include <filesystem>
#include <QFontDatabase>
#include <QFileDialog>
#include <cstring>
#include <QAudioDeviceInfo>
#include <QAudioOutput>
#include <QGamepad>
#include <QKeyEvent>
#include <QGamepadManager>
#include <chrono>

#include <agb_boot.h>
#include <sgb_boot.h>
#include <cgb_boot.h>
#include <dmg_boot.h>
#include <sgb2_boot.h>

static void load_boot_rom(GB_gameboy_t *gb, GB_boot_rom_t type) {
    switch(type) {
        case GB_BOOT_ROM_DMG0:
        case GB_BOOT_ROM_DMG:
            GB_load_boot_rom_from_buffer(gb, dmg_boot, sizeof(dmg_boot));
            break;
        case GB_BOOT_ROM_SGB2:
            GB_load_boot_rom_from_buffer(gb, sgb2_boot, sizeof(sgb2_boot));
            break;
        case GB_BOOT_ROM_SGB:
            GB_load_boot_rom_from_buffer(gb, sgb_boot, sizeof(sgb_boot));
            break;
        case GB_BOOT_ROM_AGB:
            GB_load_boot_rom_from_buffer(gb, agb_boot, sizeof(agb_boot));
            break;
        case GB_BOOT_ROM_CGB0:
        case GB_BOOT_ROM_CGB:
            GB_load_boot_rom_from_buffer(gb, cgb_boot, sizeof(cgb_boot));
            break;
        default:
            std::fprintf(stderr, "cannot find a suitable boot rom\n");
            break;
    }
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
    connect(file_menu, &QMenu::aboutToShow, this, &GameWindow::action_showing_menu);
    connect(file_menu, &QMenu::aboutToHide, this, &GameWindow::action_hiding_menu);
    
    auto *open = file_menu->addAction("Open ROM...");
    open->setShortcut(QKeySequence::Open);
    open->setIcon(GET_ICON("document-open"));
    connect(open, &QAction::triggered, this, &GameWindow::action_open_rom);
    
    auto *save = file_menu->addAction("Save battery");
    save->setShortcut(QKeySequence::Save);
    save->setIcon(GET_ICON("document-save"));
    connect(save, &QAction::triggered, this, &GameWindow::action_save_battery);
    
    file_menu->addSeparator();
    
    auto *quit = file_menu->addAction("Quit");
    quit->setShortcut(QKeySequence::Quit);
    quit->setIcon(GET_ICON("application-exit"));
    connect(quit, &QAction::triggered, this, &GameWindow::close);
    
    
    // Emulation menu
    auto *emulation_menu = bar->addMenu("Emulation");
    connect(emulation_menu, &QMenu::aboutToShow, this, &GameWindow::action_showing_menu);
    connect(emulation_menu, &QMenu::aboutToHide, this, &GameWindow::action_hiding_menu);
    
    auto *pause = emulation_menu->addAction("Pause");
    connect(pause, &QAction::triggered, this, &GameWindow::action_toggle_pause);
    pause->setIcon(GET_ICON("media-playback-pause"));
    pause->setCheckable(true);
    pause->setChecked(this->paused);
    auto *reset = emulation_menu->addAction("Reset");
    connect(reset, &QAction::triggered, this, &GameWindow::action_reset);
    reset->setIcon(GET_ICON("view-refresh"));
    
    emulation_menu->addSeparator();
    auto *pause_on_menu = emulation_menu->addAction("Pause if menu is open");
    connect(pause_on_menu, &QAction::triggered, this, &GameWindow::action_toggle_pause_in_menu);
    pause_on_menu->setIcon(GET_ICON("media-playback-pause"));
    pause_on_menu->setCheckable(true);
    pause_on_menu->setChecked(this->pause_on_menu);
    
    // Audio menu
    auto *audio_menu = bar->addMenu("Audio");
    connect(audio_menu, &QMenu::aboutToShow, this, &GameWindow::action_showing_menu);
    connect(audio_menu, &QMenu::aboutToHide, this, &GameWindow::action_hiding_menu);
    
    auto *mute = audio_menu->addAction("Mute");
    connect(mute, &QAction::triggered, this, &GameWindow::action_toggle_audio);
    mute->setIcon(GET_ICON("audio-volume-muted"));
    mute->setCheckable(true);
    
    auto *volume = audio_menu->addMenu("Volume");
    
    auto *raise_volume = volume->addAction("Increase volume");
    raise_volume->setIcon(GET_ICON("audio-volume-high"));
    raise_volume->setShortcut(static_cast<int>(Qt::CTRL) + static_cast<int>(Qt::Key_Up));
    raise_volume->setData(10);
    connect(raise_volume, &QAction::triggered, this, &GameWindow::action_add_volume);
    
    auto *reduce_volume = volume->addAction("Decrease volume");
    reduce_volume->setIcon(GET_ICON("audio-volume-low"));
    reduce_volume->setShortcut(static_cast<int>(Qt::CTRL) + static_cast<int>(Qt::Key_Down));
    reduce_volume->setData(-10);
    connect(reduce_volume, &QAction::triggered, this, &GameWindow::action_add_volume);
    
    volume->addSeparator();
    for(int i = 100; i >= 0; i-=10) {
        char text[5];
        std::snprintf(text, sizeof(text), "%i%%", i);
        auto *action = volume->addAction(text);
        action->setData(i);
        connect(action, &QAction::triggered, this, &GameWindow::action_set_volume);
        action->setCheckable(true);
        action->setChecked(i == this->volume);
        this->volume_options.emplace_back(action);
    }
    
    // Video menu
    auto *video_menu = bar->addMenu("Video");
    connect(video_menu, &QMenu::aboutToShow, this, &GameWindow::action_showing_menu);
    connect(video_menu, &QMenu::aboutToHide, this, &GameWindow::action_hiding_menu);
    
    auto *toggle_fps = video_menu->addAction("Show FPS");
    connect(toggle_fps, &QAction::triggered, this, &GameWindow::action_toggle_showing_fps);
    toggle_fps->setCheckable(true);
    toggle_fps->setShortcut(static_cast<int>(Qt::Key_F3));

    // Add scaling options
    auto *scaling = video_menu->addMenu("Scaling");
    for(int i = 1; i <= 8; i *= 2) {
        char text[4];
        std::snprintf(text, sizeof(text), "%ix", i);
        auto *action = scaling->addAction(text);
        action->setData(i);
        connect(action, &QAction::triggered, this, &GameWindow::action_set_scaling);
        action->setCheckable(true);
        action->setChecked(i == this->scaling);
        
        scaling_options.emplace_back(action);
    }
    
    
    auto *central_widget = new QWidget(this);
    auto *layout = new QHBoxLayout(central_widget);
    
    this->pixel_buffer_view = new QGraphicsView(central_widget);
    this->pixel_buffer_view->setEnabled(false);
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
    layout->setContentsMargins(0,0,0,0);
    central_widget->setLayout(layout);
    this->setCentralWidget(central_widget);
    
    // Remove any garbage pixels
    this->redraw_pixel_buffer();
    
    // Get the audio
    QAudioFormat format = {};
    format.setChannelCount(2);
    format.setSampleRate(44100);
    format.setSampleSize(16);
    format.setCodec("audio/pcm");
    format.setByteOrder(QAudioFormat::LittleEndian);
    format.setSampleType(QAudioFormat::SignedInt);

    // Check if it works
    QAudioDeviceInfo info = QAudioDeviceInfo::defaultOutputDevice();
    if(info.isFormatSupported(format)) {
        int best_sample_rate = 0;
        for(auto &i : info.supportedSampleRates()) {
            if(i > best_sample_rate && i <= 96000) {
                best_sample_rate = i;
            }
        }
        format.setSampleRate(best_sample_rate);
        
        this->audio_output = new QAudioOutput(format /*, this */); // TODO: FIGURE OUT WHY UNCOMMENTING THIS CRASHES WHEN YOU CLOSE THE WINDOW
        this->audio_output->setNotifyInterval(1);
        connect(this->audio_output, &QAudioOutput::notify, this, &GameWindow::play_audio_buffer); // play the audio buffer whenever possible
        this->audio_device = this->audio_output->start();
        
        GB_set_sample_rate(&this->gameboy, best_sample_rate);
        GB_apu_set_sample_callback(&this->gameboy, GameWindow::on_sample);
        
        sample_buffer.reserve(1024);
    }
    else {
        std::fprintf(stderr, "Could not get an audio device. Audio will be disabled.\n");
    }
    
    connect(QGamepadManager::instance(), &QGamepadManager::connectedGamepadsChanged, this, &GameWindow::action_gamepads_changed);
    this->action_gamepads_changed();
    
    // Fire game_loop as often as possible
    QTimer *timer = new QTimer(this);
    timer->callOnTimeout(this, &GameWindow::game_loop);
    timer->start();
}

void GameWindow::action_set_volume() {
    // Uses the user data from the sender to get volume
    auto *action = qobject_cast<QAction *>(sender());
    int volume = action->data().toInt(); 
    this->volume = volume;
    this->show_new_volume_text();
}

void GameWindow::action_add_volume() {
    // Uses the user data from the sender to get volume delta
    auto *action = qobject_cast<QAction *>(sender());
    int volume = this->volume + action->data().toInt(); 
    this->volume = std::max(std::min(volume, 100), 0);
    this->show_new_volume_text();
}

void GameWindow::show_new_volume_text() {
    char volume_text[256];
    std::snprintf(volume_text, sizeof(volume_text), "Volume: %i%%", volume);
    
    this->show_status_text(volume_text);
    
    for(auto *i : this->volume_options) {
        i->setChecked(volume == i->data().toInt());
    }
}

void GameWindow::on_sample(GB_gameboy_s *gb, GB_sample_t *sample) {
    auto *window = reinterpret_cast<GameWindow *>(GB_get_user_data(gb));
    
    if(window->muted) {
        return;
    }
    
    auto &buffer = window->sample_buffer;
    
    if(window->volume == 100) {
        // If volume is 100, do not scale anything
        buffer.emplace_back(sample->left);
        buffer.emplace_back(sample->right);
    }
    else {
        // Scale samples
        double scale = QAudio::convertVolume(window->volume / 100.0, QAudio::VolumeScale::LogarithmicVolumeScale, QAudio::VolumeScale::LinearVolumeScale);
        buffer.emplace_back(sample->left * scale);
        buffer.emplace_back(sample->right * scale);
    }
    
    window->play_audio_buffer();
}

void GameWindow::play_audio_buffer() {
    auto &buffer = this->sample_buffer;
    
    std::size_t sample_count = buffer.size();
    auto *sample_data = buffer.data();
    
    std::size_t bytes_available = sample_count * sizeof(*sample_data);
    std::size_t period_size = this->audio_output->periodSize() * sizeof(*sample_data);
    
    if(bytes_available > period_size) {
        this->audio_device->write(reinterpret_cast<const char *>(sample_data), period_size);
        buffer.erase(buffer.begin(), buffer.begin() + period_size / sizeof(*sample_data));
    }
}

void GameWindow::load_rom(const char *rom_path) noexcept {
    this->rom_loaded = true;
    GB_load_rom(&this->gameboy, rom_path);
    save_path = std::filesystem::path(rom_path).replace_extension(".sav").string();
    GB_load_battery(&this->gameboy, save_path.c_str());
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
    auto now = clock::now();
    
    if(this->status_text != nullptr && now > this->status_text_deletion) {
        delete this->status_text;
        delete this->status_text_shadow;
        
        this->status_text = nullptr;
        this->status_text_shadow = nullptr;
    }
    
    if(!this->rom_loaded || this->paused || (this->pause_on_menu && this->menu_open)) {
        return;
    }
    
    // Run until vblank occurs or we take waaaay too long (e.g. 50 ms)
    auto timeout = now + std::chrono::milliseconds(50);
    
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

void GameWindow::action_toggle_audio() noexcept {
    this->muted = !this->muted;
    this->sample_buffer.clear();
    
    if(this->muted) {
        this->show_status_text("Muted");
    }
    else {
        this->show_status_text("Unmuted");
    }
}

void GameWindow::show_status_text(const char *text) {
    if(this->status_text) {
        delete this->status_text;
        delete this->status_text_shadow;
    }
    
    this->status_text_shadow = this->pixel_buffer_scene->addText(text, QFontDatabase::systemFont(QFontDatabase::FixedFont));
    status_text_shadow->setDefaultTextColor(QColor::fromRgb(0,0,0));
    status_text_shadow->setPos(1, 16);
    
    this->status_text = this->pixel_buffer_scene->addText(text, QFontDatabase::systemFont(QFontDatabase::FixedFont));
    status_text->setDefaultTextColor(QColor::fromRgb(255,255,0));
    status_text->setPos(0, 15);
    
    this->status_text_deletion = clock::now() + std::chrono::seconds(3);
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

void GameWindow::action_gamepads_changed() {
    delete this->gamepad;
    this->gamepad = nullptr;
    for(auto &i : QGamepadManager::instance()->connectedGamepads()) {
        this->gamepad = new QGamepad(i);
        connect(this->gamepad, &QGamepad::buttonAChanged, this, &GameWindow::action_gamepad_a);
        connect(this->gamepad, &QGamepad::buttonBChanged, this, &GameWindow::action_gamepad_b);
        connect(this->gamepad, &QGamepad::buttonStartChanged, this, &GameWindow::action_gamepad_start);
        connect(this->gamepad, &QGamepad::buttonSelectChanged, this, &GameWindow::action_gamepad_select);
        connect(this->gamepad, &QGamepad::buttonUpChanged, this, &GameWindow::action_gamepad_up);
        connect(this->gamepad, &QGamepad::buttonDownChanged, this, &GameWindow::action_gamepad_down);
        connect(this->gamepad, &QGamepad::buttonLeftChanged, this, &GameWindow::action_gamepad_left);
        connect(this->gamepad, &QGamepad::buttonRightChanged, this, &GameWindow::action_gamepad_right);
        connect(this->gamepad, &QGamepad::axisLeftXChanged, this, &GameWindow::action_gamepad_axis_x);
        connect(this->gamepad, &QGamepad::axisRightXChanged, this, &GameWindow::action_gamepad_axis_x);
        connect(this->gamepad, &QGamepad::axisLeftYChanged, this, &GameWindow::action_gamepad_axis_y);
        connect(this->gamepad, &QGamepad::axisRightYChanged, this, &GameWindow::action_gamepad_axis_y);
    }
}

#define ACTION_GAMEPAD(fn, KEY) void GameWindow::fn(bool button) noexcept {\
    GB_set_key_state(&this->gameboy, GB_key_t::KEY, button);\
}

ACTION_GAMEPAD(action_gamepad_a, GB_KEY_A)
ACTION_GAMEPAD(action_gamepad_b, GB_KEY_B)
ACTION_GAMEPAD(action_gamepad_start, GB_KEY_START)
ACTION_GAMEPAD(action_gamepad_select, GB_KEY_SELECT)

#define ACTION_GAMEPAD_DPAD(fn, KEY_ON, KEY_OFF) void GameWindow::fn(bool button) noexcept {\
    GB_set_key_state(&this->gameboy, GB_key_t::KEY_ON, button);\
    if(button) {\
        GB_set_key_state(&this->gameboy, GB_key_t::KEY_OFF, false); /*prevent impossible inputs*/\
    }\
}

ACTION_GAMEPAD_DPAD(action_gamepad_up, GB_KEY_UP, GB_KEY_DOWN)
ACTION_GAMEPAD_DPAD(action_gamepad_down, GB_KEY_DOWN, GB_KEY_UP)
ACTION_GAMEPAD_DPAD(action_gamepad_left, GB_KEY_LEFT, GB_KEY_RIGHT)
ACTION_GAMEPAD_DPAD(action_gamepad_right, GB_KEY_RIGHT, GB_KEY_LEFT)

void GameWindow::action_gamepad_axis_x(double axis) noexcept {
    if(axis < 0.0) {
        GB_set_key_state(&this->gameboy, GB_key_t::GB_KEY_RIGHT, false); // prevent impossible inputs
        GB_set_key_state(&this->gameboy, GB_key_t::GB_KEY_LEFT, axis < -0.5);
    }
    else {
        GB_set_key_state(&this->gameboy, GB_key_t::GB_KEY_RIGHT, axis > 0.5);
        GB_set_key_state(&this->gameboy, GB_key_t::GB_KEY_LEFT, false);
    }
}
void GameWindow::action_gamepad_axis_y(double axis) noexcept {
    if(axis < 0.0) {
        GB_set_key_state(&this->gameboy, GB_key_t::GB_KEY_DOWN, false);
        GB_set_key_state(&this->gameboy, GB_key_t::GB_KEY_UP, axis < -0.5);
    }
    else {
        GB_set_key_state(&this->gameboy, GB_key_t::GB_KEY_DOWN, axis > 0.5);
        GB_set_key_state(&this->gameboy, GB_key_t::GB_KEY_UP, false);
    }
}

// TODO: Add configuration. 
void GameWindow::handle_keyboard_key(QKeyEvent *event, bool press) {
    switch(event->key()) {
        case Qt::Key::Key_Up:
            action_gamepad_up(press);
            break;
        case Qt::Key::Key_Down:
            action_gamepad_down(press);
            break;
        case Qt::Key::Key_Left:
            action_gamepad_left(press);
            break;
        case Qt::Key::Key_Right:
            action_gamepad_right(press);
            break;
        case Qt::Key::Key_X:
            action_gamepad_a(press);
            break;
        case Qt::Key::Key_Z:
            action_gamepad_b(press);
            break;
        case Qt::Key::Key_Return:
            action_gamepad_start(press);
            break;
        case Qt::Key::Key_Shift:
            action_gamepad_select(press);
            break;
        default:
            break;
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

void GameWindow::action_toggle_pause_in_menu() noexcept {
    this->pause_on_menu = !this->pause_on_menu;
}

void GameWindow::action_save_battery() noexcept {
    if(this->rom_loaded) {
        GB_save_battery(&this->gameboy, save_path.c_str());
        this->show_status_text("Saved cartridge RAM");
    }
}

GameWindow::~GameWindow() {
    this->action_save_battery();
}

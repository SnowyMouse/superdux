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
#include <QKeyEvent>
#include <QApplication>
#include <QGraphicsDropShadowEffect>
#include <chrono>
#include <QMessageBox>
#include <QMimeData>
#include "edit_advanced_game_boy_model_dialog.hpp"
#include "edit_speed_control_settings_dialog.hpp"

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
#define SETTINGS_RTC_MODE "rtc_mode"
#define SETTINGS_COLOR_CORRECTION_MODE "color_correction_mode"
#define SETTINGS_TEMPORARY_SAVE_BUFFER_LENGTH "temporary_save_buffer_length"
#define SETTINGS_HIGHPASS_FILTER_MODE "highpass_filter_mode"
#define SETTINGS_RUMBLE_MODE "rumble_mode"
#define SETTINGS_STATUS_TEXT_HIDDEN "status_text_hidden"
#define SETTINGS_REWIND_LENGTH "rewind_length"
#define SETTINGS_MAX_TURBO "max_turbo"
#define SETTINGS_MAX_SLOWMO "max_slowmo"

#define SETTINGS_TURBO_ENABLED "turbo_enabled"
#define SETTINGS_SLOWMO_ENABLED "slowmo_enabled"
#define SETTINGS_REWIND_ENABLED "rewind_enabled"

#define SETTINGS_GB_BOOT_ROM "gb_boot_rom"
#define SETTINGS_GBC_BOOT_ROM "gbc_boot_rom"
#define SETTINGS_GBA_BOOT_ROM "gba_boot_rom"
#define SETTINGS_SGB_BOOT_ROM "sgb_boot_rom"
#define SETTINGS_SGB2_BOOT_ROM "sgb2_boot_rom"

#define SETTINGS_GB_REVISION "gb_model_revision"
#define SETTINGS_GBC_REVISION "gbc_model_revision"
#define SETTINGS_GBA_REVISION "gba_model_revision"
#define SETTINGS_SGB_REVISION "sgb_model_revision"
#define SETTINGS_SGB2_REVISION "sgb2_model_revision"

#define SETTINGS_SGB_CROP_BORDER "sgb_crop_border"
#define SETTINGS_SGB2_CROP_BORDER "sgb2_crop_border"

#define SETTINGS_GBC_FAST_BOOT "gbc_fast_boot_rom"

#define SGB_WIDTH 256
#define SGB_HEIGHT 224
#define GB_WIDTH 160
#define GB_HEIGHT 144

#ifdef DEBUG
#define print_debug_message(...) std::printf("Debug: " __VA_ARGS__)
#else
#define print_debug_message(...) (void(0))
#endif

#define MAKE_MODE_SETTER_WITH_VARIABLE(instance_fn, variable, array) { \
    auto *action = qobject_cast<QAction *>(sender()); \
    auto mode = static_cast<decltype(variable)>(action->data().toInt()); \
    this->instance->instance_fn(mode); \
    variable = mode; \
    \
    for(auto &i : array) { \
        i->setChecked(i->data().toInt() == mode); \
    } \
}

void GameWindow::action_set_rtc_mode() noexcept MAKE_MODE_SETTER_WITH_VARIABLE(set_rtc_mode, this->rtc_mode, this->rtc_mode_options)
void GameWindow::action_set_highpass_filter_mode() noexcept MAKE_MODE_SETTER_WITH_VARIABLE(set_highpass_filter_mode, this->highpass_filter_mode, this->highpass_filter_mode_options)
void GameWindow::action_set_rumble_mode() noexcept MAKE_MODE_SETTER_WITH_VARIABLE(set_rumble_mode, this->rumble_mode, this->rumble_mode_options)
void GameWindow::action_set_color_correction_mode() noexcept MAKE_MODE_SETTER_WITH_VARIABLE(set_color_correction_mode, this->color_correction_mode, this->color_correction_mode_options)

void GameWindow::action_set_channel_count() noexcept {
    // Uses the user data from the sender to get volume
    auto *action = qobject_cast<QAction *>(sender());
    int channel_count = action->data().toInt();
    this->instance->set_mono_forced(channel_count == 1);

    for(auto &i : this->channel_count_options) {
        i->setChecked(i->data().toInt() == channel_count);
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

GB_model_t GameWindow::model_for_type(GameBoyType type) const noexcept {
    switch(type) {
        case GameBoyType::GameBoyGB:
            return this->gb_rev;
        case GameBoyType::GameBoyGBC:
            return this->gbc_rev;
        case GameBoyType::GameBoyGBA:
            return this->gba_rev;
        case GameBoyType::GameBoySGB:
            return this->sgb_rev;
        case GameBoyType::GameBoySGB2:
            return this->sgb2_rev;
        default:
            std::terminate();
    }
}

const std::optional<std::filesystem::path> &GameWindow::boot_rom_for_type(GameBoyType type) const noexcept {
    switch(type) {
        case GameBoyType::GameBoyGB:
            return this->gb_boot_rom_path;
        case GameBoyType::GameBoyGBC:
            return this->gbc_boot_rom_path;
        case GameBoyType::GameBoyGBA:
            return this->gba_boot_rom_path;
        case GameBoyType::GameBoySGB:
            return this->sgb_boot_rom_path;
        case GameBoyType::GameBoySGB2:
            return this->sgb2_boot_rom_path;
        default:
            std::terminate();
    }
}

bool GameWindow::use_fast_boot_rom_for_type(GameBoyType type) const noexcept {
    switch(type) {
        case GameBoyType::GameBoyGB:
            return false;
        case GameBoyType::GameBoyGBC:
            return this->gbc_fast_boot_rom;
        case GameBoyType::GameBoyGBA:
            return false;
        case GameBoyType::GameBoySGB:
            return false;
        case GameBoyType::GameBoySGB2:
            return false;
        default:
            std::terminate();
    }
}

bool GameWindow::use_crop_border_for_type(GameBoyType type) const noexcept {
    switch(type) {
        case GameBoyType::GameBoyGB:
            return false;
        case GameBoyType::GameBoyGBC:
            return false;
        case GameBoyType::GameBoyGBA:
            return false;
        case GameBoyType::GameBoySGB:
            return this->sgb_crop_border;
        case GameBoyType::GameBoySGB2:
            return this->sgb2_crop_border;
        default:
            std::terminate();
    }
}

#define GET_ICON(what) QIcon::fromTheme(QStringLiteral(what))

GameWindow::GameWindow() {
    // Load settings
    QSettings settings;
    this->scaling = settings.value(SETTINGS_SCALE, this->scaling).toInt();
    this->show_fps = settings.value(SETTINGS_SHOW_FPS, this->show_fps).toBool();
    auto gb_type_maybe = static_cast<decltype(this->gb_type)>(settings.value(SETTINGS_GB_MODEL, static_cast<int>(this->gb_type)).toInt());
    if(gb_type_maybe < 0 || gb_type_maybe >= GameBoyType::GameBoy_END) {
        std::fprintf(stderr, "Invalid Game Boy type in config - defaulting to GBC\n");
        this->gb_type = GameBoyType::GameBoyGBC;
    }
    else {
        this->gb_type = gb_type_maybe;
    }

    this->recent_roms = settings.value(SETTINGS_RECENT_ROMS).toStringList();

    // Set boot ROM path
    auto set_boot_rom_path = [](std::optional<std::filesystem::path> &path, const std::string &str) {
        if(!str.empty()) {
            path = std::filesystem::path(str);
        }
    };

    set_boot_rom_path(this->gb_boot_rom_path, settings.value(SETTINGS_GB_BOOT_ROM).toString().toStdString());
    set_boot_rom_path(this->gbc_boot_rom_path, settings.value(SETTINGS_GBC_BOOT_ROM).toString().toStdString());
    set_boot_rom_path(this->gba_boot_rom_path, settings.value(SETTINGS_GBA_BOOT_ROM).toString().toStdString());
    set_boot_rom_path(this->sgb_boot_rom_path, settings.value(SETTINGS_SGB_BOOT_ROM).toString().toStdString());
    set_boot_rom_path(this->sgb2_boot_rom_path, settings.value(SETTINGS_SGB2_BOOT_ROM).toString().toStdString());

    #define LOAD_INT_SETTING_VALUE(var, setting) var = static_cast<decltype(var)>(settings.value(setting, var).toInt())
    #define LOAD_UINT_SETTING_VALUE(var, setting) var = static_cast<decltype(var)>(settings.value(setting, var).toUInt())
    #define LOAD_BOOL_SETTING_VALUE(var, setting) var = static_cast<decltype(var)>(settings.value(setting, var).toBool())
    #define LOAD_DOUBLE_SETTING_VALUE(var, setting) var = static_cast<decltype(var)>(settings.value(setting, var).toDouble())

    LOAD_INT_SETTING_VALUE(this->gb_rev, SETTINGS_GB_REVISION);
    LOAD_INT_SETTING_VALUE(this->gbc_rev, SETTINGS_GBC_REVISION);
    LOAD_INT_SETTING_VALUE(this->gba_rev, SETTINGS_GBA_REVISION);
    LOAD_INT_SETTING_VALUE(this->sgb_rev, SETTINGS_SGB_REVISION);
    LOAD_INT_SETTING_VALUE(this->sgb2_rev, SETTINGS_SGB2_REVISION);
    LOAD_INT_SETTING_VALUE(this->rtc_mode, SETTINGS_RTC_MODE);
    LOAD_INT_SETTING_VALUE(this->rumble_mode, SETTINGS_RUMBLE_MODE);
    LOAD_INT_SETTING_VALUE(this->highpass_filter_mode, SETTINGS_HIGHPASS_FILTER_MODE);
    LOAD_INT_SETTING_VALUE(this->color_correction_mode, SETTINGS_COLOR_CORRECTION_MODE);

    LOAD_UINT_SETTING_VALUE(this->temporary_save_state_buffer_length, SETTINGS_TEMPORARY_SAVE_BUFFER_LENGTH);
    LOAD_UINT_SETTING_VALUE(this->sample_rate, SETTINGS_SAMPLE_RATE);
    LOAD_UINT_SETTING_VALUE(this->sample_count, SETTINGS_SAMPLE_BUFFER_SIZE);

    LOAD_BOOL_SETTING_VALUE(this->sgb_crop_border, SETTINGS_SGB_CROP_BORDER);
    LOAD_BOOL_SETTING_VALUE(this->sgb2_crop_border, SETTINGS_SGB2_CROP_BORDER);
    LOAD_BOOL_SETTING_VALUE(this->gbc_fast_boot_rom, SETTINGS_GBC_FAST_BOOT);
    LOAD_BOOL_SETTING_VALUE(this->status_text_hidden, SETTINGS_STATUS_TEXT_HIDDEN);
    LOAD_BOOL_SETTING_VALUE(this->turbo_enabled, SETTINGS_TURBO_ENABLED);
    LOAD_BOOL_SETTING_VALUE(this->slowmo_enabled, SETTINGS_SLOWMO_ENABLED);
    LOAD_BOOL_SETTING_VALUE(this->rewind_enabled, SETTINGS_REWIND_ENABLED);

    LOAD_DOUBLE_SETTING_VALUE(this->rewind_length, SETTINGS_REWIND_LENGTH);
    LOAD_DOUBLE_SETTING_VALUE(this->max_slowmo, SETTINGS_MAX_SLOWMO);
    LOAD_DOUBLE_SETTING_VALUE(this->max_turbo, SETTINGS_MAX_TURBO);

    #undef LOAD_INT_SETTING_VALUE
    #undef LOAD_UINT_SETTING_VALUE
    #undef LOAD_BOOL_SETTING_VALUE
    
    // Instantiate the gameboy
    this->instance = std::make_unique<GameInstance>(this->model_for_type(this->gb_type));
    this->instance->set_use_fast_boot_rom(this->use_fast_boot_rom_for_type(this->gb_type));
    this->instance->set_boot_rom_path(this->boot_rom_for_type(this->gb_type));
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

    // Save states
    this->save_state_menu = file_menu->addMenu("Save States");
    std::vector<QMenu *> save_state_menus(10);

    auto *revert_save_state = this->save_state_menu->addAction("Revert Load State");
    connect(revert_save_state, &QAction::triggered, this, &GameWindow::action_revert_save_state);
    revert_save_state->setShortcut(static_cast<int>(Qt::CTRL) + Qt::Key_Minus);
    auto *unrevert_save_state = this->save_state_menu->addAction("Unrevert Load State");
    connect(unrevert_save_state, &QAction::triggered, this, &GameWindow::action_unrevert_save_state);
    unrevert_save_state->setShortcut(static_cast<int>(Qt::CTRL) + Qt::Key_Equal);

    this->save_state_menu->addSeparator();

    for(int i = 0; i < 10; i++) {
        char m[256];
        std::snprintf(m, sizeof(m), "Save State #%i", i);
        save_state_menus[i] = this->save_state_menu->addMenu(m);

        int base_key;
        switch(i) {
            case 0:
                base_key = Qt::Key_0;
                break;
            case 1:
                base_key = Qt::Key_1;
                break;
            case 2:
                base_key = Qt::Key_2;
                break;
            case 3:
                base_key = Qt::Key_3;
                break;
            case 4:
                base_key = Qt::Key_4;
                break;
            case 5:
                base_key = Qt::Key_5;
                break;
            case 6:
                base_key = Qt::Key_6;
                break;
            case 7:
                base_key = Qt::Key_7;
                break;
            case 8:
                base_key = Qt::Key_8;
                break;
            case 9:
                base_key = Qt::Key_9;
                break;
            default:
                std::terminate();
        }

        auto *save = save_state_menus[i]->addAction("Save");
        save->setShortcut(static_cast<int>(Qt::CTRL) + base_key);
        connect(save, &QAction::triggered, this, &GameWindow::action_create_save_state);
        save->setData(i);
        auto *load = save_state_menus[i]->addAction("Load");
        load->setShortcut(static_cast<int>(Qt::CTRL) + static_cast<int>(Qt::SHIFT) + base_key);
        connect(load, &QAction::triggered, this, &GameWindow::action_load_save_state);
        load->setData(i);
    }
    this->save_state_menu->addSeparator();
    auto *import = this->save_state_menu->addAction("Import...");
    connect(import, &QAction::triggered, this, &GameWindow::action_import_save_state);

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
    std::pair<const char *, GameBoyType> models[] = {
        {"Game Boy", GameBoyType::GameBoyGB},
        {"Game Boy Color", GameBoyType::GameBoyGBC},
        {"Game Boy Advance (GBC mode)", GameBoyType::GameBoyGBA},
        {"Super Game Boy", GameBoyType::GameBoySGB},
        {"Super Game Boy 2", GameBoyType::GameBoySGB2},
    };
    for(auto &m : models) {
        auto *action = this->gameboy_model_menu->addAction(m.first);
        action->setData(static_cast<int>(m.second));
        action->setCheckable(true);
        action->setChecked(m.second == this->gb_type);
        connect(action, &QAction::triggered, this, &GameWindow::action_set_model);
        this->gb_model_actions.emplace_back(action);
    }
    gameboy_model_menu->addSeparator();
    auto *advanced_options = gameboy_model_menu->addAction("Advanced Options...");
    connect(advanced_options, &QAction::triggered, this, &GameWindow::action_show_advanced_model_options);

    // RTC modes
    this->instance->set_rtc_mode(this->rtc_mode);
    auto *rtc_mode = edit_menu->addMenu("Real-Time Clock Mode");
    std::pair<const char *, GB_rtc_mode_t> rtc_modes[] = {
        {"Accurate", GB_rtc_mode_t::GB_RTC_MODE_ACCURATE},
        {"Synced to Host", GB_rtc_mode_t::GB_RTC_MODE_SYNC_TO_HOST},
    };
    for(auto &i : rtc_modes) {
        auto *action = rtc_mode->addAction(i.first);
        action->setData(i.second);
        connect(action, &QAction::triggered, this, &GameWindow::action_set_rtc_mode);
        action->setCheckable(true);
        action->setChecked(i.second == this->rtc_mode);
        this->rtc_mode_options.emplace_back(action);
    }

    // Rumble modes
    this->instance->set_rumble_mode(this->rumble_mode);
    auto *rumble_mode = edit_menu->addMenu("Rumble Mode");
    std::pair<const char *, GB_rumble_mode_t> rumble_modes[] = {
        {"Off", GB_rumble_mode_t::GB_RUMBLE_DISABLED},
        {"Cartridge Only", GB_rumble_mode_t::GB_RUMBLE_CARTRIDGE_ONLY},
        {"All Games", GB_rumble_mode_t::GB_RUMBLE_ALL_GAMES},
    };
    for(auto &i : rumble_modes) {
        auto *action = rumble_mode->addAction(i.first);
        action->setData(i.second);
        connect(action, &QAction::triggered, this, &GameWindow::action_set_rumble_mode);
        action->setCheckable(true);
        action->setChecked(i.second == this->rumble_mode);
        this->rumble_mode_options.emplace_back(action);
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

    // Highpass mode
    this->instance->set_rtc_mode(this->rtc_mode);
    auto *highpass_filter_mode = edit_menu->addMenu("Highpass Filter Mode");
    std::pair<const char *, GB_highpass_mode_t> highpass_filter_modes[] = {
        {"Off", GB_highpass_mode_t::GB_HIGHPASS_OFF},
        {"Accurate", GB_highpass_mode_t::GB_HIGHPASS_ACCURATE},
        {"Remove DC Offset", GB_highpass_mode_t::GB_HIGHPASS_REMOVE_DC_OFFSET},
    };
    for(auto &i : highpass_filter_modes) {
        auto *action = highpass_filter_mode->addAction(i.first);
        action->setData(i.second);
        connect(action, &QAction::triggered, this, &GameWindow::action_set_highpass_filter_mode);
        action->setCheckable(true);
        action->setChecked(i.second == this->highpass_filter_mode);
        this->highpass_filter_mode_options.emplace_back(action);
    }
    
    edit_menu->addSeparator();

    // Scaling
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

    // Color correction options
    auto *color_correction_mode = edit_menu->addMenu("Color Correction Mode");
    this->instance->set_color_correction_mode(this->color_correction_mode);
    std::pair<const char *, GB_color_correction_mode_t> color_correction_modes[] = {
        {"Disabled", GB_color_correction_mode_t::GB_COLOR_CORRECTION_DISABLED},
        {"Correct Curves", GB_color_correction_mode_t::GB_COLOR_CORRECTION_CORRECT_CURVES},
        {"Emulate Hardware", GB_color_correction_mode_t::GB_COLOR_CORRECTION_EMULATE_HARDWARE},
        {"Reduce Contrast", GB_color_correction_mode_t::GB_COLOR_CORRECTION_REDUCE_CONTRAST},
        {"Low Contrast", GB_color_correction_mode_t::GB_COLOR_CORRECTION_LOW_CONTRAST},
    };
    for(auto &i : color_correction_modes) {
        auto *action = color_correction_mode->addAction(i.first);
        action->setData(i.second);
        connect(action, &QAction::triggered, this, &GameWindow::action_set_color_correction_mode);
        action->setCheckable(true);
        action->setChecked(i.second == this->color_correction_mode);
        this->color_correction_mode_options.emplace_back(action);
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

    // Status text?
    auto *hide_status_text = edit_menu->addAction("Hide Status Text");
    connect(hide_status_text, &QAction::triggered, this, &GameWindow::action_toggle_hide_status_text);
    hide_status_text->setCheckable(true);
    hide_status_text->setChecked(this->status_text_hidden);

    // Add controls options
    auto *controls = edit_menu->addAction("Configure Controls...");
    connect(controls, &QAction::triggered, this, &GameWindow::action_edit_controls);

    // Add controls options
    auto *speed_control = edit_menu->addAction("Configure Rewind and Speed...");
    connect(speed_control, &QAction::triggered, this, &GameWindow::action_edit_speed_control);
    
    
    // Emulation menu
    auto *emulation_menu = bar->addMenu("Emulation");
    connect(emulation_menu, &QMenu::aboutToShow, this, &GameWindow::action_showing_menu);
    connect(emulation_menu, &QMenu::aboutToHide, this, &GameWindow::action_hiding_menu);

    // Pause options
    this->pause_action = emulation_menu->addAction("Pause");
    connect(this->pause_action, &QAction::triggered, this, &GameWindow::action_toggle_pause);
    this->pause_action->setIcon(GET_ICON("media-playback-pause"));
    this->pause_action->setCheckable(true);
    this->pause_action->setChecked(false);
    
    // Reset
    this->reset_rom_action = emulation_menu->addAction("Reset");
    connect(this->reset_rom_action, &QAction::triggered, this, &GameWindow::action_reset);
    this->reset_rom_action->setIcon(GET_ICON("view-refresh"));
    this->reset_rom_action->setEnabled(false);

    emulation_menu->addSeparator();
    
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
    bool result = this->instance->set_up_sdl_audio(this->sample_rate, this->sample_count);
    if(!result) {
        std::printf("Debug) Failed to start up audio with SDL: %s\n", SDL_GetError());
    }
    else {
        this->instance->set_audio_enabled(!muted);
        std::printf("(Debug) Sample rate: %u Hz\n", this->instance->get_current_sample_rate());
    }

    // Reload devices
    this->reload_devices();
    
    // Fire game_loop repeatedly
    this->game_thread_timer.callOnTimeout(this, &GameWindow::game_loop);
    this->game_thread_timer.start();
}

void GameWindow::action_set_volume() {
    // Uses the user data from the sender to get volume
    auto *action = qobject_cast<QAction *>(sender());
    int volume = action->data().toInt(); 
    this->instance->set_volume(volume);
    this->show_new_volume_text();
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
    
    // Start thread
    if(r == 0 && !instance_thread.joinable()) {
        instance_thread = std::thread(GameInstance::start_game_loop, this->instance.get());
    }

    // Fire this once
    this->game_loop();
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
    // Make sure we have enough data to store the pixels
    std::uint32_t width, height;
    this->instance->get_dimensions(width, height);
    this->pixel_buffer.resize(width * height, 0xFF000000);

    // If we have a ROM loaded, copy the pixel buffer
    if(this->instance->is_rom_loaded()) {
        this->instance->read_pixel_buffer(this->pixel_buffer.data(), this->pixel_buffer.size());
    }

    // If we aren't cropping SGB border, just output the pixel buffer
    if(!this->use_crop_border_for_type(this->gb_type)) {
        this->pixel_buffer_pixmap.convertFromImage(QImage(reinterpret_cast<const uchar *>(this->pixel_buffer.data()), width, height, QImage::Format::Format_ARGB32));
    }

    // Otherwise, crop it
    else {
        assert(width == SGB_WIDTH && height == SGB_HEIGHT);

        // Perform the crop
        for(std::uint32_t y = 0; y < GB_HEIGHT; y++) {
            for(std::uint32_t x = 0; x < GB_WIDTH; x++) {
                this->pixel_buffer[x + y * GB_WIDTH] = this->pixel_buffer[x + (SGB_WIDTH - GB_WIDTH) / 2 + (y + (SGB_HEIGHT - GB_HEIGHT) / 2) * SGB_WIDTH];
            }
        }

        // Boom
        this->pixel_buffer_pixmap.convertFromImage(QImage(reinterpret_cast<const uchar *>(this->pixel_buffer.data()), GB_WIDTH, GB_HEIGHT, QImage::Format::Format_ARGB32));
    }

    // Set our pixmap
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

    // If cropping border, override
    if(this->use_crop_border_for_type(this->gb_type)) {
        width = GB_WIDTH;
        height = GB_HEIGHT;
    }

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

    SDL_Event event;
    while(SDL_PollEvent(&event)) {
        switch(event.type) {
            // If we hit ctrl-c, close the window (saves)
            case SDL_EventType::SDL_QUIT:
                this->close();

                // If the window wasn't closed, warn
                if(this->isVisible()) {
                    std::fprintf(stderr, "Can't close the main window. Finish what you're doing, first!\n");
                }
                break;

            // Audio hotplugging
            case SDL_EventType::SDL_AUDIODEVICEADDED:
            case SDL_EventType::SDL_AUDIODEVICEREMOVED:
                this->instance->set_up_sdl_audio(this->sample_rate);
                break;

            // Controller hotplugging
            case SDL_EventType::SDL_CONTROLLERDEVICEADDED:
            case SDL_EventType::SDL_CONTROLLERDEVICEREMOVED:
                this->reload_devices();
                break;

            // Controller input
            case SDL_EventType::SDL_CONTROLLERAXISMOTION:
                this->handle_joypad_event(event.caxis);
                break;
            case SDL_EventType::SDL_CONTROLLERBUTTONDOWN:
            case SDL_EventType::SDL_CONTROLLERBUTTONUP:
                this->handle_joypad_event(event.cbutton);
                break;

            // No.
            case SDL_EventType::SDL_JOYDEVICEADDED:
            case SDL_EventType::SDL_JOYDEVICEREMOVED:
            case SDL_EventType::SDL_JOYAXISMOTION:
            case SDL_EventType::SDL_JOYBALLMOTION:
            case SDL_EventType::SDL_JOYHATMOTION:
            case SDL_EventType::SDL_JOYBUTTONDOWN:
            case SDL_EventType::SDL_JOYBUTTONUP:
                break;

            // If we didn't handle something, complain
            default:
                print_debug_message("Unhandled SDL event %i\n", event.type);
                break;
        }
    }

    // If we're paused, we don't need to fire as often since not as much information is being changed (unless we have status text to show?), saving CPU usage
    if((this->instance->is_paused() || !this->instance->is_rom_loaded()) && (status_text == nullptr)) {
        this->game_thread_timer.setInterval(100);
    }

    // Otherwise, we should fire quickly
    else {
        this->game_thread_timer.setInterval(5);

        if(this->last_used_input_device) {
            this->last_used_input_device->apply_rumble(this->instance->get_rumble());
        }
    }
}

void GameWindow::handle_joypad_event(const SDL_ControllerButtonEvent &event) noexcept {
    bool value = event.state == SDL_PRESSED;

    for(auto &i : this->devices) {
        auto *gp = dynamic_cast<InputDeviceGamepad *>(i.get());
        if(gp && gp->get_joystick_id() == event.which) {
            gp->handle_input(static_cast<SDL_GameControllerButton>(event.button), value);
            this->last_used_input_device = gp;
            return;
        }
    }

    print_debug_message("Got a joypad event from an unknown device %u\n", event.which);
}

void GameWindow::handle_joypad_event(const SDL_ControllerAxisEvent &event) noexcept {
    double value = event.value > 0 ? event.value / 32767.0 : event.value / 32768.0;

    for(auto &i : this->devices) {
        auto *gp = dynamic_cast<InputDeviceGamepad *>(i.get());
        if(gp && gp->get_joystick_id() == event.which) {
            gp->handle_input(static_cast<SDL_GameControllerAxis>(event.axis), value);
            this->last_used_input_device = gp;
            return;
        }
    }

    print_debug_message("Got a joypad event from an unknown device %u\n", event.which);
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
    qfd.setWindowTitle("Select a Game Boy ROM");
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
    if(this->status_text_hidden) {
        return;
    }

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
    this->last_used_input_device = nullptr;
    
    event->ignore();
}

void GameWindow::keyPressEvent(QKeyEvent *event) {
    if(event->isAutoRepeat()) {
        return;
    }
    this->handle_keyboard_key(event, true);
}

void GameWindow::keyReleaseEvent(QKeyEvent *event) {
    if(event->isAutoRepeat()) {
        return;
    }
    this->handle_keyboard_key(event, false);
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
    this->gb_type = static_cast<decltype(this->gb_type)>(action->data().toInt());
    this->instance->set_boot_rom_path(this->boot_rom_for_type(this->gb_type));
    this->instance->set_use_fast_boot_rom(this->use_fast_boot_rom_for_type(this->gb_type));
    this->instance->set_model(this->model_for_type(this->gb_type));
    
    for(auto &i : this->gb_model_actions) {
        i->setChecked(i->data().toInt() == this->gb_type);
    }
    
    this->set_pixel_view_scaling(this->scaling);
}

void GameWindow::action_quit_without_saving() noexcept {
    QMessageBox qmb(QMessageBox::Icon::Question, "Are You Sure?", "This will close the emulator without saving your SRAM.\n\nAny save data that has not been saved to disk will be lost.", QMessageBox::Cancel | QMessageBox::Ok);
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
    settings.setValue(SETTINGS_GB_MODEL, static_cast<int>(this->gb_type));
    settings.setValue(SETTINGS_SAMPLE_BUFFER_SIZE, this->sample_count);
    settings.setValue(SETTINGS_SAMPLE_RATE, this->sample_rate);
    settings.setValue(SETTINGS_BUFFER_MODE, instance->get_pixel_buffering_mode());
    settings.setValue(SETTINGS_RTC_MODE, this->rtc_mode);
    settings.setValue(SETTINGS_COLOR_CORRECTION_MODE, this->color_correction_mode);
    settings.setValue(SETTINGS_TEMPORARY_SAVE_BUFFER_LENGTH, this->temporary_save_state_buffer_length);
    settings.setValue(SETTINGS_HIGHPASS_FILTER_MODE, this->highpass_filter_mode);
    settings.setValue(SETTINGS_RUMBLE_MODE, this->rumble_mode);
    settings.setValue(SETTINGS_STATUS_TEXT_HIDDEN, this->status_text_hidden);
    settings.setValue(SETTINGS_REWIND_LENGTH, this->rewind_length);
    settings.setValue(SETTINGS_MAX_SLOWMO, this->max_slowmo);
    settings.setValue(SETTINGS_MAX_TURBO, this->max_turbo);
    settings.setValue(SETTINGS_REWIND_ENABLED, this->rewind_enabled);
    settings.setValue(SETTINGS_SLOWMO_ENABLED, this->slowmo_enabled);
    settings.setValue(SETTINGS_TURBO_ENABLED, this->turbo_enabled);

    settings.setValue(SETTINGS_GB_BOOT_ROM, this->gb_boot_rom_path.value_or(std::filesystem::path()).string().c_str());
    settings.setValue(SETTINGS_GBC_BOOT_ROM, this->gbc_boot_rom_path.value_or(std::filesystem::path()).string().c_str());
    settings.setValue(SETTINGS_GBA_BOOT_ROM, this->gba_boot_rom_path.value_or(std::filesystem::path()).string().c_str());
    settings.setValue(SETTINGS_SGB_BOOT_ROM, this->sgb_boot_rom_path.value_or(std::filesystem::path()).string().c_str());
    settings.setValue(SETTINGS_SGB2_BOOT_ROM, this->sgb2_boot_rom_path.value_or(std::filesystem::path()).string().c_str());

    settings.setValue(SETTINGS_GB_REVISION, this->gb_rev);
    settings.setValue(SETTINGS_GBC_REVISION, this->gbc_rev);
    settings.setValue(SETTINGS_GBA_REVISION, this->gba_rev);
    settings.setValue(SETTINGS_SGB_REVISION, this->sgb_rev);
    settings.setValue(SETTINGS_SGB2_REVISION, this->sgb2_rev);

    settings.setValue(SETTINGS_SGB_CROP_BORDER, this->sgb_crop_border);
    settings.setValue(SETTINGS_SGB2_CROP_BORDER, this->sgb2_crop_border);

    settings.setValue(SETTINGS_GBC_FAST_BOOT, this->gbc_fast_boot_rom);

    QApplication::quit();
}

void GameWindow::action_edit_controls() {
    EditControlsDialog d(this);
    this->disable_input = true;
    d.exec();
    this->disable_input = false;
    this->reload_devices();
}

void GameWindow::action_edit_speed_control() {
    EditSpeedControlSettingsDialog d(this);
    this->disable_input = true;
    d.exec();
    this->disable_input = false;
    this->reload_devices();
}

std::vector<std::shared_ptr<InputDevice>> GameWindow::get_all_devices() {
    return this->devices;
}

void GameWindow::reload_devices() {
    this->devices.clear();
    this->last_used_input_device = nullptr;
    devices.emplace_back(std::make_shared<InputDeviceKeyboard>());

    for(int i = 0; i < SDL_NumJoysticks(); i++) {
        auto *sdl = SDL_GameControllerOpen(i);
        devices.emplace_back(std::make_shared<InputDeviceGamepad>(sdl));
    }

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
        case InputDevice::Input_RapidA:
            this->instance->set_rapid_button_state(GB_key_t::GB_KEY_A, boolean_input);
            break;
        case InputDevice::Input_RapidB:
            this->instance->set_rapid_button_state(GB_key_t::GB_KEY_B, boolean_input);
            break;
        case InputDevice::Input_RapidStart:
            this->instance->set_rapid_button_state(GB_key_t::GB_KEY_START, boolean_input);
            break;
        case InputDevice::Input_RapidSelect:
            this->instance->set_rapid_button_state(GB_key_t::GB_KEY_SELECT, boolean_input);
            break;
        case InputDevice::Input_RapidUp:
            this->instance->set_rapid_button_state(GB_key_t::GB_KEY_UP, boolean_input);
            break;
        case InputDevice::Input_RapidDown:
            this->instance->set_rapid_button_state(GB_key_t::GB_KEY_DOWN, boolean_input);
            break;
        case InputDevice::Input_RapidLeft:
            this->instance->set_rapid_button_state(GB_key_t::GB_KEY_LEFT, boolean_input);
            break;
        case InputDevice::Input_RapidRight:
            this->instance->set_rapid_button_state(GB_key_t::GB_KEY_RIGHT, boolean_input);
            break;
        case InputDevice::Input_Turbo:
            if(this->turbo_enabled && input > 0.1) {
                double max_increase = this->max_turbo - 1.0;
                this->instance->set_turbo_mode(true, 1.0 + max_increase * ((input - 0.1) / 0.9));
            }
            else {
                this->instance->set_turbo_mode(false);
            }
            break;
        case InputDevice::Input_Slowmo:
            if(this->slowmo_enabled && input > 0.1) {
                double max_decrease = (1.0 - this->max_slowmo);
                this->instance->set_speed_multiplier(this->max_slowmo + max_decrease * (1.0 - (input - 0.1) / 0.9));
            }
            else {
                this->instance->set_speed_multiplier(1.0);
            }
            break;
        case InputDevice::Input_Rewind:
            if(!this->rewind_enabled) {
                boolean_input = false;
            }
            this->instance->set_rewind(boolean_input);
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

std::filesystem::path GameWindow::get_save_state_path(int index) const {
    char extension[64];
    std::snprintf(extension, sizeof(extension), ".s%i", index); // use SameBoy's convention (.s0, .s1, etc.)
    return std::filesystem::path(this->save_path).replace_extension(extension);
}

void GameWindow::action_create_save_state() {
    auto save_state = qobject_cast<QAction *>(sender())->data().toInt();
    auto path = this->get_save_state_path(save_state);

    // Attempt to save
    if(this->instance->create_save_state(path)) {
        char msg[256];
        std::snprintf(msg, sizeof(msg), "Created save state #%i", save_state);
        this->show_status_text(msg);
    }
    else {
        char err[256];
        std::snprintf(err, sizeof(err), "Failed to create save state #%i", save_state);
        this->show_status_text(err);
    }
}

bool GameWindow::load_save_state(const std::filesystem::path &path) {
    // Back up the save state in case this was done by mistake
    auto backup = this->instance->create_save_state();
    if(this->instance->load_save_state(path)) {
        // If we hit the maximum save states, remove the first state before adding a new state (and don't increment the counter)
        if(this->next_temporary_save_state + 1 > this->temporary_save_state_buffer_length) {
            this->temporary_save_states.erase(this->temporary_save_states.begin());
        }

        // Otherwise, increment the counter
        else {
            this->next_temporary_save_state++;
        }

        // Push the save state into the buffer
        this->temporary_save_states.resize(this->next_temporary_save_state);
        this->temporary_save_states[this->next_temporary_save_state - 1] = backup;

        return true;
    }
    else {
        return false;
    }
}

void GameWindow::action_load_save_state() {
    auto save_state = qobject_cast<QAction *>(sender())->data().toInt();
    auto path = this->get_save_state_path(save_state);
    if(!std::filesystem::is_regular_file(path)) {
        char err[256];
        std::snprintf(err, sizeof(err), "Save state #%i does not exist", save_state);
        this->show_status_text(err);
        return;
    }

    // Attempt to load
    if(this->load_save_state(path)) {
        char msg[256];
        std::snprintf(msg, sizeof(msg), "Loaded save state #%i", save_state);
        this->show_status_text(msg);
    }
    else {
        char err[256];
        std::snprintf(err, sizeof(err), "Failed to load save state #%i", save_state);
        this->show_status_text(err);
    }
}

void GameWindow::action_import_save_state() {
    QFileDialog qfd;
    qfd.setWindowTitle("Import a Save State");

    if(qfd.exec() != QDialog::DialogCode::Accepted) {
        return;
    }

    // Maybe attempt to load
    auto path = std::filesystem::path(qfd.selectedFiles().at(0).toStdString());
    if(this->load_save_state(path)) {
        this->show_status_text("Loaded imported save state");
    }
    else {
        this->show_status_text("Failed to load imported save state");
    }
}

void GameWindow::action_show_advanced_model_options() noexcept {
    EditAdvancedGameBoyModelDialog dialog(this);
    dialog.exec();
}

void GameWindow::action_revert_save_state() {
    if(this->next_temporary_save_state == 0) {
        this->show_status_text("No save state to revert");
        return;
    }

    // Try to load the last save state
    auto &state = this->temporary_save_states[this->next_temporary_save_state - 1];
    auto backup = this->instance->create_save_state();
    if(this->instance->load_save_state(state)) {
        char m[256];
        std::snprintf(m, sizeof(m), "Loaded temp save state %u / %zu", this->next_temporary_save_state, this->temporary_save_states.size());
        this->show_status_text(m);
        this->next_temporary_save_state--;

        // Back up save state if we want to un-revert
        this->temporary_save_states[this->next_temporary_save_state] = backup;
    }
    else {
        char m[256];
        std::snprintf(m, sizeof(m), "Failed to load temp state %u / %zu", this->next_temporary_save_state, this->temporary_save_states.size());
        this->show_status_text(m);
    }
}

void GameWindow::action_unrevert_save_state() {
    if(this->next_temporary_save_state == this->temporary_save_states.size()) {
        this->show_status_text("No save state to unrevert");
        return;
    }

    // Load the next save state
    auto &state = this->temporary_save_states[this->next_temporary_save_state];
    auto backup = this->instance->create_save_state();
    if(this->instance->load_save_state(state)) {
        // Back up save state if we want to un-un-revert
        this->temporary_save_states[this->next_temporary_save_state] = backup;
        this->next_temporary_save_state++;

        char m[256];
        std::snprintf(m, sizeof(m), "Undid temp save state %u / %zu", this->next_temporary_save_state, this->temporary_save_states.size());
        this->show_status_text(m);

    }
    else {
        char m[256];
        std::snprintf(m, sizeof(m), "Failed to undo temp save state %u / %zu", this->next_temporary_save_state, this->temporary_save_states.size());
        this->show_status_text(m);
    }
}

void GameWindow::action_toggle_hide_status_text() noexcept {
    this->status_text_hidden = !this->status_text_hidden;
    qobject_cast<QAction *>(sender())->setChecked(this->status_text_hidden);

    if(this->status_text_hidden) {
        delete this->status_text;
        this->status_text = nullptr;
    }
}

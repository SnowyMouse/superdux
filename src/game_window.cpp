#include "game_window.hpp"
#include "debugger.hpp"
#include "edit_controls_dialog.hpp"
#include "settings.hpp"

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
#include <QCheckBox>
#include <bit>
#include <QMimeData>
#include "printer.hpp"
#include "edit_advanced_game_boy_model_dialog.hpp"
#include "edit_speed_control_settings_dialog.hpp"

#include <QLabel>

#include "vram_viewer.hpp"
#include "input_device.hpp"

#define SETTINGS_VOLUME "volume"
#define SETTINGS_SCALE "scale"
#define SETTINGS_SCALING_FILTER "scale_filter"
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
#define SETTINGS_REWIND_SPEED "rewind_speed"
#define SETTINGS_BASE_SPEED "base_speed"
#define SETTINGS_MAX_TURBO "max_turbo"
#define SETTINGS_MAX_SLOWMO "max_slowmo"
#define SETTINGS_MAX_CPU_MULTIPLIER "max_cpu_multiplier"

#define SETTINGS_INTEGRITY_CHECK_CORRUPT "integrity_check_corrupt"
#define SETTINGS_INTEGRITY_CHECK_COMPATIBLE "integrity_check_compatible"

#define SETTINGS_TURBO_ENABLED "turbo_enabled"
#define SETTINGS_SLOWMO_ENABLED "slowmo_enabled"
#define SETTINGS_REWIND_ENABLED "rewind_enabled"

#define SETTINGS_GB_BOOT_ROM "gb_external_boot_rom"
#define SETTINGS_GBC_BOOT_ROM "gbc_external_boot_rom"
#define SETTINGS_GBA_BOOT_ROM "gba_external_boot_rom"
#define SETTINGS_SGB_BOOT_ROM "sgb_external_boot_rom"
#define SETTINGS_SGB2_BOOT_ROM "sgb2_external_boot_rom"

#define SETTINGS_GB_ALLOW_EXTERNAL_BOOT_ROM "gb_use_external_boot_rom"
#define SETTINGS_GBC_ALLOW_EXTERNAL_BOOT_ROM "gbc_use_external_boot_rom"
#define SETTINGS_GBA_ALLOW_EXTERNAL_BOOT_ROM "gba_use_external_boot_rom"
#define SETTINGS_SGB_ALLOW_EXTERNAL_BOOT_ROM "sgb_use_external_boot_rom"
#define SETTINGS_SGB2_ALLOW_EXTERNAL_BOOT_ROM "sgb2_use_external_boot_rom"

#define SETTINGS_GB_REVISION "gb_model_revision"
#define SETTINGS_GBC_REVISION "gbc_model_revision"
#define SETTINGS_GBA_REVISION "gba_model_revision"
#define SETTINGS_SGB_REVISION "sgb_model_revision"
#define SETTINGS_SGB2_REVISION "sgb2_model_revision"

#define SETTINGS_GB_BORDER "gb_border"
#define SETTINGS_GBC_BORDER "gbc_border"
#define SETTINGS_GBA_BORDER "gba_border"
#define SETTINGS_SGB_BORDER "sgb_border"
#define SETTINGS_SGB2_BORDER "sgb2_border"

#define SETTINGS_GBC_FAST_BOOT "gbc_skip_intro"
#define SETTINGS_SGB_SKIP_INTRO "sgb_skip_intro"
#define SETTINGS_SGB2_SKIP_INTRO "sgb2_skip_intro"

#define SGB_WIDTH 256
#define SGB_HEIGHT 224
#define GB_WIDTH 160
#define GB_HEIGHT 144

#ifndef NDEBUG
#define print_debug_message(...) std::printf("Debug: " __VA_ARGS__)
#else
#define print_debug_message(...) (void(0))
#endif

#define MAKE_INSTANCE_MODE_SETTER_WITH_VARIABLE(instance_fn, variable, array) { \
    auto *action = qobject_cast<QAction *>(sender()); \
    auto mode = static_cast<decltype(variable)>(action->data().toInt()); \
    this->instance->instance_fn(mode); \
    variable = mode; \
    \
    for(auto &i : array) { \
        i->setChecked(i->data().toInt() == mode); \
    } \
}

#define MAKE_MODE_SETTER_WITH_VARIABLE(variable, array) { \
    auto *action = qobject_cast<QAction *>(sender()); \
    auto mode = static_cast<decltype(variable)>(action->data().toInt()); \
    variable = mode; \
    \
    for(auto &i : array) { \
        i->setChecked(i->data().toInt() == mode); \
    } \
}

void GameWindow::action_set_rtc_mode() noexcept MAKE_INSTANCE_MODE_SETTER_WITH_VARIABLE(set_rtc_mode, this->rtc_mode, this->rtc_mode_options)
void GameWindow::action_set_highpass_filter_mode() noexcept MAKE_INSTANCE_MODE_SETTER_WITH_VARIABLE(set_highpass_filter_mode, this->highpass_filter_mode, this->highpass_filter_mode_options)
void GameWindow::action_set_rumble_mode() noexcept MAKE_INSTANCE_MODE_SETTER_WITH_VARIABLE(set_rumble_mode, this->rumble_mode, this->rumble_mode_options)
void GameWindow::action_set_color_correction_mode() noexcept MAKE_INSTANCE_MODE_SETTER_WITH_VARIABLE(set_color_correction_mode, this->color_correction_mode, this->color_correction_mode_options)

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
                auto path = std::filesystem::path(urls[0].toLocalFile().toStdString());
                auto extension = path.extension().string();
                if(extension == ".gb" || extension == ".gbc" || extension == ".sgb" || extension == ".isx") {
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
    static const std::optional<std::filesystem::path> null_path = std::nullopt;

    switch(type) {
        case GameBoyType::GameBoyGB:
            return this->gb_allow_external_boot_rom ? this->gb_boot_rom_path : null_path;
        case GameBoyType::GameBoyGBC:
            return this->gbc_allow_external_boot_rom ? this->gbc_boot_rom_path : null_path;
        case GameBoyType::GameBoyGBA:
            return this->gba_allow_external_boot_rom ? this->gba_boot_rom_path : null_path;
        case GameBoyType::GameBoySGB:
            return this->sgb_allow_external_boot_rom ? this->sgb_boot_rom_path : null_path;
        case GameBoyType::GameBoySGB2:
            return this->sgb2_allow_external_boot_rom ? this->sgb2_boot_rom_path : null_path;
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
            return this->sgb_skip_intro;
        case GameBoyType::GameBoySGB2:
            return this->sgb2_skip_intro;
        default:
            std::terminate();
    }
}

bool GameWindow::use_border_for_type(GameBoyType type) const noexcept {
    switch(type) {
        case GameBoyType::GameBoyGB:
            return this->gb_border;
        case GameBoyType::GameBoyGBC:
            return this->gbc_border;
        case GameBoyType::GameBoyGBA:
            return this->gba_border;
        case GameBoyType::GameBoySGB:
            return this->sgb_border;
        case GameBoyType::GameBoySGB2:
            return this->sgb2_border;
        default:
            std::terminate();
    }
}

#define GET_ICON(what) QIcon::fromTheme(QStringLiteral(what))

GameWindow::GameWindow() {
    // Load settings
    auto settings = get_superdux_settings();
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
    #define LOAD_DOUBLE_SETTING_VALUE_MIN(var, setting, min) var = std::max(static_cast<decltype(var)>(settings.value(setting, var).toDouble()), min)

    LOAD_INT_SETTING_VALUE(this->gb_rev, SETTINGS_GB_REVISION);
    LOAD_INT_SETTING_VALUE(this->gbc_rev, SETTINGS_GBC_REVISION);
    LOAD_INT_SETTING_VALUE(this->gba_rev, SETTINGS_GBA_REVISION);
    LOAD_INT_SETTING_VALUE(this->sgb_rev, SETTINGS_SGB_REVISION);
    LOAD_INT_SETTING_VALUE(this->sgb2_rev, SETTINGS_SGB2_REVISION);
    LOAD_INT_SETTING_VALUE(this->rtc_mode, SETTINGS_RTC_MODE);
    LOAD_INT_SETTING_VALUE(this->rumble_mode, SETTINGS_RUMBLE_MODE);
    LOAD_INT_SETTING_VALUE(this->highpass_filter_mode, SETTINGS_HIGHPASS_FILTER_MODE);
    LOAD_INT_SETTING_VALUE(this->color_correction_mode, SETTINGS_COLOR_CORRECTION_MODE);
    LOAD_INT_SETTING_VALUE(this->scaling_filter, SETTINGS_SCALING_FILTER);

    LOAD_UINT_SETTING_VALUE(this->temporary_save_state_buffer_length, SETTINGS_TEMPORARY_SAVE_BUFFER_LENGTH);
    LOAD_UINT_SETTING_VALUE(this->sample_rate, SETTINGS_SAMPLE_RATE);
    LOAD_UINT_SETTING_VALUE(this->sample_count, SETTINGS_SAMPLE_BUFFER_SIZE);

    LOAD_BOOL_SETTING_VALUE(this->gbc_fast_boot_rom, SETTINGS_GBC_FAST_BOOT);
    LOAD_BOOL_SETTING_VALUE(this->sgb_skip_intro, SETTINGS_SGB_SKIP_INTRO);
    LOAD_BOOL_SETTING_VALUE(this->sgb2_skip_intro, SETTINGS_SGB2_SKIP_INTRO);
    LOAD_BOOL_SETTING_VALUE(this->status_text_hidden, SETTINGS_STATUS_TEXT_HIDDEN);
    LOAD_BOOL_SETTING_VALUE(this->turbo_enabled, SETTINGS_TURBO_ENABLED);
    LOAD_BOOL_SETTING_VALUE(this->slowmo_enabled, SETTINGS_SLOWMO_ENABLED);
    LOAD_BOOL_SETTING_VALUE(this->rewind_enabled, SETTINGS_REWIND_ENABLED);
    LOAD_BOOL_SETTING_VALUE(this->gb_allow_external_boot_rom, SETTINGS_GB_ALLOW_EXTERNAL_BOOT_ROM);
    LOAD_BOOL_SETTING_VALUE(this->gbc_allow_external_boot_rom, SETTINGS_GBC_ALLOW_EXTERNAL_BOOT_ROM);
    LOAD_BOOL_SETTING_VALUE(this->gba_allow_external_boot_rom, SETTINGS_GBA_ALLOW_EXTERNAL_BOOT_ROM);
    LOAD_BOOL_SETTING_VALUE(this->sgb_allow_external_boot_rom, SETTINGS_SGB_ALLOW_EXTERNAL_BOOT_ROM);
    LOAD_BOOL_SETTING_VALUE(this->sgb2_allow_external_boot_rom, SETTINGS_SGB2_ALLOW_EXTERNAL_BOOT_ROM);

    LOAD_BOOL_SETTING_VALUE(this->integrity_check_corrupt, SETTINGS_INTEGRITY_CHECK_CORRUPT);
    LOAD_BOOL_SETTING_VALUE(this->integrity_check_compatible, SETTINGS_INTEGRITY_CHECK_COMPATIBLE);

    LOAD_BOOL_SETTING_VALUE(this->sgb_border, SETTINGS_SGB_BORDER);
    LOAD_BOOL_SETTING_VALUE(this->sgb2_border, SETTINGS_SGB2_BORDER);
    LOAD_BOOL_SETTING_VALUE(this->gb_border, SETTINGS_GB_BORDER);
    LOAD_BOOL_SETTING_VALUE(this->gbc_border, SETTINGS_GBC_BORDER);
    LOAD_BOOL_SETTING_VALUE(this->gba_border, SETTINGS_GBA_BORDER);

    LOAD_DOUBLE_SETTING_VALUE_MIN(this->rewind_length, SETTINGS_REWIND_LENGTH, 0.0);
    LOAD_DOUBLE_SETTING_VALUE_MIN(this->max_slowmo, SETTINGS_MAX_SLOWMO, 0.0);
    LOAD_DOUBLE_SETTING_VALUE_MIN(this->max_turbo, SETTINGS_MAX_TURBO, 0.0);
    LOAD_DOUBLE_SETTING_VALUE_MIN(this->rewind_speed, SETTINGS_REWIND_SPEED, 0.0);
    LOAD_DOUBLE_SETTING_VALUE_MIN(this->base_multiplier, SETTINGS_BASE_SPEED, 0.0);
    LOAD_DOUBLE_SETTING_VALUE_MIN(this->max_cpu_multiplier, SETTINGS_MAX_CPU_MULTIPLIER, 1.0);

    // Prevent invalid values
    this->max_slowmo = std::max(this->max_slowmo, 0.0);
    this->max_turbo = std::max(this->max_turbo, 1.0);

    #undef LOAD_INT_SETTING_VALUE
    #undef LOAD_UINT_SETTING_VALUE
    #undef LOAD_BOOL_SETTING_VALUE

    // Instantiate the gameboy
    this->instance = std::make_unique<GameInstance>(this->model_for_type(this->gb_type), this->use_border_for_type(this->gb_type) ? GB_border_mode_t::GB_BORDER_ALWAYS : GB_border_mode_t::GB_BORDER_NEVER);
    this->instance->set_use_fast_boot_rom(this->use_fast_boot_rom_for_type(this->gb_type));
    this->instance->set_boot_rom_path(this->boot_rom_for_type(this->gb_type));
    this->instance->set_pixel_buffering_mode(static_cast<GameInstance::PixelBufferMode>(settings.value(SETTINGS_BUFFER_MODE, instance->get_pixel_buffering_mode()).toInt()));
    this->instance->set_rewind_length(this->rewind_length);

    // Set window title and enable drag-n-dropping files
    this->setAcceptDrops(true);
    this->setWindowTitle("SuperDUX");

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
    this->save_state_menu->setEnabled(false);
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

    this->save_sram_now = file_menu->addAction("Save SRAM to Disk");
    this->save_sram_now->setEnabled(false);
    this->save_sram_now->setShortcut(QKeySequence::Save);
    this->save_sram_now->setIcon(GET_ICON("document-save"));
    connect(this->save_sram_now, &QAction::triggered, this, &GameWindow::action_save_sram);

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

    scaling->addSeparator();
    auto *scaling_filters = scaling->addMenu("Scaling Filter");
    std::pair<const char *, ScalingFilter> scaling_filters_all[] = {
        {"Nearest Neighbor", ScalingFilter::SCALING_FILTER_NEAREST},
        {"Bilinear", ScalingFilter::SCALING_FILTER_BILINEAR}
    };
    for(auto &i : scaling_filters_all) {
        auto *action = scaling_filters->addAction(i.first);
        action->setData(i.second);
        connect(action, &QAction::triggered, this, &GameWindow::action_set_scale_filter);
        action->setCheckable(true);
        action->setChecked(i.second == this->scaling_filter);
        this->scaling_filter_options.emplace_back(action);
    }

    // Color correction options
    auto *color_correction_mode = edit_menu->addMenu("Color Correction Mode");
    this->instance->set_color_correction_mode(this->color_correction_mode);
    std::pair<const char *, GB_color_correction_mode_t> color_correction_modes[] = {
        {"Disabled", GB_color_correction_mode_t::GB_COLOR_CORRECTION_DISABLED},
        {"Correct Curves", GB_color_correction_mode_t::GB_COLOR_CORRECTION_CORRECT_CURVES},
        {"Modern (Accurate)", GB_color_correction_mode_t::GB_COLOR_CORRECTION_MODERN_ACCURATE},
        {"Modern (Balanced)", GB_color_correction_mode_t::GB_COLOR_CORRECTION_MODERN_BALANCED},
        {"Modern (Boost Contrast)", GB_color_correction_mode_t::GB_COLOR_CORRECTION_MODERN_BOOST_CONTRAST},
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
    this->reset_rom_action->setShortcut(static_cast<int>(Qt::CTRL) | Qt::Key::Key_R);
    connect(this->reset_rom_action, &QAction::triggered, this, &GameWindow::action_reset);
    this->reset_rom_action->setIcon(GET_ICON("view-refresh"));
    this->reset_rom_action->setEnabled(false);
    emulation_menu->addSeparator();

    // Create the printer
    this->printer_window = new Printer(this);
    this->show_printer = emulation_menu->addAction("Show Printer");
    this->show_printer->setShortcut(QKeySequence::Print);
    connect(this->show_printer, &QAction::triggered, this->printer_window, &Printer::show);
    connect(this->show_printer, &QAction::triggered, this->printer_window, &Printer::activateWindow);
    this->show_printer->setEnabled(false);

    emulation_menu->addSeparator();

    // Debug menu
    auto *debug_menu = bar->addMenu("Debug");
    connect(debug_menu, &QMenu::aboutToShow, this, &GameWindow::action_showing_menu);
    connect(debug_menu, &QMenu::aboutToHide, this, &GameWindow::action_hiding_menu);

    this->show_fps_button = debug_menu->addAction("Show FPS");
    connect(this->show_fps_button, &QAction::triggered, this, &GameWindow::action_toggle_showing_fps);
    this->show_fps_button->setCheckable(true);
    debug_menu->addSeparator();

    // Here's the layout
    auto *central_widget = new QWidget(this);
    auto *layout = new QHBoxLayout(central_widget);
    layout->setContentsMargins(0,0,0,0);
    central_widget->setLayout(layout);
    this->setCentralWidget(central_widget);

    // Set our pixel buffer parameters
    this->pixel_buffer_view = new GamePixelBufferView(central_widget, this);
    this->pixel_buffer_view->setFrameStyle(0);
    this->pixel_buffer_view->setHorizontalScrollBarPolicy(Qt::ScrollBarPolicy::ScrollBarAlwaysOff);
    this->pixel_buffer_view->setVerticalScrollBarPolicy(Qt::ScrollBarPolicy::ScrollBarAlwaysOff);
    this->pixel_buffer_view->setSizePolicy(QSizePolicy::Policy::Fixed, QSizePolicy::Policy::Fixed);
    layout->addWidget(this->pixel_buffer_view);

    // Create the debugger now that everything else is set up
    this->debugger_window = new Debugger(this);
    this->show_debugger = debug_menu->addAction("Show Debugger");
    connect(this->show_debugger, &QAction::triggered, this->debugger_window, &Debugger::show);
    connect(this->show_debugger, &QAction::triggered, this->debugger_window, &Debugger::activateWindow);
    this->show_debugger->setEnabled(false);

    // And the VRAM viewer
    this->vram_viewer_window = new VRAMViewer(this);
    this->show_vram_viewer = debug_menu->addAction("Show VRAM Viewer");
    connect(this->show_vram_viewer, &QAction::triggered, this->vram_viewer_window, &VRAMViewer::show);
    connect(this->show_vram_viewer, &QAction::triggered, this->vram_viewer_window, &VRAMViewer::activateWindow);
    this->show_vram_viewer->setEnabled(false);

    // Now, set this
    this->set_pixel_view_scaling(this->scaling);

    // If showing FPS, trigger it
    if(this->show_fps) {
        this->show_fps = false;
        this->action_toggle_showing_fps();
    }

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

    // Update the emulation speed
    this->update_emulation_speed();
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
    auto path = std::filesystem::path(rom_path);

    std::error_code ec;
    if(!std::filesystem::exists(path, ec)) {
        if(ec) {
            char err[256];
            std::snprintf(err, sizeof(err), "Error: Cannot open ROM (%s)", ec.message().c_str());
            print_debug_message("%s\n", err);
            this->show_status_text(err);
        }
        else {
            this->show_status_text("Error: ROM not found");
            print_debug_message("Could not find %s\n", rom_path);
        }
        return;
    }

    this->instance->remove_all_breakpoints();
    this->save_if_loaded();
    this->reset_rom_action->setEnabled(true);
    this->exit_without_saving->setEnabled(true);

    // Remove the ROM from the list (if present) so we can re-place it at the top of the list
    this->recent_roms.removeAll(rom_path);
    this->recent_roms.push_front(rom_path);
    this->recent_roms = this->recent_roms.mid(0, 10);
    this->update_recent_roms_list();

    // Make path
    this->save_path = std::filesystem::path(path).replace_extension(".sav");
    auto sym_path = std::filesystem::path(path).replace_extension(".sym");

    // Success?
    int r = 0;

    if(path.extension() == ".isx") {
        r = this->instance->load_isx(path, save_path, sym_path);
    }
    else {
        FILE *f = std::fopen(rom_path, "rb");

        if(!f) {
            this->show_status_text("Error: Failed to open ROM");
            print_debug_message("fopen(): Could not open %s\n", rom_path);
            return;
        }

        std::vector<std::uint8_t> rom_data;
        std::fseek(f, 0, SEEK_END);
        auto l = std::ftell(f);

        // Allocate space for the ROM
        try {
            rom_data.resize(l);
        }
        catch(std::exception &e) {
            this->show_status_text("Error: Failed to read ROM");
            print_debug_message("std::vector::resize(): Could not resize for %li bytes: %s\n", l, e.what());
            std::fclose(f);
            return;
        }

        std::fseek(f, 0, SEEK_SET);

        // Read it
        if(std::fread(rom_data.data(), rom_data.size(), 1, f) != 1) {
            this->show_status_text("Error: Failed to read ROM");
            print_debug_message("fread(): Could not read %s\n", rom_path);
            std::fclose(f);
            return;
        }

        std::fclose(f);

        // Is it even valid?
        if(rom_data.size() < 0x150) {
            this->show_status_text("Error: Invalid ROM");
            print_debug_message("ROM %s is too small (%zu < %zu)\n", rom_path, rom_data.size(), static_cast<std::size_t>(0x150));
            std::fclose(f);
            return;
        }

        // Validate some things - https://gbdev.io/pandocs/The_Cartridge_Header.html
        if(integrity_check_corrupt || integrity_check_compatible) {
            bool valid_header_checksum;
            bool valid_nintendo_logo;
            bool valid_nintendo_logo_cgb;
            bool valid_cartridge_checksum;
            bool would_load_on_actual_hardware;

            bool corrupt;
            bool incompatible;

            // Figure out the header checksum
            std::uint8_t x = 0;
            for(std::size_t i = 0x134; i < 0x14D; i++) {
                x = x - rom_data[i] - 1;
            }
            valid_header_checksum = x == rom_data[0x14D];

            // Now check the Nintendo logo
            static constexpr const std::uint8_t logo_bytes[] = { 0xCE, 0xED, 0x66, 0x66, 0xCC, 0x0D, 0x00, 0x0B, 0x03, 0x73, 0x00, 0x83, 0x00, 0x0C, 0x00, 0x0D, 0x00, 0x08, 0x11, 0x1F, 0x88, 0x89, 0x00, 0x0E, 0xDC, 0xCC, 0x6E, 0xE6, 0xDD, 0xDD, 0xD9, 0x99, 0xBB, 0xBB, 0x67, 0x63, 0x6E, 0x0E, 0xEC, 0xCC, 0xDD, 0xDC, 0x99, 0x9F, 0xBB, 0xB9, 0x33, 0x3E };
            valid_nintendo_logo = std::memcmp(&rom_data[0x104], logo_bytes, sizeof(logo_bytes)) == 0;
            valid_nintendo_logo_cgb = std::memcmp(&rom_data[0x104], logo_bytes, sizeof(logo_bytes) / 2) == 0;

            // Now check the global checksum
            std::uint16_t global_checksum = 0;
            for(std::size_t i = 0; i < 0x14E; i++) {
                global_checksum += rom_data[i];
            }
            for(std::size_t i = 0x150; i < rom_data.size(); i++) {
                global_checksum += rom_data[i];
            }

            // Needs to be big or little endian to do this check
            std::uint16_t expected_global_checksum = *reinterpret_cast<const std::uint16_t *>(&rom_data[0x14E]);
            static_assert(std::endian::native == std::endian::little || std::endian::native == std::endian::big);

            // The checksum is stored in big endian so it needs swapped
            if(std::endian::native == std::endian::little) {
                expected_global_checksum = ((expected_global_checksum & 0xFF) << 8) | ((expected_global_checksum & 0xFF00) >> 8);
            }
            valid_cartridge_checksum = expected_global_checksum == global_checksum;

            // Would this load on real hardware? Or is it also corrupt?
            would_load_on_actual_hardware = valid_header_checksum && (this->gb_type == GameBoyType::GameBoyGBC ? valid_nintendo_logo_cgb : valid_nintendo_logo);
            corrupt = !(valid_nintendo_logo && valid_cartridge_checksum && valid_header_checksum);
            incompatible = !would_load_on_actual_hardware;

            // Do we fail?
            bool fails = false;
            if(integrity_check_corrupt) {
                fails = fails || corrupt;
            }
            if(integrity_check_compatible) {
                fails = fails || incompatible;
            }

            // If we failed, let's figure out why and show a message to the user
            if(fails) {
                // First build our message
                QString message;
                if(!valid_header_checksum) {
                    message += "- The header checksum is wrong\n";
                }
                if(!valid_nintendo_logo) {
                    if(valid_nintendo_logo_cgb) {
                        message += "- The second half of the header logo is wrong\n";
                    }
                    else {
                        message += "- The entire header logo is wrong\n";
                    }
                }
                if(!valid_cartridge_checksum) {
                    message += "- The cartridge checksum is wrong\n";
                }

                // We shouldn't say the ROM is corrupt if it (probably) isn't
                QString title;
                message = " These issues were found:\n\n" + message;
                if(corrupt) {
                    message = "The ROM appears to be corrupt." + message;
                    title = "ROM Appears Corrupt";
                }
                else {
                    message = "The ROM appears to be invalid." + message;
                    title = "ROM Appears Invalid";
                }

                // Warn what may happen if it is loaded
                message += "\n";
                if(would_load_on_actual_hardware) {
                    if(!corrupt) {
                        message += "This will technically load on actual hardware with the given configuration, but it may have issues or not work fully.";
                    }
                    else {
                        message += "This will technically load on actual hardware with the given configuration, but it may have issues or not work correctly.";
                    }
                }
                else {
                    message += "This will NOT load on real hardware with the given configuration, and it will likely crash if it's not a Game Boy ROM.";
                }

                // Want to load anyway?
                message += "\n\nWould you like to try to load this ROM anyway?";

                QMessageBox warning(QMessageBox::Icon::Warning, title, message, static_cast<QMessageBox::StandardButtons>(QMessageBox::StandardButton::Cancel) | QMessageBox::StandardButton::Ok);

                if(warning.exec() != QMessageBox::StandardButton::Ok) {
                    return;
                }
            }
        }

        this->instance->load_rom(reinterpret_cast<const std::byte *>(rom_data.data()), rom_data.size(), save_path, sym_path);
    }

    // Success!
    if(r == 0) {
        // Start the thread
        if(!instance_thread.joinable()) {
            instance_thread = std::thread(GameInstance::start_game_loop, this->instance.get());
        }

        // Enable these options
        this->show_debugger->setEnabled(true);
        this->show_vram_viewer->setEnabled(true);
        this->save_state_menu->setEnabled(true);
        this->save_sram_now->setEnabled(true);
        this->show_printer->setEnabled(true);

        // Fire this once
        this->game_loop();
    }
}


void GameWindow::action_clear_all_roms() noexcept {
    this->recent_roms.clear();
    this->update_recent_roms_list();
}

void GameWindow::action_clear_missing_roms() noexcept {
    auto recent_roms_copy = this->recent_roms;
    decltype(this->recent_roms) new_recent_roms;

    bool retain_inaccessible_roms = false;
    for(auto &i : recent_roms_copy) {
        bool should_retain;

        // Check if it exists
        std::error_code ec;
        if(std::filesystem::exists(i.toStdString(), ec)) {
            should_retain = true;
        }
        else if(ec) {
            // Did the user hit "No to all"?
            if(retain_inaccessible_roms) {
                should_retain = true;
            }

            // If not, ask
            else {
                char msg[512];
                std::snprintf(msg, sizeof(msg), "Failed to query if %s exists due to OS error %i:\n\n%s\n\nThe file may or may not still exist, but the underlying location could not be accessed.\n\nDo you want to remove this as well?", i.toUtf8().data(), ec.value(), ec.message().c_str());

                auto r = QMessageBox(QMessageBox::Icon::Question, "Unable to verify if a ROM exists", msg, QMessageBox::StandardButton::Yes | QMessageBox::StandardButton::No | QMessageBox::StandardButton::NoToAll).exec();

                switch(r) {
                    case QMessageBox::StandardButton::NoToAll:
                        retain_inaccessible_roms = true;
                        // fallthrough
                    case QMessageBox::StandardButton::No:
                        should_retain = true;
                        break;

                    // User hit yes
                    default:
                    case QMessageBox::StandardButton::Yes:
                        should_retain = false;
                        break;
                }
            }
        }
        else {
            should_retain = false;
        }

        if(should_retain) {
            new_recent_roms.append(i);
        }
    }

    this->recent_roms = std::move(new_recent_roms);
    this->update_recent_roms_list();
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

    // Add some clean up buttons
    bool recent_roms_empty = this->recent_roms.empty();
    if(!recent_roms_empty) {
        this->recent_roms_menu->addSeparator();
    }

    // Add buttons for removing missing/all ROMs
    if(!recent_roms_empty) {
        auto *clear_missing = this->recent_roms_menu->addAction("Clear Missing");
        auto *clear_all = this->recent_roms_menu->addAction("Clear All");
        connect(clear_all, &QAction::triggered, this, &GameWindow::action_clear_all_roms);
        connect(clear_missing, &QAction::triggered, this, &GameWindow::action_clear_missing_roms);
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

    // Output pixel buffer
    this->pixel_buffer_pixmap.convertFromImage(QImage(reinterpret_cast<const uchar *>(this->pixel_buffer.data()), width, height, QImage::Format::Format_ARGB32));

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
        auto fps = this->instance->get_frame_rate();
        auto multiplier = this->base_multiplier * this->rewind_multiplier * this->slowmo_multiplier * this->turbo_multiplier;

        if(this->last_fps != fps || this->last_speed != multiplier) {
            char fps_str[64];
            char mul_str[64] = {};
            char fps_text_str[64];
            if(fps == 0.0) {
                std::snprintf(fps_str, sizeof(fps_str), "--");
            }
            else {
                std::snprintf(fps_str, sizeof(fps_str), "%.01f", fps);
            }

            if(multiplier != 1.0) {
                std::snprintf(mul_str, sizeof(mul_str), "(%.01f%% speed)", multiplier * 100.0);
            }

            std::snprintf(fps_text_str, sizeof(fps_text_str), "FPS: %-6s %s", fps_str, mul_str);

            this->fps_text->setPlainText(fps_text_str);
            this->last_fps = fps;
            this->last_speed = multiplier;
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

    auto view_width = width * this->scaling;
    auto view_height = height * this->scaling;
    this->pixel_buffer_view->setFixedSize(view_width, view_height);
    this->pixel_buffer_view->setTransform(QTransform::fromScale(scaling, scaling));

    switch(this->scaling_filter) {
        case ScalingFilter::SCALING_FILTER_NEAREST:
            this->pixel_buffer_pixmap_item->resetTransform();
            this->pixel_buffer_pixmap_item->setTransformationMode(Qt::TransformationMode::FastTransformation);
            break;
        case ScalingFilter::SCALING_FILTER_BILINEAR:
            this->pixel_buffer_pixmap_item->resetTransform();
            this->pixel_buffer_pixmap_item->setTransformationMode(Qt::TransformationMode::SmoothTransformation);
            break;
    }

    this->make_shadow(this->fps_text);
    this->make_shadow(this->status_text);
    this->redraw_pixel_buffer();

    // Go through all scaling options. Uncheck/check whatever applies.
    for(auto *option : this->scaling_options) {
        option->setChecked(option->data().toInt() == scaling);
    }

    this->setFixedSize(view_width, view_height + this->menuBar()->sizeHint().height());
}

void GameWindow::game_loop() {
    this->redraw_pixel_buffer();
    this->debugger_window->refresh_view();
    this->vram_viewer_window->refresh_view();
    this->printer_window->refresh_view();

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

void GameWindow::action_set_scale_filter() noexcept {
    MAKE_MODE_SETTER_WITH_VARIABLE(this->scaling_filter, this->scaling_filter_options);
    this->set_pixel_view_scaling(this->scaling);
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
    this->show_fps_button->setChecked(this->show_fps);

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
    qfd.setNameFilters(QStringList { "Any Game Boy Game (*.gb *.gbc *.sgb *.bin *.isx)", "Game Boy ROM (*.gb)", "Game Boy Color ROM (*.gbc)", "Super Game Boy Enhanced ROM (*.sgb)", "BIN File (*.bin)", "ISX Binary (*.isx)" });

    if(qfd.exec() == QDialog::DialogCode::Accepted) {
        this->load_rom(qfd.selectedFiles().at(0).toUtf8().data());
    }
}

void GameWindow::disconnect_serial() {
    this->instance->disconnect_serial();
    this->printer_window->force_disconnect_printer();
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
        char message[256];
        std::snprintf(message, sizeof(message), "Failed to save %s", filename.c_str());
        this->show_status_text(message);
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
    this->instance->set_model(this->model_for_type(this->gb_type), this->use_border_for_type(this->gb_type) ? GB_border_mode_t::GB_BORDER_ALWAYS : GB_border_mode_t::GB_BORDER_NEVER);

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

    auto settings = get_superdux_settings();
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
    settings.setValue(SETTINGS_REWIND_SPEED, this->rewind_speed);
    settings.setValue(SETTINGS_MAX_SLOWMO, this->max_slowmo);
    settings.setValue(SETTINGS_MAX_TURBO, this->max_turbo);
    settings.setValue(SETTINGS_BASE_SPEED, this->base_multiplier);
    settings.setValue(SETTINGS_MAX_CPU_MULTIPLIER, this->max_cpu_multiplier);
    settings.setValue(SETTINGS_REWIND_ENABLED, this->rewind_enabled);
    settings.setValue(SETTINGS_SLOWMO_ENABLED, this->slowmo_enabled);
    settings.setValue(SETTINGS_TURBO_ENABLED, this->turbo_enabled);
    settings.setValue(SETTINGS_SCALING_FILTER, this->scaling_filter);
    settings.setValue(SETTINGS_INTEGRITY_CHECK_CORRUPT, this->integrity_check_corrupt);
    settings.setValue(SETTINGS_INTEGRITY_CHECK_COMPATIBLE, this->integrity_check_compatible);

    settings.setValue(SETTINGS_GB_BOOT_ROM, this->gb_boot_rom_path.value_or(std::filesystem::path()).string().c_str());
    settings.setValue(SETTINGS_GBC_BOOT_ROM, this->gbc_boot_rom_path.value_or(std::filesystem::path()).string().c_str());
    settings.setValue(SETTINGS_GBA_BOOT_ROM, this->gba_boot_rom_path.value_or(std::filesystem::path()).string().c_str());
    settings.setValue(SETTINGS_SGB_BOOT_ROM, this->sgb_boot_rom_path.value_or(std::filesystem::path()).string().c_str());
    settings.setValue(SETTINGS_SGB2_BOOT_ROM, this->sgb2_boot_rom_path.value_or(std::filesystem::path()).string().c_str());

    settings.setValue(SETTINGS_GB_ALLOW_EXTERNAL_BOOT_ROM, this->gb_allow_external_boot_rom);
    settings.setValue(SETTINGS_GBC_ALLOW_EXTERNAL_BOOT_ROM, this->gbc_allow_external_boot_rom);
    settings.setValue(SETTINGS_GBA_ALLOW_EXTERNAL_BOOT_ROM, this->gba_allow_external_boot_rom);
    settings.setValue(SETTINGS_SGB_ALLOW_EXTERNAL_BOOT_ROM, this->sgb_allow_external_boot_rom);
    settings.setValue(SETTINGS_SGB2_ALLOW_EXTERNAL_BOOT_ROM, this->sgb2_allow_external_boot_rom);

    settings.setValue(SETTINGS_GB_REVISION, this->gb_rev);
    settings.setValue(SETTINGS_GBC_REVISION, this->gbc_rev);
    settings.setValue(SETTINGS_GBA_REVISION, this->gba_rev);
    settings.setValue(SETTINGS_SGB_REVISION, this->sgb_rev);
    settings.setValue(SETTINGS_SGB2_REVISION, this->sgb2_rev);

    settings.setValue(SETTINGS_SGB_BORDER, this->sgb_border);
    settings.setValue(SETTINGS_SGB2_BORDER, this->sgb2_border);
    settings.setValue(SETTINGS_GB_BORDER, this->gb_border);
    settings.setValue(SETTINGS_GBC_BORDER, this->gbc_border);
    settings.setValue(SETTINGS_GBA_BORDER, this->gba_border);

    settings.setValue(SETTINGS_GBC_FAST_BOOT, this->gbc_fast_boot_rom);
    settings.setValue(SETTINGS_SGB_SKIP_INTRO, this->sgb_skip_intro);
    settings.setValue(SETTINGS_SGB2_SKIP_INTRO, this->sgb2_skip_intro);

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
                this->turbo_multiplier = 1.0 + max_increase * ((input - 0.1) / 0.9);
            }
            else {
                this->turbo_multiplier = 1.0;
            }
            this->update_emulation_speed();
            break;
        case InputDevice::Input_Slowmo:
            if(this->slowmo_enabled && input > 0.1) {
                double max_decrease = (1.0 - this->max_slowmo);
                this->slowmo_multiplier = this->max_slowmo + max_decrease * (1.0 - (input - 0.1) / 0.9);
            }
            else {
                this->slowmo_multiplier = 1.0;
            }
            this->update_emulation_speed();
            break;
        case InputDevice::Input_Rewind:
            if(!this->rewind_enabled) {
                boolean_input = false;
            }
            this->rewind_multiplier = boolean_input ? -rewind_speed : 1.0;
            this->update_emulation_speed();
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
        case InputDevice::Input_ShowFPS:
            if(boolean_input) {
                this->action_toggle_showing_fps();
            }
            break;
        default: break;
    }
}

void GameWindow::update_emulation_speed() {
    double total_multiplier = this->base_multiplier * this->rewind_multiplier * this->turbo_multiplier * this->slowmo_multiplier;
    double abs_speed = std::fabs(total_multiplier);

    // Are we rewinding?
    this->instance->set_rewind(total_multiplier < 0.0);

    // Do we exceed the max CPU speed? If so, engage turbo
    double cpu_speed = std::min(abs_speed, this->max_cpu_multiplier);
    if(cpu_speed != abs_speed) {
        this->instance->set_turbo_mode(true, 1.0 + (abs_speed - cpu_speed) / cpu_speed);
    }
    else {
        this->instance->set_turbo_mode(false);
    }

    // Set the multiplier
    this->instance->set_speed_multiplier(cpu_speed);
}

void GameWindow::reset_emulation_speed() {
    this->rewind_multiplier = 1.0F;
    this->slowmo_multiplier = 1.0F;
    this->turbo_multiplier = 1.0F;
    this->update_emulation_speed();
}

std::filesystem::path GameWindow::get_save_state_path(int index) const {
    char extension[64];
    std::snprintf(extension, sizeof(extension), ".s%i", index); // use SameBoy's convention (.s0, .s1, etc.)
    return std::filesystem::path(this->save_path).replace_extension(extension);
}

void GameWindow::action_create_save_state() {
    // Nope!
    if(!this->save_states_allowed()) {
        return;
    }

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
    // Nope!
    if(!this->save_states_allowed()) {
        return false;
    }

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
    // Nope!
    if(!this->save_states_allowed()) {
        return;
    }

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
    // Nope!
    if(!this->save_states_allowed()) {
        return;
    }

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

bool GameWindow::save_states_allowed() noexcept {
    return this->instance->is_rom_loaded();
}

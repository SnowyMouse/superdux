#include "game_instance.hpp"

#include <agb_boot.h>
#include <sgb_boot.h>
#include <cgb_boot.h>
#include <dmg_boot.h>
#include <sgb2_boot.h>

#include <chrono>
#include <cstring>
#include <thread>

#define MAKE_GETTER(what) { \
    this->mutex.lock(); \
    auto r = what; \
    this->mutex.unlock(); \
    return r; \
}

#define MAKE_SETTER(...) { \
    this->mutex.lock(); \
    __VA_ARGS__; \
    this->mutex.unlock(); \
}

// Copy the string into a buffer allocated with malloc() since GB_gameboy_s deallocates it with free()
static char *malloc_string(const char *string) {
    auto string_length = std::strlen(string);
    char *str = reinterpret_cast<char *>(calloc(string_length + 1, sizeof(*str)));
    std::strncpy(str, string, string_length);
    return str;
}

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
            std::fprintf(stderr, "Unable to find a suitable boot ROM for GB_boot_rom_t type %i\n", type);
            break;
    }
}

static std::uint32_t rgb_encode(GB_gameboy_t *, uint8_t r, uint8_t g, uint8_t b) {
    return 0xFF000000 | (r << 16) | (g << 8) | (b << 0);
}

void GameInstance::on_vblank(GB_gameboy_s *gameboy) noexcept {
    auto *instance = resolve_instance(gameboy);

    // If we need to wait for a frame, do it
    if(instance->turbo_mode_enabled) {
        while(clock::now() < instance->next_expected_frame) {
        }
        instance->next_expected_frame = clock::now() + std::chrono::microseconds(static_cast<unsigned long>(1000000.0 / GB_get_usual_frame_rate(&instance->gameboy) / instance->turbo_mode_speed_ratio));
    }
    
    // Increment the work buffer index by 1, wrapping around to 0 when we've hit the number of buffers
    instance->previous_buffer_second = instance->previous_buffer;
    instance->previous_buffer = instance->work_buffer;
    instance->work_buffer = (instance->work_buffer + 1) % (sizeof(instance->pixel_buffer) / sizeof(instance->pixel_buffer[0]));
    instance->assign_work_buffer();
    
    // Set this since we hit vblank
    instance->vblank_hit = true;
}

GameInstance::GameInstance(GB_model_t model) {
    GB_init(&this->gameboy, model);
    GB_set_user_data(&this->gameboy, this);
    GB_set_boot_rom_load_callback(&this->gameboy, load_boot_rom);
    GB_set_rgb_encode_callback(&this->gameboy, rgb_encode);
    GB_set_vblank_callback(&this->gameboy, GameInstance::on_vblank);
    GB_set_log_callback(&this->gameboy, GameInstance::on_log);
    GB_set_input_callback(&this->gameboy, GameInstance::on_input_requested);
    GB_apu_set_sample_callback(&this->gameboy, GameInstance::on_sample);
    
    this->update_pixel_buffer_size();
}

GameInstance::~GameInstance() {
    this->end_game_loop();
    this->close_sdl_audio_device();
    GB_free(&this->gameboy);
}

char *GameInstance::on_input_requested(GB_gameboy_s *gameboy) {
    auto *instance = resolve_instance(gameboy);
    instance->reset_audio();
    
    // Indicate we've paused
    instance->bp_paused = true;
    char *continue_text = nullptr;
    
    // Unlock mutex since the thread is now halted
    instance->mutex.unlock();
    
    // Check until we can continue
    while(true) {
        // Lock
        instance->mutex.lock();
        
        // Exit if we need to
        if(instance->loop_finishing) {
            continue_text = malloc_string("continue");
            break;
        }
        
        if(instance->continue_text.has_value()) {
            continue_text = malloc_string(instance->continue_text->c_str());
            break;
        }
        
        // Unlock
        instance->mutex.unlock();

        // Keep CPU usage low here
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    
    // Unpause (mutex is locked from loop)
    instance->continue_text = std::nullopt;
    return continue_text;
}

float GameInstance::get_frame_rate() noexcept MAKE_GETTER(this->frame_rate)

void GameInstance::reset() noexcept {
    this->mutex.lock();
    GB_reset(&this->gameboy);
    this->reset_audio();
    this->mutex.unlock();
}

void GameInstance::set_model(GB_model_t model) {
    this->mutex.lock();
    GB_switch_model_and_reset(&this->gameboy, model);
    this->reset_audio();
    this->update_pixel_buffer_size();
    this->mutex.unlock();
}

void GameInstance::start_game_loop(GameInstance *instance) noexcept {
    if(instance->loop_running) {
        std::terminate();
    }
    
    instance->loop_running = true;
    
    while(true) {
        // Burn the thread until we have the mutex (bad practice, but it minimizes latency)
        while(!instance->mutex.try_lock()) {}
        
        // Run some cycles on the gameboy
        if(!instance->manual_paused) {
            GB_run(&instance->gameboy);
            
            // Wait until the end of GB_run to calculate frame rate
            if(instance->vblank_hit) {
                auto now = clock::now();
                
                // Get time in microseconds (high precision) and convert to seconds, recording the time
                auto difference_us = std::chrono::duration_cast<std::chrono::microseconds>(now - instance->last_frame_time).count();
                auto fps_index = instance->frame_time_index;
                instance->frame_times[fps_index] = difference_us / 1000000.0;
                instance->last_frame_time = now;
                
                // Get buffer size
                static constexpr const std::size_t fps_buffer_size = (sizeof(instance->frame_times) / sizeof(instance->frame_times[0]));
                auto new_index = (fps_index + 1) % fps_buffer_size;
                instance->frame_time_index = new_index;
                if(new_index == 0) {
                    float f_total = 0.0;
                    for(auto f : instance->frame_times) {
                        f_total += f;
                    }
                    instance->frame_rate = fps_buffer_size / f_total;
                }
                
                // Done
                instance->vblank_hit = false;
            }
        }

        // If we're paused, we can sleep
        else {
            instance->mutex.unlock();
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            instance->mutex.lock();
        }
        
        // Are we getting done?
        if(instance->loop_finishing) {
            instance->mutex.unlock();
            break;
        }
        
        // Allow other threads to access our data for now
        instance->mutex.unlock();
    }
    
    instance->loop_running = false;
}

std::vector<std::pair<std::string, std::uint16_t>> GameInstance::get_backtrace() {
    std::vector<std::pair<std::string, std::uint16_t>> backtrace;
    auto *cmd = malloc_string("backtrace");
    
    // Get the backtrace string
    this->mutex.lock();
    auto backtrace_str = execute_command_without_mutex(cmd);
    std::size_t bt_count = get_gb_backtrace_size(&this->gameboy);
    backtrace.resize(bt_count);
    for(std::size_t b = 1; b < bt_count; b++) {
        backtrace[bt_count - b].second = get_gb_backtrace_address(&this->gameboy, b);
    }
    if(bt_count > 0) {
        backtrace[0].second = get_gb_register(&this->gameboy, gbz80_register::GBZ80_REG_PC);
    }
    this->mutex.unlock();
    
    // Process the backtraces now that the mutex has unlocked
    std::size_t backtrace_str_len = backtrace_str.size();
    std::size_t str_start = 0;
    std::size_t backtrace_line = 0;
    for(std::size_t b = 0; b < backtrace_str_len && backtrace_line < bt_count; b++) {
        if(backtrace_str[b] == '\n') {
            backtrace[backtrace_line].first = std::string(backtrace_str, str_start, b - str_start);
            str_start = b + 1;
            backtrace_line++;
        }
    }
    
    return backtrace;
}

std::vector<std::uint16_t> GameInstance::get_breakpoints() {
    std::vector<std::uint16_t> breakpoints;
    
    this->mutex.lock();
    std::size_t bp_count = get_gb_breakpoint_size(&this->gameboy);
    breakpoints.resize(bp_count);
    for(std::size_t b = 0; b < bp_count; b++) {
        breakpoints[b] = get_gb_breakpoint_address(&this->gameboy, b);
    }
    this->mutex.unlock();
    
    return breakpoints;
}

bool GameInstance::read_pixel_buffer(std::uint32_t *destination, std::size_t destination_length) noexcept {
    this->mutex.lock();
    auto required_length = this->pixel_buffer[0].size();
    bool success = required_length == destination_length;

    if(success) {
        std::size_t bytes = required_length * sizeof(*this->pixel_buffer[0].data());
        switch(this->pixel_buffer_mode) {
            case PixelBufferMode::PixelBufferSingle:
                std::memcpy(destination, this->pixel_buffer[this->work_buffer].data(), bytes);
                break;
            case PixelBufferMode::PixelBufferDouble:
                std::memcpy(destination, this->pixel_buffer[this->previous_buffer].data(), bytes);
                break;
            case PixelBufferMode::PixelBufferDoubleBlend:
                std::memcpy(destination, this->pixel_buffer[this->previous_buffer].data(), bytes);
                auto *a = reinterpret_cast<std::uint8_t *>(destination);
                auto *b = reinterpret_cast<std::uint8_t *>(this->pixel_buffer[this->previous_buffer_second].data());
                for(std::size_t q = 0; q < bytes; q++) {
                    a[q] = (static_cast<unsigned int>(a[q]) + static_cast<unsigned int>(b[q])) / 2;
                }
                break;
        }
    }

    // Done
    this->mutex.unlock();
    
    // Done
    return success;
}

void GameInstance::end_game_loop() noexcept {
    this->mutex.lock();
    
    // Are we already finishing?
    if(this->loop_finishing) {
        this->mutex.unlock();
        return;
    }
    
    // Finish now
    this->loop_finishing = true;
    this->mutex.unlock();
    
    bool finished = false;
    while(!finished) {
        finished = !this->loop_running;
    }
    
    this->mutex.lock();
    this->loop_finishing = false;
    this->mutex.unlock();
}

void GameInstance::set_button_state(GB_key_t button, bool pressed) MAKE_SETTER(GB_set_key_state(&this->gameboy, button, pressed))

void GameInstance::get_dimensions(std::uint32_t &width, std::uint32_t &height) noexcept {
    this->mutex.lock();
    height = GB_get_screen_height(&this->gameboy);
    width = GB_get_screen_width(&this->gameboy);
    this->mutex.unlock();
}

void GameInstance::update_pixel_buffer_size() {
    for(auto &i : this->pixel_buffer) {
        i = std::vector<std::uint32_t>(this->get_pixel_buffer_size_without_mutex(), 0xFF000000);
        this->work_buffer = 0;
        this->previous_buffer = 0;
        this->previous_buffer_second = 0;
        this->assign_work_buffer();
    }
}

std::vector<std::int16_t> GameInstance::get_sample_buffer() noexcept {
    this->mutex.lock();
    auto ret = std::move(this->sample_buffer);
    this->sample_buffer.clear();
    this->mutex.unlock();
    return ret;
}

void GameInstance::transfer_sample_buffer(std::vector<std::int16_t> &destination) noexcept {
    this->mutex.lock();
    destination.insert(destination.end(), this->sample_buffer.begin(), this->sample_buffer.end());
    this->sample_buffer.clear();
    this->mutex.unlock();
}

void GameInstance::on_sample(GB_gameboy_s *gameboy, GB_sample_t *sample) {
    auto *instance = resolve_instance(gameboy);
    if(instance->audio_enabled) {
        auto &buffer = instance->sample_buffer;

        auto &left = sample->left;
        auto &right = sample->right;

        // Do we have to modify any samples?
        if(instance->volume < 100 || instance->force_mono) {
            // Convert to mono if we want
            if(instance->force_mono) {
                left = (left + right) / 2;
                right = left;
            }

            // Scale samples (logarithm to linear)
            if(instance->volume < 100 && instance->volume >= 0) {
                left *= instance->volume_scale;
                right *= instance->volume_scale;
            }
        }

        // Send them to SDL if we need to
        if(instance->sdl_audio_device.has_value()) {
            auto dev = instance->sdl_audio_device.value();

            // Doing these checks can be kinda hacky, but sameboy does not send samples at precisely the sample rate, and in some cases (such as SGB/SGB2's intro ), sends way too many samples

            // Check how many frames queued
            std::size_t frames_queued = SDL_GetQueuedAudioSize(dev) / sizeof(*sample);
            auto buffer_size = instance->sdl_audio_buffer_size;
            bool turbo_mode = instance->turbo_mode_enabled;
            std::size_t max_frames_queued = buffer_size * (turbo_mode ? 4 : 8);

            // If we have too many frames queued, flush the buffer (causes popping but prevents high delay)
            if(frames_queued > max_frames_queued) {
                if(!turbo_mode) {
                    instance->reset_audio();
                }
                return;
            }

            // Add our samples
            instance->sample_buffer.emplace_back(left);
            instance->sample_buffer.emplace_back(right);

            // If in turbo mode, send samples as we get them. Otherwise, buffer them and send when ready.
            std::size_t required_buffered_frames = turbo_mode ? 0 :
                                                   (
                                                       frames_queued < buffer_size * 2 ? buffer_size * 4 : // if we have no frames queued, send a large buffer to ensure we always have samples (prevents popping)
                                                                                         buffer_size
                                                   );
            std::size_t actual_buffered_frames = instance->sample_buffer.size() / 2;

            if(actual_buffered_frames >= required_buffered_frames) {
                SDL_QueueAudio(dev, instance->sample_buffer.data(), instance->sample_buffer.size() * sizeof(*instance->sample_buffer.data()));
                instance->sample_buffer.clear();
                instance->unpause_sdl_audio();
            }
        }

        // Otherwise, just emplace it
        else {
            buffer.emplace_back(left);
            buffer.emplace_back(right);
        }
    }
}

void GameInstance::set_audio_enabled(bool enabled, std::uint32_t sample_rate) noexcept {
    this->mutex.lock();
    this->sample_buffer.clear();
    
    if(enabled) {
        if(!this->sdl_audio_device.has_value()) {
            this->set_current_sample_rate(sample_rate);
            this->sample_buffer.reserve(sample_rate); // reserve one second
            GB_set_sample_rate(&this->gameboy, sample_rate);
        }
    }
    else if(!this->sdl_audio_device.has_value()) {
        this->set_current_sample_rate(0);
    }

    this->reset_audio();
    this->audio_enabled = enabled;
    this->mutex.unlock();
}

void GameInstance::set_speed_multiplier(double speed_multiplier) noexcept MAKE_SETTER(GB_set_clock_multiplier(&this->gameboy, speed_multiplier))
bool GameInstance::is_audio_enabled() noexcept MAKE_GETTER(this->audio_enabled)

std::size_t GameInstance::get_pixel_buffer_size() noexcept {
    std::uint32_t height, width;
    this->get_dimensions(width, height);
    return height*width;
}

void GameInstance::on_log(GB_gameboy_s *gameboy, const char *log, GB_log_attributes) noexcept {
    auto *instance = resolve_instance(gameboy);
    
    if(instance->log_buffer_retained) {
        instance->log_buffer += log;
    }
    else {
        std::printf("%s", log);
    }
}

std::string GameInstance::clear_log_buffer() {
    auto buffer_copy = std::move(this->log_buffer);
    this->log_buffer.clear();
    return buffer_copy;
}

void GameInstance::break_immediately() noexcept {
    this->mutex.lock();
    GB_debugger_break(&this->gameboy);
    this->mutex.unlock();
}

void GameInstance::unbreak(const char *command) {
    if(this->is_paused_from_breakpoint()) {
        this->mutex.lock();
        this->continue_text = command;
        this->bp_paused = false;
        this->mutex.unlock();
    }
}

std::uint16_t GameInstance::get_register_value(gbz80_register reg) noexcept MAKE_GETTER(get_gb_register(&this->gameboy, reg))
void GameInstance::set_register_value(gbz80_register reg, std::uint16_t value) noexcept MAKE_SETTER(set_gb_register(&this->gameboy, reg, value))

std::optional<std::uint16_t> GameInstance::evaluate_expression(const char *expression) noexcept {
    std::uint16_t result_maybe;
    if(GB_debugger_evaluate(&this->gameboy, expression, &result_maybe, nullptr) == 0) {
        return result_maybe;
    }
    else {
        return std::nullopt;
    }
}

void GameInstance::assign_work_buffer() noexcept {
    GB_set_pixels_output(&this->gameboy, this->pixel_buffer[this->work_buffer].data());
}

int GameInstance::load_rom(const std::filesystem::path &rom_path, const std::optional<std::filesystem::path> &sram_path, const std::optional<std::filesystem::path> &symbol_path) noexcept {
    this->mutex.lock();

    // Pause SDL audio
    this->reset_audio();
    
    // Reset the gameboy
    GB_reset(&this->gameboy);
    
    // Reset frame times
    this->frame_time_index = 0;
    this->last_frame_time = clock::now();
    
    // Load the ROM
    int result = GB_load_rom(&this->gameboy, rom_path.string().c_str());
    
    // If successful, load the battery and symbol files
    if(result == 0) {
        this->load_save_and_symbols(sram_path, symbol_path);
    }
    
    this->mutex.unlock();
    return result;
}

int GameInstance::load_isx(const std::filesystem::path &isx_path, const std::optional<std::filesystem::path> &sram_path, const std::optional<std::filesystem::path> &symbol_path) noexcept {
    this->mutex.lock();

    // Pause the audio
    this->reset_audio();
    
    // Reset the gameboy
    GB_reset(&this->gameboy);
    
    // Load the ISX
    int result = GB_load_isx(&this->gameboy, isx_path.string().c_str());
    
    // If successful, load the battery and symbol files
    if(result == 0) {
        this->load_save_and_symbols(sram_path, symbol_path);
    }
    
    this->mutex.unlock();
    return result;
}

void GameInstance::load_save_and_symbols(const std::optional<std::filesystem::path> &sram_path, const std::optional<std::filesystem::path> &symbol_path) {
    GB_debugger_clear_symbols(&this->gameboy);
    this->rom_loaded = true;
    
    if(sram_path.has_value()) {
        GB_load_battery(&this->gameboy, sram_path->string().c_str());
    }
    if(symbol_path.has_value()) {
        GB_debugger_load_symbol_file(&this->gameboy, symbol_path->string().c_str());
    }
}


int GameInstance::save_sram(const std::filesystem::path &path) noexcept MAKE_GETTER(GB_save_battery(&this->gameboy, path.string().c_str()))

std::string GameInstance::execute_command_without_mutex(char *command) {
    this->retain_logs(true);
    GB_debugger_execute_command(&this->gameboy, command);
    this->retain_logs(false);
    return this->clear_log_buffer();
}

std::string GameInstance::execute_command(const char *command) {
    // Allocate the command
    char *cmd = malloc_string(command);
    
    // Lock the mutex.
    this->mutex.lock();
    
    // Execute
    auto logs = this->execute_command_without_mutex(cmd);
    
    // Unlock mutex
    this->mutex.unlock();
    
    // Done
    return logs;
}

std::string GameInstance::disassemble_address(std::uint16_t address, std::uint8_t count) {
    // Lock the mutex. Enable log retention
    this->mutex.lock();
    this->retain_logs(true);
    
    // Execute
    GB_cpu_disassemble(&this->gameboy, address, count);
    
    // Disable log retention, unlock mutex, and get what we got
    this->retain_logs(false);
    auto logs = this->clear_log_buffer();
    this->mutex.unlock();
    
    // Done
    return logs;
}

std::size_t GameInstance::get_pixel_buffer_size_without_mutex() noexcept {
    return GB_get_screen_width(&this->gameboy) * GB_get_screen_height(&this->gameboy);
}
bool GameInstance::set_up_sdl_audio(std::uint32_t sample_rate, std::uint32_t buffer_size) noexcept {
    this->mutex.lock();
    SDL_AudioSpec request = {}, result = {};
    request.freq = sample_rate;
    request.format = AUDIO_S16SYS;
    request.channels = 2;
    request.samples = buffer_size;
    request.userdata = this;

    auto device = SDL_OpenAudioDevice(0, 0, &request, &result, SDL_AUDIO_ALLOW_FREQUENCY_CHANGE | SDL_AUDIO_ALLOW_SAMPLES_CHANGE);
    if(device != 0) {
        this->close_sdl_audio_device();

        // Now...
        this->set_current_sample_rate(result.freq);
        this->sdl_audio_device = device;
        this->sdl_audio_buffer_size = result.samples;
        this->sample_buffer.reserve(this->current_sample_rate); // reserve one second
        GB_set_sample_rate(&this->gameboy, this->current_sample_rate);
    }

    this->mutex.unlock();
    return device > 0;
}

int GameInstance::get_volume() noexcept MAKE_GETTER(this->volume)
void GameInstance::set_volume(int volume) noexcept {
    this->mutex.lock();
    this->volume = std::min(100, std::max(0, volume)); // clamp from 0 to 100
    this->volume_scale = std::pow(100.0, this->volume / 100.0) / 100.0 - 0.01 * (100.0 - this->volume) / 100.0; // convert between logarithmic volume and linear volume
    this->mutex.unlock();
}

bool GameInstance::is_mono_forced() noexcept MAKE_GETTER(this->force_mono)
void GameInstance::set_mono_forced(bool mono) noexcept MAKE_SETTER(this->force_mono = mono)

bool GameInstance::is_paused_manually() noexcept MAKE_GETTER(this->manual_paused)
void GameInstance::set_paused_manually(bool paused) noexcept MAKE_SETTER(this->manual_paused = paused)

GameInstance::PixelBufferMode GameInstance::get_pixel_buffering_mode() noexcept MAKE_GETTER(this->pixel_buffer_mode)
void GameInstance::set_pixel_buffering_mode(PixelBufferMode mode) noexcept MAKE_SETTER(this->pixel_buffer_mode = mode)

void GameInstance::set_rtc_mode(GB_rtc_mode_t mode) noexcept MAKE_SETTER(GB_set_rtc_mode(&this->gameboy, mode))

void GameInstance::unpause_sdl_audio() noexcept {
    if(this->sdl_audio_device.has_value()) {
        SDL_PauseAudioDevice(*this->sdl_audio_device, 0);
    }
}

void GameInstance::reset_audio() noexcept {
    if(this->sdl_audio_device.has_value()) {
        SDL_PauseAudioDevice(*this->sdl_audio_device, 1);
        SDL_ClearQueuedAudio(*this->sdl_audio_device);
    }
    this->sample_buffer.clear();
}

void GameInstance::close_sdl_audio_device() noexcept {
    if(this->sdl_audio_device.has_value()) {
        SDL_CloseAudioDevice(*this->sdl_audio_device);
        this->sdl_audio_device = std::nullopt;
        this->current_sample_rate = 0;
    }
}

void GameInstance::set_current_sample_rate(std::uint32_t new_sample_rate) noexcept {
    this->current_sample_rate = new_sample_rate;
}

void GameInstance::set_turbo_mode(bool turbo, float ratio) noexcept {
    this->mutex.lock();
    GB_set_turbo_mode(&this->gameboy, turbo, true);
    this->turbo_mode_enabled = turbo;
    this->turbo_mode_speed_ratio = ratio; // SameBoy runs the game uncapped if turbo mode is enabled, so we need to make our own frame rate limiter
    this->mutex.unlock();
}

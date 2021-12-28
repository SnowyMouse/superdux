#include "game_instance.hpp"
#include "built_in_boot_rom.h"

#include <chrono>
#include <cstring>
#include <thread>
#include <cassert>

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

void GameInstance::load_boot_rom(GB_gameboy_t *gb, GB_boot_rom_t type) noexcept {
    auto *instance = reinterpret_cast<GameInstance *>(GB_get_user_data(gb));

    bool fast_override = instance->fast_boot_rom;

    // If a boot rom is set, load that... unless it fails
    if(!fast_override && instance->boot_rom_path.has_value()) {
        if(GB_load_boot_rom(gb, instance->boot_rom_path->string().c_str()) == 0) {
            return;
        }
        else {
            std::fprintf(stderr, "Boot ROM loading failed - using internal boot ROM instead\n");
        }
    }

    // Otherwise, load a built-in one
    const std::uint8_t *boot_rom;
    std::size_t boot_rom_size;

    switch(type) {
        case GB_BOOT_ROM_DMG_0:
        case GB_BOOT_ROM_DMG:
            boot_rom = built_in_dmg_boot_room(&boot_rom_size);
            break;
        case GB_BOOT_ROM_SGB2:
            boot_rom = built_in_sgb_boot_room(&boot_rom_size);
            break;
        case GB_BOOT_ROM_SGB:
            boot_rom = built_in_sgb2_boot_room(&boot_rom_size);
            break;
        case GB_BOOT_ROM_AGB:
            boot_rom = built_in_agb_boot_room(&boot_rom_size);
            break;
        case GB_BOOT_ROM_CGB_0:
        case GB_BOOT_ROM_CGB:
            if(fast_override) {
                boot_rom = built_in_fast_cgb_boot_room(&boot_rom_size);
            }
            else {
                boot_rom = built_in_cgb_boot_room(&boot_rom_size);
            }
            break;
        default:
            std::fprintf(stderr, "Unable to find a suitable boot ROM for GB_boot_rom_t type %i\n", type);
            return;
    }

    GB_load_boot_rom_from_buffer(gb, boot_rom, boot_rom_size);
}

static std::uint32_t rgb_encode(GB_gameboy_t *, uint8_t r, uint8_t g, uint8_t b) {
    return 0xFF000000 | (r << 16) | (g << 8) | (b << 0);
}

void GameInstance::on_vblank(GB_gameboy_s *gameboy) noexcept {
    auto *instance = resolve_instance(gameboy);

    // Lock this
    instance->vblank_mutex.lock();
    
    // Increment the work buffer index by 1, wrapping around to 0 when we've hit the number of buffers
    instance->previous_buffer_second = instance->previous_buffer;
    instance->previous_buffer = instance->work_buffer;
    instance->work_buffer = (instance->work_buffer + 1) % (sizeof(instance->pixel_buffer) / sizeof(instance->pixel_buffer[0]));
    instance->assign_work_buffer();

    // Handle rapid fire buttons
    instance->rapid_button_frames = (instance->rapid_button_frames + 1) % instance->rapid_button_switch_frames;
    if(instance->rapid_button_frames == 0) { // we hit the nth frame, so switch
        instance->rapid_button_state = !instance->rapid_button_state;
        for(auto i : instance->rapid_buttons) {
            GB_set_key_state(&instance->gameboy, i, instance->rapid_button_state);
        }
    }
    
    // Set this since we hit vblank
    instance->vblank_hit = true;

    instance->should_rewind = instance->rewinding;
    instance->vblank_mutex.unlock();
}

GameInstance::GameInstance(GB_model_t model, GB_border_mode_t border) {
    GB_init(&this->gameboy, model);
    GB_set_border_mode(&this->gameboy, border);
    GB_set_user_data(&this->gameboy, this);
    GB_set_boot_rom_load_callback(&this->gameboy, GameInstance::load_boot_rom);
    GB_set_rgb_encode_callback(&this->gameboy, rgb_encode);
    GB_set_vblank_callback(&this->gameboy, GameInstance::on_vblank);
    GB_set_log_callback(&this->gameboy, GameInstance::on_log);
    GB_set_input_callback(&this->gameboy, GameInstance::on_input_requested);
    GB_apu_set_sample_callback(&this->gameboy, GameInstance::on_sample);
    GB_set_rumble_mode(&this->gameboy, GB_rumble_mode_t::GB_RUMBLE_CARTRIDGE_ONLY);
    GB_set_rumble_callback(&this->gameboy, GameInstance::on_rumble);
    
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

    // Check if we're breaking and tracing?
    bool bnt = false;
    if(instance->current_break_and_trace_remaining > 0) {
        bnt = (--instance->current_break_and_trace_remaining) > 0;

        bool hit_breakpoint = false;
        auto pc = get_gb_register(&instance->gameboy, sm83_register_t::SM83_REG_PC);
        for(auto i : instance->get_breakpoints_without_mutex()) {
            if(i == pc) {
                hit_breakpoint = true;
                break;
            }
        }

        // If we hit a breakpoint in the middle of breaking and tracing, end prematurely
        if(hit_breakpoint) {
            bnt = false;
            instance->current_break_and_trace_remaining = 0;
        }

        // If we've hit the end, do we continue or stop now?
        else if(!bnt && !instance->current_break_and_trace_break_when_done) {
            return malloc_string("continue");
        }
    }

    // If that didn't satisfy it, maybe we have something set here?
    if(!bnt) {
        for(auto b = instance->break_and_trace_breakpoints.begin(); b != instance->break_and_trace_breakpoints.end(); b++) {
            auto pc = get_gb_register(&instance->gameboy, sm83_register_t::SM83_REG_PC);

            auto &[bp_address, break_count, step_over, break_when_done] = *b;
            if(pc == bp_address) {
                instance->current_break_and_trace_remaining = break_count;
                instance->current_break_and_trace_step_over = step_over;
                instance->current_break_and_trace_break_when_done = break_when_done;
                instance->break_and_trace_result.emplace_back().reserve(instance->current_break_and_trace_remaining + 1);
                bnt = true;

                // Remove the breakpoint
                char command[512];
                std::snprintf(command, sizeof(command), "delete $%04x", pc);
                instance->execute_command_without_mutex(malloc_string(command));
                instance->break_and_trace_breakpoints.erase(b);

                break;
            }
        }
    }

    // If we are, continue after we record the current state
    if(bnt) {
        auto &b = instance->break_and_trace_result[instance->break_and_trace_result.size() - 1].emplace_back();
        b.a = get_gb_register(&instance->gameboy, sm83_register_t::SM83_REG_A);
        b.b = get_gb_register(&instance->gameboy, sm83_register_t::SM83_REG_B);
        b.c = get_gb_register(&instance->gameboy, sm83_register_t::SM83_REG_C);
        b.d = get_gb_register(&instance->gameboy, sm83_register_t::SM83_REG_D);
        b.e = get_gb_register(&instance->gameboy, sm83_register_t::SM83_REG_E);
        b.f = get_gb_register(&instance->gameboy, sm83_register_t::SM83_REG_F);
        b.h = get_gb_register(&instance->gameboy, sm83_register_t::SM83_REG_H);
        b.l = get_gb_register(&instance->gameboy, sm83_register_t::SM83_REG_L);
        b.sp = get_gb_register(&instance->gameboy, sm83_register_t::SM83_REG_SP);
        b.pc = get_gb_register(&instance->gameboy, sm83_register_t::SM83_REG_PC);
        b.carry = b.f & GB_CARRY_FLAG;
        b.half_carry = b.f & GB_HALF_CARRY_FLAG;
        b.subtract = b.f & GB_SUBTRACT_FLAG;
        b.zero = b.f & GB_ZERO_FLAG;
        b.step_over = instance->current_break_and_trace_step_over;

        b.disassembly = instance->disassemble_without_mutex(b.pc, 1);

        if(instance->current_break_and_trace_step_over) {
            return malloc_string("next");
        }
        else {
            return malloc_string("step");
        }
    }
    
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

float GameInstance::get_frame_rate() noexcept {
    this->vblank_mutex.lock();
    auto r = this->frame_rate;
    this->vblank_mutex.unlock();
    return r;
}

void GameInstance::reset() noexcept {
    this->mutex.lock();
    this->reset_to_original_model();
    this->reset_audio();
    this->mutex.unlock();
}

void GameInstance::set_model(GB_model_t model, GB_border_mode_t border) {
    this->mutex.lock();
    this->original_model = std::nullopt; // we're changing models so it doesn't matter
    GB_switch_model_and_reset(&this->gameboy, model);
    GB_set_border_mode(&this->gameboy, border);
    this->reset_audio();
    this->update_pixel_buffer_size();
    this->mutex.unlock();
}

void GameInstance::set_border_mode(GB_border_mode_t border) noexcept {
    this->mutex.lock();
    GB_set_border_mode(&this->gameboy, border);
    this->update_pixel_buffer_size();
    this->mutex.unlock();
}

void GameInstance::start_game_loop(GameInstance *instance) noexcept {
    if(instance->loop_running) {
        std::terminate();
    }
    
    instance->loop_running = true;

    bool rewind_paused = false;
    
    while(true) {
        // Burn the thread until we have the mutex (bad practice, but it minimizes latency)
        while(!instance->mutex.try_lock()) {}

        // If we aren't holding the rewinding button, cancel the rewind pause
        instance->rewind_paused = instance->rewind_paused && instance->rewinding;
        
        // Run some cycles on the gameboy
        if(!instance->manual_paused && !instance->rewind_paused && !instance->pause_zero_speed) {
            if(instance->should_rewind) {
                GB_rewind_pop(&instance->gameboy);
                if(!GB_rewind_pop(&instance->gameboy)) { // if we can't rewind any further, pause until the user lets go of the rewind button
                    instance->rewind_paused = true;
                }
                instance->should_rewind = false;
            }

            // Skip intro if needed
            instance->skip_sgb_intro_if_needed();

            // Do stuff now
            GB_set_key_mask(&instance->gameboy, static_cast<GB_key_mask_t>(static_cast<std::uint16_t>(instance->button_bitfield)));
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

                // If we need to wait for a frame, do it
                if(instance->turbo_mode_enabled) {
                    // Burn the thread until we get the next frame (I never said I was a good coder)
                    auto next_expected_frame = instance->next_expected_frame;

                    // Unlock the mutex so other things can access this in the meantime without waiting
                    instance->mutex.unlock();
                    while(clock::now() < instance->next_expected_frame) {}
                    instance->mutex.lock();

                    // Update when the next frame should happen
                    instance->next_expected_frame = clock::now() + std::chrono::microseconds(static_cast<unsigned long>(1000000.0 / GB_get_usual_frame_rate(&instance->gameboy) / instance->turbo_mode_speed_ratio));
                }
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
    // Get the backtrace string
    this->mutex.lock();

    std::vector<std::pair<std::string, std::uint16_t>> backtrace;
    auto *cmd = malloc_string("backtrace");
    auto backtrace_str = execute_command_without_mutex(cmd);
    std::size_t bt_count = get_gb_backtrace_size(&this->gameboy);
    backtrace.resize(bt_count);
    for(std::size_t b = 1; b < bt_count; b++) {
        backtrace[bt_count - b].second = get_gb_backtrace_address(&this->gameboy, b);
    }
    if(bt_count > 0) {
        backtrace[0].second = get_gb_register(&this->gameboy, sm83_register_t::SM83_REG_PC);
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

std::vector<std::uint16_t> GameInstance::get_breakpoints_without_mutex() {
    std::vector<std::uint16_t> breakpoints;
    std::size_t bp_count = get_gb_breakpoint_size(&this->gameboy);
    breakpoints.resize(bp_count);
    for(std::size_t b = 0; b < bp_count; b++) {
        breakpoints[b] = get_gb_breakpoint_address(&this->gameboy, b);
    }
    return breakpoints;
}

std::vector<std::uint16_t> GameInstance::get_breakpoints() MAKE_GETTER(this->get_breakpoints_without_mutex())

bool GameInstance::read_pixel_buffer(std::uint32_t *destination, std::size_t destination_length) noexcept {
    this->vblank_mutex.lock();

    auto required_length = this->pixel_buffer[0].size();
    bool success = required_length == destination_length;

    if(success) {
        std::size_t bytes = required_length * sizeof(*this->pixel_buffer[0].data());
        switch(this->pixel_buffer_mode) {
            case PixelBufferMode::PixelBufferSingle:
                this->mutex.lock();
                std::memcpy(destination, this->pixel_buffer[this->work_buffer].data(), bytes);
                this->mutex.unlock();
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
    this->vblank_mutex.unlock();
    
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

void GameInstance::set_button_state(GB_key_t button, bool pressed) {
    if(pressed) {
        this->button_bitfield |= (1 << button);
    }
    else {
        this->button_bitfield &= ~(1 << button);
    }
}

void GameInstance::clear_all_button_states() MAKE_SETTER(this->clear_all_button_states_no_mutex());

void GameInstance::clear_all_button_states_no_mutex() noexcept {
    this->button_bitfield = 0;
    GB_set_key_mask(&this->gameboy, static_cast<GB_key_mask_t>(0));
}

void GameInstance::get_dimensions(std::uint32_t &width, std::uint32_t &height) noexcept {
    this->vblank_mutex.lock();
    height = this->pb_height;
    width = this->pb_width;
    this->vblank_mutex.unlock();
}

void GameInstance::update_pixel_buffer_size() {
    this->vblank_mutex.lock();
    this->pb_width = GB_get_screen_width(&this->gameboy);
    this->pb_height = GB_get_screen_height(&this->gameboy);

    for(auto &i : this->pixel_buffer) {
        i = std::vector<std::uint32_t>(this->get_pixel_buffer_size_without_mutex(), 0xFF000000);
        this->work_buffer = 0;
        this->previous_buffer = 0;
        this->previous_buffer_second = 0;
        this->assign_work_buffer();
    }
    this->vblank_mutex.unlock();
}

std::vector<std::int16_t> GameInstance::get_sample_buffer() noexcept {
    this->vblank_mutex.lock();
    auto ret = std::move(this->sample_buffer);
    this->sample_buffer.clear();
    this->vblank_mutex.unlock();
    return ret;
}

void GameInstance::transfer_sample_buffer(std::vector<std::int16_t> &destination) noexcept {
    this->vblank_mutex.lock();
    destination.insert(destination.end(), this->sample_buffer.begin(), this->sample_buffer.end());
    this->sample_buffer.clear();
    this->vblank_mutex.unlock();
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

void GameInstance::set_speed_multiplier(double speed_multiplier) noexcept {
    this->mutex.lock();
    if(speed_multiplier < 0.001) {
        this->pause_zero_speed = true; // prevents a floating point exception that occurs if 0 speed
        speed_multiplier = 0.001;
    }
    else {
        this->pause_zero_speed = false;
    }
    GB_set_clock_multiplier(&this->gameboy, speed_multiplier);
    this->mutex.unlock();
}
bool GameInstance::is_audio_enabled() noexcept MAKE_GETTER(this->audio_enabled)

std::size_t GameInstance::get_pixel_buffer_size() noexcept {
    this->vblank_mutex.lock();
    auto r = this->pb_height*this->pb_width;
    this->vblank_mutex.unlock();
    return r;
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
    if(this->current_break_and_trace_remaining == 0) {
        GB_debugger_break(&this->gameboy);
    }
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

std::uint16_t GameInstance::get_register_value(sm83_register_t reg) noexcept MAKE_GETTER(get_gb_register(&this->gameboy, reg))
void GameInstance::set_register_value(sm83_register_t reg, std::uint16_t value) noexcept MAKE_SETTER(set_gb_register(&this->gameboy, reg, value))

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
    this->begin_loading_rom();
    
    // Load the ROM
    int result = GB_load_rom(&this->gameboy, rom_path.string().c_str());
    
    // If successful, load the battery and symbol files
    if(result == 0) {
        this->load_save_and_symbols(sram_path, symbol_path);
    }
    
    this->mutex.unlock();
    return result;
}

void GameInstance::begin_loading_rom() noexcept {
    // Lock this guy
    this->mutex.lock();

    // Reset this
    this->rumble = 0.0;
    this->rewinding = false;
    this->clear_all_button_states_no_mutex();

    // Reset break and trace
    this->current_break_and_trace_remaining = 0;
    this->break_and_trace_result.clear();
    this->break_and_trace_breakpoints.clear();

    // Pause SDL audio
    this->reset_audio();

    // Reset the gameboy
    this->reset_to_original_model();

    // Reset frame times
    this->frame_time_index = 0;
    this->last_frame_time = clock::now();
}

void GameInstance::load_rom(const std::byte *rom_data, const std::size_t rom_size, const std::optional<std::filesystem::path> &sram_path, const std::optional<std::filesystem::path> &symbol_path) noexcept {
    this->begin_loading_rom();

    // Load the ROM
    GB_load_rom_from_buffer(&this->gameboy, reinterpret_cast<const std::uint8_t *>(rom_data), rom_size);
    this->load_save_and_symbols(sram_path, symbol_path);

    this->mutex.unlock();
}

int GameInstance::load_isx(const std::filesystem::path &isx_path, const std::optional<std::filesystem::path> &sram_path, const std::optional<std::filesystem::path> &symbol_path) noexcept {
    this->begin_loading_rom();
    
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

std::string GameInstance::disassemble_without_mutex(std::uint16_t address, std::uint8_t count) {
    // Retain the logs since that's where the disassembly goes
    this->retain_logs(true);

    // Execute
    GB_cpu_disassemble(&this->gameboy, address, count);

    // Disable log retention, unlock mutex, and get what we got
    this->retain_logs(false);
    return this->clear_log_buffer();
}

std::string GameInstance::disassemble_address(std::uint16_t address, std::uint8_t count) MAKE_GETTER(disassemble_without_mutex(address, count))

std::size_t GameInstance::get_pixel_buffer_size_without_mutex() noexcept {
    return GB_get_screen_width(&this->gameboy) * GB_get_screen_height(&this->gameboy);
}
bool GameInstance::set_up_sdl_audio(std::uint32_t sample_rate, std::uint32_t buffer_size) noexcept {
    this->mutex.lock();
    SDL_AudioSpec request = {}, result = {}, preferred = {};
    request.format = AUDIO_S16SYS;
    request.channels = 2;
    request.userdata = this;

    SDL_GetAudioDeviceSpec(0, 0, &preferred);
    request.freq = preferred.freq;
    request.samples = preferred.samples;

    int flags = 0;

    if(sample_rate != 0) {
        request.freq = sample_rate;
        flags |= SDL_AUDIO_ALLOW_FREQUENCY_CHANGE;
    }

    if(buffer_size != 0) {
        request.samples = buffer_size;
        flags |= SDL_AUDIO_ALLOW_SAMPLES_CHANGE;
    }

    auto device = SDL_OpenAudioDevice(0, 0, &request, &result, flags);
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

void GameInstance::set_boot_rom_path(const std::optional<std::filesystem::path> &boot_rom_path) MAKE_SETTER(this->boot_rom_path = boot_rom_path)
void GameInstance::set_use_fast_boot_rom(bool fast_boot_rom) noexcept MAKE_SETTER(this->fast_boot_rom = fast_boot_rom)

void GameInstance::break_and_trace_at(std::uint16_t address, std::size_t n, bool step_over, bool break_when_done) {
    // Remove the breakpoint
    this->remove_breakpoint(address);

    // Re-add it now
    this->mutex.lock();
    this->break_and_trace_breakpoints.emplace_back(address, n, step_over, break_when_done);

    char command[512];
    std::snprintf(command, sizeof(command), "breakpoint $%04x", address);
    this->execute_command_without_mutex(malloc_string(command));

    this->mutex.unlock();
}

void GameInstance::break_at(std::uint16_t address) noexcept {
    this->mutex.lock();

    char command[512];
    std::snprintf(command, sizeof(command), "breakpoint $%04x", address);
    this->execute_command_without_mutex(malloc_string(command));

    this->mutex.unlock();
}

std::optional<std::vector<GameInstance::BreakAndTraceResult>> GameInstance::pop_break_and_trace_results() {
    this->mutex.lock();
    if(this->break_and_trace_results_ready_no_mutex()) {
        auto top = this->break_and_trace_result[0];
        this->break_and_trace_result.erase(this->break_and_trace_result.begin());
        this->mutex.unlock();
        return top;
    }
    this->mutex.unlock();
    return std::nullopt;
}

void GameInstance::remove_breakpoint(std::uint16_t breakpoint) noexcept {
    this->mutex.lock();
    char cmd[256];
    std::snprintf(cmd, sizeof(cmd), "delete $%04x", breakpoint);
    this->execute_command_without_mutex(malloc_string(cmd));

    // Remove all matching breakpoints
    while(true) {
        bool did_it = true;
        for(auto b = this->break_and_trace_breakpoints.begin(); b != this->break_and_trace_breakpoints.end(); b++) {
            if(std::get<0>(*b) == breakpoint) {
                this->break_and_trace_breakpoints.erase(b);
                did_it = false;
                break;
            }
        }
        if(did_it) {
            break;
        }
    }

    this->mutex.unlock();
}

void GameInstance::set_highpass_filter_mode(GB_highpass_mode_t mode) noexcept MAKE_SETTER(GB_set_highpass_filter_mode(&this->gameboy, mode))

void GameInstance::remove_all_breakpoints() noexcept {
    this->mutex.lock();
    this->execute_command_without_mutex(malloc_string("delete"));
    this->break_and_trace_breakpoints.clear();
    this->mutex.unlock();
}

void GameInstance::set_color_correction_mode(GB_color_correction_mode_t mode) noexcept MAKE_SETTER(GB_set_color_correction_mode(&this->gameboy, mode))


bool GameInstance::create_save_state(const std::filesystem::path &path) noexcept MAKE_GETTER(GB_save_state(&this->gameboy, path.string().c_str()) == 0)

std::vector<std::uint8_t> GameInstance::create_save_state() {
    this->mutex.lock();
    std::vector<std::uint8_t> data(GB_get_save_state_size(&this->gameboy));
    GB_save_state_to_buffer(&this->gameboy, data.data());
    this->mutex.unlock();
    return data;
}

bool GameInstance::load_save_state(const std::filesystem::path &path) noexcept {
    // Load the state maybe
    this->mutex.lock();
    auto model_before = GB_get_model(&this->gameboy);
    auto success = GB_load_state(&this->gameboy, path.string().c_str()) == 0;
    auto model_after = GB_get_model(&this->gameboy);

    // If we changed models due to the save state change, store what we had before
    if(model_before != model_after && !original_model.has_value()) {
        original_model = model_before;
    }

    // Done
    this->mutex.unlock();
    return success;
}

bool GameInstance::load_save_state(const std::vector<std::uint8_t> &state) noexcept MAKE_GETTER(GB_load_state_from_buffer(&this->gameboy, state.data(), state.size()) == 0)

void GameInstance::set_rapid_button_state(GB_key_t button, bool pressed) {
    this->mutex.lock();
    std::size_t rb_len = this->rapid_buttons.size();
    bool found = false;
    for(std::size_t q = 0; q < rb_len; q++) {
        if(this->rapid_buttons[q] == button) {
            found = true;
            if(!pressed) {
                this->rapid_buttons.erase(this->rapid_buttons.begin() + q);
            }
            break;
        }
    }
    if(!found && pressed) {
        this->rapid_buttons.emplace_back(button);
    }
    GB_set_key_state(&this->gameboy, button, pressed ? this->rapid_button_state : false); // unset if we're releasing the button. otherwise set to current rapid button state
    this->mutex.unlock();
}

void GameInstance::on_rumble(GB_gameboy_s *gb, double rumble) noexcept {
    reinterpret_cast<GameInstance *>(GB_get_user_data(gb))->rumble = rumble;
}

void GameInstance::set_rumble_mode(GB_rumble_mode_t mode) noexcept MAKE_SETTER(GB_set_rumble_mode(&this->gameboy, mode))

void GameInstance::set_rewind(bool rewinding) noexcept MAKE_SETTER(this->rewinding = rewinding)

void GameInstance::set_rewind_length(double seconds) noexcept MAKE_SETTER(GB_set_rewind_length(&this->gameboy, seconds))

void color_block(GB_gameboy_s *gb, std::uint32_t *block, const std::uint8_t *tile_data, GB_palette_type_t palette_type, unsigned int palette_index, unsigned int stride = GameInstance::GB_TILESET_TILE_LENGTH) {
    // Get palettes
    auto *new_palette = get_gb_palette(gb, palette_type, palette_index);

    // Go through each pixel in the tile and color it
    for(std::size_t ty = 0; ty < GameInstance::GB_TILESET_TILE_LENGTH; ty++) {
        for(std::size_t tx = 0, access_shift = GameInstance::GB_TILESET_TILE_LENGTH - 1; tx < GameInstance::GB_TILESET_TILE_LENGTH; tx++, access_shift--) {
            auto &pixel = block[tx + ty * stride];
            auto *tile_pixel = tile_data + ty * 2;

            // Bit shift left-to-right - https://gbdev.io/pandocs/Tile_Data.html
            // First byte is lower bit. Second byte is upper bit
            std::uint8_t color_index_low = (tile_pixel[0] >> access_shift) & 0b1;
            std::uint8_t color_index_high = (tile_pixel[1] >> access_shift) & 0b1;

            std::uint8_t color_index = color_index_low | (color_index_high << 1);

            pixel = new_palette[color_index];
        }
    }
}

void GameInstance::draw_tileset(std::uint32_t *destination, GB_palette_type_t palette_type, std::uint8_t index) noexcept {
    // Yes I know that SameBoy has GB_draw_tileset and it's quite good, but it does not implement
    // automatic palette detection (GB_PALETTE_AUTO does the same thing as GB_PALETTE_NONE).

    this->mutex.lock();
    auto is_cgb = GB_is_cgb(&this->gameboy);

    // Get the tileset info here
    std::size_t size;
    const auto *tileset = reinterpret_cast<std::uint8_t *>(GB_get_direct_access(&this->gameboy, GB_direct_access_t::GB_DIRECT_ACCESS_VRAM, &size, nullptr));

    // Make sure the sizes are correct
    if(is_cgb) {
        assert(size > 0x2000);
    }
    else {
        assert(size >= 0x2000);
    }

    const std::uint8_t *tileset_banks[2] = {tileset, tileset + 0x2000};

    // Get the tilset info if we are doing auto
    static constexpr const auto tile_count = sizeof(TilesetInfo::tiles) / sizeof(TilesetInfo::tiles[0]);

    #define DEFINE_X_Y_BLOCK(i) \
    std::uint8_t x = i % (GB_TILESET_BLOCK_WIDTH); \
    std::uint8_t y = i / (GB_TILESET_BLOCK_WIDTH); \
    auto *block = destination + x * GB_TILESET_TILE_LENGTH + y * GB_TILESET_TILE_LENGTH * GB_TILESET_BLOCK_WIDTH * GB_TILESET_TILE_LENGTH; \
    auto bank = x >= GB_TILESET_PAGE_BLOCK_WIDTH; \

    // Get the tileset info
    TilesetInfo ti = this->get_tileset_info_without_mutex();

    // In case we change types.
    static_assert(sizeof(ti.tiles) / sizeof(ti.tiles[0]) == tile_count);

    // DMG only has one bank
    bool ignore_second_tileset_bank = !is_cgb;

    // Do the thing
    if(palette_type == GB_palette_type_t::GB_PALETTE_AUTO) {
        for(auto i = 0; i < tile_count; i++) {
            // Get the block data
            const auto &info = ti.tiles[i];
            DEFINE_X_Y_BLOCK(i);

            if(bank != 0 && ignore_second_tileset_bank) {
                continue;
            }

            // Go through each pixel in the tile and color it
            color_block(&this->gameboy, block, tileset_banks[info.tile_bank] + info.tile_index * 0x10, info.accessed_palette_type, info.accessed_tile_palette_index, GB_TILESET_WIDTH);
        }
    }
    else {
        for(auto i = 0; i < tile_count; i++) {
            // Get the block data
            const auto &info = ti.tiles[i];
            DEFINE_X_Y_BLOCK(i);

            if(bank != 0 && ignore_second_tileset_bank) {
                continue;
            }

            // Go through each pixel in the tile and color it
            color_block(&this->gameboy, block, tileset_banks[info.tile_bank] + info.tile_index * 0x10, palette_type, index, GB_TILESET_WIDTH);
        }
    }

    #undef DEFINE_X_Y_BLOCK

    this->mutex.unlock();
}

void GameInstance::draw_tilemap(std::uint32_t *destination, GB_map_type_t map_type, GB_tileset_type_t tileset_type) noexcept MAKE_SETTER(GB_draw_tilemap(&this->gameboy, destination, GB_palette_type_t::GB_PALETTE_AUTO, 0, map_type, tileset_type))

std::uint8_t GameInstance::read_memory(std::uint16_t address) noexcept MAKE_GETTER(GB_safe_read_memory(&this->gameboy, address))

const uint32_t *GameInstance::get_palette(GB_palette_type_t palette_type, unsigned char palette_index) noexcept MAKE_GETTER(get_gb_palette(&this->gameboy, palette_type, palette_index))

GameInstance::TilesetInfo GameInstance::get_tileset_info() noexcept MAKE_GETTER(this->get_tileset_info_without_mutex())

GameInstance::TilesetInfo GameInstance::get_tileset_info_without_mutex() noexcept {
    // Instantiate this
    GameInstance::TilesetInfo tileset_info;

    // Are we in GBC mode?
    bool cgb_mode = GB_is_cgb_in_cgb_mode(&this->gameboy);

    // Get the OAM data
    auto lcdc = GB_safe_read_memory(&this->gameboy, 0xFF40);
    bool double_sprite_height = (lcdc & 0b100) != 0;
    auto oam = this->get_object_attribute_info_without_mutex();

    // Background tile data
    std::size_t size = 0;
    const auto *tile_9800 = reinterpret_cast<const std::uint8_t *>(GB_get_direct_access(&this->gameboy, GB_DIRECT_ACCESS_VRAM, &size, nullptr)) + 0x1800;
    const auto *tile_9C00 = tile_9800 + 0x400;
    assert(size >= 0x1C00);

    bool sprites_enabled = (lcdc & 0b10);
    bool bg_window_enabled = cgb_mode || (lcdc & 0b1);
    bool window_enabled = (lcdc & 0b100000) && bg_window_enabled;
    auto window_x = GB_safe_read_memory(&this->gameboy, 0xFF4B), window_y = GB_safe_read_memory(&this->gameboy, 0xFF4A);

    const auto *background = (lcdc & 0b1000) ? tile_9C00 : tile_9800;
    const auto *background_attributes = background + 0x2000;

    const auto *window = (lcdc & 0b1000000) ? tile_9C00 : tile_9800;
    const auto *window_attributes = window + 0x2000;
    auto window_x_end = (32 - window_x / 8);
    auto window_y_end = (32 - window_y / 8);
    auto window_visible = window_x <= 166 && window_y <= 143 && window_enabled;

    bool background_window_8800 = !(lcdc & 0b10000);

    // First, set all of these base attributes
    for(auto y = 0; y < GB_TILESET_BLOCK_HEIGHT; y++) {
        for(auto x = 0; x < GB_TILESET_BLOCK_WIDTH; x++) {
            std::uint16_t tile_number = 0;

            std::uint16_t tileset_number = 0;
            std::uint16_t virtual_x = x;

            if(x >= GB_TILESET_PAGE_BLOCK_WIDTH) {
                tileset_number = 1;
                virtual_x -= GB_TILESET_PAGE_BLOCK_WIDTH;
            }

            // Get the tile number
            tile_number = virtual_x + (y * GB_TILESET_PAGE_BLOCK_WIDTH);

            // Set these
            auto &block_info = tileset_info.tiles[x + y * GB_TILESET_BLOCK_WIDTH];
            block_info.tile_index = tile_number;
            block_info.tile_bank = tileset_number;
            block_info.tile_address = 0x8000 + tile_number * 0x10;

            // Some defaults
            block_info.accessed_palette_type = GB_palette_type_t::GB_PALETTE_NONE;
            block_info.accessed_type = TilesetInfoTileType::TILESET_INFO_NONE;
        }
    }

    // Next, check background and window
    if(bg_window_enabled) {
        #define READ_BG_WINDOW(index, tile_data, tile_data_attributes, access_type) { \
            std::uint8_t accessed_tile_index = tile_data[index]; \
            std::uint16_t tile = accessed_tile_index; \
            if(background_window_8800) { \
                if(tile < 128) { \
                    tile += 0x100; \
                } \
            } \
            std::uint8_t bw_tileset_number, tile_palette; \
            if(cgb_mode) { \
                std::uint8_t tile_attributes = tile_data_attributes[index]; \
                bw_tileset_number = (tile_attributes & 0b1000) >> 3; \
                tile_palette = tile_attributes & 0b111; \
            } \
            else { \
                bw_tileset_number = 0; \
                tile_palette = 0; \
            } \
         \
            auto x = tile % GB_TILESET_PAGE_BLOCK_WIDTH + (bw_tileset_number == 1 ? GB_TILESET_PAGE_BLOCK_WIDTH : 0); \
            auto y = tile / GB_TILESET_PAGE_BLOCK_WIDTH; \
            auto &block_info = tileset_info.tiles[x + y * GB_TILESET_BLOCK_WIDTH]; \
            block_info.accessed_type = access_type; \
            block_info.accessed_tile_index = tile; \
            block_info.accessed_tile_palette_index = tile_palette; \
            block_info.accessed_palette_type = GB_palette_type_t::GB_PALETTE_BACKGROUND; \
        }

        // First, background. Always 32x32 blocks
        for(auto block = 0; block < 32 * 32; block++) {
            READ_BG_WINDOW(block, background, background_attributes, TilesetInfoTileType::TILESET_INFO_BACKGROUND)
        }

        // Next, window
        if(window_visible) {
            for(auto wy = 0; wy < window_y_end; wy++) {
                for(auto wx = 0; wx < window_x_end; wx++) {
                    auto block = (wx + wy * 32);
                    READ_BG_WINDOW(block, window, window_attributes, TilesetInfoTileType::TILESET_INFO_WINDOW)
                }
            }
        }
    }

    // Lastly check sprites
    if(sprites_enabled) {
        for(auto i = 0; i < sizeof(oam.objects) / sizeof(oam.objects[0]); i++) {
            auto &object = oam.objects[i];

            if(!object.on_screen) {
                continue;
            }

            auto tile = object.tile;
            auto x = tile % GB_TILESET_PAGE_BLOCK_WIDTH + (object.tileset_bank == 1 ? GB_TILESET_PAGE_BLOCK_WIDTH : 0);
            auto y = tile / GB_TILESET_PAGE_BLOCK_WIDTH;
            auto &block_info = tileset_info.tiles[x + y * GB_TILESET_BLOCK_WIDTH];

            block_info.accessed_tile_index = tile;
            block_info.accessed_type = TilesetInfoTileType::TILESET_INFO_OAM;
            block_info.accessed_tile_palette_index = object.palette;
            block_info.accessed_user_index = i;
            block_info.accessed_palette_type = GB_palette_type_t::GB_PALETTE_OAM;

            // If double sprite height, include the next tile as well
            if(double_sprite_height) {
                auto &next_block_info = *(&block_info + 1);
                next_block_info.accessed_tile_index = tile + 1;
                next_block_info.accessed_type = TilesetInfoTileType::TILESET_INFO_OAM;
                next_block_info.accessed_tile_palette_index = object.palette;
                next_block_info.accessed_user_index = i;
                next_block_info.accessed_palette_type = GB_palette_type_t::GB_PALETTE_OAM;
            }
        }
    }

    return tileset_info;
}

GameInstance::ObjectAttributeInfo GameInstance::get_object_attribute_info() noexcept MAKE_GETTER(this->get_object_attribute_info_without_mutex())

GameInstance::ObjectAttributeInfo GameInstance::get_object_attribute_info_without_mutex() noexcept {
    std::uint8_t lcdc = GB_safe_read_memory(&this->gameboy, 0xFF40);
    auto cgb_mode = GB_is_cgb_in_cgb_mode(&this->gameboy);
    std::uint8_t sprite_height = (lcdc & 0b100) != 0 ? 16 : 8;
    bool sprites_enabled = (lcdc & 0b10);
    const auto *oam_data = reinterpret_cast<const std::uint8_t *>(GB_get_direct_access(&this->gameboy, GB_DIRECT_ACCESS_OAM, NULL, NULL));

    std::size_t size;
    const auto *tileset = reinterpret_cast<std::uint8_t *>(GB_get_direct_access(&this->gameboy, GB_direct_access_t::GB_DIRECT_ACCESS_VRAM, &size, nullptr));
    assert(cgb_mode ? size > 0x2000 : size >= 0x2000);
    const std::uint8_t *tileset_banks[2] = {tileset, tileset + 0x2000};

    ObjectAttributeInfo oam;
    oam.height = sprite_height;

    for(std::uint8_t i = 0; i < sizeof(oam.objects) / sizeof(oam.objects[0]); i++) {
        auto &object_info = oam.objects[i];
        auto *object = oam_data + i * 4;

        // Tileset bank
        auto flags = object[3];
        object_info.tileset_bank = cgb_mode ? (
                                                  (flags & 0b1000) >> 3 // CGB uses bit#3 (the fourth bit)
                                              ) : 0; // DMG is always tileset 0

        // Tile
        object_info.tile = object[2];

        // Are we offscreen?
        auto oam_x = object[1];
        auto oam_y = object[0];
        object_info.on_screen = sprites_enabled && !(oam_x == 0 || oam_x >= 168 || oam_y + sprite_height <= 16 || oam_y >= 160);
        object_info.obscurred_by_line_limit = false; // todo: figure this out
        object_info.x = oam_x;
        object_info.y = oam_y;

        // Palette number
        object_info.palette = cgb_mode ? (
                                             (flags & 0b111) // CGB uses the first three bits (up to 2^3 or 8 palettes)
                                         ) : ((flags & 0b10000) >> 4); // DMG uses the 5th bit

        // Voltorb flip
        object_info.flip_x = (flags & 0b100000) != 0;
        object_info.flip_y = (flags & 0b1000000) != 0;

        // This flag
        object_info.bg_window_over_obj = (flags& 0b10000000) != 0;

        // Color it
        auto *first_half = object_info.pixel_data;
        auto *second_half = first_half + sizeof(object_info.pixel_data) / sizeof(object_info.pixel_data[0]) / 2;
        auto *tile = tileset_banks[object_info.tileset_bank] + 0x10 * object_info.tile;
        color_block(&this->gameboy, first_half, tile, GB_palette_type_t::GB_PALETTE_OAM, object_info.palette);

        // Color the second half too
        if(sprite_height == 16) {
            color_block(&this->gameboy, second_half, tile + 0x10, GB_palette_type_t::GB_PALETTE_OAM, object_info.palette);
        }

        // Or not!
        else {
            std::memset(second_half, 0, sizeof(object_info.pixel_data) / sizeof(object_info.pixel_data[0]) / 2);
        }

        // Flip X
        if(object_info.flip_x) {
            for(std::size_t y = 0; y < sprite_height; y++) {
                for(std::size_t x = 0, sx = GB_TILESET_TILE_LENGTH - 1; x < GB_TILESET_TILE_LENGTH / 2; x++, sx--) {
                    std::swap(object_info.pixel_data[y * GB_TILESET_TILE_LENGTH + x], object_info.pixel_data[y * GB_TILESET_TILE_LENGTH + sx]);
                }
            }
        }

        // Flip Y
        if(object_info.flip_y) {
            for(std::size_t x = 0; x < GB_TILESET_TILE_LENGTH; x++) {
                for(std::size_t y = 0, sy = sprite_height - 1; y < sprite_height / 2; y++, sy--) {
                    std::swap(object_info.pixel_data[y * GB_TILESET_TILE_LENGTH + x], object_info.pixel_data[sy * GB_TILESET_TILE_LENGTH + x]);
                }
            }
        }
    }

    return oam;
}

void GameInstance::get_raw_palette(GB_palette_type_t type, std::size_t palette, std::uint16_t *output) noexcept {
    this->mutex.lock();

    // No.
    assert(type == GB_palette_type_t::GB_PALETTE_BACKGROUND || type == GB_palette_type_t::GB_PALETTE_OAM);
    assert(palette < 8);

    if(GB_is_cgb(&this->gameboy)) {
        // Get the color data
        std::size_t size;
        auto *direct_access = reinterpret_cast<std::uint8_t *>(GB_get_direct_access(&this->gameboy, type == GB_palette_type_t::GB_PALETTE_BACKGROUND ? GB_direct_access_t::GB_DIRECT_ACCESS_BGP : GB_direct_access_t::GB_DIRECT_ACCESS_OBP, &size, nullptr));
        assert(size >= sizeof(*output) * 4 * 8);
        direct_access += sizeof(*output) * 4 * palette;

        for(std::size_t i = 0; i < 4; i++) {
            output[i] = reinterpret_cast<std::uint16_t *>(direct_access)[i];
        }
    }
    else {
        std::uint8_t p = 0;
        if(type == GB_palette_type_t::GB_PALETTE_BACKGROUND && palette == 0) {
            p = GB_safe_read_memory(&this->gameboy, 0xFF47);
        }
        else if(type == GB_palette_type_t::GB_PALETTE_OAM && palette < 2) {
            p = GB_safe_read_memory(&this->gameboy, 0xFF48 + palette);
        }
        for(unsigned int i = 0; i < 4; i++) {
            output[i] = (p >> (2 * i)) & 0b11;
        }
    }

    this->mutex.unlock();
}

void GameInstance::reset_to_original_model() noexcept {
    // If we have an original model set, use that
    if(this->original_model.has_value()) {
        GB_switch_model_and_reset(&this->gameboy, *this->original_model);
        this->original_model = std::nullopt;
    }

    // Otherwise reset
    else {
        GB_reset(&this->gameboy);
    }
}

bool GameInstance::break_and_trace_results_ready() MAKE_GETTER(this->break_and_trace_results_ready_no_mutex())

bool GameInstance::break_and_trace_results_ready_no_mutex() const noexcept {
    return (this->break_and_trace_result.size() > 1) || (this->break_and_trace_result.size() == 1 && this->current_break_and_trace_remaining == 0);
}

bool GameInstance::is_game_boy_color() noexcept MAKE_GETTER(GB_is_cgb(&this->gameboy))
bool GameInstance::is_game_boy_color_in_cgb_mode() noexcept MAKE_GETTER(GB_is_cgb_in_cgb_mode(&this->gameboy))

void GameInstance::connect_printer() noexcept MAKE_SETTER(GB_connect_printer(&this->gameboy, this->print_image))
void GameInstance::disconnect_serial() noexcept MAKE_SETTER(GB_disconnect_serial(&this->gameboy));

void GameInstance::print_image(GB_gameboy_t *gb, std::uint32_t *image, std::uint8_t height, std::uint8_t top_margin, std::uint8_t bottom_margin, std::uint8_t exposure) {
    std::size_t print_width = 160;
    std::size_t print_height = height + top_margin + bottom_margin;
    auto *instance = reinterpret_cast<GameInstance *>(GB_get_user_data(gb));

    // Add the printed data
    instance->printer_mutex.lock();
    std::vector<std::uint32_t> printer_data(print_width * print_height, 0xFFFFFFFF);
    std::memcpy(printer_data.data() + print_width * top_margin, image, GameInstance::GB_PRINTER_WIDTH * height * sizeof(*image));
    instance->printer_data.emplace_back(std::move(printer_data), print_height);
    instance->printer_mutex.unlock();
}

std::optional<std::vector<std::uint32_t>> GameInstance::pop_printed_image(std::size_t &height) {
    this->printer_mutex.lock();

    if(this->printer_data.empty()) {
        this->printer_mutex.unlock();
        return std::nullopt;
    }
    else {
        auto [p_data, p_height] = std::move(*this->printer_data.begin());
        this->printer_data.erase(this->printer_data.begin());
        this->printer_mutex.unlock();
        height = p_height;
        return p_data;
    }
}

void GameInstance::skip_sgb_intro_if_needed() noexcept {
    // Skip intro animation?
    if(GB_is_sgb(&this->gameboy) && this->fast_boot_rom) {
        skip_sgb_intro_animation(&this->gameboy);
    }
}

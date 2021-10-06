#ifndef GAME_INSTANCE_HPP
#define GAME_INSTANCE_HPP

extern "C" {
#include <Core/gb.h>
}

#include "gb_proxy.h"
#include <string>
#include <vector>
#include <mutex>
#include <atomic>
#include <optional>
#include <filesystem>
#include <chrono>
#include <SDL2/SDL.h>

class GameInstance {
public: // all public functions assume the mutex is not locked
    using clock = std::chrono::steady_clock;
    
    GameInstance(GB_model_t model);
    ~GameInstance();

    enum PixelBufferMode {
        /** Use single buffering. Calls to read_pixel_buffer() will give you the work buffer. This will result in slightly less visual latency, but in most cases, this will result in heavy tearing that will look terrible on any LCD display. */
        PixelBufferSingle,

        /** Use double buffering (default). Calls to read_pixel_buffer() will give you the last completed buffer. */
        PixelBufferDouble,

        /** Use interframe blending. Calls to read_pixel_buffer() will give you an average of the last two completed buffers. */
        PixelBufferDoubleBlend
    };
    
    /**
     * Execute the game loop. This function will not return until end_game_loop is run().
     */
    static void start_game_loop(GameInstance *instance) noexcept;
    
    /**
     * End the game loop.
     */
    void end_game_loop() noexcept;
    
    /**
     * Set whether or not audio is enabled
     * 
     * @param enabled     audio is enabled
     * @param sample_rate set sample rate (optional - only needed if not using SDL audio)
     */
    void set_audio_enabled(bool enabled, std::uint32_t sample_rate = 0) noexcept;

    /**
     * Set up SDL audio
     *
     * @param sample_rate preferred sample rate to use in Hz
     * @param buffer_size preferred buffer size
     *
     * @return success
     */
    bool set_up_sdl_audio(std::uint32_t sample_rate, std::uint32_t buffer_size = 1024) noexcept;

    /**
     * Get the current sample rate on the gameboy instance
     *
     * @return sample rate or 0 if not applicable
     */
    std::uint32_t get_current_sample_rate() noexcept { return this->current_sample_rate; }

    /**
     * Set whether or not audio is enabled
     *
     * @return audio is enabled
     */
    bool is_audio_enabled() noexcept;
    
    /**
     * Load the ROM at the given path
     * 
     * @param  rom_path    ROM path to load
     * @param  sram_path   optional SRAM path to load
     * @param  symbol_path optional symbol path to load
     * @return             0 on success, non-zero on failure
     */
    int load_rom(const std::filesystem::path &rom_path, const std::optional<std::filesystem::path> &sram_path, const std::optional<std::filesystem::path> &symbol_path) noexcept;
    
    /**
     * Load the ISX at the given path
     * 
     * @param  isx_path    ISX path to load
     * @param  sram_path   optional SRAM path to load
     * @param  symbol_path optional symbol path to load
     * @return             0 on success, non-zero on failure
     */
    int load_isx(const std::filesystem::path &rom_path, const std::optional<std::filesystem::path> &sram_path, const std::optional<std::filesystem::path> &symbol_path) noexcept;
    
    /**
     * Get whether or not a ROM is loaded
     * 
     * @return rom is loaded
     */
    bool is_rom_loaded() const noexcept { return this->rom_loaded; }
    
    /**
     * Set the playback speed
     * 
     * @param speed_multiplier new speed multiplier (1.0 = normal speed)
     */
    void set_speed_multiplier(double speed_multiplier) noexcept;
    
    /**
     * Save the SRAM to the given path
     * 
     * @param path path to save
     * @return     0 on success, non-zero on failure
     */
    int save_sram(const std::filesystem::path &path) noexcept;
    
    /**
     * Reset the gameboy. Note that this does not unload the ROMs, save data, etc.
     */
    void reset() noexcept;

    /**
     * Set the volume (clamped between 0 and 100)
     *
     * @param volume to set to
     */
    void set_volume(int volume) noexcept;

    /**
     * Get the volume
     *
     * @return volume
     */
    int get_volume() noexcept;

    /**
     * Get whether mono is forced
     *
     * @return mono is forced
     */
    bool is_mono_forced() noexcept;

    /**
     * Set whether mono is forced
     *
     * @param mono mono is forced
     */
    void set_mono_forced(bool mono) noexcept;
    
    /**
     * Reset the gameboy and switch models.
     * 
     * @param model model to set to
     */
    void set_model(GB_model_t model);
    
    /**
     * Get all currently set breakpoints
     * 
     * @return breakpoint addresses
     */
    std::vector<std::uint16_t> get_breakpoints();
    
    /**
     * Get the current backtrace
     * 
     * @return backtrace addresses and string names
     */
    std::vector<std::pair<std::string, std::uint16_t>> get_backtrace();
    
    /**
     * Get the current value of the given register
     * 
     * @param  reg register to probe
     * @return     register value
     */
    std::uint16_t get_register_value(gbz80_register reg) const noexcept;
    
    /**
     * Set the current value of the given register
     * 
     * @param reg   register to probe
     * @param value value to set it to
     */
    void set_register_value(gbz80_register reg, std::uint16_t value) noexcept;
    
    /**
     * Get the current sample buffer and clear it
     * 
     * @return sample buffer
     */
    std::vector<std::int16_t> get_sample_buffer() noexcept;
    
    /**
     * Empty the sample buffer into the target buffer vector
     * 
     * @param destination vector to empty buffer into
     */
    void transfer_sample_buffer(std::vector<std::int16_t> &destination) noexcept;

    /**
     * Set the pixel buffer mode setting.
     *
     * @param mode mode to set to
     */
    void set_pixel_buffering_mode(PixelBufferMode mode) noexcept;

    /**
     * Get the current pixel buffer mode setting.
     *
     * @return current pixel buffer setting
     */
    PixelBufferMode get_pixel_buffering_mode() noexcept;
    
    /**
     * Set the button state of the Game Boy instance
     * 
     * @param button  button to set
     * @param pressed state to set to
     */
    void set_button_state(GB_key_t button, bool pressed);
    
    /**
     * Get whether or not the instance is paused
     * 
     * @param paused
     */
    bool is_paused() noexcept { return this->is_paused_manually() || this->is_paused_from_breakpoint(); }
    
    /**
     * Set whether or not the instance is paused manually
     * 
     * @param paused paused manually
     */
    void set_paused_manually(bool paused) noexcept;
    
    /**
     * Get whether or not the instance is paused manually
     * 
     * @return paused from breakpoint
     */
    bool is_paused_manually() noexcept;
    
    /**
     * Get whether or not the instance is paused due to a breakpoint
     * 
     * @return paused due to breakpoint
     */
    bool is_paused_from_breakpoint() const noexcept { return this->bp_paused; }
    
    /**
     * Get the current frame rate
     * 
     * @return frame rate
     */
    float get_frame_rate() noexcept;
    
    /**
     * Get the size of the pixel buffer
     * 
     * @return size of pixel buffer in 32-bit pixels
     */
    std::size_t get_pixel_buffer_size() noexcept;
    
    /**
     * Get the height and width
     * 
     * @param width  width
     * @param height height
     */
    void get_dimensions(std::uint32_t &width, std::uint32_t &height) noexcept;
    
    /**
     * Write the pixel buffer to the given address. If the destination size not equal to the pixel buffer size, then nothing will be read.
     * 
     * @param destination        address to write to
     * @param destination_length number of 32-bit integers available at the given address
     * @return                   true if pixel buffer was read, false if the destination size is incorrect
     */
    bool read_pixel_buffer(std::uint32_t *destination, std::size_t destination_length) noexcept;
    
    /**
     * Execute the command on the instance
     * 
     * @param  command command to execute
     * @return         result of command
     */
    std::string execute_command(const char *command);
    
    /**
     * Disassemble the given address
     * 
     * @param  address command to execute
     * @param  count   maximum instructions to disassemble
     * @return         result of disassembly
     */
    std::string disassemble_address(std::uint16_t address, std::uint8_t count);

    /**
     * Get the audio buffer size
     *
     * @return audio buffer size
     */
    std::size_t get_sdl_audio_buffer_size() noexcept;

    /**
     * Set the audio buffer size
     *
     * @param buffer_size audio buffer size
     */
    void set_sdl_audio_buffer_size(std::size_t buffer_size) noexcept;
    
    /**
     * Resolve the expression
     * 
     * @param expression expression to resolve
     * @return           result of expression, if found
     */
    std::optional<std::uint16_t> evaluate_expression(const char *expression) noexcept;
    
    /**
     * Breakpoint immediately
     */
    void break_immediately() noexcept;
    
    /**
     * Unbreak
     * 
     * @param command command to run to unbreak (default is "continue")
     */
    void unbreak(const char *command = "continue");
    
private: // all private functions assume the mutex is locked by the caller
    // Save/symbols
    void load_save_and_symbols(const std::optional<std::filesystem::path> &sram_path, const std::optional<std::filesystem::path> &symbol_path);
    
    // Gameboy instance
    GB_gameboy_s gameboy = {};
    
    // Update pixel buffer size. This will clear the screen.
    void update_pixel_buffer_size();
    
    // Pixel buffer - holds the current pixels
    std::vector<std::uint32_t> pixel_buffer[3];

    // SDL audio device
    std::optional<SDL_AudioDeviceID> sdl_audio_device;
    std::size_t sdl_audio_buffer_size;
    
    // Index of the work buffer
    std::size_t work_buffer = 0;
    
    // Index of the previous buffer
    std::size_t previous_buffer = 0;

    // Index of the second previous buffer
    std::size_t previous_buffer_second = 0;
    
    // Vblank hit - calculate frame rate
    bool vblank_hit = false;

    // Use buffering
    std::atomic_bool vblank_buffering = true;
    
    // Is a ROM loaded?
    std::atomic_bool rom_loaded = false;
    
    // Paused
    bool manual_paused = false;

    // Paused from breakpoint
    std::atomic_bool bp_paused = false;
    
    // Loop is running
    std::atomic_bool loop_running = false;
    
    // Loop is finishing
    bool loop_finishing = false;
    
    // Command to end the breakpoint
    std::optional<std::string> continue_text;
    
    // Frame time information
    float frame_rate = 0.0;
    clock::time_point last_frame_time;
    std::size_t frame_time_index = 0;
    float frame_times[30] = {};

    // Pixel buffer  mode
    PixelBufferMode pixel_buffer_mode = PixelBufferMode::PixelBufferDouble;
    
    // Assign the gameboy to the current buffer
    void assign_work_buffer() noexcept;
    
    // Handle vblank
    static void on_vblank(GB_gameboy_s *gameboy) noexcept;
    
    // Log
    static void on_log(GB_gameboy_s *gameboy, const char *text, GB_log_attributes attributes) noexcept;
    
    // Input requested (basically used for breakpoints)
    static char *on_input_requested(GB_gameboy_s *gameboy);
    
    // Audio
    static void on_sample(GB_gameboy_s *gameboy, GB_sample_t *sample);
    bool audio_enabled = false;
    std::vector<std::int16_t> sample_buffer;
    std::atomic<std::uint32_t> current_sample_rate = 0;
    bool force_mono = false;
    int volume = 100;
    double volume_scale = 1.0;
    
    // Set whether or not to retain logs into a buffer instead of printing to the console
    void retain_logs(bool retain) noexcept { this->log_buffer_retained = retain; }
    
    // Resolve the instance from the gameboy
    static GameInstance *resolve_instance(GB_gameboy_s *gameboy) noexcept { return reinterpret_cast<GameInstance *>(GB_get_user_data(gameboy)); }
    
    // Should log buffer be retained to a buffer
    bool log_buffer_retained = false;
    
    // Buffer
    std::string log_buffer;
    
    // Return the contents of the buffer and clear it
    std::string clear_log_buffer();
    
    // Mutex - thread safety
    std::mutex mutex;
    
    // Execute the given command without locking the mutex. Command must be already allocated with malloc()
    std::string execute_command_without_mutex(char *command);
    
    // Get the pixel buffer size without locking the mutex.
    std::size_t get_pixel_buffer_size_without_mutex() noexcept;

    // Reset audio buffer (prevents high latency)
    void reset_audio() noexcept;

    // Unpause SDL audio
    void unpause_sdl_audio() noexcept;

    // Close the SDL audio device if one is open
    void close_sdl_audio_device() noexcept;

    // Set the current sample rate
    void set_current_sample_rate(std::uint32_t new_sample_rate) noexcept;
};

#endif

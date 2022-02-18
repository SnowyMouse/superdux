#ifndef GAME_INSTANCE_HPP
#define GAME_INSTANCE_HPP

extern "C" {
#include <Core/gb.h>
}

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
    GameInstance(GB_model_t model, GB_border_mode_t border);
    ~GameInstance();

    enum PixelBufferMode {
        /** Use single buffering. Calls to read_pixel_buffer() will give you the work buffer. This will result in slightly less visual latency, but in most cases, this will result in heavy tearing that will look terrible on any LCD display. */
        PixelBufferSingle,

        /** Use double buffering (default). Calls to read_pixel_buffer() will give you the last completed buffer. */
        PixelBufferDouble,

        /** Use interframe blending. Calls to read_pixel_buffer() will give you an average of the last two completed buffers. */
        PixelBufferDoubleBlend
    };

    using clock = std::chrono::steady_clock;

    typedef enum {
        // 8-bit registers
        SM83_REG_A,
        SM83_REG_B,
        SM83_REG_C,
        SM83_REG_D,
        SM83_REG_E,
        SM83_REG_F, // there is technically no 'accessible' F register, but here's a way to access it separately anyway
        SM83_REG_H,
        SM83_REG_L,

        // 16-bit combined registers
        SM83_REG_AF,
        SM83_REG_BC,
        SM83_REG_DE,
        SM83_REG_HL,

        // Stack pointer
        SM83_REG_SP,

        // PC (current instruction pointer)
        SM83_REG_PC,
    } SM83Register;
    
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
     * @param sample_rate preferred sample rate to use in Hz (if 0, it will use the system's preferred sample rate)
     * @param buffer_size preferred buffer size (If 0, it will use the system's preferred buffer size)
     *
     * @return success
     */
    bool set_up_sdl_audio(std::uint32_t sample_rate = 0, std::uint32_t buffer_size = 1024) noexcept;

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
     * Load the ROM with the given buffer
     *
     * @param  rom_data    ROM data to load
     * @param  rom_size    ROM size
     * @param  sram_path   optional SRAM path to load
     * @param  symbol_path optional symbol path to load
     */
    void load_rom(const std::byte *rom_data, const std::size_t rom_size, const std::optional<std::filesystem::path> &sram_path, const std::optional<std::filesystem::path> &symbol_path) noexcept;
    
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
     * @param model  model to set to
     * @param border border mode to use
     */
    void set_model(GB_model_t model, GB_border_mode_t border);

    /**
     * Set the border mode
     *
     * @param border border mode
     */
    void set_border_mode(GB_border_mode_t border) noexcept;
    
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
    std::uint16_t get_register_value(SM83Register reg) noexcept;
    
    /**
     * Set the current value of the given register
     * 
     * @param reg   register to probe
     * @param value value to set it to
     */
    void set_register_value(SM83Register reg, std::uint16_t value) noexcept;
    
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
     * Clear all button states
     */
    void clear_all_button_states();

    /**
     * Set the rapid fire button state of the Game Boy instance
     *
     * @param button  button to set
     * @param pressed state to set to
     */
    void set_rapid_button_state(GB_key_t button, bool pressed);
    
    /**
     * Get whether or not the instance is paused
     * 
     * @param paused
     */
    bool is_paused() noexcept { return this->is_paused_manually() || this->is_paused_from_breakpoint() || this->is_paused_from_rewind() || this->is_paused_from_zero_speed(); }
    
    /**
     * Set whether or not the instance is paused manually
     * 
     * @param paused paused manually
     */
    void set_paused_manually(bool paused) noexcept { this->manual_paused = paused; }
    
    /**
     * Get whether or not the instance is paused manually
     * 
     * @return paused from breakpoint
     */
    bool is_paused_manually() { return this->manual_paused; }
    
    /**
     * Get whether or not the instance is paused due to a breakpoint
     * 
     * @return paused due to breakpoint
     */
    bool is_paused_from_breakpoint() const noexcept { return this->bp_paused; }

    /**
     * Get whether or not the instance is paused because of rewind
     *
     * @return paused from rewind
     */
    bool is_paused_from_rewind() noexcept { return this->rewind_paused; }

    /**
     * Get whether or not the instance is paused because of speed multiplier being 0
     *
     * @return paused from zero speed
     */
    bool is_paused_from_zero_speed() noexcept { return this->pause_zero_speed; }
    
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
     * Set the real-time clock mode
     *
     * @param mode mode to set to
     */
    void set_rtc_mode(GB_rtc_mode_t mode) noexcept;
    
    /**
     * Breakpoint immediately
     */
    void break_immediately() noexcept;

    /**
     * Remove the given breakpoint
     *
     * @param breakpoint breakpoint to remove
     */
    void remove_breakpoint(std::uint16_t breakpoint) noexcept;

    /**
     * Remove all breakpoints
     */
    void remove_all_breakpoints() noexcept;
    
    /**
     * Unbreak
     * 
     * @param command command to run to unbreak (default is "continue")
     */
    void unbreak(const char *command = "continue");

    /**
     * Set turbo mode. Ratio is a fraction (1.0 = 100%, 2.0 = 200%, etc.)
     *
     * @param turbo       enable turbo mode
     * @param speed_ratio speed ratio to use (ignored if turbo mode is off)
     */
    void set_turbo_mode(bool turbo, float speed_ratio = 1.0) noexcept;

    /**
     * Set the boot rom path
     *
     * @param boot_rom_path boot rom to set to (if nullopt, use built-in)
     */
    void set_boot_rom_path(const std::optional<std::filesystem::path> &boot_rom_path = std::nullopt);

    /**
     * Set whether or not to use a fast boot ROM variant (if available)
     *
     * @param fast_boot_rom use fast ROM
     */
    void set_use_fast_boot_rom(bool fast_boot_rom) noexcept;

    /**
     * Add a break-and-trace breakpoint at address
     *
     * @param address         address to breakpoint
     * @param n               number of times to step
     * @param step_over       step over
     * @param break_when_done stop execution on completion
     */
    void break_and_trace_at(std::uint16_t address, std::size_t n, bool step_over, bool break_when_done);

    /**
     * Add a breakpoint at address
     *
     * @param address address to breakpoint
     */
    void break_at(std::uint16_t address) noexcept;

    struct BreakAndTraceResult {
        std::uint8_t a,b,c,d,e,f,h,l;
        bool step_over;
        std::uint16_t sp, pc;
        bool carry, half_carry, subtract, zero; // C, H, N, Z
        std::string disassembly;
    };

    /**
     * Get whether or not we can use our tracing results
     *
     * @return true if ready, false if not
     */
    bool break_and_trace_results_ready();

    /**
     * Get the break and trace results
     *
     * @return results
     */
    std::optional<std::vector<BreakAndTraceResult>> pop_break_and_trace_results();

    /**
     * Set the color correction mode
     *
     * @param mode mode to set to
     */
    void set_color_correction_mode(GB_color_correction_mode_t mode) noexcept;

    /**
     * Create a save state at the given path
     *
     * @param path path to save to
     * @return     true if successful
     */
    bool create_save_state(const std::filesystem::path &path) noexcept;

    /**
     * Create a save state with the given data
     *
     * @return     save state
     */
    std::vector<std::uint8_t> create_save_state();

    /**
     * Load a save state at the given path
     *
     * @param path path to load from
     * @return     true if successful
     */
    bool load_save_state(const std::filesystem::path &path) noexcept;

    /**
     * Load a save state at the given path
     *
     * @param state save state
     * @return      true if successful
     */
    bool load_save_state(const std::vector<std::uint8_t> &state) noexcept;

    /**
     * Set the audio highpass filter mode
     *
     * @param mode mode to set to
     */
    void set_highpass_filter_mode(GB_highpass_mode_t mode) noexcept;

    /**
     * Get the current rumble
     *
     * @return rumble
     */
    double get_rumble() noexcept { return this->rumble; }

    /**
     * Set the rumble mode
     *
     * @param mode mode to set to
     */
    void set_rumble_mode(GB_rumble_mode_t mode) noexcept;

    /**
     * Start rewinding
     *
     * @return rewinding rewinding mode
     */
    void set_rewind(bool rewinding) noexcept;

    /**
     * Set the rewind time
     *
     * @param seconds seconds to rewind
     */
    void set_rewind_length(double seconds) noexcept;

    static const constexpr std::size_t GB_TILESET_WIDTH = 256,
                                       GB_TILESET_PAGE_WIDTH = GB_TILESET_WIDTH / 2,
                                       GB_TILESET_HEIGHT = 192,
                                       GB_TILESET_TILE_LENGTH = 8,
                                       GB_TILESET_BLOCK_WIDTH = GB_TILESET_WIDTH / GB_TILESET_TILE_LENGTH,
                                       GB_TILESET_PAGE_BLOCK_WIDTH = GB_TILESET_PAGE_WIDTH / GB_TILESET_TILE_LENGTH,
                                       GB_TILESET_BLOCK_HEIGHT = GB_TILESET_HEIGHT / GB_TILESET_TILE_LENGTH,
                                       GB_PRINTER_WIDTH = 160;

    /**
     * Draw the tileset to the given pointer. The pointer must be big enough to hold GB_TILESET_WIDTH*GB_TILESET_HEIGHT 32-bit pixels.
     *
     * @param destination  destination array
     * @param palette_type palette type
     * @param index        palette index
     */
    void draw_tileset(std::uint32_t *destination, GB_palette_type_t palette_type, std::uint8_t index) noexcept;

    static const constexpr std::size_t GB_TILEMAP_WIDTH = 256, GB_TILEMAP_HEIGHT = 256;

    /**
     * Draw the tileset to the given pointer. The pointer must be big enough to hold GB_TILEMAP_WIDTH*GB_TILEMAP_HEIGHT 32-bit pixels.
     *
     * @param destination  destination array
     * @param map_type     map type
     * @param tileset_type tileset type
     */
    void draw_tilemap(std::uint32_t *destination, GB_map_type_t map_type, GB_tileset_type_t tileset_type) noexcept;

    /**
     * Get the memory at the address
     *
     * @param address address to read
     * @return        byte at address
     */
    std::uint8_t read_memory(std::uint16_t address) noexcept;

    /**
     * Get the palete colors. There are 4 colors.
     *
     * @param palette_type  type of palette
     * @param palette_index index of palette
     * @return pointer to palette colors
     */
    const uint32_t *get_palette(GB_palette_type_t palette_type, unsigned char palette_index) noexcept;

    enum TilesetInfoTileType : std::uint8_t {
        /** No access made */
        TILESET_INFO_NONE = 0,

        /** OAM */
        TILESET_INFO_OAM,

        /** Background */
        TILESET_INFO_BACKGROUND,

        /** Window (uses background palette) */
        TILESET_INFO_WINDOW
    };

    struct TilesetInfoTile {
        /** Address in VRAM */
        std::uint16_t tile_address;

        /** Index in the tileset */
        std::uint16_t tile_index;

        /** Tileset bank used (applies only to GameBoy Color games) */
        std::uint8_t tile_bank;

        /** Did we access it? */
        TilesetInfoTileType accessed_type;

        /** Palette type */
        GB_palette_type_t accessed_palette_type;

        /** If we accessed it, what's the index used to access it? (applies mainly to background/window. otherwise it's the same as tile_index for OAM) */
        std::uint8_t accessed_tile_index;

        /** If we accessed it, palette used for this tile. */
        std::uint8_t accessed_tile_palette_index;

        /** If we accessed it, what's the index of the user (0 if background/window, the index if oam) */
        std::uint8_t accessed_user_index;
    };

    struct TilesetInfo {
        /** Tiles */
        TilesetInfoTile tiles[GB_TILESET_PAGE_BLOCK_WIDTH * GB_TILESET_BLOCK_HEIGHT * 2];
    };

    /**
     * Get tileset metadata
     *
     * @param tileset info
     */
    TilesetInfo get_tileset_info() noexcept;

    /** OAM object count */
    static constexpr const std::size_t GB_OAM_OBJECT_COUNT = 40;

    struct ObjectAttributeInfoObject {
        /** X coordinate */
        std::uint8_t x;

        /** Y coordinate */
        std::uint8_t y;

        /** Tileset tile (offset from $8800) */
        std::uint8_t tile;

        /** Tileset bank */
        std::uint8_t tileset_bank : 1;

        /** Palette number */
        std::uint8_t palette : 3;

        /** X flip */
        bool flip_x : 1;

        /** Y flip */
        bool flip_y : 1;

        /** Onscreen (can still be obscured by 10 object limit) */
        bool on_screen : 1;

        /** Obscured by line limit */
        bool obscurred_by_line_limit : 1;

        /** BG/window colors 1-3 over this object */
        bool bg_window_over_obj : 1;

        /** Drawn data */
        std::uint32_t pixel_data[GB_TILESET_TILE_LENGTH*2*GB_TILESET_TILE_LENGTH] = {};
    };

    struct ObjectAttributeInfo {
        /** Objects */
        ObjectAttributeInfoObject objects[GB_OAM_OBJECT_COUNT];

        /** Width dimension */
        std::uint8_t width = 8;

        /** Height dimension */
        std::uint8_t height;
    };

    /**
     * Get object metadata
     *
     * @return object metadata
     */
    ObjectAttributeInfo get_object_attribute_info() noexcept;

    /**
     * Get the raw 15-bit palette
     *
     * @param type    type of palette
     * @param palette palette index
     * @param output  pointer to 4 integers
     */
    void get_raw_palette(GB_palette_type_t type, std::size_t palette, std::uint16_t *output) noexcept;

    /**
     * Get whether or not the instance is a Game Boy Color
     *
     * @return true if CGB in CGB mode
     */
    bool is_game_boy_color() noexcept;

    /**
     * Get whether or not the instance is a Game Boy Color in CGB mode
     *
     * @return true if CGB in CGB mode
     */
    bool is_game_boy_color_in_cgb_mode() noexcept;

    /**
     * Enable the printer. This uses the serial port connection.
     */
    void connect_printer() noexcept;

    /**
     * Disconnect the serial. This disables any features that require link cable support, including the printer.
     */
    void disconnect_serial() noexcept;

    /**
     * Get the last printer result and clear it. This will always be 160 pixels in width.
     *
     * @param height height of the page in pixels
     * @return last printer result if any
     */
    std::optional<std::vector<std::uint32_t>> pop_printed_image(std::size_t &height);

    
private: // all private functions assume the mutex is locked by the caller
    // Save/symbols
    void load_save_and_symbols(const std::optional<std::filesystem::path> &sram_path, const std::optional<std::filesystem::path> &symbol_path);

    // Pixel buffer width/height
    std::uint16_t pb_width;
    std::uint16_t pb_height;

    // Button bitfield
    std::atomic<GB_key_mask_t> button_bitfield = static_cast<decltype(button_bitfield.load())>(0);
    
    // Gameboy instance
    GB_gameboy_s gameboy = {};
    
    // Update pixel buffer size. This will clear the screen.
    void update_pixel_buffer_size();
    
    // Pixel buffer - holds the current pixels
    std::vector<std::uint32_t> pixel_buffer[3];

    // Break and trace addresses
    std::vector<std::tuple<std::uint16_t, std::size_t, bool, bool>> break_and_trace_breakpoints;
    std::vector<std::vector<BreakAndTraceResult>> break_and_trace_result;
    std::size_t current_break_and_trace_remaining = 0;
    bool current_break_and_trace_step_over = false;
    bool current_break_and_trace_break_when_done = false;
    bool break_and_trace_results_ready_no_mutex() const noexcept;

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
    std::atomic_bool manual_paused = false;

    // Paused from breakpoint
    std::atomic_bool bp_paused = false;
    
    // Loop is running
    std::atomic_bool loop_running = false;

    // Pause due to zero speed
    std::atomic_bool pause_zero_speed = false;
    
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
    int volume = 50;
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

    // Vblank mutex - thread safety, but faster since only older information needs to be read
    std::mutex vblank_mutex;

    // Printer mutex - thread safety for the printer data
    std::mutex printer_mutex;
    
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

    // Turbo mode and turbo mode speed
    bool turbo_mode_enabled = false;
    float turbo_mode_speed_ratio = 1.0;
    clock::time_point next_expected_frame = {};

    // Boot ROM callback
    static void load_boot_rom(GB_gameboy_t *gb, GB_boot_rom_t type) noexcept;
    std::optional<std::filesystem::path> boot_rom_path;
    bool fast_boot_rom;

    // Disassemble without that mutex
    std::string disassemble_without_mutex(std::uint16_t address, std::uint8_t count);

    // Get the backtrace without a mutex
    std::vector<std::uint16_t> get_breakpoints_without_mutex();

    // Rumble
    std::atomic<double> rumble = 0.0;
    static void on_rumble(GB_gameboy_s *gb, double rumble) noexcept;
    bool should_rewind = false;
    std::atomic_bool rewind_paused = false;

    // Rapid buttons
    std::atomic<GB_key_mask_t> rapid_button_bitfield = static_cast<decltype(button_bitfield.load())>(0);
    bool rapid_button_state = false;
    std::uint8_t rapid_button_frames = 0;
    std::uint8_t rapid_button_switch_frames = 4;

    bool rewinding = false;

    // Do it without mutex
    TilesetInfo get_tileset_info_without_mutex() noexcept;

    // Do it without mutex too
    ObjectAttributeInfo get_object_attribute_info_without_mutex() noexcept;

    // Model to change to (if we changed model due to a save state)
    std::optional<GB_model_t> original_model;

    // Switch back to the original model if we need to. Otherwise, do a simple reset.
    void reset_to_original_model() noexcept;

    // Begin loading ROM (lock mutex, reset values)
    void begin_loading_rom() noexcept;

    // Clear all the button states
    void clear_all_button_states_no_mutex() noexcept;

    // Print
    static void print_image(GB_gameboy_t *gb, std::uint32_t *image, std::uint8_t height, std::uint8_t top_margin, std::uint8_t bottom_margin, std::uint8_t exposure);
    std::vector<std::pair<std::vector<std::uint32_t>, std::size_t>> printer_data;

    // Skip intro?
    void skip_sgb_intro_if_needed() noexcept;
};



#endif

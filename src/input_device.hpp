#ifndef SB_QT_INPUT_DEVICE_HPP
#define SB_QT_INPUT_DEVICE_HPP

#include <QObject>
#include <cstring>
#include <optional>
#include <QKeyEvent>
#include <SDL2/SDL.h>
#include <QList>

class GameWindow;

class InputDevice : public QObject {
    Q_OBJECT
    
public:
    #define DO_EVERYTHING DO_THIS(A) \
                          DO_THIS(B) \
                          DO_THIS(Start) \
                          DO_THIS(Select) \
                          DO_THIS(Up) \
                          DO_THIS(Down) \
                          DO_THIS(Left) \
                          DO_THIS(Right) \
                          DO_THIS(RapidA) \
                          DO_THIS(RapidB) \
                          DO_THIS(RapidStart) \
                          DO_THIS(RapidSelect) \
                          DO_THIS(RapidUp) \
                          DO_THIS(RapidDown) \
                          DO_THIS(RapidLeft) \
                          DO_THIS(RapidRight) \
                          DO_THIS(Turbo) \
                          DO_THIS(Slowmo) \
                          DO_THIS(Rewind) \
                          DO_THIS(VolumeUp) \
                          DO_THIS(VolumeDown) \
                          DO_THIS(ShowFPS)
    
    enum InputType : unsigned int  {
        #define DO_THIS(type) Input_##type,
        DO_EVERYTHING
        #undef DO_THIS
        Input_COUNT
    };
                          
    #define DO_THIS(type) case Input_##type: return # type;
    static const char *input_type_to_string(InputType input_type) noexcept {
        switch(input_type) {
            DO_EVERYTHING
            default:
                return nullptr;
        }
    }
    #undef DO_THIS
    #define DO_THIS(type) if(std::strcmp(str, # type) == 0) { return Input_##type; } else
    static std::optional<InputType> input_type_from_string(const char *str) noexcept {
        DO_EVERYTHING
        return std::nullopt;
    }
    #undef DO_THIS
    #undef DO_EVERYTHING
    
    void emit_input(std::uint32_t input_type, double value);
    
    virtual std::optional<QString> control_to_string(std::uint32_t what) = 0;
    virtual std::optional<std::uint32_t> control_from_string(const QString &what) = 0;
    virtual void load_sane_defaults() = 0;
    
    virtual QString name() const noexcept {
        return "Unknown";
    }
    
    virtual ~InputDevice() = 0;
    
    QList<std::uint32_t> settings[Input_COUNT];
    void save_settings();
    
signals:
    void input(InputType type, double input);
    void control_input(std::uint32_t, double input);
    
protected:
    void load_settings();
};

class InputDeviceKeyboard : public InputDevice {
    Q_OBJECT
    
public:
    InputDeviceKeyboard();
    ~InputDeviceKeyboard() override;
    
    QString name() const noexcept override {
        return "Keyboard";
    }
    
    QMap<QString, QVariant> get_settings();
    std::optional<QString> control_to_string(std::uint32_t what) override;
    std::optional<std::uint32_t> control_from_string(const QString &what) override;
    
    void handle_key_event(QKeyEvent *event, bool pressed);
    void load_sane_defaults() override;
};

class InputDeviceGamepad : public InputDevice {
    Q_OBJECT
    
public:
    std::optional<QString> control_to_string(std::uint32_t what) override;
    std::optional<std::uint32_t> control_from_string(const QString &what) override;

    void handle_input(SDL_GameControllerButton type, bool value);
    void handle_input(SDL_GameControllerAxis type, double value);
    void apply_rumble(double rumble) noexcept;
    SDL_JoystickID get_joystick_id() const noexcept;

    InputDeviceGamepad(SDL_GameController *gamepad);
    ~InputDeviceGamepad() override;
    
    QString name() const noexcept override;
    void load_sane_defaults() override;
    
private:
    SDL_GameController *gamepad;
};

#endif

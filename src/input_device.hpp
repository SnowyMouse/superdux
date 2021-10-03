#ifndef SB_QT_INPUT_DEVICE_HPP
#define SB_QT_INPUT_DEVICE_HPP

#include <QGamepad>
#include <QObject>
#include <cstring>
#include <optional>
#include <QKeyEvent>
#include <QList>

class QGamepad;
class GameWindow;

class InputDevice : public QObject {
    Q_OBJECT
    
public:
    enum InputType : unsigned int  {
        Input_A,
        Input_B,
        Input_Start,
        Input_Select,
        Input_Up,
        Input_Down,
        Input_Left,
        Input_Right,
        Input_Turbo,
        Input_VolumeUp,
        Input_VolumeDown,
        Input_COUNT
    };
    
    #define DO_EVERYTHING DO_THIS(A) \
                          DO_THIS(B) \
                          DO_THIS(Start) \
                          DO_THIS(Select) \
                          DO_THIS(Up) \
                          DO_THIS(Down) \
                          DO_THIS(Left) \
                          DO_THIS(Right) \
                          DO_THIS(Turbo) \
                          DO_THIS(VolumeUp) \
                          DO_THIS(VolumeDown)
    
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
    enum ControllerInputType : unsigned int {
        Controller_Input_A,
        Controller_Input_B,
        Controller_Input_Select,
        Controller_Input_Start,
        Controller_Input_Up,
        Controller_Input_Down,
        Controller_Input_Left,
        Controller_Input_Right,
        Controller_Input_LeftX,
        Controller_Input_LeftY,
        Controller_Input_RightX,
        Controller_Input_RightY,
        Controller_Input_Center,
        Controller_Input_Guide,
        Controller_Input_L1,
        Controller_Input_L2,
        Controller_Input_L3,
        Controller_Input_R1,
        Controller_Input_R2,
        Controller_Input_R3,
        Controller_Input_X,
        Controller_Input_Y,
        Controller_Input_COUNT
    };
    
    #define DO_EVERYTHING DO_THIS(A, bool, button) \
                          DO_THIS(B, bool, button) \
                          DO_THIS(Select, bool, button) \
                          DO_THIS(Start, bool, button) \
                          DO_THIS(Up, bool, button) \
                          DO_THIS(Down, bool, button) \
                          DO_THIS(Left, bool, button) \
                          DO_THIS(Right, bool, button) \
                          DO_THIS(LeftX, double, axis) \
                          DO_THIS(LeftY, double, axis) \
                          DO_THIS(RightX, double, axis) \
                          DO_THIS(RightY, double, axis) \
                          DO_THIS(Center, bool, button) \
                          DO_THIS(Guide, bool, button) \
                          DO_THIS(L1, bool, button) \
                          DO_THIS(L2, double, button) \
                          DO_THIS(L3, bool, button) \
                          DO_THIS(R1, bool, button) \
                          DO_THIS(R2, double, button) \
                          DO_THIS(R3, bool, button) \
                          DO_THIS(X, bool, button) \
                          DO_THIS(Y, bool, button)
    
    #define DO_THIS(type, ...) case Controller_Input_##type: return # type;
    static const char *gamepad_input_type_to_string(ControllerInputType input_type) noexcept {
        switch(input_type) {
            DO_EVERYTHING
            default:
                return nullptr;
        }
    }
    #undef DO_THIS
    #define DO_THIS(type, ...) if(std::strcmp(str, # type) == 0) { return Controller_Input_##type; } else
    static std::optional<ControllerInputType> gamepad_input_type_from_string(const char *str) noexcept {
        DO_EVERYTHING
        return std::nullopt;
    }
    #undef DO_THIS
    
    std::optional<QString> control_to_string(std::uint32_t what) override;
    std::optional<std::uint32_t> control_from_string(const QString &what) override;
    
    void handle_input(ControllerInputType type, double value);
    
    #define DO_THIS(type, rval, ...) void on_##type(rval v) { handle_input(Controller_Input_##type, v); }
    DO_EVERYTHING
    #undef DO_THIS
    
    InputDeviceGamepad(int gamepadDeviceID) {
        this->gamepad = new QGamepad(gamepadDeviceID, this);
        this->load_settings();
        #define DO_THIS(type, rval, stype) connect(this->gamepad, &QGamepad::stype ## type ## Changed, this, &InputDeviceGamepad::on_##type);
        DO_EVERYTHING
        #undef DO_THIS
    }
    
    #undef DO_EVERYTHING
    ~InputDeviceGamepad() override;
    
    QString name() const noexcept override {
        return this->gamepad->name();
    }
    
    void load_sane_defaults() override;
    
private:
    QGamepad *gamepad;
};

#endif

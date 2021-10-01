#ifndef SB_QT_INPUT_DEVICE_HPP
#define SB_QT_INPUT_DEVICE_HPP

#include <QGamepad>
#include <QObject>
#include <cstring>
#include <optional>
#include <QKeyEvent>

class QGamepad;
class GameWindow;

class InputDevice : public QObject {
    Q_OBJECT
    
public:
    enum InputType {
        Input_A,
        Input_B,
        Input_Start,
        Input_Select,
        Input_Up,
        Input_Down,
        Input_Left,
        Input_Right,
        Input_Turbo,
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
                          DO_THIS(Turbo)
    
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
    
    virtual ~InputDevice() = 0;
    
signals:
    void input(InputType type, double input);
};

class InputDeviceKeyboard : public InputDevice {
    Q_OBJECT
    
public:
    InputDeviceKeyboard(GameWindow *window);
    ~InputDeviceKeyboard() override;
    
    void handle_key_event(QKeyEvent *event, bool pressed);
    
private:
    std::optional<Qt::Key> settings[Input_COUNT] = {};
};

class InputDeviceGamepad : public InputDevice {
    Q_OBJECT
    
public:
    enum ControllerInputType {
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
    
    void handle_input(ControllerInputType type, double value);
    
    #define DO_THIS(type, rval, ...) void on_##type(rval v) { handle_input(Controller_Input_##type, v); }
    DO_EVERYTHING
    #undef DO_THIS
    
    InputDeviceGamepad(int gamepadDeviceID) {
        this->gamepad = new QGamepad(gamepadDeviceID, this);
        
        this->settings[Controller_Input_A][0] = InputType::Input_A;
        this->settings[Controller_Input_B][0] = InputType::Input_B;
        this->settings[Controller_Input_Start][0] = InputType::Input_Start;
        this->settings[Controller_Input_Select][0] = InputType::Input_Select;
        this->settings[Controller_Input_Up][0] = InputType::Input_Up;
        this->settings[Controller_Input_Down][0] = InputType::Input_Down;
        this->settings[Controller_Input_Left][0] = InputType::Input_Left;
        this->settings[Controller_Input_Right][0] = InputType::Input_Right;
        this->settings[Controller_Input_R2][0] = InputType::Input_Turbo;
        
        #define DO_THIS(type, rval, stype) connect(this->gamepad, &QGamepad::stype ## type ## Changed, this, &InputDeviceGamepad::on_##type);
        DO_EVERYTHING
        #undef DO_THIS
    }
    
    #undef DO_EVERYTHING
    ~InputDeviceGamepad() override;
    
signals:
    void controllerInput(ControllerInputType type, float value);
    void disconnected();
    
private:
    QGamepad *gamepad;
    std::optional<InputType> settings[Controller_Input_COUNT][2] = {};
};

#endif

#include "settings.hpp"
#include "input_device.hpp"

InputDevice::~InputDevice() {}

#define SETTINGS_NAME(name) QString("device_" + name + "_controls")

void InputDevice::save_settings() {
    auto application_settings = get_superdux_settings();
    
    // Build a map of settings to a list of possible values.
    // This will allow people to bind multiple inputs to multiple results.
    QMap<QString, QVariant> settings_to_write;
    for(std::size_t i = 0; i < sizeof(this->settings) / sizeof(this->settings[0]); i++) {
        auto &s = this->settings[i];
        if(s.size() == 0) {
            continue;
        }
        QString key = this->input_type_to_string(static_cast<InputType>(i));
        QList<QVariant> value;
        for(auto &q : s) {
            auto q_str = this->control_to_string(q);
            if(q_str.has_value()) {
                value.append(*q_str);
            }
        }
        settings_to_write.insert(key, value);
    }
    application_settings.setValue(SETTINGS_NAME(this->name()), settings_to_write);
}

void InputDevice::load_settings() {
    auto application_settings = get_superdux_settings();
    
    auto controls = application_settings.value(SETTINGS_NAME(this->name()), QMap<QString, QVariant> {}).toMap();
    auto controls_keys = controls.keys();
    
    if(controls_keys.length() > 0) {
        for(auto &i : controls.keys()) {
            auto k = input_type_from_string(i.toUtf8().data());
            if(k.has_value()) {
                QList<std::uint32_t> list;
                for(auto &j : controls[i].toList()) {
                    auto control = this->control_from_string(j.toString());
                    if(control.has_value()) {
                        list.append(*control);
                    }
                }
                this->settings[*k] = list;
            }
        }
    }
    else {
        std::printf("No settings found for %s. Loading sane defaults...\n", this->name().toUtf8().data());
        this->load_sane_defaults();
        this->save_settings();
    }
}

void InputDevice::emit_input(std::uint32_t input_type, double value) {
    emit control_input(input_type, value);
    for(std::size_t i = 0; i < Input_COUNT; i++) {
        auto &s = this->settings[i];
        if(s.contains(static_cast<std::uint32_t>(input_type))) {
            emit input(static_cast<InputType>(i), value);
            return;
        }
    }
}

InputDeviceKeyboard::InputDeviceKeyboard() {
    this->load_settings();
}

void InputDeviceKeyboard::handle_key_event(QKeyEvent *event, bool pressed) {
    this->emit_input(static_cast<std::uint32_t>(event->key()), pressed ? 1.0 : 0.0);
}

static constexpr const std::uint32_t CONTROLLER_BUTTON_MASK = 0x0000FFFF;
static constexpr const std::uint32_t CONTROLLER_BUTTON_SHIFT = 0;

static constexpr const char CONTROLLER_NEGATIVE_CHAR = '-';
static constexpr const char CONTROLLER_POSITIVE_CHAR = '+';
static constexpr const std::uint32_t CONTROLLER_NEGATIVE_MASK = 0x80000000;
static constexpr const std::uint32_t CONTROLLER_AXIS_MASK = 0xFF000000 ^ CONTROLLER_NEGATIVE_MASK;
static constexpr const std::uint32_t CONTROLLER_AXIS_SHIFT = 24;

static constexpr std::uint32_t controller_input_to_key(SDL_GameControllerButton axis) noexcept {
    return (static_cast<std::uint32_t>(axis + 1) << CONTROLLER_BUTTON_SHIFT) & CONTROLLER_BUTTON_MASK;
}

static constexpr std::uint32_t controller_input_to_key(SDL_GameControllerAxis button) noexcept {
    return (static_cast<std::uint32_t>(button + 1) << CONTROLLER_AXIS_SHIFT) & CONTROLLER_AXIS_MASK;
}

static constexpr std::variant<SDL_GameControllerButton, SDL_GameControllerAxis> controller_key_to_input(std::uint32_t what) noexcept {
    if(what & CONTROLLER_BUTTON_MASK) {
        return static_cast<SDL_GameControllerButton>(((what & CONTROLLER_BUTTON_MASK) >> CONTROLLER_BUTTON_SHIFT) - 1);
    }
    else if(what & CONTROLLER_AXIS_MASK) {
        return static_cast<SDL_GameControllerAxis>(((what & CONTROLLER_AXIS_MASK) >> CONTROLLER_AXIS_SHIFT) - 1);
    }
    else {
        std::terminate();
    }
}


std::optional<QString> InputDeviceKeyboard::control_to_string(std::uint32_t what) {
    switch(what) {
        case Qt::Key_Shift:
            return "Shift";
        case Qt::Key_Alt:
            return "Alt";
        case Qt::Key_Control:
            return "Control";
    }
    
    auto s = QKeySequence(what);
    if(!s.isEmpty()) {
        return s.toString();
    }
    return std::nullopt;
}
std::optional<std::uint32_t> InputDeviceKeyboard::control_from_string(const QString &what) {
    if(what == "Shift") {
        return Qt::Key::Key_Shift;
    }
    else if(what == "Control") {
        return Qt::Key::Key_Control;
    }
    else if(what == "Alt") {
        return Qt::Key::Key_Alt;
    }
    
    auto s = QKeySequence::fromString(what);
    if(!s.isEmpty()) {
        return s[0];
    }
    else {
        return std::nullopt;
    }
}

std::optional<QString> InputDeviceGamepad::control_to_string(std::uint32_t what) {
    auto input = controller_key_to_input(what);

    auto *button = std::get_if<SDL_GameControllerButton>(&input);
    auto *axis = std::get_if<SDL_GameControllerAxis>(&input);

    if(button) {
        return SDL_GameControllerGetStringForButton(*button);
    }
    else if(axis) {
        return QString(SDL_GameControllerGetStringForAxis(*axis)) + ((what & CONTROLLER_NEGATIVE_MASK) ? CONTROLLER_NEGATIVE_CHAR : CONTROLLER_POSITIVE_CHAR);
    }
    else {
        return std::nullopt;
    }
}
std::optional<std::uint32_t> InputDeviceGamepad::control_from_string(const QString &what) {
    if(what.endsWith(CONTROLLER_POSITIVE_CHAR) || what.endsWith(CONTROLLER_NEGATIVE_CHAR)) {
        auto axis = SDL_GameControllerGetAxisFromString(what.mid(0, what.length() - 1).toUtf8().data());
        if(axis != SDL_GameControllerAxis::SDL_CONTROLLER_AXIS_INVALID) {
            return controller_input_to_key(axis);
        }
    }

    auto button = SDL_GameControllerGetButtonFromString(what.toUtf8().data());
    if(button != SDL_GameControllerButton::SDL_CONTROLLER_BUTTON_INVALID) {
        return controller_input_to_key(button);
    }
    else {
        return std::nullopt;
    }
}

QString InputDeviceGamepad::name() const noexcept {
    return SDL_GameControllerName(this->gamepad);
}

InputDeviceKeyboard::~InputDeviceKeyboard() {}
InputDeviceGamepad::~InputDeviceGamepad() {
    SDL_GameControllerClose(this->gamepad);
}

void InputDeviceGamepad::handle_input(SDL_GameControllerButton type, bool value) {
    this->emit_input(controller_input_to_key(type), value ? 1.0 : 0.0);
}
void InputDeviceGamepad::handle_input(SDL_GameControllerAxis type, double value) {
    this->emit_input(controller_input_to_key(type) | (value < 0.0 ? CONTROLLER_NEGATIVE_MASK : 0), std::fabs(value));
}

void InputDeviceKeyboard::load_sane_defaults() {
    this->settings[Input_A] = {Qt::Key_X, Qt::Key_S}; // based on VBA/SameBoy SDL and BGB, respectively
    this->settings[Input_B] = {Qt::Key_Z, Qt::Key_A}; // based on VBA/SameBoy SDL and BGB, respectively
    this->settings[Input_Start] = {Qt::Key_Return};
    this->settings[Input_Select] = {Qt::Key_Shift};
    this->settings[Input_Left] = {Qt::Key_Left};
    this->settings[Input_Right] = {Qt::Key_Right};
    this->settings[Input_Up] = {Qt::Key_Up};
    this->settings[Input_Down] = {Qt::Key_Down};
    this->settings[Input_Turbo] = {Qt::Key_Space};
    this->settings[Input_Slowmo] = {Qt::Key_C};
    this->settings[Input_Rewind] = {Qt::Key_Backspace};
    this->settings[Input_ShowFPS] = {Qt::Key_F3};
}

void InputDeviceGamepad::load_sane_defaults() {
    this->settings[Input_A] = {controller_input_to_key(SDL_GameControllerButton::SDL_CONTROLLER_BUTTON_A)};
    this->settings[Input_B] = {controller_input_to_key(SDL_GameControllerButton::SDL_CONTROLLER_BUTTON_B)};
    this->settings[Input_Start] = {controller_input_to_key(SDL_GameControllerButton::SDL_CONTROLLER_BUTTON_START)};
    this->settings[Input_Select] = {controller_input_to_key(SDL_GameControllerButton::SDL_CONTROLLER_BUTTON_BACK)};
    this->settings[Input_Left] = {controller_input_to_key(SDL_GameControllerButton::SDL_CONTROLLER_BUTTON_DPAD_LEFT)};
    this->settings[Input_Right] = {controller_input_to_key(SDL_GameControllerButton::SDL_CONTROLLER_BUTTON_DPAD_RIGHT)};
    this->settings[Input_Up] = {controller_input_to_key(SDL_GameControllerButton::SDL_CONTROLLER_BUTTON_DPAD_UP)};
    this->settings[Input_Down] = {controller_input_to_key(SDL_GameControllerButton::SDL_CONTROLLER_BUTTON_DPAD_DOWN)};
    this->settings[Input_Turbo] = {controller_input_to_key(SDL_GameControllerAxis::SDL_CONTROLLER_AXIS_TRIGGERRIGHT)};
    this->settings[Input_Slowmo] = {controller_input_to_key(SDL_GameControllerAxis::SDL_CONTROLLER_AXIS_TRIGGERLEFT)};
    this->settings[Input_Rewind] = {controller_input_to_key(SDL_GameControllerButton::SDL_CONTROLLER_BUTTON_X)};
}

InputDeviceGamepad::InputDeviceGamepad(SDL_GameController *gamepad) : gamepad(gamepad) {
    this->load_settings();
}

SDL_JoystickID InputDeviceGamepad::get_joystick_id() const noexcept {
    return SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(this->gamepad));
}

void InputDeviceGamepad::apply_rumble(double rumble) noexcept {
    SDL_GameControllerRumble(this->gamepad, 0, 0xFFFF * rumble, 33);
}

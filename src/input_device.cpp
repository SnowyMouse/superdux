#include <QGamepad>
#include <QSettings>

#include "input_device.hpp"

InputDevice::~InputDevice() {}

#define SETTINGS_NAME(name) QString("device_" + name + "_controls")

void InputDevice::save_settings() {
    QSettings application_settings;
    
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
    QSettings application_settings;
    
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
    emit controlInput(input_type, value);
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

static std::pair<std::uint32_t, const char *> input_map[] = {
    { InputDeviceGamepad::Controller_Input_A, "A" },
    { InputDeviceGamepad::Controller_Input_B, "B" },
    { InputDeviceGamepad::Controller_Input_Select, "Select" },
    { InputDeviceGamepad::Controller_Input_Start, "Start" },
    { InputDeviceGamepad::Controller_Input_Up, "Up" },
    { InputDeviceGamepad::Controller_Input_Down, "Down" },
    { InputDeviceGamepad::Controller_Input_Left, "Left" },
    { InputDeviceGamepad::Controller_Input_Right, "Right" },
    { InputDeviceGamepad::Controller_Input_LeftX, "LeftX+" },
    { ~InputDeviceGamepad::Controller_Input_LeftX, "LeftX-" },
    { InputDeviceGamepad::Controller_Input_LeftY, "LeftY+" },
    { ~InputDeviceGamepad::Controller_Input_LeftY, "LeftY-" },
    { InputDeviceGamepad::Controller_Input_RightX, "RightX+" },
    { ~InputDeviceGamepad::Controller_Input_RightX, "RightY-" },
    { InputDeviceGamepad::Controller_Input_RightY, "RightY+" },
    { ~InputDeviceGamepad::Controller_Input_RightY, "RightY-" },
    { InputDeviceGamepad::Controller_Input_Center, "Center" },
    { InputDeviceGamepad::Controller_Input_Guide, "Guide" },
    { InputDeviceGamepad::Controller_Input_L1, "L1" },
    { InputDeviceGamepad::Controller_Input_L2, "L2+" },
    { ~InputDeviceGamepad::Controller_Input_L2, "L2-" },
    { InputDeviceGamepad::Controller_Input_L3, "L3" },
    { InputDeviceGamepad::Controller_Input_R1, "R1" },
    { InputDeviceGamepad::Controller_Input_R2, "R2+" },
    { ~InputDeviceGamepad::Controller_Input_R2, "R2-" },
    { InputDeviceGamepad::Controller_Input_R3, "R3" },
    { InputDeviceGamepad::Controller_Input_X, "X" },
    { InputDeviceGamepad::Controller_Input_Y, "Y" }
};



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
std::optional<QString> InputDeviceGamepad::control_to_string(std::uint32_t what) {
    for(auto &i : input_map) {
        if(what == i.first) {
            return i.second;
        }
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
std::optional<std::uint32_t> InputDeviceGamepad::control_from_string(const QString &what) {
    for(auto &i : input_map) {
        if(what == i.second) {
            return i.first;
        }
    }
    return std::nullopt;
    
}

InputDeviceKeyboard::~InputDeviceKeyboard() {}
InputDeviceGamepad::~InputDeviceGamepad() {}

void InputDeviceGamepad::handle_input(ControllerInputType type, double value) {
    // Negate if negative
    std::uint32_t t;
    if(value < 0.0) {
        t = static_cast<std::uint32_t>(~type);
        value *= -1.0;
    }
    else {
        t = static_cast<std::uint32_t>(type);
    }
    
    // Deadzones
    if(value < 0.25) {
        value = 0.0;
    }
    
    // Done
    this->emit_input(t, value);
}

void InputDeviceKeyboard::load_sane_defaults() {
    this->settings[Input_A] = {Qt::Key_X};
    this->settings[Input_B] = {Qt::Key_Z};
    this->settings[Input_Start] = {Qt::Key_Return};
    this->settings[Input_Select] = {Qt::Key_Shift};
    this->settings[Input_Left] = {Qt::Key_Left};
    this->settings[Input_Right] = {Qt::Key_Right};
    this->settings[Input_Up] = {Qt::Key_Up};
    this->settings[Input_Down] = {Qt::Key_Down};
    this->settings[Input_Turbo] = {Qt::Key_C};
}

void InputDeviceGamepad::load_sane_defaults() {
    this->settings[Input_A] = {Controller_Input_A};
    this->settings[Input_B] = {Controller_Input_B};
    this->settings[Input_Start] = {Controller_Input_Start};
    this->settings[Input_Select] = {Controller_Input_Select};
    this->settings[Input_Left] = {Controller_Input_Left};
    this->settings[Input_Right] = {Controller_Input_Right};
    this->settings[Input_Up] = {Controller_Input_Up};
    this->settings[Input_Down] = {Controller_Input_Down};
    this->settings[Input_Turbo] = {Controller_Input_R2};
}

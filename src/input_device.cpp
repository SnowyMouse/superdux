#include <QGamepad>
#include <QSettings>

#include "input_device.hpp"

InputDevice::~InputDevice() {}

InputDeviceKeyboard::InputDeviceKeyboard() {
    QSettings application_settings;
    
    auto controls_keyboard = application_settings.value("controls_keyboard", QMap<QString, QVariant> {
        // Sane defaults
        { input_type_to_string(Input_A), Qt::Key_X },
        { input_type_to_string(Input_B), Qt::Key_Z },
        { input_type_to_string(Input_Start), Qt::Key_Return },
        { input_type_to_string(Input_Select), Qt::Key_Shift },
        { input_type_to_string(Input_Left), Qt::Key_Left },
        { input_type_to_string(Input_Right), Qt::Key_Right },
        { input_type_to_string(Input_Up), Qt::Key_Up },
        { input_type_to_string(Input_Down), Qt::Key_Down },
        { input_type_to_string(Input_Turbo), Qt::Key_C }
    }).toMap();
    
    for(auto &i : controls_keyboard.keys()) {
        auto k = input_type_from_string(i.toUtf8().data());
        if(k.has_value()) {
            settings[*k] = static_cast<Qt::Key>(controls_keyboard[i].toInt());
        }
    }
}

void InputDeviceKeyboard::handle_key_event(QKeyEvent *event, bool pressed) {
    auto k = event->key();
    
    for(auto &i : settings) {
        if(i == k) {
            emit input(static_cast<InputType>(&i - &settings[0]), pressed ? 1.0 : 0.0);
            return;
        }
    }
}

InputDeviceKeyboard::~InputDeviceKeyboard() {}

InputDeviceGamepad::~InputDeviceGamepad() {}

void InputDeviceGamepad::handle_input(ControllerInputType type, double value) {
    // Always emit this
    emit controllerInput(type, value);

    // If it's set, emit it
    int index;
    if(value >= 0.0) {
        index = 0;
    }
    else {
        index = 1;
        value *= -1.0;
    }
    
    // Deadzones
    if(value < 0.3) {
        value = 0.0;
    }
    
    auto &t = this->settings[type][index];
    if(t.has_value()) {
        emit input(*t, value);
    }
}

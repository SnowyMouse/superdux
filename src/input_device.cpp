#include <QGamepad>

#include "input_device.hpp"

InputDevice::~InputDevice() {}

InputDeviceKeyboard::InputDeviceKeyboard() {
    settings[Input_A] = Qt::Key_X;
    settings[Input_B] = Qt::Key_Z;
    settings[Input_Start] = Qt::Key_Return;
    settings[Input_Select] = Qt::Key_Shift;
    settings[Input_Left] = Qt::Key_Left;
    settings[Input_Right] = Qt::Key_Right;
    settings[Input_Up] = Qt::Key_Up;
    settings[Input_Down] = Qt::Key_Down;
    settings[Input_Turbo] = Qt::Key_C;
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

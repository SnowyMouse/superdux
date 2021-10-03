#include "edit_controls_dialog.hpp"
#include "game_window.hpp"

class EditControlsDialog::InputLineEdit : public QLineEdit {
public:
    InputLineEdit(QWidget *parent, EditControlsDialog *dialog) : QLineEdit(parent), dialog(dialog) {}
    ~InputLineEdit() {}
    
    void keyPressEvent(QKeyEvent *event) override {
        event->ignore(); // this should not be directly edited
    }
    void mousePressEvent(QMouseEvent *) override {
        this->setText("");
        this->dialog->save_settings();
    }
    
    EditControlsDialog *dialog;
};

EditControlsDialog::EditControlsDialog() : QDialog() {
    this->setSizePolicy(QSizePolicy::Policy::Minimum, QSizePolicy::Policy::Minimum);
    this->setWindowTitle("Control Settings");
    
    this->layout = new QVBoxLayout(this);
    this->setLayout(layout);
    
    // Make the device picker
    this->device_picker = new QWidget(this);
    this->device_picker_layout = new QHBoxLayout(this->device_picker);
    auto *device_label = new QLabel("Device:", this->device_picker);
    device_label->setSizePolicy(QSizePolicy::Policy::Maximum, QSizePolicy::Policy::Maximum);
    this->device_picker_layout->addWidget(device_label);
    this->device_picker_layout->setContentsMargins(0,0,0,0);
    this->regenerate_device_list();
    this->layout->addWidget(this->device_picker);
    
    // Now for the buttons
    this->regenerate_button_settings(-1);
}

EditControlsDialog::~EditControlsDialog() {}

void EditControlsDialog::regenerate_device_list() {
    delete this->device_box;
    
    this->device_box = new QComboBox(device_picker);
    this->device_box->setSizePolicy(QSizePolicy::Policy::Minimum, QSizePolicy::Policy::Minimum);
    auto all_devices = GameWindow::get_all_devices();
    for(auto &device : all_devices) {
        this->device_box->addItem(device->name());
    }
    this->device_picker_layout->addWidget(this->device_box);
    connect(this->device_box, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this, &EditControlsDialog::regenerate_button_settings);
}

void EditControlsDialog::handle_control_input(std::uint32_t index, double value) {
    if(value < 0.5) {
        return;
    }
    
    for(int i = 0; i < sizeof(this->settings) / sizeof(settings[0]); i++) {
        auto &s = this->device->settings[i];
        s.clear();
        for(int j = 0; j < sizeof(this->settings[0]) / sizeof(this->settings[0][0]); j++) {
            // Set it if it has focus
            auto &textbox = this->settings[i][j];
            if(textbox->hasFocus()) {
                textbox->setText(this->device->control_to_string(index).value());
                this->save_settings();
                return;
            }
        }
    }
}

void EditControlsDialog::save_settings() {
    for(int i = 0; i < sizeof(this->settings) / sizeof(settings[0]); i++) {
        auto &s = this->device->settings[i];
        s.clear();
        for(int j = 0; j < sizeof(this->settings[0]) / sizeof(this->settings[0][0]); j++) {
            auto &textbox = this->settings[i][j];
            
            // Get the text in the textbox
            QString text = textbox->text();
            
            if(!text.isEmpty()) {
                auto input_maybe = this->device->control_from_string(text);
                if(input_maybe.has_value()) {
                    s.append(*input_maybe);
                }
            }
        }
    }
    
    this->device->save_settings();
}

void EditControlsDialog::keyPressEvent(QKeyEvent *event) {
    auto *d = this->device.get();
    if(d) {
        auto *d_keyboard = dynamic_cast<InputDeviceKeyboard *>(d);
        if(d_keyboard) {
            d_keyboard->handle_key_event(event, true);
        }
    }
    
    event->ignore();
}

void EditControlsDialog::regenerate_button_settings(int) {
    delete this->table;
    
    // Get the device first
    this->device = {};
    auto all_devices = GameWindow::get_all_devices();
    for(auto &device_maybe : all_devices) {
        if(device_maybe->name() == this->device_box->currentText()) {
            this->device = std::move(device_maybe);
            connect(this->device.get(), &InputDevice::control_input, this, &EditControlsDialog::handle_control_input);
            break;
        }
    }
    
    // Add the other elements
    this->table = new QWidget(this);
    auto *table_layout = new QGridLayout(this->table);
    table_layout->setContentsMargins(0,0,0,0);
    for(int i = 0; i < sizeof(this->settings) / sizeof(settings[0]); i++) {
        table_layout->addWidget(new QLabel(InputDevice::input_type_to_string(static_cast<InputDevice::InputType>(i))), i, 0);
        for(int j = 0; j < sizeof(this->settings[0]) / sizeof(this->settings[0][0]); j++) {
            auto *l = new InputLineEdit(table, this);
            if(this->device) {
                auto &setting = this->device->settings[i];
                if(setting.size() > j) {
                    l->setText(this->device->control_to_string(setting.at(j)).value());
                }
            }
            else {
                l->setEnabled(false);
            }
            this->settings[i][j] = l;
            table_layout->addWidget(l, i, j + 1);
        }
    }
    this->table->setLayout(table_layout);
    this->layout->addWidget(this->table);
}

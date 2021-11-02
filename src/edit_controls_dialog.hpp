#ifndef EDIT_CONTROLS_DIALOG_HPP
#define EDIT_CONTROLS_DIALOG_HPP

#include <QDialog>

#include "input_device.hpp"

class GameWindow;
class QComboBox;
class QLayout;

class EditControlsDialog : public QDialog {
    Q_OBJECT
    
public:
    EditControlsDialog(GameWindow *game_window);
    ~EditControlsDialog() override;
    
private:
    std::shared_ptr<InputDevice> device;
    
    class InputLineEdit;
    
    QComboBox *device_box = nullptr;
    
    QWidget *device_picker;
    QLayout *device_picker_layout = nullptr;
    
    InputLineEdit *settings[InputDevice::Input_COUNT][4];
    void handle_control_input(std::uint32_t, double);
    void save_settings();
    
    void regenerate_device_list();
    void regenerate_button_settings(int);
    void keyPressEvent(QKeyEvent *) override;

    GameWindow *game_window;
};

#endif

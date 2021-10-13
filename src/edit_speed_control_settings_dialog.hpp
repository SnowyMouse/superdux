#ifndef EDIT_SPEED_CONTROL_SETTINGS_DIALOG_HPP
#define EDIT_SPEED_CONTROL_SETTINGS_DIALOG_HPP

#include <QDialog>

class GameWindow;
class QCheckBox;
class QLineEdit;
class QSlider;

class EditSpeedControlSettingsDialog : public QDialog {
public:
    EditSpeedControlSettingsDialog(GameWindow *window);

private:
    void perform_accept();

    GameWindow *window;

    QCheckBox *enable_rewind, *enable_turbo, *enable_slowmo;
    QLineEdit *rewind_amount, *turbo_amount, *slowmo_amount;
    QSlider *rewind_slider, *turbo_slider, *slowmo_slider;

    void update_sliders();

    void update_rewind_textbox(int);
    void update_turbo_textbox(int);
    void update_slowmo_textbox(int);
};

#endif

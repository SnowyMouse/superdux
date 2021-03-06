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
    QLineEdit *base_speed_amount, *rewind_amount, *rewind_speed_amount, *turbo_amount, *slowmo_amount, *max_cpu_multiplier_amount;
    QSlider *base_speed_slider, *rewind_slider, *rewind_speed_slider, *turbo_slider, *slowmo_slider, *max_cpu_multiplier_slider;

    void update_sliders(const QString &);

    void update_rewind_textbox(int);
    void update_rewind_speed_textbox(int);
    void update_turbo_textbox(int);
    void update_slowmo_textbox(int);
    void update_base_speed_textbox(int);
    void update_max_cpu_multiplier_textbox(int);
};

#endif

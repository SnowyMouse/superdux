#ifndef EDIT_ADVANCED_GAMEBOY_MODEL_DIALOG_HPP
#define EDIT_ADVANCED_GAMEBOY_MODEL_DIALOG_HPP

#include <QDialog>

class GameWindow;
class QLineEdit;
class QComboBox;
class QCheckBox;

class EditAdvancedGameBoyModelDialog : public QDialog {
    Q_OBJECT

public:
    EditAdvancedGameBoyModelDialog(GameWindow *window);

private:
    void perform_accept();
    GameWindow *window;

    QLineEdit *gb_boot_rom_le, *gbc_boot_rom_le, *gba_boot_rom_le, *sgb_boot_rom_le, *sgb2_boot_rom_le;
    QComboBox *gb_rev, *gbc_rev, *gba_rev, *sgb_rev, *sgb2_rev;
    QCheckBox *gbc_fast_cb, *sgb_fast_cb, *sgb2_fast_cb;
    QCheckBox *gb_border_cb, *gbc_border_cb, *gba_border_cb, *sgb_border_cb, *sgb2_border_cb;
    QCheckBox *gb_allow_external_boot_rom, *gbc_allow_external_boot_rom, *gba_allow_external_boot_rom, *sgb_allow_external_boot_rom, *sgb2_allow_external_boot_rom;

private slots:
    void find_gb_boot_rom();
    void find_gbc_boot_rom();
    void find_gba_boot_rom();
    void find_sgb_boot_rom();
    void find_sgb2_boot_rom();
};

#endif

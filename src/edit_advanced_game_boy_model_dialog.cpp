#include "edit_advanced_game_boy_model_dialog.hpp"
#include "game_window.hpp"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QLabel>
#include <QPushButton>
#include <QCheckBox>
#include <QComboBox>
#include <QMessageBox>
#include <filesystem>
#include <QFileDialog>

EditAdvancedGameBoyModelDialog::EditAdvancedGameBoyModelDialog(GameWindow *window) : window(window) {
    this->setWindowTitle("Advanced Game Boy Model Settings");

    auto *dialog = this;
    auto *layout = new QVBoxLayout(this);
    layout->setSizeConstraint(QLayout::SetFixedSize);

    auto *tab_widget = new QTabWidget(this);

    auto add_tab = [&tab_widget, &dialog] (
            const char *title,

            QLineEdit **boot_rom_path,
            const std::optional<std::filesystem::path> &boot_rom_path_path,

            QComboBox **revision,
            const std::vector<std::pair<const char *, GB_model_t>> &revisions,
            GB_model_t revision_v,

            auto find_rom_fn,

            const std::vector<std::pair<QString, std::vector<QWidget *>>> &additional_controls = {}
        ) -> QVBoxLayout * {
        auto *w = new QWidget(tab_widget);
        auto *layout = new QVBoxLayout(w);

        // Boot ROM
        auto *boot_rom_widget = new QWidget(w);
        auto *boot_rom_layout = new QHBoxLayout(boot_rom_widget);
        boot_rom_layout->setContentsMargins(0,0,0,0);
        auto *boot_rom_label = new QLabel("Boot ROM path:", boot_rom_widget);
        boot_rom_layout->addWidget(boot_rom_label);
        *boot_rom_path = new QLineEdit(boot_rom_widget);

        if(boot_rom_path_path.has_value()) {
            (*boot_rom_path)->setText((*boot_rom_path_path).string().c_str());
        }

        (*boot_rom_path)->setPlaceholderText("Use built-in boot ROM");
        (*boot_rom_path)->setMinimumWidth(400);
        boot_rom_layout->addWidget(*boot_rom_path);
        auto *pb = new QPushButton("Find...", boot_rom_widget);
        connect(pb, &QPushButton::clicked, dialog, find_rom_fn);
        boot_rom_layout->addWidget(pb);
        layout->addWidget(boot_rom_widget);

        // Use this->window for sizing other labels so it looks nice
        int label_width = boot_rom_label->sizeHint().width();

        // Revisions
        auto *revisions_widget = new QWidget(w);
        auto *revisions_layout = new QHBoxLayout(revisions_widget);
        revisions_layout->setContentsMargins(0,0,0,0);
        auto *revisions_label = new QLabel("Revision:", revisions_widget);
        revisions_label->setFixedWidth(label_width);
        revisions_layout->addWidget(revisions_label);
        (*revision) = new QComboBox(w);
        revisions_layout->addWidget((*revision));
        auto rev_count = revisions.size();
        std::size_t rev_index = 0;
        for(std::size_t i = 0; i < rev_count; i++) {
            auto &r = revisions[i];
            (*revision)->addItem(r.first, r.second);
            if(r.second == revision_v) {
                rev_index = i;
            }
        }
        (*revision)->setCurrentIndex(static_cast<int>(rev_index)); // select current revision
        (*revision)->setEnabled(revisions.size() > 1); // if there is only one option, then you can't change it obviously
        revisions_widget->setLayout(revisions_layout);
        layout->addWidget(revisions_widget);

        for(auto &i : additional_controls) {
            auto *c_widget = new QWidget(w);
            c_widget->setFixedHeight(revisions_widget->sizeHint().height());
            auto *c_widget_layout = new QHBoxLayout(c_widget);
            c_widget_layout->setContentsMargins(0,0,0,0);
            auto *c_widget_label = new QLabel(i.first, c_widget);
            c_widget_label->setMinimumWidth(label_width);
            c_widget_layout->addWidget(c_widget_label);

            for(auto &j : i.second) {
                c_widget_layout->addWidget(j);
            }

            c_widget_layout->addStretch(1);
            c_widget->setLayout(c_widget_layout);

            layout->addWidget(c_widget);
        }

        layout->addStretch(1);
        w->setLayout(layout);
        tab_widget->addTab(w, title);
        return layout;
    };

    // Add each model
    add_tab("Game Boy", &this->gb_boot_rom_le, this->window->gb_boot_rom_path, &gb_rev, {
                {"DMG_B", GB_model_t::GB_MODEL_DMG_B}
            }, this->window->gb_rev, &EditAdvancedGameBoyModelDialog::find_gb_boot_rom);


    add_tab("Game Boy Color", &this->gbc_boot_rom_le, this->window->gbc_boot_rom_path, &this->gbc_rev, {
                {"CGB_C", GB_model_t::GB_MODEL_CGB_C},
                {"CGB_E", GB_model_t::GB_MODEL_CGB_E}
            }, this->window->gbc_rev, &EditAdvancedGameBoyModelDialog::find_gbc_boot_rom, {
                { "Skip intro:", {this->gbc_fast_cb = new QCheckBox(), new QLabel("(overrides boot ROM)")} }
            });
    this->gbc_fast_cb->setChecked(window->gbc_fast_boot_rom);

    add_tab("Game Boy Advance", &this->gba_boot_rom_le, this->window->gba_boot_rom_path, &this->gba_rev, {
                {"AGB", GB_model_t::GB_MODEL_AGB}
            }, this->window->gba_rev, &EditAdvancedGameBoyModelDialog::find_gba_boot_rom);

    add_tab("Super Game Boy", &this->sgb_boot_rom_le, this->window->sgb_boot_rom_path, &this->sgb_rev, {
                {"NTSC", GB_model_t::GB_MODEL_SGB_NTSC},
                {"PAL", GB_model_t::GB_MODEL_SGB_PAL}
            }, this->window->sgb_rev, &EditAdvancedGameBoyModelDialog::find_sgb_boot_rom, {
                { "Crop border:", {this->sgb_crop_border_cb = new QCheckBox()} }
            });
    add_tab("Super Game Boy 2", &this->sgb2_boot_rom_le, this->window->sgb2_boot_rom_path, &this->sgb2_rev, {
                {"SGB2", GB_model_t::GB_MODEL_SGB2}
            }, this->window->sgb2_rev, &EditAdvancedGameBoyModelDialog::find_sgb2_boot_rom, {
                { "Crop border:", {this->sgb2_crop_border_cb = new QCheckBox()} }
            });

    this->sgb_crop_border_cb->setChecked(window->sgb_crop_border);
    this->sgb2_crop_border_cb->setChecked(window->sgb2_crop_border);

    layout->addWidget(tab_widget);

    // OK button
    auto *ok = new QWidget(this);
    auto *ok_layout = new QHBoxLayout(ok);
    ok_layout->setContentsMargins(0,0,0,0);
    ok_layout->addWidget(new QWidget(ok));
    auto *ok_button = new QPushButton("OK", ok);
    ok_button->setSizePolicy(QSizePolicy::Policy::Maximum, QSizePolicy::Policy::Maximum);
    ok_layout->addWidget(ok_button);
    layout->addWidget(ok);
    connect(ok_button, &QPushButton::clicked, this, &EditAdvancedGameBoyModelDialog::perform_accept);

    this->setLayout(layout);
}

void EditAdvancedGameBoyModelDialog::perform_accept() {
    // Check if we need to reset?
    bool requires_reset = false;

    auto current_revision = this->window->model_for_type(this->window->gb_type);
    auto current_boot_rom = this->window->boot_rom_for_type(this->window->gb_type).value_or(std::filesystem::path()).string();
    auto current_fast_boot_rom = this->window->use_fast_boot_rom_for_type(this->window->gb_type);

    if(this->window->instance->is_rom_loaded()) {
        switch(this->window->gb_type) {
            case GameWindow::GameBoyType::GameBoyGB:
                requires_reset = (current_boot_rom != this->gb_boot_rom_le->text().toStdString()) || (current_revision != this->gb_rev->currentData().toInt());
                break;
            case GameWindow::GameBoyType::GameBoyGBC:
                requires_reset = (current_revision != this->gbc_rev->currentData().toInt()) || (                  // if we change revisions, yes we should reset always BUT...
                    (current_fast_boot_rom != this->gbc_fast_cb->isChecked()) ||                                  // if not, then we should only reset if the fast rom setting is changed
                    (!current_fast_boot_rom && (current_boot_rom != this->gbc_boot_rom_le->text().toStdString())) // OR if we aren't using a fast ROM, then if the boot ROM was changed
                );
                break;
            case GameWindow::GameBoyType::GameBoyGBA:
                requires_reset = (current_boot_rom != this->gba_boot_rom_le->text().toStdString()) || (current_revision != this->gba_rev->currentData().toInt());
                break;
            case GameWindow::GameBoyType::GameBoySGB:
                requires_reset = (current_boot_rom != this->sgb_boot_rom_le->text().toStdString()) || (current_revision != this->sgb_rev->currentData().toInt());
                break;
            case GameWindow::GameBoyType::GameBoySGB2:
                requires_reset = (current_boot_rom != this->sgb2_boot_rom_le->text().toStdString()) || (current_revision != this->sgb2_rev->currentData().toInt());
                break;
            default:
                requires_reset = false;
        }
    }

    // If we require reset and the user says "no", disregard everything
    if(requires_reset) {
        QMessageBox qmb(QMessageBox::Icon::Question, "Are You Sure?", "You have a game currently running.\n\nThese changes will require resetting the emulator.\n\nUnsaved data will be lost.", QMessageBox::Cancel | QMessageBox::Ok);
        if(qmb.exec() == QMessageBox::Cancel) {
            return; // cancelled
        }
    }

    // Set our settings
    this->window->gb_rev = static_cast<GB_model_t>(this->gb_rev->currentData().toInt());
    this->window->gbc_rev = static_cast<GB_model_t>(this->gbc_rev->currentData().toInt());
    this->window->gba_rev = static_cast<GB_model_t>(this->gba_rev->currentData().toInt());
    this->window->sgb_rev = static_cast<GB_model_t>(this->sgb_rev->currentData().toInt());
    this->window->sgb2_rev = static_cast<GB_model_t>(this->sgb2_rev->currentData().toInt());
    this->window->gbc_fast_boot_rom = this->gbc_fast_cb->isChecked();
    this->window->sgb_crop_border = this->sgb_crop_border_cb->isChecked();
    this->window->sgb2_crop_border = this->sgb2_crop_border_cb->isChecked();

    // If empty, set to nullopt
    auto set_possible_path = [](std::optional<std::filesystem::path> &path, const QString &str) {
        if(str.isEmpty()) {
            path = std::nullopt;
        }
        else {
            path = str.toStdString();
        }
    };
    set_possible_path(this->window->gb_boot_rom_path, this->gb_boot_rom_le->text());
    set_possible_path(this->window->gbc_boot_rom_path, this->gbc_boot_rom_le->text());
    set_possible_path(this->window->gba_boot_rom_path, this->gba_boot_rom_le->text());
    set_possible_path(this->window->sgb_boot_rom_path, this->sgb_boot_rom_le->text());
    set_possible_path(this->window->sgb2_boot_rom_path, this->sgb2_boot_rom_le->text());

    // If resetting, set the model, boot ROM path, etc.
    if(requires_reset) {
        this->window->instance->set_boot_rom_path(this->window->boot_rom_for_type(this->window->gb_type));
        this->window->instance->set_use_fast_boot_rom(this->window->use_fast_boot_rom_for_type(this->window->gb_type));
        this->window->instance->set_model(this->window->model_for_type(this->window->gb_type));
    }

    // Reset scaling
    this->window->set_pixel_view_scaling(this->window->scaling);

    this->accept();
}

static void find_boot_rom(QLineEdit *le) {
    QFileDialog qfd;
    if(qfd.exec() == QDialog::DialogCode::Accepted) {
        le->setText(qfd.selectedFiles().at(0));
    }
}

void EditAdvancedGameBoyModelDialog::find_gb_boot_rom() {
    find_boot_rom(this->gb_boot_rom_le);
}
void EditAdvancedGameBoyModelDialog::find_gbc_boot_rom() {
    find_boot_rom(this->gbc_boot_rom_le);
}
void EditAdvancedGameBoyModelDialog::find_gba_boot_rom() {
    find_boot_rom(this->gba_boot_rom_le);
}
void EditAdvancedGameBoyModelDialog::find_sgb_boot_rom() {
    find_boot_rom(this->sgb_boot_rom_le);
}
void EditAdvancedGameBoyModelDialog::find_sgb2_boot_rom() {
    find_boot_rom(this->sgb2_boot_rom_le);
}

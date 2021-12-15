#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QComboBox>
#include <QScrollBar>
#include <QFileDialog>
#include <QPushButton>
#include <QClipboard>
#include <QGuiApplication>
#include "printer.hpp"
#include "game_window.hpp"

static constexpr const unsigned int scale = 4;

Printer::Printer(GameWindow *window) : QMainWindow(window), game_window(window) {
    this->setWindowTitle("Printer");

    auto *central_widget = new QWidget(this);
    auto *layout = new QVBoxLayout(central_widget);

    this->printed_scene_outer_widget = new QWidget(central_widget);
    this->printed_scene_outer_widget_layout = new QVBoxLayout(this->printed_scene_outer_widget);
    this->printed_scene_outer_widget_layout->setContentsMargins(0,0,0,0);
    this->printed_scene_outer_widget->setLayout(this->printed_scene_outer_widget_layout);
    layout->addWidget(this->printed_scene_outer_widget);

    this->setCentralWidget(central_widget);

    auto *button_widget = new QWidget(central_widget);
    auto *blayout = new QHBoxLayout(button_widget);
    blayout->setContentsMargins(0,0,0,0);
    button_widget->setLayout(blayout);

    this->connect_button = new QPushButton(button_widget);
    this->reset_connect_button(false);
    blayout->addWidget(this->connect_button);
    connect(this->connect_button, &QPushButton::clicked, this, &Printer::connect_printer);

    this->clipboard_button = new QPushButton("Copy to Clipboard", button_widget);
    blayout->addWidget(this->clipboard_button);
    connect(this->clipboard_button, &QPushButton::clicked, this, &Printer::clipboard);

    this->save_button = new QPushButton("Save as PNG...", button_widget);
    blayout->addWidget(this->save_button);
    connect(this->save_button, &QPushButton::clicked, this, &Printer::save);

    this->clear_button = new QPushButton("Clear", button_widget);
    blayout->addWidget(this->clear_button);
    connect(this->clear_button, &QPushButton::clicked, this, &Printer::clear);

    layout->addWidget(button_widget);

    // Regenerate view widget
    this->clear();

    this->setFixedWidth(this->sizeHint().width());
}

Printer::~Printer() {}

void Printer::connect_printer() {
    bool dropping_connection = this->connected;

    // Disconnect the serial connection
    this->game_window->disconnect_serial();

    // If we were connected before, run this function and then disconnect
    if(dropping_connection) {
        this->force_disconnect_printer();
        return;
    }

    this->game_window->get_instance().connect_printer();
    this->connected = true;
    this->reset_connect_button(true);
}

void Printer::reset_connect_button(bool connected) {
    this->connect_button->setText(connected ? "Disconnect Printer" : "Connect Printer");
}

void Printer::refresh_view() {
    if(!this->connected || !this->isVisible()) {
        return;
    }

    // Do we have printed data?
    std::size_t height;
    auto new_data = this->game_window->get_instance().pop_printed_image(height);

    if(!new_data.has_value()) {
        return;
    }

    this->printed_height += height;
    this->printed_data.insert(this->printed_data.end(), new_data->begin(), new_data->end());

    this->printed_pixmap->setPixmap(QPixmap::fromImage(QImage(reinterpret_cast<uchar *>(this->printed_data.data()), GameInstance::GB_PRINTER_WIDTH, this->printed_height, QImage::Format::Format_ARGB32)));
    this->save_button->setEnabled(true);
    this->clipboard_button->setEnabled(true);
    this->clear_button->setEnabled(true);
}

void Printer::force_disconnect_printer() {
    this->connected = false;
    this->reset_connect_button(false);
}

void Printer::clipboard() {
    QGuiApplication::clipboard()->setImage(QImage(reinterpret_cast<uchar *>(this->printed_data.data()), GameInstance::GB_PRINTER_WIDTH, this->printed_height, QImage::Format::Format_ARGB32));
}

void Printer::save() {
    // Make a temporary copy of the feed in case it gets updated while the user is choosing a file
    auto printed_data_copy = this->printed_data;
    auto printed_height_copy = this->printed_height;
    QImage printed_data_output(reinterpret_cast<uchar *>(printed_data_copy.data()), GameInstance::GB_PRINTER_WIDTH, printed_height_copy, QImage::Format::Format_ARGB32);

    // Ask the user where to save
    QFileDialog qfd;
    qfd.setWindowTitle("Save to PNG");
    qfd.setAcceptMode(QFileDialog::AcceptMode::AcceptSave);
    qfd.setFileMode(QFileDialog::FileMode::AnyFile);
    qfd.setNameFilters(QStringList { "Portable Network Graphics Image (*.png)" });
    if(qfd.exec() != QFileDialog::Accepted) {
        return;
    }

    // Save
    printed_data_output.save(qfd.selectedFiles()[0], "PNG");
}

void Printer::clear() {
    // Regenerate the view widget
    delete this->printed_view;
    this->printed_view = new QGraphicsView(this->printed_scene_outer_widget);
    this->printed_scene = new QGraphicsScene(this->printed_view);
    this->printed_view->setFrameShape(QFrame::Shape::NoFrame);
    this->printed_view->setScene(this->printed_scene);
    this->printed_scene_outer_widget_layout->addWidget(this->printed_view);
    this->printed_pixmap = this->printed_scene->addPixmap(QPixmap());
    this->printed_pixmap->setTransform(QTransform::fromScale(scale, scale));
    this->printed_view->setFixedWidth(GameInstance::GB_PRINTER_WIDTH * scale + this->printed_view->verticalScrollBar()->sizeHint().width());
    this->printed_view->setHorizontalScrollBarPolicy(Qt::ScrollBarPolicy::ScrollBarAlwaysOff);
    this->printed_view->setAlignment(Qt::AlignmentFlag::AlignTop | Qt::AlignmentFlag::AlignLeft);
    this->printed_view->setMinimumHeight(150 * scale);

    // Clear our feed
    this->printed_height = 0;
    this->printed_data.clear();

    // Save and clear is no longer relevant
    this->save_button->setEnabled(false);
    this->clipboard_button->setEnabled(false);
    this->clear_button->setEnabled(false);
}


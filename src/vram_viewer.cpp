#include "vram_viewer.hpp"

#include <QGraphicsScene>
#include <QGraphicsPixmapItem>
#include <QGraphicsView>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QCheckBox>

#include "game_window.hpp"

VRAMViewer::VRAMViewer(GameWindow *window) : window(window),
    gb_tileset_image(reinterpret_cast<uchar *>(this->gb_tileset_image_data), GameInstance::GB_TILESET_WIDTH, GameInstance::GB_TILESET_HEIGHT, QImage::Format::Format_ARGB32),
    gb_tilemap_image(reinterpret_cast<uchar *>(this->gb_tilemap_image_data), GameInstance::GB_TILEMAP_WIDTH, GameInstance::GB_TILEMAP_HEIGHT, QImage::Format::Format_ARGB32) {

    // Set up our viewer
    this->setWindowTitle("VRAM Viewer");

    auto *central_w = new QWidget(this);
    this->setCentralWidget(central_w);
    auto *layout = new QHBoxLayout(central_w);
    central_w->setLayout(layout);


    // First the tilemap
    auto *gb_tilemap_view_frame = new QGroupBox(central_w);
    gb_tilemap_view_frame->setTitle("Tilemap");
    auto *gb_tilemap_view_frame_layout = new QVBoxLayout(gb_tilemap_view_frame);
    gb_tilemap_view_frame->setLayout(gb_tilemap_view_frame_layout);

    this->gb_tilemap_view = new QGraphicsView(gb_tilemap_view_frame);
    this->gb_tilemap_scene = new QGraphicsScene(this->gb_tilemap_view);
    this->gb_tilemap_pixmap = this->gb_tilemap_scene->addPixmap(QPixmap::fromImage(this->gb_tilemap_image));
    this->gb_tilemap_pixmap->setTransform(QTransform::fromScale(2, 2));
    this->gb_tilemap_view->setScene(this->gb_tilemap_scene);
    gb_tilemap_view_frame_layout->addWidget(this->gb_tilemap_view);
    this->gb_tilemap_show_viewport_box = new QCheckBox("Show Viewport", gb_tilemap_view_frame);
    this->gb_tilemap_show_viewport_box->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    gb_tilemap_view_frame_layout->addWidget(this->gb_tilemap_show_viewport_box);
    this->gb_tilemap_show_viewport_box->setChecked(true);
    connect(this->gb_tilemap_show_viewport_box, &QCheckBox::clicked, this, &VRAMViewer::redraw_tilemap);

    layout->addWidget(gb_tilemap_view_frame);
}

VRAMViewer::~VRAMViewer() {}

void VRAMViewer::refresh_view() {
    if(this->isHidden()) {
        return;
    }

    this->redraw_tilemap();
}

void VRAMViewer::redraw_tilemap() noexcept {

    auto &instance = this->window->get_instance();
    instance.draw_tilemap(this->gb_tilemap_image_data);

    // Show the viewport?
    if(this->gb_tilemap_show_viewport_box->isChecked()) {
        std::uint8_t tm_x = instance.read_memory(0xFF43); // SCX
        std::uint8_t tm_y = instance.read_memory(0xFF42); // SCY

        std::size_t top_y = (tm_y - 1) % GameInstance::GB_TILEMAP_HEIGHT;
        std::size_t bottom_y = (tm_y + 144) % GameInstance::GB_TILEMAP_HEIGHT;

        std::size_t left_x = (tm_x - 1) % GameInstance::GB_TILEMAP_WIDTH;
        std::size_t right_x = (tm_x + 160) % GameInstance::GB_TILEMAP_WIDTH;

        auto *tilemap_data = this->gb_tilemap_image_data;

        auto invert_color = [&tilemap_data, &left_x, &right_x, &top_y, &bottom_y](std::uint8_t x, std::uint8_t y) {
            auto &color = tilemap_data[x + y * GameInstance::GB_TILEMAP_WIDTH];

            if(x == GameInstance::GB_TILEMAP_WIDTH - 1 && x == left_x) {
                return;
            }

            if(x == 0 && x == right_x) {
                return;
            }

            if(y == GameInstance::GB_TILEMAP_HEIGHT - 1 && y == top_y) {
                return;
            }

            if(y == 0 && y == bottom_y) {
                return;
            }

            color = color ^ 0xFFFFFF;
        };

        // Draw the sides
        for(std::uint8_t x = left_x; x != (right_x + 1) % GameInstance::GB_TILEMAP_HEIGHT; x++) {
            invert_color(x, top_y);
            invert_color(x, bottom_y);
        }

        for(std::uint8_t y = top_y; y != (bottom_y + 1) % GameInstance::GB_TILEMAP_WIDTH; y++) {
            invert_color(left_x, y);
            invert_color(right_x, y);
        }

        // Invert the colors since they were double inverted by the above for() loops
        invert_color(left_x, top_y);
        invert_color(right_x, top_y);
        invert_color(left_x, bottom_y);
        invert_color(right_x, bottom_y);
    }

    // Draw it
    this->gb_tilemap_pixmap->setPixmap(QPixmap::fromImage(this->gb_tilemap_image));
}

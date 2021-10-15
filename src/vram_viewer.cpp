#include "vram_viewer.hpp"

#include <QGraphicsScene>
#include <QGraphicsPixmapItem>
#include <QGraphicsView>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QCheckBox>
#include <QComboBox>
#include <QSpinBox>
#include <QLabel>

#include "game_window.hpp"

VRAMViewer::VRAMViewer(GameWindow *window) : window(window),
    gb_tilemap_image(reinterpret_cast<uchar *>(this->gb_tilemap_image_data), GameInstance::GB_TILEMAP_WIDTH, GameInstance::GB_TILEMAP_HEIGHT, QImage::Format::Format_ARGB32),
    gb_tileset_image(reinterpret_cast<uchar *>(this->gb_tileset_image_data), GameInstance::GB_TILESET_WIDTH, GameInstance::GB_TILESET_HEIGHT, QImage::Format::Format_ARGB32) {

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
    this->gb_tilemap_view->setDisabled(true);
    gb_tilemap_view_frame_layout->addWidget(this->gb_tilemap_view);

    std::pair<const char *, GB_tileset_type_t> tilesets[] = {
        {"Auto", GB_tileset_type_t::GB_TILESET_AUTO},
        {"$8000", GB_tileset_type_t::GB_TILESET_8000},
        {"$8800", GB_tileset_type_t::GB_TILESET_8800}
    };


    std::pair<const char *, GB_map_type_t> maps[] = {
        {"Auto", GB_map_type_t::GB_MAP_AUTO},
        {"$9800", GB_map_type_t::GB_MAP_9800},
        {"$9c00", GB_map_type_t::GB_MAP_9C00}
    };

    auto *tilemap_options = new QWidget(gb_tilemap_view_frame);
    auto *tilemap_options_layout = new QHBoxLayout(tilemap_options);

    // Map option
    auto *map_w = new QWidget(tilemap_options);
    auto *map_w_layout = new QHBoxLayout(map_w);
    map_w_layout->setContentsMargins(0,0,0,0);
    map_w_layout->addWidget(new QLabel("Map:", map_w));
    this->tilemap_map_type = new QComboBox(map_w);
    for(auto &m : maps) {
        this->tilemap_map_type->addItem(m.first, m.second);
    }
    map_w_layout->addWidget(this->tilemap_map_type);
    map_w->setLayout(map_w_layout);
    map_w->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

    // Tileset
    auto *tileset_w = new QWidget(tilemap_options);
    auto *tileset_w_layout = new QHBoxLayout(tileset_w);
    tileset_w_layout->setContentsMargins(0,0,0,0);
    tileset_w_layout->addWidget(new QLabel("Tileset:", tileset_w));
    this->tilemap_tileset_type = new QComboBox(tileset_w);
    for(auto &t : tilesets) {
        this->tilemap_tileset_type->addItem(t.first, t.second);
    }
    tileset_w_layout->addWidget(this->tilemap_tileset_type);
    tileset_w->setLayout(tileset_w_layout);
    tileset_w->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

    this->gb_tilemap_show_viewport_box = new QCheckBox("Show Viewport", gb_tilemap_view_frame);
    this->gb_tilemap_show_viewport_box->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    this->gb_tilemap_show_viewport_box->setChecked(true);
    connect(this->gb_tilemap_show_viewport_box, &QCheckBox::clicked, this, &VRAMViewer::redraw_tilemap);
    this->gb_tilemap_view->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

    tilemap_options_layout->addWidget(map_w);
    tilemap_options_layout->addWidget(tileset_w);
    connect(this->tilemap_map_type, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this, &VRAMViewer::redraw_tilemap);
    connect(this->tilemap_tileset_type, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this, &VRAMViewer::redraw_tilemap);
    tilemap_options_layout->addWidget(this->gb_tilemap_show_viewport_box);
    tilemap_options_layout->setContentsMargins(0,0,0,0);
    gb_tilemap_view_frame_layout->addWidget(tilemap_options);

    layout->addWidget(gb_tilemap_view_frame);

    // Next, tilesets
    auto *gb_tileset_view_frame = new QGroupBox(central_w);
    gb_tileset_view_frame->setTitle("Tileset");
    auto *gb_tileset_view_frame_layout = new QVBoxLayout(gb_tileset_view_frame);
    gb_tileset_view_frame->setLayout(gb_tileset_view_frame_layout);
    this->gb_tileset_view = new QGraphicsView(gb_tileset_view_frame);
    this->gb_tileset_scene = new QGraphicsScene(this->gb_tileset_view);
    this->gb_tileset_pixmap = this->gb_tileset_scene->addPixmap(QPixmap::fromImage(this->gb_tileset_image));
    this->gb_tileset_pixmap->setTransform(QTransform::fromScale(2, 2));
    this->gb_tileset_view->setScene(this->gb_tileset_scene);
    this->gb_tileset_view->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    this->gb_tileset_view->setDisabled(true);
    gb_tileset_view_frame_layout->addWidget(this->gb_tileset_view);
    gb_tileset_view_frame_layout->addStretch(1);

    auto *palette_selector = new QWidget(gb_tileset_view_frame);
    auto *palette_selector_layout = new QHBoxLayout(palette_selector);
    palette_selector_layout->setContentsMargins(0,0,0,0);

    auto *palette_label = new QLabel("Palette:");
    palette_label->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    palette_selector_layout->addWidget(palette_label);

    this->tileset_palette_type = new QComboBox(central_w);
    std::pair<const char *, GB_palette_type_t> palettes[] = {
        {"None", GB_palette_type_t::GB_PALETTE_NONE},
        {"Background", GB_palette_type_t::GB_PALETTE_BACKGROUND},
        {"OAM", GB_palette_type_t::GB_PALETTE_OAM}
    };
    for(auto &i : palettes) {
        this->tileset_palette_type->addItem(i.first, i.second);
    }
    palette_selector_layout->addWidget(this->tileset_palette_type);

    this->tileset_palette_index = new QSpinBox(central_w);
    this->tileset_palette_index->setMinimum(0);
    this->tileset_palette_index->setMaximum(7);
    palette_selector_layout->addWidget(this->tileset_palette_index);
    this->tileset_palette_index->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    connect(this->tileset_palette_index, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), this, &VRAMViewer::redraw_tileset);
    palette_selector->setLayout(palette_selector_layout);

    // Align heights
    tilemap_options->setMinimumHeight(palette_selector->sizeHint().height());
    palette_selector->setMinimumHeight(tilemap_options->sizeHint().height());

    gb_tileset_view_frame_layout->addWidget(palette_selector);

    layout->addWidget(gb_tileset_view_frame);
    this->setFixedSize(this->sizeHint());
}

VRAMViewer::~VRAMViewer() {}

void VRAMViewer::refresh_view() {
    if(this->isHidden()) {
        return;
    }

    this->redraw_tilemap();
    this->redraw_tileset();
}

void VRAMViewer::redraw_tilemap() noexcept {
    auto &instance = this->window->get_instance();
    instance.draw_tilemap(this->gb_tilemap_image_data, static_cast<GB_map_type_t>(this->tilemap_map_type->currentData().toInt()), static_cast<GB_tileset_type_t>(this->tilemap_tileset_type->currentData().toInt()));

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

void VRAMViewer::redraw_tileset() noexcept {
    auto &instance = this->window->get_instance();
    instance.draw_tileset(this->gb_tileset_image_data, static_cast<GB_palette_type_t>(this->tileset_palette_type->currentData().toInt()), this->tileset_palette_index->value());
    this->gb_tileset_pixmap->setPixmap(QPixmap::fromImage(this->gb_tileset_image));
}

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
#include <QMouseEvent>
#include <QFontDatabase>

#include "game_window.hpp"

class TilesetView : public QGraphicsView {
public:
    TilesetView(QWidget *parent, VRAMViewer *window) : QGraphicsView(parent), window(window) {
        this->setMouseTracking(true);
    }
    ~TilesetView() {}

    void mousePressEvent(QMouseEvent *event) override {
        this->handle_tile_selection(event, event->buttons() & Qt::MouseButton::LeftButton);
    }

    void mouseMoveEvent(QMouseEvent *event) override {
        this->handle_tile_selection(event, false);
    }

    void handle_tile_selection(QMouseEvent *event, bool click) {
        #if QT_VERSION >= 0x060000
        auto local_pos = event->position();
        #else
        auto local_pos = event->localPos(); // deprecated in Qt 6 (position() is not available in Qt5 however)
        #endif

        std::size_t x = local_pos.x() / 2 / GameInstance::GB_TILESET_TILE_LENGTH;
        std::size_t y = local_pos.y() / 2 / GameInstance::GB_TILESET_TILE_LENGTH;

        // Don't allow out of bounds fun
        if(x > 31 || y > 23) {
            window->show_info_for_tile(std::nullopt, false);
            return;
        }

        std::uint16_t tile = (x % GameInstance::GB_TILEMAP_WIDTH) + (y * GameInstance::GB_TILEMAP_HEIGHT / GameInstance::GB_TILESET_TILE_LENGTH);
        window->show_info_for_tile(tile, click);
    }

    void leaveEvent(QEvent *event) override {
        window->show_info_for_tile(std::nullopt, false);
    }

private:
    VRAMViewer *window;
};

VRAMViewer::VRAMViewer(GameWindow *window) : window(window),
    gb_tilemap_image(reinterpret_cast<uchar *>(this->gb_tilemap_image_data), GameInstance::GB_TILEMAP_WIDTH, GameInstance::GB_TILEMAP_HEIGHT, QImage::Format::Format_ARGB32),
    gb_tileset_image(reinterpret_cast<uchar *>(this->gb_tileset_image_data), GameInstance::GB_TILESET_WIDTH, GameInstance::GB_TILESET_HEIGHT, QImage::Format::Format_ARGB32),
    gb_tileset_grid_image(reinterpret_cast<uchar *>(this->gb_tileset_grid_data), GameInstance::GB_TILESET_WIDTH*2, GameInstance::GB_TILESET_HEIGHT*2, QImage::Format::Format_ARGB32) {

    auto table_font = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    table_font.setPixelSize(14);

    // Set up our viewer
    this->setWindowTitle("VRAM Viewer");

    auto *central_w = new QWidget(this);
    this->setCentralWidget(central_w);
    auto *layout = new QHBoxLayout(central_w);
    central_w->setLayout(layout);

    // First the tilemap
    this->gb_tab_view = new QTabWidget(central_w);

    this->gb_tilemap_view_frame = new QWidget(this->gb_tab_view);
    auto *gb_tilemap_view_frame_layout = new QVBoxLayout(this->gb_tilemap_view_frame);
    this->gb_tilemap_view_frame->setLayout(gb_tilemap_view_frame_layout);
    this->gb_tilemap_view = new QGraphicsView(this->gb_tilemap_view_frame);
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
        {"Auto - Background", GB_map_type_t::GB_MAP_AUTO},
        {"Auto - Window", GB_map_type_t::GB_MAP_AUTO},
        {"$9800", GB_map_type_t::GB_MAP_9800},
        {"$9c00", GB_map_type_t::GB_MAP_9C00}
    };

    auto *tilemap_options = new QWidget(this->gb_tilemap_view_frame);
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

    this->gb_tilemap_show_viewport_box = new QCheckBox("Show Viewport", this->gb_tilemap_view_frame);
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


    tilemap_options->setContentsMargins(0,0,0,0);
    gb_tilemap_view_frame_layout->addWidget(tilemap_options);

    this->gb_tab_view->addTab(this->gb_tilemap_view_frame, "Tilemap");

    layout->addWidget(this->gb_tab_view);


    // Next, tilesets
    auto *tileset_palette = new QWidget(central_w);
    auto *tileset_palette_layout = new QVBoxLayout(tileset_palette);
    tileset_palette_layout->setContentsMargins(0,0,0,0);
    tileset_palette->setLayout(tileset_palette_layout);

    auto *gb_tileset_view_frame = new QGroupBox(tileset_palette);
    gb_tileset_view_frame->setTitle("Tileset");
    auto *gb_tileset_view_frame_layout = new QVBoxLayout(gb_tileset_view_frame);
    gb_tileset_view_frame->setLayout(gb_tileset_view_frame_layout);
    this->gb_tileset_view = new TilesetView(gb_tileset_view_frame, this);
    this->gb_tileset_scene = new QGraphicsScene(this->gb_tileset_view);

    this->gb_tileset_pixmap = this->gb_tileset_scene->addPixmap(QPixmap::fromImage(this->gb_tileset_image));
    this->gb_tileset_pixmap->setTransform(QTransform::fromScale(2, 2));

    this->gb_tileset_grid_pixelmap = this->gb_tileset_scene->addPixmap(QPixmap::fromImage(this->gb_tileset_grid_image));


    this->gb_tileset_view->setScene(this->gb_tileset_scene);
    //this->gb_tileset_view->setDisabled(true);
    gb_tileset_view_frame_layout->addWidget(this->gb_tileset_view);

    // Tileset mouse over info
    auto *tileset_mouse_over_widget = new QWidget(gb_tileset_view_frame);
    auto *tileset_mouse_over_layout = new QGridLayout(tileset_mouse_over_widget);
    int row_count = 0;
    auto add_tileset_mouse_over_info = [&row_count, &tileset_mouse_over_widget, &tileset_mouse_over_layout, &table_font](QLabel **label) {
        *label = new QLabel("TEST", tileset_mouse_over_widget);
        (*label)->setFont(table_font);
        tileset_mouse_over_layout->addWidget(*label, row_count, 0);
        row_count++;
    };
    add_tileset_mouse_over_info(&this->moused_over_tile_address);
    add_tileset_mouse_over_info(&this->moused_over_tile_accessed_index);
    add_tileset_mouse_over_info(&this->moused_over_tile_user);
    add_tileset_mouse_over_info(&this->moused_over_tile_palette);
    int palette_row_index = row_count - 1;

    this->gb_show_tileset_grid = new QCheckBox("Show Grid", tileset_mouse_over_widget);
    this->gb_show_tileset_grid->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Ignored);
    this->gb_show_tileset_grid->setChecked(true);
    tileset_mouse_over_layout->addWidget(this->gb_show_tileset_grid, palette_row_index, 1);

    // Palette
    auto *palette_preview = new QWidget(tileset_mouse_over_widget);
    auto *palette_preview_layout = new QHBoxLayout(palette_preview);
    palette_preview_layout->setContentsMargins(0,0,0,0);
    palette_preview_layout->addWidget(this->palette_a = new QWidget(palette_preview));
    palette_preview_layout->addWidget(this->palette_b = new QWidget(palette_preview));
    palette_preview_layout->addWidget(this->palette_c = new QWidget(palette_preview));
    palette_preview_layout->addWidget(this->palette_d = new QWidget(palette_preview));
    this->palette_a->setStyleSheet("background-color: #000");
    this->palette_b->setStyleSheet("background-color: #000");
    this->palette_c->setStyleSheet("background-color: #000");
    this->palette_d->setStyleSheet("background-color: #000");

    int palette_hw = this->moused_over_tile_palette->sizeHint().height();
    this->palette_a->setFixedSize(palette_hw, palette_hw);
    this->palette_b->setFixedSize(palette_hw, palette_hw);
    this->palette_c->setFixedSize(palette_hw, palette_hw);
    this->palette_d->setFixedSize(palette_hw, palette_hw);
    palette_preview->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    tileset_mouse_over_layout->addWidget(palette_preview, palette_row_index, 2);

    tileset_mouse_over_widget->setLayout(tileset_mouse_over_layout);
    gb_tileset_view_frame_layout->addWidget(tileset_mouse_over_widget);
    tileset_palette_layout->addWidget(gb_tileset_view_frame);
    this->gb_tileset_view->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);


    // Palettes
    auto *palette_group = new QGroupBox(central_w);
    palette_group->setTitle("Tileset Palette");
    palette_group->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
    auto *palette_group_layout = new QVBoxLayout(palette_group);
    palette_group->setLayout(palette_group_layout);

    auto *palette_selector = new QWidget(palette_group);
    auto *palette_selector_layout = new QHBoxLayout(palette_selector);
    palette_selector_layout->setContentsMargins(0,0,0,0);
    palette_selector->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);

    auto *palette_label = new QLabel("Palette:", palette_selector);
    palette_label->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    palette_selector_layout->addWidget(palette_label);

    this->tileset_palette_type = new QComboBox(central_w);
    std::pair<const char *, GB_palette_type_t> palettes[] = {
        {"Auto", GB_palette_type_t::GB_PALETTE_AUTO},
        {"None", GB_palette_type_t::GB_PALETTE_NONE},
        {"Background", GB_palette_type_t::GB_PALETTE_BACKGROUND},
        {"OAM", GB_palette_type_t::GB_PALETTE_OAM}
    };
    for(auto &i : palettes) {
        this->tileset_palette_type->addItem(i.first, i.second);
    }
    palette_selector_layout->addWidget(this->tileset_palette_type);

    this->tileset_palette_index_label = new QLabel("Index:", palette_selector);
    this->tileset_palette_index_label->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    palette_selector_layout->addWidget(this->tileset_palette_index_label);

    this->tileset_palette_index = new QSpinBox(palette_selector);
    this->tileset_palette_index->setMinimum(0);
    this->tileset_palette_index->setMaximum(7);
    palette_selector_layout->addWidget(this->tileset_palette_index);
    this->tileset_palette_index->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    connect(this->tileset_palette_index, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), this, &VRAMViewer::redraw_palette);
    connect(this->tileset_palette_type, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this, &VRAMViewer::redraw_palette);
    palette_selector->setLayout(palette_selector_layout);
    palette_group_layout->addWidget(palette_selector);
    tileset_palette_layout->addWidget(palette_group);

    layout->addWidget(tileset_palette);
    this->setFixedSize(this->sizeHint());
}

VRAMViewer::~VRAMViewer() {}

void VRAMViewer::refresh_view() {
    if(this->isHidden()) {
        return;
    }

    this->redraw_palette();
}

void VRAMViewer::redraw_palette() noexcept {
    this->redraw_tilemap();
    this->redraw_tileset();
}

void VRAMViewer::redraw_tilemap() noexcept {
    auto &instance = this->window->get_instance();
    auto tilemap_index = this->tilemap_map_type->currentIndex();
    auto tilemap_type = static_cast<GB_map_type_t>(this->tilemap_map_type->currentData().toInt());

    auto lcdc = instance.read_memory(0xFF40);

    // Showing screen
    if(tilemap_index == 1) {
        tilemap_type = (lcdc & 0b1000000) ? GB_map_type_t::GB_MAP_9C00 : GB_map_type_t::GB_MAP_9800; // LCDC
    }

    instance.draw_tilemap(this->gb_tilemap_image_data, tilemap_type, static_cast<GB_tileset_type_t>(this->tilemap_tileset_type->currentData().toInt()));

    // Show the viewport?
    if(this->gb_tilemap_show_viewport_box->isChecked()) {
        std::size_t top_y, bottom_y, left_x, right_x;

        if(tilemap_index == 1) {
            auto wx = instance.read_memory(0xFF4B);
            auto wy = instance.read_memory(0xFF4A);
            top_y = GameInstance::GB_TILEMAP_HEIGHT - 1;
            left_x = GameInstance::GB_TILEMAP_WIDTH - 1;

            // Check if the window is visible
            if((lcdc & 0b100000) && wx <= 166 && wy <= 143) {
                right_x = 167 - wx;
                bottom_y = 144 - wy;
            }
            else {
                bottom_y = 0;
                right_x = 0;
            }
        }
        else {
            auto scx = instance.read_memory(0xFF43); // SCX
            auto scy = instance.read_memory(0xFF42); // SCY

            top_y = (scy - 1) % GameInstance::GB_TILEMAP_HEIGHT;
            bottom_y = (scy + 144) % GameInstance::GB_TILEMAP_HEIGHT;

            left_x = (scx - 1) % GameInstance::GB_TILEMAP_WIDTH;
            right_x = (scx + 160) % GameInstance::GB_TILEMAP_WIDTH;
        }

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
    auto type = static_cast<GB_palette_type_t>(this->tileset_palette_type->currentData().toInt());

    auto &instance = this->window->get_instance();
    instance.draw_tileset(this->gb_tileset_image_data, static_cast<GB_palette_type_t>(this->tileset_palette_type->currentData().toInt()), this->tileset_palette_index->value());
    this->gb_tileset_pixmap->setPixmap(QPixmap::fromImage(this->gb_tileset_image));
    this->tileset_object_info = instance.get_tileset_object_info();

    // update the grid
    if(this->gb_show_tileset_grid->isChecked()) {
        for(std::size_t y = GameInstance::GB_TILESET_TILE_LENGTH*2-1; y < GameInstance::GB_TILESET_HEIGHT * 2; y+=GameInstance::GB_TILESET_TILE_LENGTH*2) {
            for(std::size_t x = 0; x < GameInstance::GB_TILESET_WIDTH * 2; x++) {
                this->gb_tileset_grid_data[x + y * GameInstance::GB_TILESET_WIDTH*2] = (this->gb_tileset_image_data[x/2 + y/2 * GameInstance::GB_TILESET_WIDTH]) ^ 0xFFFFFF;
            }
        }

        for(std::size_t x = GameInstance::GB_TILESET_TILE_LENGTH*2-1; x < GameInstance::GB_TILESET_WIDTH * 2; x+=GameInstance::GB_TILESET_TILE_LENGTH*2) {
            for(std::size_t y = 0; y < GameInstance::GB_TILESET_HEIGHT * 2; y++) {
                this->gb_tileset_grid_data[x + y * GameInstance::GB_TILESET_WIDTH*2] = (this->gb_tileset_image_data[x/2 + y/2 * GameInstance::GB_TILESET_WIDTH]) ^ 0xFFFFFF;
            }
        }
        this->gb_tileset_grid_pixelmap->setPixmap(QPixmap::fromImage(this->gb_tileset_grid_image));
        this->gb_tileset_grid_pixelmap->setVisible(true);
    }
    else {
        this->gb_tileset_grid_pixelmap->setVisible(false);
    }

    // if we are mousing over a tile, show data for that tile
    const std::uint32_t *new_palette;
    if(moused_over_tile_index.has_value()) {
        auto &info = tileset_object_info.tiles[*moused_over_tile_index];

        char tile_address[256];
        std::snprintf(tile_address, sizeof(tile_address), "Tile address: $%i:%04x", info.tile_bank, info.tile_address);
        this->moused_over_tile_address->setText(tile_address);

        char tile_index[256];
        if(info.tile_index >= 0x100) {
            std::snprintf(tile_index, sizeof(tile_index), "Index: $%02x ($8800 mode)", info.tile_index - 0x100);
        }
        else if(info.tile_index >= 0x80) {
            std::snprintf(tile_index, sizeof(tile_index), "Index: $%02x ($8000 / $8800 mode)", info.tile_index);
        }
        else {
            std::snprintf(tile_index, sizeof(tile_index), "Index: $%02x ($8000 mode)", info.tile_index);
        }
        this->moused_over_tile_accessed_index->setText(tile_index);


        if(info.accessed_type) {
            char palette_text[256];

            // Find the palette
            GB_palette_type_t palette = info.accessed_type == tileset_object_info_tile_type::TILESET_INFO_OAM ? GB_palette_type_t::GB_PALETTE_OAM : GB_palette_type_t::GB_PALETTE_BACKGROUND;
            std::snprintf(palette_text, sizeof(palette_text), "Palette: %u (%s)", info.accessed_tile_palette_index, palette == GB_palette_type_t::GB_PALETTE_OAM ? "sprite" : "background");
            this->moused_over_tile_palette->setText(palette_text);

            char user_text[256];
            char user_name[256] = "???";

            switch(info.accessed_type) {
                case tileset_object_info_tile_type::TILESET_INFO_OAM:
                    std::snprintf(user_name, sizeof(user_name), "sprite #%02u ($%02x)", info.accessed_user_index, info.accessed_user_index);
                    break;
                case tileset_object_info_tile_type::TILESET_INFO_WINDOW:
                    std::snprintf(user_name, sizeof(user_name), "window");
                    break;
                case tileset_object_info_tile_type::TILESET_INFO_BACKGROUND:
                    std::snprintf(user_name, sizeof(user_name), "background");
                    break;
                default:
                    break;
            }

            std::snprintf(palette_text, sizeof(palette_text), "User: %s", user_name);
            new_palette = instance.get_palette(palette, info.accessed_tile_palette_index);
            this->moused_over_tile_user->setText(palette_text);
        }
        else {
            this->moused_over_tile_palette->setText(" ");
            this->moused_over_tile_user->setText(" ");
            new_palette = instance.get_palette(type, this->tileset_palette_index->value());
        }

    }
    else {
        new_palette = instance.get_palette(type, this->tileset_palette_index->value());
        this->moused_over_tile_address->setText(" ");
        this->moused_over_tile_accessed_index->setText(" ");
        this->moused_over_tile_palette->setText(" ");
        this->moused_over_tile_user->setText(" ");
    }

    // Is the palette different?
    if(std::memcmp(new_palette, this->current_palette, sizeof(this->current_palette)) != 0) {
        std::memcpy(this->current_palette, new_palette, sizeof(this->current_palette));

        auto format_background_color_for_palette = [](const std::uint32_t &p, QWidget *w) {
            std::uint8_t b = (p & 0xFF);
            std::uint8_t g = ((p & 0xFF00) >> 8) & 0xFF;
            std::uint8_t r = ((p & 0xFF0000) >> 16) & 0xFF;

            char stylesheet[256];
            std::snprintf(stylesheet, sizeof(stylesheet), "background-color: #%02x%02x%02x", r, g, b);
            w->setStyleSheet(stylesheet);
        };
        format_background_color_for_palette(this->current_palette[0], this->palette_a);
        format_background_color_for_palette(this->current_palette[1], this->palette_b);
        format_background_color_for_palette(this->current_palette[2], this->palette_c);
        format_background_color_for_palette(this->current_palette[3], this->palette_d);
    }

    // Gray out the index if we're on none
    this->tileset_palette_index_label->setEnabled(type != GB_palette_type_t::GB_PALETTE_NONE && type != GB_palette_type_t::GB_PALETTE_AUTO);
    this->tileset_palette_index->setEnabled(type != GB_palette_type_t::GB_PALETTE_NONE && type != GB_palette_type_t::GB_PALETTE_AUTO);
}

void VRAMViewer::show_info_for_tile(const std::optional<std::uint16_t> &tile, bool show_on_left_pane) {
    this->moused_over_tile_index = tile;
    this->redraw_tileset();

    if(show_on_left_pane && tile.has_value()) {
        auto &t = this->tileset_object_info.tiles[*tile];
        switch(static_cast<tileset_object_info_tile_type>(t.accessed_type)) {
            case tileset_object_info_tile_type::TILESET_INFO_BACKGROUND:
                this->tilemap_map_type->setCurrentIndex(0);
                this->gb_tab_view->setCurrentWidget(this->gb_tilemap_view_frame);
                break;
            case tileset_object_info_tile_type::TILESET_INFO_WINDOW:
                this->tilemap_map_type->setCurrentIndex(1);
                this->gb_tab_view->setCurrentWidget(this->gb_tilemap_view_frame);
                break;
            case tileset_object_info_tile_type::TILESET_INFO_OAM:
                std::printf("Unimplemented! This should switch to the OAM tab.\n");
                break;
            case tileset_object_info_tile_type::TILESET_INFO_NONE:
                break;
        }
    }
}

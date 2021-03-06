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
#include <QScrollBar>
#include <QScrollArea>

#include "settings.hpp"
#include "game_window.hpp"

#define SETTING_SHOW_GRID "vram_show_grid"
#define SETTING_SHOW_VIEWPORT "vram_show_viewport"

class VRAMViewer::PaletteViewData::PaletteViewDataWidget : public QWidget {
public:
    PaletteViewDataWidget(QWidget *parent, VRAMViewer *window, GB_palette_type_t palette, std::size_t index) : QWidget(parent), window(window), palette(palette), index(index) {
        this->setMouseTracking(true);
        this->setAutoFillBackground(true);
    }

    ~PaletteViewDataWidget() {}
private:
    VRAMViewer *window;
    GB_palette_type_t palette;
    std::size_t index;

    void mouseMoveEvent(QMouseEvent *event) override {
        this->window->show_info_for_palette(this->palette, this->index);
    }

    void leaveEvent(QEvent *event) override {
        this->window->show_info_for_palette(std::nullopt);
    }
};

static inline constexpr std::uint32_t grid_pixel(std::uint32_t color) noexcept {
    auto alpha = color & 0xFF000000;

    constexpr const std::uint8_t RED_WEIGHT = 54;
    constexpr const std::uint8_t GREEN_WEIGHT = 182;
    constexpr const std::uint8_t BLUE_WEIGHT = 19;
    static_assert(RED_WEIGHT + GREEN_WEIGHT + BLUE_WEIGHT == UINT8_MAX, "red + green + blue weights (grayscale) must equal 255");

    auto b = (color & 0xFF) >> 0;
    auto g = (color & 0xFF00) >> 8;
    auto r = (color & 0xFF0000) >> 16;

    std::uint32_t l = (r * RED_WEIGHT + g * GREEN_WEIGHT + b * BLUE_WEIGHT) / (RED_WEIGHT + GREEN_WEIGHT + BLUE_WEIGHT); // convert to luminosity with luma

    // Increase contrast
    if(l < 127) {
        l += 64 * (127 - l) / 127;
        l = l * 4 / 3;
    }
    else {
        l = std::min(l * 3 / 5, static_cast<decltype(l)>(255));
    }

    return alpha | l | (l << 8) | (l << 16);
}

void format_label(QLabel *label, const GameInstance::ObjectAttributeInfoObject &object, std::size_t index) {
    char str[64];
    std::snprintf(str, sizeof(str), "%02zu ($%02zx)\nXY: $%02x,%02x\nT#: $%02x:%02x\nFL: %s%s", index, index, object.x, object.y, object.tileset_bank, object.tile, object.flip_x ? "X" : "_", object.flip_y ? "Y" : "_");
    label->setText(str);
}

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
        this->window->show_info_for_tile(tile, click);
    }

    void leaveEvent(QEvent *event) override {
        this->window->show_info_for_tile(std::nullopt, false);
    }

private:
    VRAMViewer *window;
};

void VRAMViewer::update_palette(PaletteViewData &palette, GB_palette_type_t type, std::size_t index, const std::uint16_t *raw_colors) {
    auto *new_palette = this->window->get_instance().get_palette(type, index);

    if(std::memcmp(new_palette, palette.current_palette, sizeof(palette.current_palette)) != 0 || (raw_colors != nullptr && std::memcmp(raw_colors, palette.raw_colors, sizeof(palette.raw_colors)) != 0) || palette.cgb != this->cgb_colors || this->was_cgb_colors != this->cgb_colors) {
        std::memcpy(palette.current_palette, new_palette, sizeof(palette.current_palette));
        palette.cgb = this->cgb_colors;
        if(raw_colors != nullptr) {
            std::memcpy(palette.raw_colors, raw_colors, sizeof(palette.raw_colors));
        }

        auto format_background_color_for_palette = [](const std::uint32_t &p, QWidget *w) {
            std::uint8_t b = (p & 0xFF);
            std::uint8_t g = ((p & 0xFF00) >> 8) & 0xFF;
            std::uint8_t r = ((p & 0xFF0000) >> 16) & 0xFF;

            char stylesheet[256];
            std::snprintf(stylesheet, sizeof(stylesheet), "background-color: #%02x%02x%02x", r, g, b);
            w->setStyleSheet(stylesheet);
        };

        for(std::size_t p = 0; p < sizeof(palette.colors) / sizeof(palette.colors[p]); p++) {
            format_background_color_for_palette(palette.current_palette[p], palette.colors[p]);

            // Format the labels if needed
            if(raw_colors != nullptr) {
                char text[8] = {};
                if(this->cgb_colors) {
                    std::snprintf(text, sizeof(text), "$%04x ", raw_colors[p]);
                }
                else {
                    std::snprintf(text, sizeof(text), "$%01x    ", raw_colors[p]);
                }
                palette.color_text[p]->setText(text);
            }
        }
    }
}

VRAMViewer::VRAMViewer(GameWindow *window) : QMainWindow(window), window(window),
    gb_tilemap_image(reinterpret_cast<uchar *>(this->gb_tilemap_image_data), GameInstance::GB_TILEMAP_WIDTH, GameInstance::GB_TILEMAP_HEIGHT, QImage::Format::Format_ARGB32),
    gb_tileset_image(reinterpret_cast<uchar *>(this->gb_tileset_image_data), GameInstance::GB_TILESET_WIDTH, GameInstance::GB_TILESET_HEIGHT, QImage::Format::Format_ARGB32),
    gb_tileset_grid_image(reinterpret_cast<uchar *>(this->gb_tileset_grid_data), GameInstance::GB_TILESET_WIDTH*2, GameInstance::GB_TILESET_HEIGHT*2, QImage::Format::Format_ARGB32) {

    auto settings = get_superdux_settings();

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

    auto *gb_tilemap_view_inner_widget = new QWidget(gb_tilemap_view_frame);
    auto *gb_tilemap_view_inner_widget_layout = new QGridLayout(gb_tilemap_view_inner_widget);
    gb_tilemap_view_inner_widget_layout->setContentsMargins(0,0,0,0);
    gb_tilemap_view_inner_widget->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);

    this->gb_tilemap_view = new QGraphicsView(this->gb_tilemap_view_frame);
    this->gb_tilemap_view->setFrameShape(QFrame::Shape::NoFrame);
    this->gb_tilemap_view->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    this->gb_tilemap_scene = new QGraphicsScene(this->gb_tilemap_view);
    this->gb_tilemap_pixmap = this->gb_tilemap_scene->addPixmap(QPixmap::fromImage(this->gb_tilemap_image));
    this->gb_tilemap_pixmap->setTransform(QTransform::fromScale(2, 2));
    this->gb_tilemap_view->setScene(this->gb_tilemap_scene);
    this->gb_tilemap_view->setDisabled(true);

    gb_tilemap_view_inner_widget_layout->addWidget(this->gb_tilemap_view);
    gb_tilemap_view_frame_layout->addWidget(gb_tilemap_view_inner_widget);

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
    this->gb_tilemap_show_viewport_box->setChecked(settings.value(SETTING_SHOW_VIEWPORT, true).toBool());
    connect(this->gb_tilemap_show_viewport_box, &QCheckBox::clicked, this, &VRAMViewer::redraw_tilemap);

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
    this->gb_tileset_view->setFrameShape(QFrame::Shape::NoFrame);
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
    tileset_mouse_over_layout->setSpacing(10);

    auto *spacing_widget = new QWidget(tileset_mouse_over_widget);
    spacing_widget->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
    tileset_mouse_over_layout->addWidget(spacing_widget);
    int row_count = 1;
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

    // Mouse over tile preview
    this->tileset_mouse_over_tile_image = QImage(reinterpret_cast<const uchar *>(this->tileset_mouse_over_tile_image_data), GameInstance::GB_TILESET_TILE_LENGTH, GameInstance::GB_TILESET_TILE_LENGTH, QImage::Format::Format_ARGB32);
    this->tileset_mouse_over_tile_image_view = new QGraphicsView(tileset_mouse_over_widget);
    this->tileset_mouse_over_tile_image_scene = new QGraphicsScene(this->tileset_mouse_over_tile_image_view);
    this->tileset_mouse_over_tile_image_pixmap = this->tileset_mouse_over_tile_image_scene->addPixmap(QPixmap::fromImage(this->tileset_mouse_over_tile_image));
    this->tileset_mouse_over_tile_image_view->setScene(this->tileset_mouse_over_tile_image_scene);

    this->tileset_mouse_over_tile_grid_scale = (table_font.pixelSize() + tileset_mouse_over_layout->spacing()) * 3 / GameInstance::GB_TILESET_TILE_LENGTH;
    this->tileset_mouse_over_tile_image_pixmap->setTransform(QTransform::fromScale(this->tileset_mouse_over_tile_grid_scale, this->tileset_mouse_over_tile_grid_scale));
    this->tileset_mouse_over_tile_image_view->setHorizontalScrollBarPolicy(Qt::ScrollBarPolicy::ScrollBarAlwaysOff);
    this->tileset_mouse_over_tile_image_view->setVerticalScrollBarPolicy(Qt::ScrollBarPolicy::ScrollBarAlwaysOff);
    this->tileset_mouse_over_tile_image_view->setFrameShape(QFrame::Shape::NoFrame);
    this->tileset_mouse_over_tile_image_view->setFixedSize(GameInstance::GB_TILESET_TILE_LENGTH * this->tileset_mouse_over_tile_grid_scale, GameInstance::GB_TILESET_TILE_LENGTH * this->tileset_mouse_over_tile_grid_scale);

    // Overlay a grid on top of the tile preview
    this->tileset_mouse_over_tile_grid.resize(this->tileset_mouse_over_tile_grid_scale * this->tileset_mouse_over_tile_grid_scale * GameInstance::GB_TILESET_TILE_LENGTH * GameInstance::GB_TILESET_TILE_LENGTH, 0);
    this->tileset_mouse_over_tile_grid_image = QImage(reinterpret_cast<const uchar *>(this->tileset_mouse_over_tile_grid.data()), this->tileset_mouse_over_tile_grid_scale * GameInstance::GB_TILESET_TILE_LENGTH, this->tileset_mouse_over_tile_grid_scale * GameInstance::GB_TILESET_TILE_LENGTH, QImage::Format::Format_ARGB32);
    this->tileset_mouse_over_tile_image_pixmap_grid = this->tileset_mouse_over_tile_image_scene->addPixmap(QPixmap::fromImage(this->tileset_mouse_over_tile_grid_image));

    // Add the preview now
    tileset_mouse_over_layout->addWidget(this->tileset_mouse_over_tile_image_view, 1, 2, 3, 1, Qt::AlignmentFlag::AlignCenter);


    this->gb_show_tileset_grid = new QCheckBox("Show Grid", tileset_mouse_over_widget);
    this->gb_show_tileset_grid->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Ignored);
    this->gb_show_tileset_grid->setChecked(settings.value(SETTING_SHOW_GRID, true).toBool());
    tileset_mouse_over_layout->addWidget(this->gb_show_tileset_grid, palette_row_index, 1);

    // Palette
    int palette_hw = this->moused_over_tile_palette->sizeHint().height();
    auto *viewer = this;

    auto initialize_palette = [&palette_hw, &table_font, &viewer](PaletteViewData &palette, QWidget *parent, bool visible_palette_text, GB_palette_type_t palette_type = GB_palette_type_t::GB_PALETTE_NONE, std::size_t palette_index = 0) {
        palette.widget = new VRAMViewer::PaletteViewData::PaletteViewDataWidget(parent, viewer, palette_type, palette_index);
        auto *palette_preview_layout = new QHBoxLayout(palette.widget);
        palette_preview_layout->setContentsMargins(0,0,0,0);

        static constexpr const std::size_t color_count = sizeof(palette.colors) / sizeof(palette.colors[0]);
        for(std::size_t c = 0; c < color_count; c++) {
            auto &box = palette.colors[c];

            box = new QWidget(palette.widget);
            box->setFixedSize(palette_hw, palette_hw);
            box->setStyleSheet("background-color: #000");
            palette_preview_layout->addWidget(box);
            box->setAttribute(Qt::WA_TransparentForMouseEvents, true);

            auto &text = palette.color_text[c];
            text = new QLabel("$0000 ", palette.widget);
            text->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
            palette_preview_layout->addWidget(text);
            text->setFont(table_font);
            text->setAttribute(Qt::WA_TransparentForMouseEvents, true);

            text->setVisible(visible_palette_text);
        }

        palette.widget->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

        // Initialize the palette background
        std::memset(palette.current_palette, 0, sizeof(palette.current_palette));

        return palette_preview_layout;
    };

    initialize_palette(this->tileset_view_palette, tileset_mouse_over_widget, false);
    tileset_mouse_over_layout->addWidget(this->tileset_view_palette.widget, palette_row_index, 2);

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
    connect(this->tileset_palette_index, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), this, &VRAMViewer::redraw_tileset_palette);
    connect(this->tileset_palette_type, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this, &VRAMViewer::redraw_tileset_palette);
    palette_selector->setLayout(palette_selector_layout);
    palette_group_layout->addWidget(palette_selector);
    tileset_palette_layout->addWidget(palette_group);
    layout->addWidget(tileset_palette);

    // OAM
    auto *gb_oam_frame = new QScrollArea(this->gb_tab_view);
    this->gb_oam_view_frame = gb_oam_frame;
    auto *gb_oam_view_frame_inner = new QWidget(this->gb_oam_view_frame);
    auto *gb_oam_view_frame_layout = new QGridLayout(gb_oam_view_frame_inner);
    for(std::size_t o = 0; o < sizeof(this->objects) / sizeof(this->objects[0]); o++) {
        auto &object_view_data = this->objects[o];

        // Initialize the image
        object_view_data.image = QImage(reinterpret_cast<const uchar *>(object_view_data.sprite_pixel_data), GameInstance::GB_TILESET_TILE_LENGTH, GameInstance::GB_TILESET_TILE_LENGTH * 2, QImage::Format::Format_ARGB32);

        // Create our frame
        object_view_data.frame = new QFrame(this->gb_oam_view_frame);
        auto *flayout = new QHBoxLayout(object_view_data.frame);
        object_view_data.frame->setLayout(flayout);
        object_view_data.frame->setFrameShape(QFrame::Shape::Panel);
        object_view_data.frame->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

        // Start with the view
        object_view_data.view = new QGraphicsView(object_view_data.frame);
        object_view_data.view->setFrameShape(QFrame::Shape::NoFrame);
        object_view_data.scene = new QGraphicsScene(object_view_data.view);
        object_view_data.pixmap = object_view_data.scene->addPixmap(QPixmap::fromImage(object_view_data.image));
        object_view_data.pixmap->setTransform(QTransform::fromScale(4, 4));
        object_view_data.view->setScene(object_view_data.scene);
        object_view_data.view->setFixedSize(object_view_data.view->sizeHint());
        object_view_data.view->setBackgroundRole(QWidget::backgroundRole());
        flayout->addWidget(object_view_data.view);

        // Now the metadata
        object_view_data.info = new QLabel(object_view_data.frame);
        object_view_data.info->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::MinimumExpanding);
        format_label(object_view_data.info, {}, o);
        object_view_data.info->setAlignment(Qt::AlignmentFlag::AlignVCenter | Qt::AlignmentFlag::AlignLeft);
        object_view_data.info->setFont(table_font);
        flayout->addWidget(object_view_data.info);

        gb_oam_view_frame_layout->addWidget(object_view_data.frame, o / 4, o % 4);
    }
    gb_oam_frame->setMinimumWidth(gb_oam_view_frame_inner->sizeHint().width() + gb_oam_frame->verticalScrollBar()->sizeHint().width());
    gb_oam_frame->setHorizontalScrollBarPolicy(Qt::ScrollBarPolicy::ScrollBarAlwaysOff);
    gb_oam_frame->setWidget(gb_oam_view_frame_inner);
    gb_oam_view_frame_inner->setLayout(gb_oam_view_frame_layout);
    this->gb_tab_view->addTab(this->gb_oam_view_frame, "Sprite Attributes");

    // Show the default text
    this->show_info_for_tile(std::nullopt, false);

    // Next, palettes
    this->gb_palette_view_frame = new QWidget(this->gb_tab_view);
    auto *gb_palette_view_frame = this->gb_palette_view_frame;
    auto *gb_palette_view_frame_layout = new QGridLayout(this->gb_palette_view_frame);
    gb_palette_view_frame_layout->setSpacing(0);

    // Add the palette to the row
    int palette_row_count = 0;
    auto *vram_viewer = this;
    auto add_palette_widget = [&gb_palette_view_frame, &gb_palette_view_frame_layout, &initialize_palette, &table_font, &palette_row_count, &vram_viewer](std::size_t i, GB_palette_type_t type, PaletteViewData &view_data, const char *title, bool border_on_bottom) {
        // Label
        char text[256];
        std::snprintf(text, sizeof(text), "%s #%zu", title, i);
        view_data.name_label = new QLabel(text, gb_palette_view_frame);
        gb_palette_view_frame_layout->addWidget(view_data.name_label, palette_row_count, 0);

        // Set up the actual widgets
        initialize_palette(view_data, gb_palette_view_frame, true, type, i)->setContentsMargins(0, (i == 0 && type == GB_palette_type_t::GB_PALETTE_BACKGROUND) ? 0 : 5, 0, 5); // have 5 px spacing on bottom and top (0 px spacing on top if first BG palette)
        gb_palette_view_frame_layout->addWidget(view_data.widget, palette_row_count, 2);

        if(border_on_bottom) {
            palette_row_count++;

            auto *f = new QFrame(gb_palette_view_frame);
            f->setFrameShape(QFrame::Shape::HLine);
            f->setMaximumHeight(2);

            gb_palette_view_frame_layout->addWidget(f, palette_row_count, 0, 1, 3);
        }

        palette_row_count++;
    };

    // Initialize the palettes here
    for(std::size_t i = 0; i < sizeof(this->gb_palette_background) / sizeof(this->gb_palette_background[0]); i++) {
        add_palette_widget(i, GB_palette_type_t::GB_PALETTE_BACKGROUND, this->gb_palette_background[i], "Background", true);
    }

    for(std::size_t i = 0; i < sizeof(this->gb_palette_oam) / sizeof(this->gb_palette_oam[0]); i++) {
        add_palette_widget(i, GB_palette_type_t::GB_PALETTE_OAM, this->gb_palette_oam[i], "OAM (Sprite)", i + 1 != sizeof(this->gb_palette_oam) / sizeof(this->gb_palette_oam[0]));
    }

    gb_palette_view_frame_layout->addWidget(this->mouse_over_palette_label = new QLabel("", gb_palette_view_frame), palette_row_count++, 0, 1, 3);
    this->mouse_over_palette_label->setAlignment(Qt::AlignmentFlag::AlignHCenter | Qt::AlignmentFlag::AlignVCenter);
    gb_palette_view_frame->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Maximum);

    this->mouse_over_palette_label->setFont(table_font);
    this->gb_palette_view_frame->setLayout(gb_palette_view_frame_layout);
    this->gb_tab_view->addTab(this->gb_palette_view_frame, "Palettes");

    this->setFixedWidth(this->sizeHint().width());
}

VRAMViewer::~VRAMViewer() {
    auto settings = get_superdux_settings();
    settings.setValue(SETTING_SHOW_VIEWPORT, this->gb_tilemap_show_viewport_box->isChecked());
    settings.setValue(SETTING_SHOW_GRID, this->gb_show_tileset_grid->isChecked());
}

void VRAMViewer::refresh_view() {
    if(this->isHidden()) {
        return;
    }

    this->cgb_colors = this->window->get_instance().is_game_boy_color();
    this->redraw_tileset_palette();
    this->was_cgb_colors = this->cgb_colors;
}

void VRAMViewer::redraw_tileset_palette() noexcept {
    this->redraw_tilemap();
    this->redraw_tileset();
    this->redraw_oam_data();
    this->redraw_palette();
}

void VRAMViewer::redraw_oam_data() noexcept {
    if(this->gb_oam_view_frame->isHidden()) {
        return;
    }

    auto oam = this->window->get_instance().get_object_attribute_info();
    bool double_resolution = oam.height == 16; // 8x16

    for(std::size_t o = 0; o < sizeof(this->objects) / sizeof(this->objects[0]); o++) {
        auto &object = this->objects[o];
        auto &object_data = oam.objects[o];
        format_label(object.info, object_data, o);

        static_assert(sizeof(object.sprite_pixel_data) == sizeof(object_data.pixel_data));
        if(double_resolution) {
            std::memcpy(object.sprite_pixel_data, object_data.pixel_data, sizeof(object_data.pixel_data));
        }
        else {
            std::memcpy(object.sprite_pixel_data, object_data.pixel_data, sizeof(object_data.pixel_data) / 2);
            std::memset(reinterpret_cast<std::byte *>(object.sprite_pixel_data) + sizeof(object_data.pixel_data) / 2, 0, sizeof(object_data.pixel_data) / 2);
        }

        object.pixmap->setPixmap(QPixmap::fromImage(object.image));
        object.frame->setEnabled(object_data.on_screen);
    }
}

void VRAMViewer::redraw_tilemap() noexcept {
    if(this->gb_tilemap_view_frame->isHidden()) {
        return; // return if not visible (such as if we're on a different tab)
    }

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

            color = grid_pixel(color);
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
    if(this->cgb_colors != this->was_cgb_colors && !this->cgb_colors) {
        std::memset(this->gb_tileset_image_data, 0, sizeof(this->gb_tileset_image_data)); // initialize this region so we don't see uninitialized/unused data such as if we switch between CGB and DMG
    }

    auto type = static_cast<GB_palette_type_t>(this->tileset_palette_type->currentData().toInt());

    auto &instance = this->window->get_instance();
    instance.draw_tileset(this->gb_tileset_image_data, static_cast<GB_palette_type_t>(this->tileset_palette_type->currentData().toInt()), this->tileset_palette_index->value());
    this->gb_tileset_pixmap->setPixmap(QPixmap::fromImage(this->gb_tileset_image));
    this->tileset_info = instance.get_tileset_info();

    // update the grid
    if(this->gb_show_tileset_grid->isChecked()) {
        for(std::size_t y = GameInstance::GB_TILESET_TILE_LENGTH*2-1; y < (GameInstance::GB_TILESET_HEIGHT - 1) * 2; y+=GameInstance::GB_TILESET_TILE_LENGTH*2) {
            for(std::size_t x = 0; x < GameInstance::GB_TILESET_WIDTH * 2; x++) {
                this->gb_tileset_grid_data[x + y * GameInstance::GB_TILESET_WIDTH*2] = grid_pixel(this->gb_tileset_image_data[x/2 + y/2 * GameInstance::GB_TILESET_WIDTH]);
            }
        }

        for(std::size_t x = GameInstance::GB_TILESET_TILE_LENGTH*2-1; x < (GameInstance::GB_TILESET_WIDTH - 1) * 2; x+=GameInstance::GB_TILESET_TILE_LENGTH*2) {
            for(std::size_t y = 0; y < GameInstance::GB_TILESET_HEIGHT * 2; y++) {
                this->gb_tileset_grid_data[x + y * GameInstance::GB_TILESET_WIDTH*2] = grid_pixel(this->gb_tileset_image_data[x/2 + y/2 * GameInstance::GB_TILESET_WIDTH]);
            }
        }
        this->gb_tileset_grid_pixelmap->setPixmap(QPixmap::fromImage(this->gb_tileset_grid_image));
        this->gb_tileset_grid_pixelmap->setVisible(true);
    }
    else {
        this->gb_tileset_grid_pixelmap->setVisible(false);
    }

    // if we are mousing over a tile, show data for that tile
    GB_palette_type_t new_palette_type;
    std::size_t new_palette_index;

    if(moused_over_tile_index.has_value()) {
        auto &info = tileset_info.tiles[*moused_over_tile_index];

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

        // Copy over tile from tileset to mouse over preview
        auto mouse_over_tile_x = (info.tile_index % GameInstance::GB_TILESET_PAGE_BLOCK_WIDTH) + (info.tile_bank ? GameInstance::GB_TILESET_PAGE_BLOCK_WIDTH : 0);
        auto mouse_over_tile_y = info.tile_index / GameInstance::GB_TILESET_PAGE_BLOCK_WIDTH;
        auto mouse_over_tile_x_px = mouse_over_tile_x * GameInstance::GB_TILESET_TILE_LENGTH;
        auto mouse_over_tile_y_px = mouse_over_tile_y * GameInstance::GB_TILESET_TILE_LENGTH;
        auto *mouse_over_tile_preview_tileset_row = this->gb_tileset_image_data + mouse_over_tile_y_px * GameInstance::GB_TILESET_WIDTH + mouse_over_tile_x_px;
        auto *mouse_over_tile_preview_tile_row = this->tileset_mouse_over_tile_image_data;
        for(std::size_t y = 0; y < GameInstance::GB_TILESET_TILE_LENGTH; y++, mouse_over_tile_preview_tileset_row += GameInstance::GB_TILESET_WIDTH, mouse_over_tile_preview_tile_row += GameInstance::GB_TILESET_TILE_LENGTH) {
            std::copy(mouse_over_tile_preview_tileset_row, mouse_over_tile_preview_tileset_row + GameInstance::GB_TILESET_TILE_LENGTH, mouse_over_tile_preview_tile_row);
        }

        // Draw the pixel grid for the moused over tile
        if(this->gb_show_tileset_grid->isChecked()) {
            for(std::size_t y = 0; y < GameInstance::GB_TILESET_TILE_LENGTH - 0; y++) {
                for(std::size_t x = 0; x < GameInstance::GB_TILESET_TILE_LENGTH - 0; x++) {
                    auto pixel_inverted = grid_pixel(this->tileset_mouse_over_tile_image_data[x + y * GameInstance::GB_TILESET_TILE_LENGTH]);

                    std::size_t x_write = x * this->tileset_mouse_over_tile_grid_scale;
                    std::size_t y_write = y * this->tileset_mouse_over_tile_grid_scale;

                    for(unsigned int i = 0; i < this->tileset_mouse_over_tile_grid_scale; i++, x_write++, y_write++) {
                        if(y != 0) {
                            this->tileset_mouse_over_tile_grid[x_write + y * (this->tileset_mouse_over_tile_grid_scale * this->tileset_mouse_over_tile_grid_scale * GameInstance::GB_TILESET_TILE_LENGTH)] = pixel_inverted;
                        }
                        if(x != 0) {
                            this->tileset_mouse_over_tile_grid[x * this->tileset_mouse_over_tile_grid_scale + y_write * (this->tileset_mouse_over_tile_grid_scale * GameInstance::GB_TILESET_TILE_LENGTH)] = pixel_inverted;
                        }
                    }
                }
            }
            this->tileset_mouse_over_tile_image_pixmap_grid->setVisible(true);
        }
        else {
            this->tileset_mouse_over_tile_image_pixmap_grid->setVisible(false);
        }

        if(info.accessed_type) {
            char palette_text[256];

            // Find the palette
            auto palette = info.accessed_type == GameInstance::TilesetInfoTileType::TILESET_INFO_OAM ? GB_palette_type_t::GB_PALETTE_OAM : GB_palette_type_t::GB_PALETTE_BACKGROUND;
            std::snprintf(palette_text, sizeof(palette_text), "Palette: %u (%s)", info.accessed_tile_palette_index, palette == GB_palette_type_t::GB_PALETTE_OAM ? "sprite" : "background");
            this->moused_over_tile_palette->setText(palette_text);

            char user_text[256];
            char user_name[256] = "???";

            switch(info.accessed_type) {
                case GameInstance::TilesetInfoTileType::TILESET_INFO_OAM:
                    std::snprintf(user_name, sizeof(user_name), "sprite #%02u ($%02x)", info.accessed_user_index, info.accessed_user_index);
                    break;
                case GameInstance::TilesetInfoTileType::TILESET_INFO_WINDOW:
                    std::snprintf(user_name, sizeof(user_name), "window");
                    break;
                case GameInstance::TilesetInfoTileType::TILESET_INFO_BACKGROUND:
                    std::snprintf(user_name, sizeof(user_name), "background");
                    break;
                default:
                    break;
            }

            std::snprintf(palette_text, sizeof(palette_text), "User: %s", user_name);

            new_palette_type = palette;
            new_palette_index = info.accessed_tile_palette_index;

            this->moused_over_tile_user->setText(palette_text);
        }
        else {
            this->moused_over_tile_palette->setText(" ");
            this->moused_over_tile_user->setText(" ");

            new_palette_type = type;
            new_palette_index = this->tileset_palette_index->value();
        }

    }
    else {
        new_palette_type = type;
        new_palette_index = this->tileset_palette_index->value();

        this->moused_over_tile_address->setText(" ");
        this->moused_over_tile_accessed_index->setText(" ");
        this->moused_over_tile_palette->setText("Mouse over a tile for information.");
        this->moused_over_tile_user->setText(" ");

        std::fill(this->tileset_mouse_over_tile_image_data, this->tileset_mouse_over_tile_image_data + sizeof(this->tileset_mouse_over_tile_image_data) / sizeof(this->tileset_mouse_over_tile_image_data[0]), 0);
        std::fill(this->tileset_mouse_over_tile_grid.begin(), this->tileset_mouse_over_tile_grid.end(), 0);
    }

    // Update the previewed image
    this->tileset_mouse_over_tile_image_pixmap->setPixmap(QPixmap::fromImage(this->tileset_mouse_over_tile_image));
    this->tileset_mouse_over_tile_image_pixmap_grid->setPixmap(QPixmap::fromImage(this->tileset_mouse_over_tile_grid_image));

    // Is the palette different?
    this->update_palette(this->tileset_view_palette, new_palette_type, new_palette_index);

    // Gray out the index if we're on none
    this->tileset_palette_index_label->setEnabled(type != GB_palette_type_t::GB_PALETTE_NONE && type != GB_palette_type_t::GB_PALETTE_AUTO);
    this->tileset_palette_index->setEnabled(type != GB_palette_type_t::GB_PALETTE_NONE && type != GB_palette_type_t::GB_PALETTE_AUTO);
}

void VRAMViewer::show_info_for_tile(const std::optional<std::uint16_t> &tile, bool show_on_left_pane) {
    // If the tile is the same, don't do anything.
    if(this->moused_over_tile_index != tile) {
        this->moused_over_tile_index = tile;
        this->redraw_tileset();
    }

    // Show the tile
    if(show_on_left_pane && tile.has_value()) {
        auto &t = this->tileset_info.tiles[*tile];
        switch(static_cast<GameInstance::TilesetInfoTileType>(t.accessed_type)) {
            case GameInstance::TilesetInfoTileType::TILESET_INFO_BACKGROUND:
                this->tilemap_map_type->setCurrentIndex(0);
                this->gb_tab_view->setCurrentWidget(this->gb_tilemap_view_frame);
                break;
            case GameInstance::TilesetInfoTileType::TILESET_INFO_WINDOW:
                this->tilemap_map_type->setCurrentIndex(1);
                this->gb_tab_view->setCurrentWidget(this->gb_tilemap_view_frame);
                break;
            case GameInstance::TilesetInfoTileType::TILESET_INFO_OAM:
                this->gb_tab_view->setCurrentWidget(this->gb_oam_view_frame);
                break;
            case GameInstance::TilesetInfoTileType::TILESET_INFO_NONE:
                break;
        }
    }
}

void VRAMViewer::redraw_palette() noexcept {
    // Do we "disable" the CGB-only colors?
    if(this->cgb_colors != this->was_cgb_colors) {
        for(std::size_t i = 1; i < sizeof(this->gb_palette_background) / sizeof(this->gb_palette_background[0]); i++) {
            this->gb_palette_background[i].widget->setEnabled(this->cgb_colors);
            this->gb_palette_background[i].name_label->setEnabled(this->cgb_colors);
        }
        for(std::size_t i = 2; i < sizeof(this->gb_palette_oam) / sizeof(this->gb_palette_oam[0]); i++) {
            this->gb_palette_oam[i].widget->setEnabled(this->cgb_colors);
            this->gb_palette_oam[i].name_label->setEnabled(this->cgb_colors);
        }
    }

    // If this view isn't visible, we don't need to do anything further
    if(this->gb_palette_view_frame->isHidden()) {
        return;
    }

    std::uint16_t buffer[4];

    auto &instance = this->window->get_instance();

    for(std::size_t i = 0; i < sizeof(this->gb_palette_background) / sizeof(this->gb_palette_background[0]); i++) {
        instance.get_raw_palette(GB_palette_type_t::GB_PALETTE_BACKGROUND, i, buffer);
        this->update_palette(this->gb_palette_background[i], GB_palette_type_t::GB_PALETTE_BACKGROUND, i, buffer);
    }
    for(std::size_t i = 0; i < sizeof(this->gb_palette_oam) / sizeof(this->gb_palette_oam[0]); i++) {
        instance.get_raw_palette(GB_palette_type_t::GB_PALETTE_OAM, i, buffer);
        this->update_palette(this->gb_palette_oam[i], GB_palette_type_t::GB_PALETTE_OAM, i, buffer);
    }

    // if we're mousing over a palette, do this
    if(this->moused_over_palette.has_value()) {
        char str[256];
        instance.get_raw_palette(*this->moused_over_palette, this->moused_over_palette_index, buffer);

        int k = 0;
        for(int i = 0; i < 4; i++) {
            if(this->cgb_colors) {
                k += std::snprintf(str + k, sizeof(str) - k, "Color %i: $%04x (Red: $%02x, Green: $%02x, Blue: $%02x)", i, buffer[i], buffer[i] & 0x1F, (buffer[i] >> 5) & 0x1F, (buffer[i] >> 10) & 0x1F);
            }
            else {
                k += std::snprintf(str + k, sizeof(str) - k, "Shade %i: $%01x", i, buffer[i]);
            }
            if(i != 3) {
                str[k++] = '\n';
                str[k] = 0;
            }
        }
        this->mouse_over_palette_label->setText(str);
    }

    // otherwise, no
    else {
        this->mouse_over_palette_label->setText("Mouse over a color for more information.");
    }
}

void VRAMViewer::show_info_for_palette(std::optional<GB_palette_type_t> palette, std::size_t index) {
    if(palette == this->moused_over_palette) {
        return;
    }

    this->moused_over_palette = palette;
    this->moused_over_palette_index = index;
}

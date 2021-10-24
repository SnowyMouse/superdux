#ifndef VRAM_VIEWER_HPP
#define VRAM_VIEWER_HPP

#include <QMainWindow>
#include <QImage>

class GameWindow;
class QGraphicsScene;
class QGraphicsPixmapItem;
class QGraphicsView;
class QCheckBox;
class QSpinBox;
class QComboBox;
class QLabel;
class QFrame;

#include "game_instance.hpp"

class VRAMViewer : public QMainWindow {
    Q_OBJECT
    
public:
    VRAMViewer(GameWindow *window);
    ~VRAMViewer() override;

    void refresh_view();

    void show_info_for_tile(const std::optional<std::uint16_t> &tile, bool show_on_left_pane);

private:
    GameWindow *window;

    QTabWidget *gb_tab_view;
    QWidget *gb_tilemap_view_frame, *gb_oam_view_frame;

    std::uint32_t gb_tileset_image_data[GameInstance::GB_TILESET_WIDTH * GameInstance::GB_TILESET_HEIGHT] = {};
    std::uint32_t gb_tileset_grid_data[GameInstance::GB_TILESET_WIDTH * 2 * GameInstance::GB_TILESET_HEIGHT * 2] = {};
    QImage gb_tileset_grid_image;
    QGraphicsScene *gb_tileset_scene;
    QGraphicsView *gb_tileset_view;
    QGraphicsPixmapItem *gb_tileset_pixmap;
    QGraphicsPixmapItem *gb_tileset_grid_pixelmap;
    QCheckBox *gb_show_tileset_grid;
    QImage gb_tileset_image;
    void redraw_tileset() noexcept;

    QLabel *tileset_palette_index_label;
    QSpinBox *tileset_palette_index;
    QComboBox *tileset_palette_type;

    QGraphicsScene *gb_tilemap_scene;
    QGraphicsView *gb_tilemap_view;
    QGraphicsPixmapItem *gb_tilemap_pixmap;
    QImage gb_tilemap_image;
    std::uint32_t gb_tilemap_image_data[GameInstance::GB_TILEMAP_WIDTH * GameInstance::GB_TILEMAP_HEIGHT] = {};
    QCheckBox *gb_tilemap_show_viewport_box;
    void redraw_tilemap() noexcept;
    QComboBox *tilemap_map_type, *tilemap_tileset_type;

    QLabel *moused_over_tile_address, *moused_over_tile_accessed_index, *moused_over_tile_palette, *moused_over_tile_user;

    GameInstance::TilesetInfo tileset_info;
    std::optional<std::uint16_t> moused_over_tile_index;
    QWidget *palette_a, *palette_b, *palette_c, *palette_d;
    std::uint32_t current_palette[4] = { 0xFF000000, 0xFF000000, 0xFF000000, 0xFF000000 };
    void redraw_palette() noexcept;

    struct OAMObjectViewData {
        std::uint32_t sprite_pixel_data[GameInstance::GB_TILESET_TILE_LENGTH * GameInstance::GB_TILESET_TILE_LENGTH * 2];
        QImage image;

        QFrame *frame;
        QLabel *info;
        QGraphicsScene *scene;
        QGraphicsView *view;
        QGraphicsPixmapItem *pixmap;
    };
    OAMObjectViewData objects[sizeof(GameInstance::ObjectAttributeInfo::objects) / sizeof(GameInstance::ObjectAttributeInfo::objects[0])];
    void redraw_oam_data() noexcept;
};

#endif

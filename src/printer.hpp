#ifndef PRINTER_HPP
#define PRINTER_HPP

#include <QMainWindow>
#include <QImage>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QGraphicsPixmapItem>

class GameWindow;
class QComboBox;
class QVBoxLayout;

class Printer : public QMainWindow {
public:
    Printer(GameWindow *window);
    ~Printer() override;

    void refresh_view();
    void force_disconnect_printer();

private:
    std::vector<std::uint32_t> printed_data;
    std::size_t printed_height = 0;
    QComboBox *printed_data_list;

    QWidget *printed_scene_outer_widget;
    QVBoxLayout *printed_scene_outer_widget_layout;

    QImage printed_image;
    QGraphicsScene *printed_scene;
    QGraphicsView *printed_view = nullptr;
    QGraphicsPixmapItem *printed_pixmap;
    bool connected = false;

    QPushButton *connect_button, *clipboard_button, *save_button, *clear_button;

    GameWindow *game_window;

    void connect_printer();
    void save();
    void clipboard();
    void clear();

    void reset_connect_button(bool connected);
};

#endif

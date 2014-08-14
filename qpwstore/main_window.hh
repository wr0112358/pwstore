#ifndef _MAINWINDOW_HH_
#define _MAINWINDOW_HH_

#include <memory>
#include <QClipboard>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QMainWindow>
#include <QMouseEvent>

#include "../pwstore.hh"

class QAction;
class QCheckBox;
class QGridLayout;
class QLineEdit;
class QListWidget;
class QListWidgetItem;
class QPushButton;
class QSpacerItem;
class QStatusBar;
class list_container;

namespace pw_store_api_cxx
{
class pwstore_api;
}

/*
  Missing:

  o qmake crosscompilation for windows

  o memorize user stuff like last used database, last used directory when
    using "open/create db"

*/

class main_window : public QMainWindow
{
    Q_OBJECT
public:
    main_window(QWidget *parent, const std::string &default_db);
    ~main_window();
    void keyPressEvent(QKeyEvent *event);

protected:
    void timerEvent(QTimerEvent *event);

private:
    QLineEdit *line_edit;
    QListWidget *list;
    QGridLayout *layout;

    QPushButton *create_button;
    QPushButton *open_button;
    QPushButton *exit_button;
    QPushButton *lock_button;
    // enabled only after a modification
    QPushButton *sync_button;
    std::unique_ptr<pw_store_api_cxx::pwstore_api> db;
    int clear_clipboard_timer;
    int lock_db_timer = -1;

    QCheckBox *show_all;
    bool show_all_checked = false;

    // edit mode stuff
    QPushButton *edit_mode;
    QPushButton *add_entry;
    QPushButton *remove_entry;
    QPushButton *modify_entry;
    bool editing = false;
    std::list<std::string> log_msgs;

private:
    bool passworddialog(std::string &password);
    bool askyesno(const std::string &question);

    void open_db(const std::string &db);
    void update_list_from_db();
    void lock_db();
    bool unlock_db();
    void restart_db_lock_timer();
    void stop_db_lock_timer();
    bool open_three_inputs_window(pw_store::data_type &date,
                                  bool modify_existing);
    void modify_entry_id(pw_store::data_type::id_type id);
    void log_err(const std::string &msg);
    void log_info(const std::string &msg);

private slots:
    void create_pressed();
    void open_pressed();
    void exit_pressed();
    void lock_pressed();
    void sync_pressed();

    void line_edit_text_changed(const QString &text);
    void list_item_activated(QListWidgetItem *item);

    void edit_mode_pressed();
    void add_entry_pressed();
    void remove_entry_pressed();
    void modify_entry_pressed();
};

#endif

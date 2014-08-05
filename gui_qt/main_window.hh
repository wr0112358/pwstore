#ifndef _MAINWINDOW_HH_
#define _MAINWINDOW_HH_

#include <memory>
#include <QClipboard>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QMainWindow>
#include <QMouseEvent>

class QAction;
class QGridLayout;
class QLineEdit;
class QListWidget;
class QListWidgetItem;
class QPushButton;
class QSpacerItem;
class QStatusBar;
class list_container;

namespace pw_store_api_cxx {
class pwstore_api;
}

/*
  Missing:
  o Editing the database not possible yet:
    button "RW/RO":
      - starts edit mode
      - changes label from RO to RW
      - displays edit buttons below
    buttons:
      "add entry":
        - open 3-lineedit dialog like for "modify", but with empty fields.
          display "create password"-button next to password field.
      "remove": if an entry is selected, ask if really should be removed.
      "modify":
        - if an entry is selected, open window with 3 lineedits containing
          the values of the selected entry(pw field with start. -> can only
          be overwritten completely or not changed at all.)
        - if entered values are valid and dialog was not cancelled, remove
          original from db and insert a copy of the removed entry containing
          the just made changes.

  o qmake crosscompilation for windows

*/

class main_window : public  QMainWindow {
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
    QGridLayout * layout;

    QPushButton *open_button;
    QPushButton *exit_button;
    QPushButton *lock_button;
    // enabled only after a modification
    QPushButton *sync_button;
    std::unique_ptr<pw_store_api_cxx::pwstore_api> db;
    int clear_clipboard_timer;
    int lock_db_timer = -1;

private:
    bool passworddialog(std::string &password);
    bool askyesno(const std::string &question);

    void open_db(const std::string &db);
    void update_list_from_db();
    void lock_db();
    void restart_db_lock_timer();

private slots:
    void open_pressed();
    void exit_pressed();
    void lock_pressed();
    void sync_pressed();

    void line_edit_text_changed(const QString &text);
    void list_item_activated(QListWidgetItem *item);

    void clipboard_changed(QClipboard::Mode mode);
    void clipboard_selection_changed();
};

#endif

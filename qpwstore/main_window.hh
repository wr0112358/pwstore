#ifndef _MAINWINDOW_HH_
#define _MAINWINDOW_HH_

#include <list>
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
#ifndef NO_GOOD
class QSocketNotifier;
#endif
class QSpacerItem;
class QStatusBar;
class list_container;

namespace pw_store_api_cxx
{
class pwstore_api;
}

/*
  Missing:

  o memorize user stuff like last used database, last used directory when
    using "open/create db"
  o close db when C-w is pressed + add close button
  o fix layput in closed-mode

*/

class main_window : public QMainWindow
{
    Q_OBJECT
public:
    main_window(QWidget *parent, const std::string &default_db);
    ~main_window();
    void keyPressEvent(QKeyEvent *event);
#ifndef NO_GOOD
    static void handle_sigint(int unused);
#endif

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

    // nothing open mode elements
    QPushButton *open_lru;
    // store backlog
    std::list<std::string> log_msgs;

#ifndef NO_GOOD
    // handle unix SIGINT signal:
    // https://qt-project.org/doc/qt-4.7/unix-signals.html
    static int sigintfd[2];
    QSocketNotifier *notify_sigint;
#endif

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

public slots:
    void handle_sigint();

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

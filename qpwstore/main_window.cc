#include "main_window.hh"

#include <iostream>

#include <QApplication>
#include <QBoxLayout>
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFormLayout>
#include <QGridLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#ifndef NO_GOOD
#include <QSocketNotifier>
#endif
#include <QStatusBar>
#include <QTimer>
#ifndef NO_GOOD
#include <signal.h>
#include <unistd.h>
#endif

#include "key_handler.hh"
#include "list_entry.hh"
#include "../pwstore_api_cxx.hh"

#ifndef NO_GOOD
int main_window::sigintfd[2];

namespace {
bool setup_signals()
{
    struct sigaction sigint;

    sigint.sa_handler = main_window::handle_sigint;
    sigemptyset(&sigint.sa_mask);
    sigint.sa_flags = SA_RESTART;

    if(sigaction(SIGINT, &sigint, 0) > 0)
        return false;

    return true;
}
}
#endif
main_window::main_window(QWidget *parent, const std::string &default_db)
    : QMainWindow(parent, Qt::FramelessWindowHint)
{
#ifndef NO_GOOD
    if(::socketpair(AF_UNIX, SOCK_STREAM, 0, sigintfd))
        qFatal("Couldn't create SIGINT socketpair");
    notify_sigint = new QSocketNotifier(sigintfd[1], QSocketNotifier::Read, this);
    connect(notify_sigint, SIGNAL(activated(int)), this, SLOT(handle_sigint()));
    setup_signals();
#endif

    layout = new QGridLayout;

    list = new QListWidget(this);
    list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    list->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    line_edit = new QLineEdit();
    create_button = new QPushButton("&create");
    open_button = new QPushButton("&open");
    exit_button = new QPushButton("&exit");
    lock_button = new QPushButton("&lock");
    sync_button = new QPushButton("&sync");

    lock_button->setEnabled(false);
    sync_button->setEnabled(false);

    connect(list, SIGNAL(itemActivated(QListWidgetItem *)), this,
            SLOT(list_item_activated(QListWidgetItem *)));
    connect(line_edit, SIGNAL(textChanged(const QString &)), this,
            SLOT(line_edit_text_changed(const QString &)));
    connect(create_button, SIGNAL(pressed()), this, SLOT(create_pressed()));
    connect(open_button, SIGNAL(pressed()), this, SLOT(open_pressed()));
    connect(exit_button, SIGNAL(pressed()), this, SLOT(exit_pressed()));
    connect(lock_button, SIGNAL(pressed()), this, SLOT(lock_pressed()));
    connect(sync_button, SIGNAL(pressed()), this, SLOT(sync_pressed()));

    layout->addWidget(create_button, 0, 7, 1, 1);
    layout->addWidget(open_button, 0, 9, 1, 1);
    layout->addWidget(exit_button, 1, 9, 1, 1);
    layout->addWidget(lock_button, 0, 8, 1, 1);
    layout->addWidget(sync_button, 1, 8, 1, 1);
    layout->addWidget(line_edit, 2, 0, 1, 10);
    layout->addWidget(list, 3, 0, 1, 10);
    lock_button->hide();
    sync_button->hide();

    show_all = new QCheckBox("&show all");
    connect(show_all, &QCheckBox::stateChanged, [&](int state) {
        if(state == Qt::Checked)
            show_all_checked = true;
        else
            show_all_checked = false;
        update_list_from_db();
    });
    show_all->setCheckState(show_all_checked ? Qt::Checked : Qt::Unchecked);
    layout->addWidget(show_all, 0, 3, 1, 1);
    show_all->hide();

    // edit mode stuff
    edit_mode = new QPushButton("&edit..");
    add_entry = new QPushButton("&Add Entry");
    remove_entry = new QPushButton("&Remove Entry");
    modify_entry = new QPushButton("&Modify Entry");

    connect(edit_mode, SIGNAL(pressed()), this, SLOT(edit_mode_pressed()));
    connect(add_entry, SIGNAL(pressed()), this, SLOT(add_entry_pressed()));
    connect(remove_entry, SIGNAL(pressed()), this,
            SLOT(remove_entry_pressed()));
    connect(modify_entry, SIGNAL(pressed()), this,
            SLOT(modify_entry_pressed()));

    layout->addWidget(edit_mode, 0, 0, 1, 1);
    layout->addWidget(add_entry, 1, 0, 1, 1);
    layout->addWidget(remove_entry, 0, 1, 1, 1);
    layout->addWidget(modify_entry, 1, 1, 1, 1);
    edit_mode->hide();
    add_entry->hide();
    remove_entry->hide();
    modify_entry->hide();

    // this will be shown if no db is open:
    static const std::string lru_db = QDir::home().absolutePath().toStdString() + "/.my_secrets";
    open_lru =
        new QPushButton(QString::fromStdString("open default database: \"" + lru_db + "\""));

    connect(open_lru, &QPushButton::clicked, [&]() {
            open_db(lru_db);
            std::cout << "open_lru_link_activated(\""<< lru_db << "\")\n";
    });

    line_edit->hide();
    list->hide();
    layout->addWidget(open_lru, 2, 0, 1, 1);

    // QMainWindow needs a central widget to be able to use a layout.
    QWidget *central_widget = new QWidget;
    layout->setSpacing(1);
    central_widget->setLayout(layout);
    setCentralWidget(central_widget);
    line_edit->setFocus();
    line_edit->setFocusPolicy(Qt::StrongFocus);

    if(!default_db.empty())
        open_db(default_db);

    QCoreApplication::instance()->installEventFilter(new key_handler);
}

main_window::~main_window() {}

#ifndef NO_GOOD
void main_window::handle_sigint(int)
{
    char a = 1;
    const auto ret = ::write(sigintfd[0], &a, sizeof(a));
    (void)ret;
}
#endif

void main_window::handle_sigint()
{
#ifndef NO_GOOD
    notify_sigint->setEnabled(false);
    char tmp;
    const auto ret = ::read(sigintfd[1], &tmp, sizeof(tmp));
    (void)ret;

    exit_pressed();

    // reset the handler
    notify_sigint->setEnabled(true);
#endif
}

void main_window::keyPressEvent(QKeyEvent *event)
{
    (void)event;
    // hack
    line_edit->setFocus();
}

void main_window::timerEvent(QTimerEvent *event)
{
    const auto id = event->timerId();
    if(id == clear_clipboard_timer) {
#ifdef NO_GOOD
        const auto mode = QClipboard::Clipboard;
#else
        const auto mode = QClipboard::Selection;
#endif
        QApplication::clipboard()->clear(mode);
        log_info("Cleared clipboard after timeout.");
    } else if(id == lock_db_timer) {
        // TODO: somehow the timer event keeps running on windows after db was locked
        log_info("Locked Database after timeout.");
        lock_db();
        update_list_from_db();
    } else {
        log_err("Unknown timeout occurred.");
    }
}

void main_window::open_db(const std::string &db_file)
{
    if(db && db->dirty()
       && !askyesno("Db was modified. Discard changes?")) {
        return;
    }

    std::string password;
    if(!passworddialog(password))
        return;
    db.reset(new pw_store_api_cxx::pwstore_api(db_file, password));

    sync_button->setEnabled(false);
    if(!db || !*db || db->locked()) {
        lock_button->setText("lock/unlock");
        lock_button->setEnabled(false);
    } else if(!db->locked()) {
        lock_button->setText("lock");
        lock_button->setEnabled(true);
        restart_db_lock_timer();
        edit_mode->show();
        lock_button->show();
        sync_button->show();
        show_all->show();
        create_button->hide();

        line_edit->show();
        list->show();
        open_lru->hide();

        if(!db->empty()) {
            const auto mod_time = db->time_of_last_write();
            log_info("Authenticity verified. Date of last modification: " +
                     mod_time);
            // TODO: Verify integrity by comparing dates.
        }
    }
    update_list_from_db();
}

void main_window::restart_db_lock_timer()
{
    stop_db_lock_timer();
    lock_db_timer = startTimer(1000 * 120);
}

void main_window::stop_db_lock_timer()
{
    if(lock_db_timer != -1) {
        killTimer(lock_db_timer);
        lock_db_timer = -1;
    }
}

void main_window::create_pressed()
{
    QString filename = QFileDialog::getSaveFileName(
        this, tr("Create Database"), QDir::home().absolutePath());

    if(filename.isEmpty())
        return;

    open_db(filename.toStdString());
}

void main_window::open_pressed()
{
    QString filename = QFileDialog::getOpenFileName(
        this, tr("Open Database"), QDir::home().absolutePath());

    if(filename.isEmpty())
        return;

    open_db(filename.toStdString());
}

void main_window::exit_pressed()
{
    if(db && db->dirty())
        if(!askyesno("Db was modified. Quit without saving?"))
            return;

    QApplication::quit();
}
namespace
{
void add_all_to_list(
    const std::list<
        std::tuple<pw_store::data_type::id_type, pw_store::data_type>> &content,
    QListWidget *list)
{
    for(const auto &c : content) {
        list_entry *item = new list_entry(list, std::get<0>(c));
        item->setText(QString::fromStdString(std::to_string(std::get<0>(c)) +
                                             std::get<1>(c).to_string()));
        list->addItem(item);
    }
}
}

void main_window::update_list_from_db()
{
    // remove all list_entries
    list->clear();

    // DEBUG
    if(!db || !*db) {
        log_err("invalid state of db.(MW_ULFD)");
        return;
    }

    if(db->locked())
        return;
    const std::string filter_string = line_edit->text().toStdString();
    if(filter_string.empty()) {
        // No filter input from user. Show all by default?
        if(show_all_checked) {
            std::list<std::tuple<pw_store::data_type::id_type,
                                 pw_store::data_type>> content;
            db->dump(content);
            add_all_to_list(content, list);
        }
        return;
    }

    std::list<std::tuple<pw_store::data_type::id_type, pw_store::data_type>>
    matches;
    db->lookup(matches, filter_string, {});
    add_all_to_list(matches, list);
}

bool main_window::unlock_db()
{
    std::string password;
    if(!passworddialog(password) || !db->unlock(password)) {
        log_err("Unlock failed.");
        lock_button->setText("unlock");
        lock_button->setEnabled(true);
        return false;
    } else {
        lock_button->setText("lock");
        lock_button->setEnabled(true);
        return true;
    }
}

void main_window::lock_db()
{
    db->lock();
    lock_button->setText("unlock");
    lock_button->setEnabled(true);
}

void main_window::lock_pressed()
{
    // DEBUG
    if(!db || !*db) {
        log_err("invalid state of db.");
        lock_button->setText("lock/unlock");
        lock_button->setEnabled(false);
        return;
    }

    if(db->locked()) {
        if(unlock_db())
            restart_db_lock_timer();
    } else if(!db->locked()) {
        lock_db();
        stop_db_lock_timer();
    }

    update_list_from_db();
}

void main_window::sync_pressed()
{
    // DEBUG
    if(!db || !*db) {
        log_err("invalid state of db.");
        return;
    }

    // db might be locked, try to unlock and sync
    if(db->locked()) {
        if(!unlock_db()) {
            log_err("could not sync database. Unlocking failed.");
            return;
        }
        restart_db_lock_timer();
    }

    if(!db->sync()) {
        log_err("could not write database.");
        return;
    }
    sync_button->setEnabled(false);
}

void main_window::edit_mode_pressed()
{
    editing = !editing;
    if(!editing) {
        add_entry->hide();
        remove_entry->hide();
        modify_entry->hide();
        edit_mode->setText("edit..");
        restart_db_lock_timer();
        log_info("Lookup mode");
    } else {
        add_entry->show();
        remove_entry->show();
        modify_entry->show();
        edit_mode->setText("stop editing");
        stop_db_lock_timer();
        log_info("Edit mode");
    }
}

void main_window::add_entry_pressed()
{
    pw_store::data_type date;
    if(open_three_inputs_window(date, false)) {
        log_err("Invalid database state.(MW_AEP_1)");
        return;
    }

    // DEBUG
    if(!db || !*db) {
        log_err("invalid state of db.(MW_AEP)");
        return;
    }

    if(db->locked()) {
        log_err("Invalid state of db: database locked in edit mode.");
        if(!unlock_db())
            return;
    }

    if(!db->add(date)) {
        log_err("Invalid database state.(MW_AEP_2)");
        return;
    }

    update_list_from_db();

    sync_button->setEnabled(true);
}

void main_window::remove_entry_pressed()
{
    const auto selected = list->currentItem();
    // is an entry selected?
    if(!selected) {
        log_err("Invalid database state.(MW_REP_1)");
        return;
    }

    list_entry *entry = dynamic_cast<list_entry *>(selected);
    const auto id = entry->id;
    pw_store::data_type date;
    if(!db->get(id, date)) {
        log_err("Invalid database state.(MW_REP_2)");
        return;
    }

    if(!askyesno("Really remove entry: " + std::to_string(id) + "?"))
        return;

    std::vector<pw_store::data_type::id_type> uids{id};
    if(!db->remove(uids)) {
        log_err("Invalid database state.(MW_REP_3)");
        return;
    }

    update_list_from_db();

    sync_button->setEnabled(true);
}

void main_window::modify_entry_id(pw_store::data_type::id_type id)
{
    pw_store::data_type date;
    if(!db->get(id, date)) {
        log_err("Invalid database state.(MW_MEP_2)");
        return;
    }

    // TODO: sensible error messages: e.g. cancel is no error
    if(open_three_inputs_window(date, true)) {
        log_err("Invalid database state.(MW_MEP_3)");
        return;
    }

    std::vector<pw_store::data_type::id_type> uids{id};
    if(!db->remove(uids)) {
        log_err("Invalid database state.(MW_MEP_4)");
        return;
    }
    if(!db->add(date)) {
        log_err("Invalid database state.(MW_MEP_5)");
        return;
    }

    update_list_from_db();

    sync_button->setEnabled(true);
}

void main_window::modify_entry_pressed()
{
    const auto selected = list->currentItem();
    // is an entry selected?
    if(!selected) {
        log_err("Invalid database state.(MW_MEP_1)");
        return;
    }

    list_entry *entry = dynamic_cast<list_entry *>(selected);
    const auto id = entry->id;
    modify_entry_id(id);
}

bool main_window::passworddialog(std::string &password)
{
    bool ok;
    QString text = QInputDialog::getText(
        this, tr("QInputDialog::getText()"), tr("Password:"),
        QLineEdit::NoEcho, QDir::home().dirName(), &ok);
    if(ok && !text.isEmpty())
        password.assign(text.toStdString());
    else
        password.assign("");

    return ok;
}

bool main_window::askyesno(const std::string &question)
{
    QMessageBox::StandardButton reply =
        QMessageBox::question(this, "", QString::fromStdString(question),
                              QMessageBox::Yes | QMessageBox::No);
    return reply == QMessageBox::Yes;
}

void main_window::line_edit_text_changed(const QString &)
{
    // Is a database open and unlocked?
    if(!db || !*db || db->locked())
        return;

    update_list_from_db();
}

void main_window::list_item_activated(QListWidgetItem *item)
{
    list_entry *entry = dynamic_cast<list_entry *>(item);

    // DEBUG
    if(!db || !*db || db->locked()) {
        log_err("Invalid database state.(MW_LIA_1)");
        return;
    }

    if(editing)
        return modify_entry_id(entry->id);
    pw_store::data_type date;
    if(!db->get(entry->id, date)) {
        log_err("Invalid database state.(MW_LIA_2)");
        return;
    }

    QClipboard *clipboard = QApplication::clipboard();
#ifdef NO_GOOD
    const auto mode = QClipboard::Clipboard;
#else
    // use selection buffer for non-windows systems
    const auto mode = clipboard->supportsSelection() ? QClipboard::Selection
                                                     : QClipboard::Clipboard;
#endif
    clipboard->setText(QString::fromStdString(date.password), mode);

    // clear clipboard after 20seconds
    clear_clipboard_timer = startTimer(1000 * 20);
}

bool main_window::open_three_inputs_window(pw_store::data_type &date,
                                           bool modify_existing)
{
    const std::string caption =
        modify_existing ? "Modify entry:" : "Create entry:";

    QDialog dialog(this);
    QFormLayout form(&dialog);
    form.addRow(new QLabel(QString::fromStdString(caption)));

    QLineEdit *edit_url = new QLineEdit(&dialog);
    QString label_url = QString("URL: ");
    QLineEdit *edit_user = new QLineEdit(&dialog);
    QString label_user = QString("Username: ");
    QLineEdit *edit_pw = new QLineEdit(&dialog);
    QString label_pw = QString("Password: ");

    if(modify_existing) {
        edit_url->setText(QString::fromStdString(date.url_string));
        edit_user->setText(QString::fromStdString(date.username));
        edit_pw->setText(QString::fromStdString(date.password));
    }

    form.addRow(label_url, edit_url);
    form.addRow(label_user, edit_user);
    form.addRow(label_pw, edit_pw);

    std::string ascii_set;
    for(int i = 0; i < 255; i++)
        if(isprint(i))
            ascii_set.push_back(char(i));

    QLineEdit *edit_pw_ascii_set = new QLineEdit(&dialog);
    QString label_pw_ascii_set = QString("Used symbols: ");
    edit_pw_ascii_set->setText(QString::fromStdString(ascii_set));
    form.addRow(label_pw_ascii_set, edit_pw_ascii_set);

    QPushButton *create_password = new QPushButton("&new password");
    form.addRow(create_password);

    connect(create_password, &QPushButton::pressed, [&]() {
        std::string password;
        if(db->gen_passwd("", "", password, false,
                          edit_pw_ascii_set->text().toStdString()))
            edit_pw->setText(QString::fromStdString(password));
    });

    // Add standard buttons (Cancel/Ok) at the bottom of the dialog
    QDialogButtonBox button_box(QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
                                Qt::Horizontal, &dialog);
    form.addRow(&button_box);
    QObject::connect(&button_box, SIGNAL(accepted()), &dialog, SLOT(accept()));
    QObject::connect(&button_box, SIGNAL(rejected()), &dialog, SLOT(reject()));

    if(dialog.exec() != QDialog::Accepted)
        return false;

    date.url_string = edit_url->text().toStdString();
    date.username = edit_user->text().toStdString();
    date.password = edit_pw->text().toStdString();

    return false;
}

void main_window::log_info(const std::string &msg)
{
    log_msgs.push_front(msg);
    log_msgs.resize(1);

    statusBar()->showMessage(tr(msg.c_str()), 120000);
}

void main_window::log_err(const std::string &msg)
{
    const std::string msg_tmp = "Error: " + msg;
    log_msgs.push_front(msg_tmp);
    log_msgs.resize(20);

    statusBar()->showMessage(tr(msg_tmp.c_str()), 120000);
}

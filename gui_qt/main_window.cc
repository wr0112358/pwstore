#include "main_window.hh"

#include <iostream>
#include <QApplication>
#include <QDir>
#include <QFileDialog>
#include <QGridLayout>
#include <QInputDialog>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QStatusBar>
#include <QTimer>

#include "list_entry.hh"
#include "../pwstore_api_cxx.hh"

main_window::main_window(QWidget *parent, const std::string &default_db)
    : QMainWindow(parent, Qt::FramelessWindowHint)
{
    layout = new QGridLayout;

    list = new QListWidget(this);
    list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    list->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    line_edit = new QLineEdit();
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
    connect(open_button, SIGNAL(pressed()), this, SLOT(open_pressed()));
    connect(exit_button, SIGNAL(pressed()), this, SLOT(exit_pressed()));
    connect(lock_button, SIGNAL(pressed()), this, SLOT(lock_pressed()));
    connect(sync_button, SIGNAL(pressed()), this, SLOT(sync_pressed()));

    connect(QApplication::clipboard(), SIGNAL(changed(QClipboard::Mode)), this,
            SLOT(clipboard_changed(QClipboard::Mode)));
    connect(QApplication::clipboard(), SIGNAL(selectionChanged()), this,
            SLOT(clipboard_selection_changed()));

    // 5 columns wide for now
    layout->addWidget(open_button, 0, 4, 1, 1);
    layout->addWidget(exit_button, 1, 4, 1, 1);
    layout->addWidget(lock_button, 0, 3, 1, 1);
    layout->addWidget(sync_button, 1, 3, 1, 1);
    layout->addWidget(line_edit, 2, 0, 1, 5);
    layout->addWidget(list, 3, 0, 1, 5);

    // QMainWindow needs a central widget to be able to use a layout.
    QWidget * central_widget = new QWidget;
    layout->setSpacing(1);
    central_widget->setLayout(layout);
    setCentralWidget(central_widget);
    line_edit->setFocus();
    line_edit->setFocusPolicy(Qt::StrongFocus);

    if(!default_db.empty())
        open_db(default_db);
}

main_window::~main_window() {}

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
        std::cout << "Clearing clipboard after timeout.\n";
    } else if(id == lock_db_timer) {
        std::cout << "Locking database after timeout.\n";
        lock_db();
        update_list_from_db();
    } else {
        std::cout << "Unknown timeout occurred.\n";
    }
}

void main_window::open_db(const std::string &db_file)
{
    std::string password;
    if(!passworddialog(password))
        return;
    db.reset(new pw_store_api_cxx::pwstore_api(db_file, password));

    if(!db || !*db) {
        lock_button->setText("lock/unlock");
        lock_button->setEnabled(false);
        sync_button->setEnabled(false);
    } else if(db->locked()) {
        lock_button->setText("unlock");
        lock_button->setEnabled(true);
        sync_button->setEnabled(false);
    } else if(!db->locked()) {
        lock_button->setText("lock");
        lock_button->setEnabled(true);
        sync_button->setEnabled(false);
    }
    restart_db_lock_timer();
    update_list_from_db();
}

void main_window::restart_db_lock_timer()
{
    if(lock_db_timer != -1)
        killTimer(lock_db_timer);
    lock_db_timer = startTimer(1000 * 30);
}

void main_window::open_pressed()
{
    QString filename = QFileDialog::getOpenFileName(this, tr("Open Database"),
                                                    QDir::home().absolutePath(),
                                                    tr("Database Files (.my_secrets*)"));
    std::cout << "void main_window::open: \"" << filename.toStdString()
              << "\"\nhome: " << QDir::home().absolutePath().toStdString()
              << "\n";
    if(filename.isEmpty())
        return;

    // TODO: handle already open//dirty case
    open_db(filename.toStdString());
}

void main_window::exit_pressed()
{
    if(db && db->dirty())
        if(!askyesno("Db was modifed. Quit without saving?"))
            return;

    QApplication::quit();
}

void main_window::update_list_from_db()
{
    // remove all list_entries
    list->clear();

    if(!db || !*db)
        // This should not happen!
        std::cerr << "Error: invalid state of db.\n";
    else if(db->locked()) {
        // TODO: clear list_container
    } else if(!db->locked()) {
        const std::string filter_string = line_edit->text().toStdString();
        if (filter_string.empty())
            return;
        std::list<std::tuple<pw_store::data_type::id_type, pw_store::data_type>>
        matches;
        db->lookup(matches, filter_string, {});
        for (const auto &match : matches) {
            list_entry *item = new list_entry(list, std::get<0>(match));
            item->setText(
                QString::fromStdString(std::to_string(std::get<0>(match)) +
                                       std::get<1>(match).to_string()));
            list->addItem(item);
        }
    }
}

void main_window::lock_db()
{
    db->lock();
    lock_button->setText("unlock");
    lock_button->setEnabled(true);
    sync_button->setEnabled(false);
}

void main_window::lock_pressed()
{
    // DEBUG
    if(!db || !*db) {
        // This should not happen!
        std::cerr << "Error: invalid state of db.\n";
        lock_button->setText("lock/unlock");
        lock_button->setEnabled(false);
        sync_button->setEnabled(false);
        return;
    }

    if(db->locked()) {
        std::string password;
        if(!passworddialog(password)
           || !db->unlock(password)) {
            std::cerr << "Error: Unlock failed.\n";
            lock_button->setText("unlock");
            lock_button->setEnabled(true);
            sync_button->setEnabled(false);
        } else {
            lock_button->setText("lock");
            lock_button->setEnabled(true);
            sync_button->setEnabled(false);
            restart_db_lock_timer();
        }
    } else if(!db->locked()) {
        lock_db();
    }

    update_list_from_db();
}

void main_window::sync_pressed()
{
    // DEBUG
    if(!db || !*db) {
        std::cerr << "Error: invalid state of db.\n";
        return;
    }

    if(!db->sync()) {
        std::cerr << "Error: could write database.\n";
        return;
    }
    sync_button->setEnabled(false);
}

bool main_window::passworddialog(std::string &password)
{
    bool ok;
    QString text = QInputDialog::getText(
        this, tr("QInputDialog::getText()"), tr("Password:"),
        //QLineEdit::PasswordEchoOnEdit,
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
    update_list_from_db();
}

void main_window::list_item_activated(QListWidgetItem *item)
{
    list_entry *entry = dynamic_cast<list_entry *>(item);
    // TODO: get key displayed in selected entry and provide value to user.
    std::cout << "list_item_activated(id = " << std::to_string(entry->id) << ")\n";

    // DEBUG
    if(!db || !*db || db->locked()) {
        std::cerr << "Error: Invalid database state.(MW_LIA_1)\n";
        return;
    }

    pw_store::data_type date;
    if(!db->get(entry->id, date)) {
        std::cerr << "Error: Invalid database state.(MW_LIA_2)\n";
        return;
    }

    QClipboard *clipboard = QApplication::clipboard();
#ifdef NO_GOOD
    const auto mode = QClipboard::Clipboard;
#else
    // use selection buffer for non-windows systems
    const auto mode = QClipboard::Selection;
#endif
    if(clipboard->supportsSelection()) {
        clipboard->setText(QString::fromStdString(date.password), mode);
    } else {
        std::cerr << "Error: Invalid database state.(MW_LIA_3)\n";
        return;
    }

    // clear clipboard after 20seconds
    clear_clipboard_timer = startTimer(1000 * 20);
}

void main_window::clipboard_changed(QClipboard::Mode mode)
{
    std::cout << "clipboard_changed: " << mode << "\n";
}

void main_window::clipboard_selection_changed()
{
    std::cout << "clipboard_selection_changed\n";
}

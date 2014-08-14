#include "list_entry.hh"

#include <iostream>

list_entry::list_entry(QListWidget *parent, pw_store::data_type::id_type id)
    : QListWidgetItem(parent), id(id)
{
}

void list_entry::keyPressEvent(QKeyEvent *event)
{
    (void)event;
    std::cout << "void record::list_entry::keyPressEvent\n";
}

void list_entry::mousePressEvent(QMouseEvent *event)
{
    (void)event;
    std::cout
        << "void record::list_entry::mousePressEvent(QMouseEvent *event)\n";
}

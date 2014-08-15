#ifndef _LIST_ENTRY_HH_
#define _LIST_ENTRY_HH_

#include <QListWidgetItem>
#include "../pwstore.hh"

class list_entry : public QListWidgetItem
{
public:
    list_entry(QListWidget *parent, pw_store::data_type::id_type id);

    void mousePressEvent(QMouseEvent *event);
    void keyPressEvent(QKeyEvent *event);

    const pw_store::data_type::id_type id;
};

#endif

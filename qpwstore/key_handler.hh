#ifndef _KEY_HANDLER_HH_
#define _KEY_HANDLER_HH_

#include <QObject>

class QEvent;

class key_handler : public QObject {
public:
    key_handler() : QObject() {}
    ~key_handler() {}
    bool eventFilter(QObject *object, QEvent *event);
};

#endif

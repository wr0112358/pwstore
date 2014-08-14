#include "key_handler.hh"

#include <QApplication>
#include <QEvent>
#include <QGuiApplication>
#include <QKeyEvent>

#include <iostream>

bool key_handler::eventFilter(QObject *object, QEvent *event)
{
    bool quit = false;
    bool cancel = false; // TODO: kill text in line_edit, close menus etc..

    bool alt = false;
    bool ctrl = false;
    switch(QGuiApplication::keyboardModifiers()) {
    case Qt::ControlModifier:
        ctrl = true;
        break;
    case Qt::AltModifier:
        alt = true;
        break;
    }

    if(event->type() == QEvent::KeyPress) {
        QKeyEvent *key_event = (QKeyEvent *)event;

        switch(key_event->key()) {
        case Qt::Key_F4:
            if(alt)
                quit = true;
        case Qt::Key_Q:
        case Qt::Key_W:
            if(ctrl)
                quit = true;
            break;
        case Qt::Key_Escape:
            cancel = true;
            break;
        case Qt::Key_G:
            if(ctrl)
                cancel = true;
            break;
        default:
            break;
        }
    }

    if(quit)
        QApplication::quit();
    (void)cancel;

    return QObject::eventFilter(object,event);
}

#include <iostream>
#include <QApplication>
#include <QGraphicsEllipseItem>

#include "main_window.hh"

int main(int argc, char **argv)
{
    QApplication app(argc, argv);

    main_window mw(0, (argc > 1 ? argv[1] : ""));
    mw.show();
    const auto ret = app.exec();
    std::cout << "test: after QApplication::Quit()\n";
    return ret;
}

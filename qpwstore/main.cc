#include <iostream>
#include <QApplication>
#include <QGraphicsEllipseItem>
#include <sys/stat.h>

#include "main_window.hh"

int main(int argc, char **argv)
{
    QApplication app(argc, argv);

    struct stat s;
    if(argc > 1 && stat(argv[1], &s)) {
        std::cerr << "Invalid database specified.\n";
        return -1;
    }

    main_window mw(0, (argc > 1 ? argv[1] : ""));
    mw.show();
    const auto ret = app.exec();
    std::cout << "test: after QApplication::Quit()\n";
    return ret;
}

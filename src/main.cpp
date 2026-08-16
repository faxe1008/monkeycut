#include <QApplication>
#include "ui/MainWindow.h"

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    app.setOrganizationName("monkeycut");
    app.setOrganizationDomain("localhost");
    app.setApplicationName("MonkeyCut");
    app.setApplicationVersion("0.1.0");

    MainWindow w;
    w.show();
    return app.exec();
}
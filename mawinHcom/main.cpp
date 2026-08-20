#include "mainwindow.h"
#include <QDebug>
#include <QApplication>
#include <QThread>
#include "log.h"
int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    Logger::instance()->init("mawinHcom.log");
    MainWindow w;
    w.show();

    int result = a.exec();

    return result;
}

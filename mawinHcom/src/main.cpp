#include "mainwindow.h"
#include <QDebug>
#include <QApplication>
#include <QThread>
#include "log.h"
int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    Logger::init("mawinHcom.log");
    Logger::info("应用程序启动");

    MainWindow w;
    w.show();

    int result = a.exec();

    Logger::info("应用程序退出");
    return result;
}

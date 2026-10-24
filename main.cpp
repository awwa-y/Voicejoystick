#include "mainwindow.h"
#include "protocolframe.h"
#include "commandprocessor.h"
#include <QDebug>
#include <QApplication>
#include <QThread>
#include <QIcon>
#include "log.h"

// 程序入口：初始化日志 -> 建主窗口 -> 进事件循环
// 日志只在这里 init 一次（单例，别处在 MainWindow 里重复 init 会清空日志文件）
int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    a.setWindowIcon(QIcon(":/app.ico"));   // 窗口左上角/任务栏图标

    Logger::instance()->init("mawinHcom.log");
    MainWindow w;
    w.show();

    int result = a.exec();

    return result;
}

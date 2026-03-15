#include "MainWindow.h"
#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    MainWindow w;
    w.hide(); // 启动时隐藏主窗口，仅托盘可见
    return a.exec();
}

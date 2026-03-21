#include "MainWindow.h"
#include <QApplication>
#include <QIcon>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    a.setApplicationName(QStringLiteral("Words-Bin"));
    a.setApplicationDisplayName(QStringLiteral("Words-Bin"));
    const QIcon appIcon(QStringLiteral(":/icons/words-bin_logo.ico"));
    if (!appIcon.isNull()) {
        a.setWindowIcon(appIcon);
    }
    a.setQuitOnLastWindowClosed(false);// 允许关闭主窗口后继续在系统托盘运行
    MainWindow w;
    w.show(); // 启动时显示主窗口，用户可以选择将其靠边隐藏
    return a.exec();
}

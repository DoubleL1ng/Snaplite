#include "MainWindow.h"
#include "TrayIcon.h"
#include <QWidget>
#include <QPainter>
#include <QGraphicsDropShadowEffect>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), tray(new TrayIcon(this))
{
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
    setAttribute(Qt::WA_TranslucentBackground);
    resize(400, 300);
    setupUI();
}

MainWindow::~MainWindow() {}

void MainWindow::setupUI()
{
    // 这里可添加苹果风格UI控件，后续补充
}

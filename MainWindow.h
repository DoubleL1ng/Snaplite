#pragma once
#include <QMainWindow>

class TrayIcon;

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private:
    TrayIcon *tray;
    void setupUI();
};

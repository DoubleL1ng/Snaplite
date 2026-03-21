#pragma once
#include "AppSettings.h"
#include <QMainWindow>
#include <QTimer>
#include <QPropertyAnimation>
#include <QEnterEvent>
#include <QShowEvent>
#include <QVBoxLayout>
#include <QPushButton>
#include <QListWidget>
#include <QLabel>
#include <QtGlobal>

class TrayIcon;
class CaptureTool;
class QToolButton;

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

protected:
    void showEvent(QShowEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;

private slots:
    void checkEdgeDocking();
    void onClipboardChanged();
    void addClipboardItem(const QVariant &data);
    void onItemClicked(QListWidgetItem *item); 
    void onCustomContextMenu(const QPoint &pos); 
    void onSettingsUpdated();
    void clearClipboardHistory();
    void onSettingsDialogVisibilityChanged(bool visible);
    void onRegionCaptureStateChanged(bool active);
    void onPreviewDialogStateChanged(bool visible);
    void onOpenSettings();
    void onOpenGitHub();
    void onTogglePinned(bool checked);
    void onRequestExit();
    void onTrayLeftClicked();

private:
    void setupUI();
    void applyTheme();
    bool isLightTheme() const;
    void updateThemeToggleUi();
    QString currentThemeMode() const;
    void updateTopBarIcons();
    int currentHistoryLimit() const;
    void enforceHistoryLimit();
    void setDockContentVisible(bool visible);
    bool shouldSuppressSidebar() const;
    QRect dockTriggerRect() const;
    void updateDockMask();
    void dockSidebar(bool animated);
    void expandSidebar(bool force = false);
    void revealSidebarWithHold();
    void updatePinnedUi();
    void applySuppressionState();
    void onToggleTheme();
    
    TrayIcon *tray; 
    CaptureTool *captureTool;
    
    QWidget *centralWidget;
    QVBoxLayout *mainLayout;
    QListWidget *historyList;
    QToolButton *githubButton = nullptr;
    QToolButton *pinButton = nullptr;
    QToolButton *themeToggleButton = nullptr;
    QToolButton *settingsButton = nullptr;
    QToolButton *exitButton = nullptr;
    
    QTimer *edgeTimer;
    QPropertyAnimation *animation;
    QTimer *hoverRevealTimer = nullptr;
    QTimer *leaveDockTimer = nullptr;
    bool isDocked; 
    int normalWidth; 
    int dockTriggerWidth = 2;
    int dockTriggerHeight = 96;
    int rightMargin = 10;
    QTimer *trayRevealHoldTimer = nullptr;
    bool trayRevealHoldActive = false;
    bool startupHoldApplied = false;
    bool sidebarPinned = false;
    bool settingsDialogVisible = false;
    bool regionCaptureActive = false;
    bool regionCaptureShouldHideSidebar = false;
    bool previewDialogVisible = false;
    QString themeMode = AppSettings::kDefaultThemeMode;
    
    // 新增：用于防止剪贴板无限复制的锁
    bool ignoreClipboardChange = false;
    QString lastClipboardSignature;
    qint64 lastClipboardChangeAtMs = 0;
};
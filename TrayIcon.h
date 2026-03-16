#pragma once
#include <QAbstractNativeEventFilter>
#include <QKeySequence>
#include <QObject>
#include <QString>
#include <QSystemTrayIcon>

class QAction;
class CaptureTool;
class QMenu;

class TrayIcon : public QObject, public QAbstractNativeEventFilter
{
    Q_OBJECT
public:
    explicit TrayIcon(QObject *parent = nullptr);
    ~TrayIcon() override;

    bool nativeEventFilter(const QByteArray &eventType, void *message, qintptr *result) override;
    void openSettingsDialog();

    // MainWindow passes in its shared capture tool instance.
    void setCaptureTool(CaptureTool *tool) { captureTool = tool; }

signals:
    void settingsUpdated();
    void clearHistoryRequested();
    void settingsDialogVisibilityChanged(bool visible);
    void trayLeftClicked();

private slots:
    void onActivated(QSystemTrayIcon::ActivationReason reason);
    void showSettings();
    void exitApp();

private:
    void createTrayMenu();
    void loadSettings();
    bool applyCaptureHotkey(const QKeySequence &sequence);

#ifdef Q_OS_WIN
    bool registerHotkey(const QKeySequence &sequence);
    void unregisterHotkey();
    bool parseHotkey(const QKeySequence &sequence, unsigned int &modifiers, unsigned int &virtualKey) const;
#endif

    QSystemTrayIcon *trayIcon = nullptr;
    QMenu *trayMenu = nullptr;
    QAction *settingsAction = nullptr;
    QString captureHotkey = QStringLiteral("Ctrl+Shift+A");
    int hotkeyId = 1001;
    bool hotkeyRegistered = false;
    CaptureTool *captureTool = nullptr;
};
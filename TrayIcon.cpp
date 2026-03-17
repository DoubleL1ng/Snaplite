#include "TrayIcon.h"
#include "AppSettings.h"
#include "CaptureTool.h"
#include "SettingsDialog.h"

#include <QAction>
#include <QApplication>
#include <QCoreApplication>
#include <QDialog>
#include <QDir>
#include <QIcon>
#include <QMenu>
#include <QSettings>
#include <QStyle>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace {
#ifdef Q_OS_WIN
constexpr auto kAutoStartRegPath = "HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run";
constexpr auto kAutoStartValueName = "Words-Bin";
constexpr auto kLegacyAutoStartValueName = "SnipLite";
#endif
} // namespace

TrayIcon::TrayIcon(QObject *parent) : QObject(parent)
{
    QIcon icon = QIcon::fromTheme("camera-photo");
    if (icon.isNull()) {
        icon = QApplication::style()->standardIcon(QStyle::SP_ComputerIcon);
    }

    loadSettings();

    trayIcon = new QSystemTrayIcon(icon, this);
    createTrayMenu();
    connect(trayIcon, &QSystemTrayIcon::activated, this, &TrayIcon::onActivated);
    trayIcon->show();

    qApp->installNativeEventFilter(this);

    QKeySequence configuredHotkey =
        QKeySequence::fromString(captureHotkey, QKeySequence::PortableText);
    if (!applyCaptureHotkey(configuredHotkey)) {
        const QKeySequence fallbackHotkey(AppSettings::kDefaultCaptureHotkey);
        applyCaptureHotkey(fallbackHotkey);
    }
}

TrayIcon::~TrayIcon()
{
#ifdef Q_OS_WIN
    unregisterHotkey();
#endif
    qApp->removeNativeEventFilter(this);
}

void TrayIcon::createTrayMenu()
{
    trayMenu = new QMenu();
    settingsAction = trayMenu->addAction(QStringLiteral("\u8bbe\u7f6e"));
    QAction *exitAction = trayMenu->addAction(QStringLiteral("\u9000\u51fa"));

    connect(settingsAction, &QAction::triggered, this, &TrayIcon::openSettingsDialog);
    connect(exitAction, &QAction::triggered, this, &TrayIcon::exitApp);

    trayIcon->setContextMenu(trayMenu);
}

void TrayIcon::openSettingsDialog() { showSettings(); }

void TrayIcon::onActivated(QSystemTrayIcon::ActivationReason reason)
{
    if (reason == QSystemTrayIcon::Trigger) {
        emit trayLeftClicked();
    }
}

void TrayIcon::showSettings()
{
    SettingsDialog dialog;
    emit settingsDialogVisibilityChanged(true);
    const int dialogResult = dialog.exec();
    emit settingsDialogVisibilityChanged(false);

    if (dialogResult != QDialog::Accepted) {
        return;
    }

    const bool hotkeyApplied = applyCaptureHotkey(dialog.captureHotkey());

    QSettings settings = AppSettings::createSettings();
    settings.setValue(AppSettings::kCaptureHotkey, captureHotkey);
    settings.setValue(AppSettings::kSavePath, dialog.savePath());
    settings.setValue(AppSettings::kSaveFormat, dialog.saveFormat());
    settings.setValue(AppSettings::kHideSidebar, dialog.hideSidebar());
    settings.setValue(AppSettings::kHistoryMaxItems, dialog.historyMaxItems());

#ifdef Q_OS_WIN
    QSettings bootSettings(QString::fromLatin1(kAutoStartRegPath), QSettings::NativeFormat);
    if (dialog.autoStartEnabled()) {
        QString appPath = QDir::toNativeSeparators(QCoreApplication::applicationFilePath());
        bootSettings.setValue(QString::fromLatin1(kAutoStartValueName), "\"" + appPath + "\"");
        bootSettings.remove(QString::fromLatin1(kLegacyAutoStartValueName));
    } else {
        bootSettings.remove(QString::fromLatin1(kAutoStartValueName));
        bootSettings.remove(QString::fromLatin1(kLegacyAutoStartValueName));
    }
#endif

    emit settingsUpdated();
    if (dialog.shouldClearHistory()) {
        emit clearHistoryRequested();
    }

    if (!hotkeyApplied && trayIcon) {
        trayIcon->showMessage(
            QStringLiteral("\u8bbe\u7f6e"),
            QStringLiteral("\u65b0\u5feb\u6377\u952e\u4e0d\u53ef\u7528\uff0c\u5df2\u4fdd\u7559\u65e7\u5feb\u6377\u952e\uff0c\u5176\u4ed6\u8bbe\u7f6e\u5df2\u751f\u6548\u3002"),
            QSystemTrayIcon::Warning,
            3000);
    }
}

void TrayIcon::exitApp() { QApplication::quit(); }

void TrayIcon::loadSettings()
{
    QSettings settings = AppSettings::createSettings();
    captureHotkey =
        settings.value(AppSettings::kCaptureHotkey, AppSettings::kDefaultCaptureHotkey).toString();
    if (captureHotkey.isEmpty()) {
        captureHotkey = AppSettings::kDefaultCaptureHotkey;
    }
}

bool TrayIcon::applyCaptureHotkey(const QKeySequence &sequence)
{
    QKeySequence targetSequence = sequence;
    if (targetSequence.isEmpty()) {
        targetSequence = QKeySequence(AppSettings::kDefaultCaptureHotkey);
    }

#ifdef Q_OS_WIN
    const QString targetText = targetSequence.toString(QKeySequence::PortableText);
    if (hotkeyRegistered && targetText == captureHotkey) {
        return true;
    }

    unsigned int modifiers = 0;
    unsigned int virtualKey = 0;
    if (!parseHotkey(targetSequence, modifiers, virtualKey) || modifiers == 0 || virtualKey == 0) {
        return false;
    }

    const int probeHotkeyId = hotkeyId + 1;
    if (RegisterHotKey(nullptr, probeHotkeyId, modifiers, virtualKey) == 0) {
        return false;
    }
    UnregisterHotKey(nullptr, probeHotkeyId);

    const QKeySequence oldSequence =
        QKeySequence::fromString(captureHotkey, QKeySequence::PortableText);

    unregisterHotkey();
    if (RegisterHotKey(nullptr, hotkeyId, modifiers, virtualKey) == 0) {
        unsigned int oldModifiers = 0;
        unsigned int oldVirtualKey = 0;
        if (parseHotkey(oldSequence, oldModifiers, oldVirtualKey) && oldModifiers != 0 &&
            oldVirtualKey != 0) {
            if (RegisterHotKey(nullptr, hotkeyId, oldModifiers, oldVirtualKey) != 0) {
                hotkeyRegistered = true;
            }
        }
        return false;
    }

    hotkeyRegistered = true;
#else
    Q_UNUSED(targetSequence);
#endif

    captureHotkey = targetSequence.toString(QKeySequence::PortableText);
    if (captureHotkey.isEmpty()) {
        captureHotkey = AppSettings::kDefaultCaptureHotkey;
    }

    return true;
}

bool TrayIcon::nativeEventFilter(const QByteArray &eventType, void *message, qintptr *result)
{
#ifdef Q_OS_WIN
    Q_UNUSED(eventType);
    MSG *msg = static_cast<MSG *>(message);
    if (msg && msg->message == WM_HOTKEY && static_cast<int>(msg->wParam) == hotkeyId) {
        if (captureTool) {
            captureTool->startCapture();
        }
        if (result) {
            *result = 0;
        }
        return true;
    }
#else
    Q_UNUSED(eventType);
    Q_UNUSED(message);
    Q_UNUSED(result);
#endif

    return false;
}

#ifdef Q_OS_WIN
bool TrayIcon::registerHotkey(const QKeySequence &sequence)
{
    unsigned int modifiers = 0;
    unsigned int virtualKey = 0;
    if (!parseHotkey(sequence, modifiers, virtualKey) || modifiers == 0 || virtualKey == 0) {
        return false;
    }

    if (RegisterHotKey(nullptr, hotkeyId, modifiers, virtualKey) == 0) {
        return false;
    }

    hotkeyRegistered = true;
    return true;
}

void TrayIcon::unregisterHotkey()
{
    if (!hotkeyRegistered) {
        return;
    }
    UnregisterHotKey(nullptr, hotkeyId);
    hotkeyRegistered = false;
}

bool TrayIcon::parseHotkey(const QKeySequence &sequence, unsigned int &modifiers, unsigned int &virtualKey) const
{
    modifiers = 0;
    virtualKey = 0;
    if (sequence.count() < 1) {
        return false;
    }

    const QKeyCombination combo = sequence[0];
    const Qt::KeyboardModifiers qtMods = combo.keyboardModifiers();
    const int key = static_cast<int>(combo.key());

    if (combo.key() == Qt::Key_unknown) {
        return false;
    }
    if (qtMods.testFlag(Qt::ControlModifier)) {
        modifiers |= MOD_CONTROL;
    }
    if (qtMods.testFlag(Qt::AltModifier)) {
        modifiers |= MOD_ALT;
    }
    if (qtMods.testFlag(Qt::ShiftModifier)) {
        modifiers |= MOD_SHIFT;
    }
    if (qtMods.testFlag(Qt::MetaModifier)) {
        modifiers |= MOD_WIN;
    }

    if (key >= Qt::Key_A && key <= Qt::Key_Z) {
        virtualKey = static_cast<unsigned int>('A' + (key - Qt::Key_A));
        return true;
    }
    if (key >= Qt::Key_0 && key <= Qt::Key_9) {
        virtualKey = static_cast<unsigned int>('0' + (key - Qt::Key_0));
        return true;
    }
    if (key >= Qt::Key_F1 && key <= Qt::Key_F24) {
        virtualKey = static_cast<unsigned int>(VK_F1 + (key - Qt::Key_F1));
        return true;
    }

    switch (key) {
    case Qt::Key_Print:
        virtualKey = VK_SNAPSHOT;
        return true;
    case Qt::Key_Space:
        virtualKey = VK_SPACE;
        return true;
    case Qt::Key_Insert:
        virtualKey = VK_INSERT;
        return true;
    case Qt::Key_Home:
        virtualKey = VK_HOME;
        return true;
    case Qt::Key_End:
        virtualKey = VK_END;
        return true;
    case Qt::Key_PageUp:
        virtualKey = VK_PRIOR;
        return true;
    case Qt::Key_PageDown:
        virtualKey = VK_NEXT;
        return true;
    default:
        break;
    }
    return false;
}
#endif
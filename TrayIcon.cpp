#include "TrayIcon.h"
#include "CaptureTool.h"

#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGuiApplication>
#include <QIcon>
#include <QKeySequenceEdit>
#include <QLabel>
#include <QMenu>
#include <QSettings>
#include <QStyle>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

TrayIcon::TrayIcon(QObject *parent)
    : QObject(parent)
{
    QIcon icon = QIcon::fromTheme("camera-photo");
    if (icon.isNull()) {
        icon = QApplication::style()->standardIcon(QStyle::SP_ComputerIcon);
    }

    captureTool = new CaptureTool(this);
    connect(captureTool, &CaptureTool::screenshotTaken, this, [](const QPixmap &pixmap) {
        QGuiApplication::clipboard()->setPixmap(pixmap);
    });

    loadSettings();

    trayIcon = new QSystemTrayIcon(icon, this);
    createTrayMenu();
    trayIcon->setContextMenu(trayMenu);
    connect(trayIcon, &QSystemTrayIcon::activated, this, &TrayIcon::onActivated);
    trayIcon->show();

    qApp->installNativeEventFilter(this);
    applyCaptureHotkey(QKeySequence::fromString(captureHotkey, QKeySequence::PortableText));
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

    QAction *startAction = trayMenu->addAction(QStringLiteral("\u65b0\u5efa\u622a\u56fe"));
    QAction *fullAction = trayMenu->addAction(QStringLiteral("\u5168\u5c4f\u622a\u56fe"));
    QAction *windowAction = trayMenu->addAction(QStringLiteral("\u7a97\u53e3\u622a\u56fe"));
    QAction *regionAction = trayMenu->addAction(QStringLiteral("\u533a\u57df\u622a\u56fe"));
    QAction *longAction = trayMenu->addAction(QStringLiteral("\u957f\u622a\u56fe"));

    trayMenu->addSeparator();
    settingsAction = trayMenu->addAction(QStringLiteral("\u8bbe\u7f6e"));
    QAction *historyAction = trayMenu->addAction(QStringLiteral("\u5386\u53f2\u622a\u56fe"));
    trayMenu->addSeparator();
    QAction *exitAction = trayMenu->addAction(QStringLiteral("\u9000\u51fa"));

    connect(startAction, &QAction::triggered, this, [this]() {
        if (captureTool) {
            captureTool->startCapture();
        }
    });
    connect(fullAction, &QAction::triggered, this, [this]() {
        if (captureTool) {
            captureTool->captureFullScreen();
        }
    });
    connect(windowAction, &QAction::triggered, this, [this]() {
        if (captureTool) {
            captureTool->captureWindow();
        }
    });
    connect(regionAction, &QAction::triggered, this, [this]() {
        if (captureTool) {
            captureTool->captureRegion();
        }
    });
    connect(longAction, &QAction::triggered, this, [this]() {
        if (captureTool) {
            captureTool->captureLongScreenshot();
        }
    });
    connect(historyAction, &QAction::triggered, this, &TrayIcon::showHistory);
    connect(settingsAction, &QAction::triggered, this, &TrayIcon::showSettings);
    connect(exitAction, &QAction::triggered, this, &TrayIcon::exitApp);

    if (settingsAction) {
        settingsAction->setText(
            QStringLiteral("\u8bbe\u7f6e\uff08\u622a\u56fe\u5feb\u6377\u952e\uff1a%1\uff09").arg(captureHotkey));
    }
}

void TrayIcon::onActivated(QSystemTrayIcon::ActivationReason reason)
{
    if (reason == QSystemTrayIcon::Trigger) {
        if (captureTool) {
            captureTool->startCapture();
        }
    }
}

void TrayIcon::showHistory()
{
    if (trayIcon) {
        trayIcon->showMessage(
            QStringLiteral("\u5386\u53f2\u622a\u56fe"),
            QStringLiteral("\u8fd9\u4e2a\u529f\u80fd\u8fd8\u5728\u5f00\u53d1\u4e2d\u3002"),
            QSystemTrayIcon::Information,
            2200);
    }
}

void TrayIcon::showSettings()
{
    QDialog dialog;
    dialog.setWindowTitle(QStringLiteral("\u5feb\u6377\u952e\u8bbe\u7f6e"));
    dialog.setModal(true);

    QFormLayout *layout = new QFormLayout(&dialog);
    QKeySequenceEdit *hotkeyEdit = new QKeySequenceEdit(
        QKeySequence::fromString(captureHotkey, QKeySequence::PortableText), &dialog);
    QLabel *hintLabel = new QLabel(
        QStringLiteral("\u5efa\u8bae\u4f7f\u7528 Ctrl/Alt/Shift \u7ec4\u5408\u952e\uff0c\u4f8b\u5982 Ctrl+Shift+A\u3002"),
        &dialog);
    hintLabel->setWordWrap(true);

    layout->addRow(QStringLiteral("\u622a\u56fe\u5feb\u6377\u952e"), hotkeyEdit);
    layout->addRow(hintLabel);

    QDialogButtonBox *buttonBox =
        new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    layout->addWidget(buttonBox);

    connect(buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    QKeySequence sequence = hotkeyEdit->keySequence();
    if (sequence.isEmpty()) {
        sequence = QKeySequence(QStringLiteral("Ctrl+Shift+A"));
    }

    if (!applyCaptureHotkey(sequence)) {
        if (trayIcon) {
            trayIcon->showMessage(
                QStringLiteral("\u5feb\u6377\u952e\u8bbe\u7f6e"),
                QStringLiteral("\u5feb\u6377\u952e\u6ce8\u518c\u5931\u8d25\uff0c\u8bf7\u6362\u4e00\u4e2a\u7ec4\u5408\u952e\u3002"),
                QSystemTrayIcon::Warning,
                2800);
        }
        return;
    }

    QSettings settings(QStringLiteral("SnipLite"), QStringLiteral("SnipLite"));
    settings.setValue(QStringLiteral("shortcuts/capture"), captureHotkey);

    if (trayIcon) {
        trayIcon->showMessage(
            QStringLiteral("\u5feb\u6377\u952e\u8bbe\u7f6e"),
            QStringLiteral("\u622a\u56fe\u5feb\u6377\u952e\u5df2\u66f4\u65b0\u4e3a %1")
                .arg(sequence.toString(QKeySequence::NativeText)),
            QSystemTrayIcon::Information,
            2200);
    }
}

void TrayIcon::exitApp()
{
    QApplication::quit();
}

void TrayIcon::loadSettings()
{
    QSettings settings(QStringLiteral("SnipLite"), QStringLiteral("SnipLite"));
    captureHotkey =
        settings.value(QStringLiteral("shortcuts/capture"), QStringLiteral("Ctrl+Shift+A")).toString();
    if (captureHotkey.isEmpty()) {
        captureHotkey = QStringLiteral("Ctrl+Shift+A");
    }
}

bool TrayIcon::applyCaptureHotkey(const QKeySequence &sequence)
{
#ifdef Q_OS_WIN
    unregisterHotkey();
    if (!registerHotkey(sequence)) {
        return false;
    }
#else
    Q_UNUSED(sequence);
#endif

    captureHotkey = sequence.toString(QKeySequence::PortableText);
    if (captureHotkey.isEmpty()) {
        captureHotkey = QStringLiteral("Ctrl+Shift+A");
    }

    if (settingsAction) {
        settingsAction->setText(
            QStringLiteral("\u8bbe\u7f6e\uff08\u622a\u56fe\u5feb\u6377\u952e\uff1a%1\uff09").arg(captureHotkey));
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
    if (!parseHotkey(sequence, modifiers, virtualKey)) {
        return false;
    }

    if (modifiers == 0 || virtualKey == 0) {
        return false;
    }

    return RegisterHotKey(nullptr, hotkeyId, modifiers, virtualKey) != 0;
}

void TrayIcon::unregisterHotkey()
{
    UnregisterHotKey(nullptr, hotkeyId);
}

bool TrayIcon::parseHotkey(
    const QKeySequence &sequence, unsigned int &modifiers, unsigned int &virtualKey) const
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

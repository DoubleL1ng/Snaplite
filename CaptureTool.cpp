#include "CaptureTool.h"

#include "AppSettings.h"

#include <QCoreApplication>
#include <QCursor>
#include <QDateTime>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QEventLoop>
#include <QFileInfo>
#include <QGuiApplication>
#include <QLabel>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QKeyEvent>
#include <QPushButton>
#include <QScreen>
#include <QSettings>
#include <QVBoxLayout>
#include <QWidget>
#include <QtGlobal>

class CapturePreviewDialog : public QDialog {
public:
    explicit CapturePreviewDialog(const QPixmap &pixmap, QWidget *parent = nullptr) : QDialog(parent) {
        setWindowTitle(QStringLiteral("\u622a\u56fe\u9884\u89c8"));
        setWindowFlags(windowFlags() | Qt::WindowStaysOnTopHint);
        setModal(true);
        resize(760, 520);
        QVBoxLayout *layout = new QVBoxLayout(this);
        QLabel *previewLabel = new QLabel(this);
        previewLabel->setAlignment(Qt::AlignCenter);
        previewLabel->setMinimumSize(480, 280);
        previewLabel->setPixmap(pixmap.scaled(720, 420, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        layout->addWidget(previewLabel, 1);

        QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
        saveButton_ = buttonBox->addButton(QStringLiteral("\u4fdd\u5b58"), QDialogButtonBox::ActionRole);
        buttonBox->button(QDialogButtonBox::Ok)->setText(QStringLiteral("\u5b8c\u6210"));
        buttonBox->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("\u53d6\u6d88"));
        layout->addWidget(buttonBox);

        connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
        connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    }

    QPushButton *saveButton() const { return saveButton_; }

private:
    QPushButton *saveButton_ = nullptr;
};

class SelectionOverlay : public QWidget {
public:
    explicit SelectionOverlay(CaptureTool *tool) : QWidget(nullptr), tool(tool) {
        setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool);
        setAttribute(Qt::WA_OpaquePaintEvent);
        setAttribute(Qt::WA_DeleteOnClose);
        setFocusPolicy(Qt::StrongFocus);
        setWindowState(Qt::WindowFullScreen);
        setCursor(Qt::CrossCursor);
    }
protected:
    void paintEvent(QPaintEvent *) override {
        QPainter p(this);
        if (!tool->frozenScreen.isNull()) {
            // frozenScreen is captured in physical pixels; draw it scaled to logical widget rect.
            p.drawPixmap(rect(), tool->frozenScreen);
        } else {
            p.fillRect(rect(), Qt::black);
        }

        p.fillRect(rect(), QColor(0, 0, 0, 80));

        if (tool->selecting) {
            const QRect sel = QRect(tool->selectionStart, tool->selectionEnd).normalized();
            if (!sel.isEmpty()) {
                if (!tool->frozenScreen.isNull()) {
                    // Re-draw the same frozen frame under clip to avoid high-DPI remap jitter.
                    p.save();
                    p.setClipRect(sel);
                    p.drawPixmap(rect(), tool->frozenScreen);
                    p.restore();
                }
                p.setBrush(Qt::NoBrush);
                p.setPen(QPen(QColor(0, 122, 255), 2));
                p.drawRect(sel.adjusted(0, 0, -1, -1));
            }
        }
    }
    void mousePressEvent(QMouseEvent *e) override {
        tool->selecting = true;
        tool->selectionStart = tool->selectionEnd = e->globalPosition().toPoint();
        update();
    }
    void mouseMoveEvent(QMouseEvent *e) override {
        if (tool->selecting) {
            tool->selectionEnd = e->globalPosition().toPoint();
            update();
        }
    }
    void mouseReleaseEvent(QMouseEvent *e) override {
        if (tool->selecting) {
            tool->selectionEnd = e->globalPosition().toPoint();
            tool->finishSelection();
        }
    }
    void keyPressEvent(QKeyEvent *e) override {
        if (e->key() == Qt::Key_Escape) {
            tool->selecting = false;
            close();
            e->accept();
            return;
        }
        QWidget::keyPressEvent(e);
    }
private:
    CaptureTool *tool;
};

CaptureTool::CaptureTool(QObject *parent) : QObject(parent), overlay(nullptr), selecting(false) {}

// 直接启动区域截图
void CaptureTool::startCapture() { captureRegion(); }

void CaptureTool::captureFullScreen() {
    QWidget *hostWindow = qobject_cast<QWidget *>(parent());
    const bool restoreHostWindow = hostWindow && hostWindow->isVisible();
    if (restoreHostWindow) {
        hostWindow->hide();
        QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents, 50);
    }

    QScreen *s = QGuiApplication::primaryScreen();
    QPixmap screenShot;
    if (s) {
        screenShot = s->grabWindow(0);
    }

    if (restoreHostWindow) {
        hostWindow->show();
    }

    if (!screenShot.isNull()) {
        confirmAndEmit(screenShot);
    }
}

void CaptureTool::captureRegion() {
    QScreen *screen = QGuiApplication::primaryScreen();
    if (!screen) {
        return;
    }

    QWidget *hostWindow = qobject_cast<QWidget *>(parent());
    const bool restoreHostWindow = hostWindow && hostWindow->isVisible();
    if (restoreHostWindow) {
        hostWindow->hide();
        QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents, 50);
    }

    frozenScreen = screen->grabWindow(0);

    if (restoreHostWindow) {
        hostWindow->show();
    }

    if (frozenScreen.isNull()) {
        return;
    }

    if (overlay) { overlay->close(); overlay = nullptr; }
    selecting = false;

    if (!regionCaptureActive) {
        regionCaptureActive = true;
        emit regionCaptureStateChanged(true);
    }

    overlay = new SelectionOverlay(this);
    connect(overlay, &QWidget::destroyed, this, [this]() {
        overlay = nullptr;
        selecting = false;
        if (regionCaptureActive) {
            regionCaptureActive = false;
            emit regionCaptureStateChanged(false);
        }
    });
    overlay->show();
    overlay->activateWindow();
    overlay->setFocus();
}

void CaptureTool::finishSelection() {
    if (!overlay) {
        frozenScreen = QPixmap();
        return;
    }

    QRect selRect = QRect(selectionStart, selectionEnd).normalized();
    overlay->close();
    overlay = nullptr;
    selecting = false;

    if (selRect.width() < 10 || selRect.height() < 10) {
        frozenScreen = QPixmap();
        return;
    }

    QPixmap pixmap;
    if (!frozenScreen.isNull()) {
        const qreal dpr = frozenScreen.devicePixelRatio();
        QRect sourceRect = selRect;
        if (!qFuzzyCompare(dpr, 1.0)) {
            sourceRect = QRect(qRound(selRect.x() * dpr),
                               qRound(selRect.y() * dpr),
                               qRound(selRect.width() * dpr),
                               qRound(selRect.height() * dpr));
        }

        sourceRect = sourceRect.intersected(QRect(QPoint(0, 0), frozenScreen.size()));
        if (sourceRect.isValid() && sourceRect.width() > 0 && sourceRect.height() > 0) {
            pixmap = frozenScreen.copy(sourceRect);
            pixmap.setDevicePixelRatio(dpr);
        }
    }

    frozenScreen = QPixmap();
    if (!pixmap.isNull()) confirmAndEmit(pixmap);
}

void CaptureTool::confirmAndEmit(const QPixmap &pixmap) {
    if (pixmap.isNull()) return;

    CapturePreviewDialog dialog(pixmap);
    emit previewDialogStateChanged(true);
    connect(dialog.saveButton(), &QPushButton::clicked, &dialog, [this, &pixmap, &dialog]() {
        QString savedPath;
        QString errorText;
        if (savePixmapToConfiguredPath(pixmap, &savedPath, &errorText)) {
            QMessageBox::information(&dialog,
                                     QStringLiteral("\u622a\u56fe\u5df2\u4fdd\u5b58"),
                                     QStringLiteral("\u5df2\u4fdd\u5b58\u5230\uff1a\n%1")
                                         .arg(QDir::toNativeSeparators(savedPath)));
        } else {
            QMessageBox::warning(&dialog,
                                 QStringLiteral("\u4fdd\u5b58\u5931\u8d25"),
                                 errorText.isEmpty()
                                     ? QStringLiteral("\u65e0\u6cd5\u4fdd\u5b58\u622a\u56fe\u6587\u4ef6\u3002")
                                     : errorText);
        }
    });

    const int result = dialog.exec();
    emit previewDialogStateChanged(false);
    if (result == QDialog::Accepted) emit screenshotTaken(pixmap);
}

bool CaptureTool::savePixmapToConfiguredPath(const QPixmap &pixmap,
                                             QString *savedPath,
                                             QString *errorText) const
{
    if (pixmap.isNull()) {
        if (errorText) {
            *errorText = QStringLiteral("\u622a\u56fe\u6570\u636e\u4e3a\u7a7a\u3002");
        }
        return false;
    }

    QSettings settings = AppSettings::createSettings();
    const QString dirPath = AppSettings::normalizeSavePath(
        settings.value(AppSettings::kSavePath, AppSettings::defaultSavePath()).toString());

    QDir dir(dirPath);
    if (!dir.exists() && !dir.mkpath(QStringLiteral("."))) {
        if (errorText) {
            *errorText = QStringLiteral("\u4fdd\u5b58\u76ee\u5f55\u4e0d\u5b58\u5728\u4e14\u65e0\u6cd5\u521b\u5efa\uff1a\n%1")
                             .arg(QDir::toNativeSeparators(dirPath));
        }
        return false;
    }

    const QString format = AppSettings::normalizeSaveFormat(
        settings.value(AppSettings::kSaveFormat, AppSettings::kDefaultSaveFormat).toString());
    const QString extension =
        (format == QStringLiteral("JPG")) ? QStringLiteral("jpg") : QStringLiteral("png");

    const QString timestamp =
        QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss_zzz"));
    const QString baseName = QStringLiteral("Words-Bin_%1").arg(timestamp);

    QString filePath =
        dir.filePath(QStringLiteral("%1.%2").arg(baseName, extension));
    int suffix = 1;
    while (QFileInfo::exists(filePath)) {
        filePath = dir.filePath(
            QStringLiteral("%1_%2.%3").arg(baseName).arg(suffix++).arg(extension));
    }

    const QByteArray formatBytes = format.toLatin1();
    if (!pixmap.save(filePath, formatBytes.constData())) {
        if (errorText) {
            *errorText = QStringLiteral("\u5199\u5165\u6587\u4ef6\u5931\u8d25\uff1a\n%1")
                             .arg(QDir::toNativeSeparators(filePath));
        }
        return false;
    }

    if (savedPath) {
        *savedPath = filePath;
    }
    if (errorText) {
        errorText->clear();
    }
    return true;
}
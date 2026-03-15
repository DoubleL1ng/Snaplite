#include "CaptureTool.h"

#include <QCursor>
#include <QDialog>
#include <QDialogButtonBox>
#include <QEvent>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QScreen>
#include <QVBoxLayout>
#include <QtGlobal>
#include <QWidget>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

class CapturePreviewDialog : public QDialog {
public:
    explicit CapturePreviewDialog(const QPixmap &pixmap, QWidget *parent = nullptr)
        : QDialog(parent)
    {
        setWindowTitle(QStringLiteral("\u622a\u56fe\u9884\u89c8"));
        setModal(true);
        resize(760, 520);

        QVBoxLayout *layout = new QVBoxLayout(this);
        QLabel *previewLabel = new QLabel(this);
        previewLabel->setAlignment(Qt::AlignCenter);
        previewLabel->setMinimumSize(480, 280);
        previewLabel->setPixmap(
            pixmap.scaled(720, 420, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        layout->addWidget(previewLabel, 1);

        QDialogButtonBox *buttonBox =
            new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
        buttonBox->button(QDialogButtonBox::Ok)->setText(QStringLiteral("\u5b8c\u6210"));
        buttonBox->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("\u53d6\u6d88"));
        layout->addWidget(buttonBox);

        connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
        connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    }
};

class LongshotPreview : public QWidget {
public:
    explicit LongshotPreview(CaptureTool *tool) : QWidget(nullptr), tool(tool)
    {
        setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool);
        setAttribute(Qt::WA_TranslucentBackground);
        setAttribute(Qt::WA_DeleteOnClose);
        setFocusPolicy(Qt::StrongFocus);
        resize(180, 240);

        QScreen *screen = QGuiApplication::primaryScreen();
        if (screen) {
            move(screen->geometry().right() - 200, 40);
        }
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::SmoothPixmapTransform);
        p.setBrush(QColor(255, 255, 255, 220));
        p.setPen(Qt::NoPen);
        p.drawRoundedRect(rect(), 12, 12);

        p.setPen(QColor(40, 40, 40));
        p.drawText(
            QRect(12, 12, width() - 24, 20),
            Qt::AlignCenter,
            QStringLiteral("\u957f\u622a\u56fe\uff08\u7a7a\u683c\u7ee7\u7eed\uff0cEnter \u5b8c\u6210\uff09"));

        if (!tool->longshotSegments.isEmpty()) {
            int w = width() - 24;
            int y = 38;
            for (const QImage &img : tool->longshotSegments) {
                if (img.width() <= 0) {
                    continue;
                }
                QPixmap pm = QPixmap::fromImage(img).scaled(
                    w,
                    img.height() * w / img.width(),
                    Qt::KeepAspectRatio,
                    Qt::SmoothTransformation);
                p.drawPixmap(12, y, pm);
                y += pm.height() + 4;
                if (y > height() - 48) {
                    break;
                }
            }
        } else {
            p.setPen(QColor(120, 120, 120));
            p.drawText(
                QRect(12, 44, width() - 24, height() - 96),
                Qt::AlignCenter | Qt::TextWordWrap,
                QStringLiteral("\u5148\u9009\u62e9\u7a97\u53e3\uff0c\u7136\u540e\u6309\u7a7a\u683c\u952e\u9010\u6b65\u91c7\u96c6\u3002"));
        }

        QRect nextRect(12, height() - 38, 72, 28);
        p.setBrush(QColor(0, 146, 78));
        p.setPen(Qt::NoPen);
        p.drawRoundedRect(nextRect, 8, 8);
        p.setPen(Qt::white);
        p.drawText(nextRect, Qt::AlignCenter, QStringLiteral("\u7ee7\u7eed"));

        QRect btnRect(width() - 70, height() - 38, 60, 28);
        p.setBrush(QColor(0, 122, 255));
        p.setPen(Qt::NoPen);
        p.drawRoundedRect(btnRect, 8, 8);
        p.setPen(Qt::white);
        p.drawText(btnRect, Qt::AlignCenter, QStringLiteral("\u5b8c\u6210"));
    }

    void mousePressEvent(QMouseEvent *e) override
    {
        QRect nextRect(12, height() - 38, 72, 28);
        if (nextRect.contains(e->pos())) {
            if (tool->longshotRect.isValid()) {
                tool->captureWindowRegion(tool->longshotRect);
                update();
            }
            return;
        }

        QRect btnRect(width() - 70, height() - 38, 60, 28);
        if (btnRect.contains(e->pos())) {
            tool->finishLongshot();
            close();
            return;
        }
        QWidget::mousePressEvent(e);
    }

private:
    CaptureTool *tool;
};

class WindowSelector : public QWidget {
public:
    explicit WindowSelector(CaptureTool *tool) : QWidget(nullptr), tool(tool)
    {
        setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool);
        setAttribute(Qt::WA_TranslucentBackground);
        setAttribute(Qt::WA_DeleteOnClose);
        setWindowState(Qt::WindowFullScreen);
        setCursor(Qt::PointingHandCursor);
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setBrush(QColor(0, 0, 0, 40));
        p.setPen(Qt::NoPen);
        p.drawRect(rect());
        if (highlightRect.isValid()) {
            p.setBrush(Qt::NoBrush);
            p.setPen(QPen(Qt::red, 2));
            p.drawRect(highlightRect);
        }
    }

    void mouseMoveEvent(QMouseEvent *e) override
    {
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
        QRect rect = findWindowRect(e->globalPosition().toPoint());
#else
        QRect rect = findWindowRect(e->globalPos());
#endif
        if (rect != highlightRect) {
            highlightRect = rect;
            update();
        }
    }

    void mousePressEvent(QMouseEvent *e) override
    {
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
        QRect rect = findWindowRect(e->globalPosition().toPoint());
#else
        QRect rect = findWindowRect(e->globalPos());
#endif
        if (rect.isValid()) {
            tool->captureWindowRegion(rect);
        }
        close();
    }

private:
    QRect findWindowRect(const QPoint &pt)
    {
#ifdef Q_OS_WIN
        POINT p = {pt.x(), pt.y()};
        HWND hwnd = WindowFromPoint(p);
        if (!hwnd) {
            return QRect();
        }

        RECT rc;
        if (GetWindowRect(hwnd, &rc)) {
            return QRect(rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top);
        }
#else
        Q_UNUSED(pt);
#endif
        return QRect();
    }

    CaptureTool *tool;
    QRect highlightRect;
};

class SelectionOverlay : public QWidget {
public:
    explicit SelectionOverlay(CaptureTool *tool) : QWidget(nullptr), tool(tool)
    {
        setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool);
        setAttribute(Qt::WA_TranslucentBackground);
        setAttribute(Qt::WA_DeleteOnClose);
        setWindowState(Qt::WindowFullScreen);
        setCursor(Qt::CrossCursor);
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setBrush(QColor(0, 0, 0, 80));
        p.setPen(Qt::NoPen);
        p.drawRect(rect());

        if (tool->selecting) {
            QRect sel(tool->selectionStart, tool->selectionEnd);
            p.setBrush(Qt::NoBrush);
            p.setPen(QPen(Qt::blue, 2));
            p.drawRect(sel.normalized());
        }
    }

    void mousePressEvent(QMouseEvent *e) override
    {
        tool->selecting = true;
    #if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
        tool->selectionStart = tool->selectionEnd = e->globalPosition().toPoint();
    #else
        tool->selectionStart = tool->selectionEnd = e->globalPos();
    #endif
        update();
    }

    void mouseMoveEvent(QMouseEvent *e) override
    {
        if (tool->selecting) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
            tool->selectionEnd = e->globalPosition().toPoint();
#else
            tool->selectionEnd = e->globalPos();
#endif
            update();
        }
    }

    void mouseReleaseEvent(QMouseEvent *e) override
    {
        if (tool->selecting) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
            tool->selectionEnd = e->globalPosition().toPoint();
#else
            tool->selectionEnd = e->globalPos();
#endif
            tool->finishSelection();
        }
    }

private:
    CaptureTool *tool;
};

CaptureTool::CaptureTool(QObject *parent) : QObject(parent) {}

void CaptureTool::startCapture()
{
    QWidget *menuWidget = new QWidget(nullptr);
    menuWidget->setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool);
    menuWidget->setAttribute(Qt::WA_TranslucentBackground);
    menuWidget->setAttribute(Qt::WA_DeleteOnClose);
    menuWidget->resize(220, 155);
    menuWidget->move(QCursor::pos() - QPoint(110, 78));

    QPushButton *btnFull = new QPushButton(QStringLiteral("\u5168\u5c4f\u622a\u56fe"), menuWidget);
    QPushButton *btnWindow = new QPushButton(QStringLiteral("\u7a97\u53e3\u622a\u56fe"), menuWidget);
    QPushButton *btnRegion = new QPushButton(QStringLiteral("\u533a\u57df\u622a\u56fe"), menuWidget);
    QPushButton *btnLong = new QPushButton(QStringLiteral("\u957f\u622a\u56fe"), menuWidget);
    btnFull->setGeometry(20, 15, 180, 25);
    btnWindow->setGeometry(20, 50, 180, 25);
    btnRegion->setGeometry(20, 85, 180, 25);
    btnLong->setGeometry(20, 120, 180, 25);

    QObject::connect(btnFull, &QPushButton::clicked, this, [this, menuWidget]() {
        menuWidget->close();
        captureFullScreen();
    });
    QObject::connect(btnWindow, &QPushButton::clicked, this, [this, menuWidget]() {
        menuWidget->close();
        captureWindow();
    });
    QObject::connect(btnRegion, &QPushButton::clicked, this, [this, menuWidget]() {
        menuWidget->close();
        captureRegion();
    });
    QObject::connect(btnLong, &QPushButton::clicked, this, [this, menuWidget]() {
        menuWidget->close();
        captureLongScreenshot();
    });

    menuWidget->show();
}

void CaptureTool::captureFullScreen()
{
    QScreen *screen = QGuiApplication::primaryScreen();
    if (!screen) {
        return;
    }

    QPixmap pixmap = screen->grabWindow(0);
    if (!pixmap.isNull()) {
        confirmAndEmit(pixmap);
    }
}

void CaptureTool::captureWindow()
{
    WindowSelector *selector = new WindowSelector(this);
    selector->show();
}

void CaptureTool::captureRegion()
{
    showSelectionOverlay();
}

void CaptureTool::showSelectionOverlay()
{
    if (overlay) {
        overlay->close();
        overlay = nullptr;
    }

    selecting = false;
    overlay = new SelectionOverlay(this);
    connect(overlay, &QWidget::destroyed, this, [this]() {
        overlay = nullptr;
        selecting = false;
    });
    overlay->show();
}

void CaptureTool::finishSelection()
{
    if (!overlay) {
        return;
    }

    QRect selRect(selectionStart, selectionEnd);
    selRect = selRect.normalized();

    overlay->close();
    overlay = nullptr;
    selecting = false;

    if (selRect.width() < 10 || selRect.height() < 10) {
        return;
    }

    QScreen *screen = QGuiApplication::primaryScreen();
    if (!screen) {
        return;
    }

    QPixmap pixmap = screen->grabWindow(0, selRect.x(), selRect.y(), selRect.width(), selRect.height());
    if (!pixmap.isNull()) {
        confirmAndEmit(pixmap);
    }
}

void CaptureTool::captureLongScreenshot()
{
    longshotMode = true;
    longshotRect = QRect();
    clearLongshot();

    if (longshotPreview) {
        longshotPreview->close();
        longshotPreview = nullptr;
    }

    WindowSelector *selector = new WindowSelector(this);
    connect(selector, &QWidget::destroyed, this, [this]() {
        if (longshotMode && longshotRect.isValid()) {
            showLongshotPreview();
            return;
        }

        longshotMode = false;
        longshotRect = QRect();
        clearLongshot();
    });
    selector->show();
}

void CaptureTool::captureWindowRegion(const QRect &rect)
{
    if (!rect.isValid()) {
        return;
    }

    QScreen *screen = QGuiApplication::primaryScreen();
    if (!screen) {
        return;
    }

    if (longshotMode) {
        longshotRect = rect;
        QImage img = screen->grabWindow(0, rect.x(), rect.y(), rect.width(), rect.height()).toImage();
        if (!img.isNull()) {
            longshotSegments.append(img);
            updateLongshotPreview();
        }
        return;
    }

    QPixmap pixmap = screen->grabWindow(0, rect.x(), rect.y(), rect.width(), rect.height());
    if (!pixmap.isNull()) {
        confirmAndEmit(pixmap);
    }
}

void CaptureTool::showLongshotPreview()
{
    if (!longshotMode || !longshotRect.isValid()) {
        return;
    }

    LongshotPreview *preview = new LongshotPreview(this);
    longshotPreview = preview;

    preview->installEventFilter(this);
    preview->show();
    preview->raise();
    preview->activateWindow();
    preview->setFocus(Qt::ActiveWindowFocusReason);
    preview->grabKeyboard();

    connect(preview, &QWidget::destroyed, this, [this]() {
        longshotPreview = nullptr;
        longshotMode = false;
        longshotRect = QRect();
        clearLongshot();
    });

    captureWindowRegion(longshotRect);
}

void CaptureTool::updateLongshotPreview()
{
    if (longshotPreview) {
        longshotPreview->update();
    }
}

void CaptureTool::finishLongshot()
{
    if (longshotSegments.isEmpty()) {
        if (longshotPreview) {
            longshotPreview->close();
        }
        return;
    }

    int w = longshotSegments.first().width();
    int h = 0;
    for (const QImage &img : longshotSegments) {
        h += img.height();
    }

    if (w <= 0 || h <= 0) {
        clearLongshot();
        return;
    }

    QImage result(w, h, QImage::Format_ARGB32);
    result.fill(Qt::white);
    QPainter p(&result);
    int y = 0;
    for (const QImage &img : longshotSegments) {
        p.drawImage(0, y, img);
        y += img.height();
    }
    p.end();

    confirmAndEmit(QPixmap::fromImage(result));

    longshotMode = false;
    longshotRect = QRect();
    clearLongshot();

    if (longshotPreview) {
        longshotPreview->close();
        longshotPreview = nullptr;
    }
}

void CaptureTool::clearLongshot()
{
    longshotSegments.clear();
}

bool CaptureTool::eventFilter(QObject *obj, QEvent *event)
{
    if (obj == longshotPreview && event && event->type() == QEvent::KeyPress) {
        QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);
        if ((keyEvent->key() == Qt::Key_Space || keyEvent->key() == Qt::Key_Down)
            && longshotRect.isValid()) {
            captureWindowRegion(longshotRect);
            return true;
        }
        if (keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter) {
            finishLongshot();
            return true;
        }
        if (keyEvent->key() == Qt::Key_Escape) {
            if (longshotPreview) {
                longshotPreview->close();
            }
            return true;
        }
    }

    return QObject::eventFilter(obj, event);
}

void CaptureTool::confirmAndEmit(const QPixmap &pixmap)
{
    if (pixmap.isNull()) {
        return;
    }

    CapturePreviewDialog dialog(pixmap);
    if (dialog.exec() == QDialog::Accepted) {
        emit screenshotTaken(pixmap);
    }
}

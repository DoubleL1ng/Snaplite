#pragma once
#include <QImage>
#include <QObject>
#include <QPixmap>
#include <QPoint>
#include <QRect>
#include <QString>

class QEvent;
class QWidget;
class SelectionOverlay;

class CaptureTool : public QObject
{
    Q_OBJECT
public:
    explicit CaptureTool(QObject *parent = nullptr);
    void startCapture();
    void captureFullScreen();
    void captureRegion();

private:
    friend class SelectionOverlay;
    void finishSelection();
    void confirmAndEmit(const QPixmap &pixmap);
    bool savePixmapToConfiguredPath(const QPixmap &pixmap,
                                    QString *savedPath,
                                    QString *errorText) const;

    QPoint selectionStart;
    QPoint selectionEnd;
    QPixmap frozenScreen;
    QWidget *overlay = nullptr;
    bool selecting = false;
    bool regionCaptureActive = false;

signals:
    void screenshotTaken(const QPixmap &pixmap);
    void regionCaptureStateChanged(bool active);
    void previewDialogStateChanged(bool visible);
};
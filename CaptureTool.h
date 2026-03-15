#pragma once

#include <QImage>
#include <QObject>
#include <QPixmap>
#include <QPoint>
#include <QRect>
#include <QVector>

class QEvent;
class QWidget;

class SelectionOverlay;
class WindowSelector;
class LongshotPreview;

class CaptureTool : public QObject
{
    Q_OBJECT
public:
    explicit CaptureTool(QObject *parent = nullptr);
    // 弹出截图模式选择
    void startCapture();
    // 全屏截图
    void captureFullScreen();
    // 窗口截图
    void captureWindow();
    // 区域截图（带选择器）
    void captureRegion();
    // 长截图（滚动窗口截图，基础实现）
    void captureLongScreenshot();

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;

private:
    friend class SelectionOverlay;
    friend class WindowSelector;
    friend class LongshotPreview;

    void showSelectionOverlay();
    void finishSelection();

    // 截取指定窗口区域
    void captureWindowRegion(const QRect &rect);

    // 长截图拼接相关
    void showLongshotPreview();
    void updateLongshotPreview();
    void finishLongshot();
    void clearLongshot();
    void confirmAndEmit(const QPixmap &pixmap);

    QPoint selectionStart;
    QPoint selectionEnd;
    QWidget *overlay = nullptr;
    bool selecting = false;

    QVector<QImage> longshotSegments;
    QRect longshotRect;
    QWidget *longshotPreview = nullptr;
    bool longshotMode = false;

signals:
    void screenshotTaken(const QPixmap &pixmap);
};

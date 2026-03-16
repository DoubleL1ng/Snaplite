#pragma once

#include <QDir>
#include <QSettings>
#include <QStandardPaths>
#include <QString>
#include <QtGlobal>

namespace AppSettings {
inline const QString kOrganization = QStringLiteral("SnipLite");
inline const QString kApplication = QStringLiteral("SnipLite");

inline const QString kCaptureHotkey = QStringLiteral("shortcuts/capture");
inline const QString kSavePath = QStringLiteral("savePath");
inline const QString kSaveFormat = QStringLiteral("saveFormat");
inline const QString kHideSidebar = QStringLiteral("hideSidebar");
inline const QString kHistoryMaxItems = QStringLiteral("history/maxItems");
inline const QString kSidebarPinned = QStringLiteral("sidebar/pinned");
inline const QString kGitHubUrl = QStringLiteral("https://github.com/DoubleL1ng/Snaplite");

inline const QString kDefaultCaptureHotkey = QStringLiteral("Ctrl+Shift+A");
inline const QString kDefaultSaveFormat = QStringLiteral("PNG");
inline constexpr bool kDefaultHideSidebar = true;
inline constexpr bool kDefaultSidebarPinned = false;
inline constexpr int kDefaultHistoryMaxItems = 30;
inline constexpr int kMinHistoryMaxItems = 1;
inline constexpr int kMaxHistoryMaxItems = 200;
inline constexpr int kDockTriggerWidth = 6;
inline constexpr int kDockTriggerHeight = 84;
inline constexpr int kSidebarRightMargin = 20;
inline constexpr int kSidebarAnimationDurationMs = 180;
inline constexpr int kEdgeCheckIntervalMs = 280;
inline constexpr int kSidebarAutoDockDelayMs = 150;
inline constexpr int kSidebarExpandDebounceMs = 250;
inline constexpr int kTrayRevealHoldMs = 3000;
inline constexpr int kPreCaptureDelayMs = 60;

inline QSettings createSettings()
{
    return QSettings(kOrganization, kApplication);
}

inline QString defaultSavePath()
{
    return QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
}

inline QString normalizeSavePath(QString path)
{
    path = path.trimmed();
    if (path.isEmpty()) {
        path = defaultSavePath();
    }
    return QDir::cleanPath(path);
}

inline QString normalizeSaveFormat(const QString &format)
{
    const QString upper = format.trimmed().toUpper();
    if (upper == QStringLiteral("JPG") || upper == QStringLiteral("JPEG")) {
        return QStringLiteral("JPG");
    }
    return QStringLiteral("PNG");
}

inline int normalizeHistoryMaxItems(int value)
{
    return qBound(kMinHistoryMaxItems, value, kMaxHistoryMaxItems);
}
} // namespace AppSettings

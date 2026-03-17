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
inline const QString kDockStripWidth = QStringLiteral("dock/stripWidth");
inline const QString kDockStripHeight = QStringLiteral("dock/stripHeight");
inline const QString kDockStripBorderRadius = QStringLiteral("dock/stripBorderRadius");
inline const QString kDockStripColorIndex = QStringLiteral("dock/stripColorIndex");
inline const QString kThemeMode = QStringLiteral("ui/themeMode");
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
inline constexpr int kDefaultDockStripWidth = 6;
inline constexpr int kDefaultDockStripHeight = 84;
inline constexpr int kDefaultDockStripBorderRadius = 3;
inline constexpr int kDefaultDockStripColorIndex = 0;
inline constexpr int kMinDockStripWidth = 4;
inline constexpr int kMaxDockStripWidth = 20;
inline constexpr int kMinDockStripHeight = 40;
inline constexpr int kMaxDockStripHeight = 200;
inline constexpr int kMinDockStripBorderRadius = 0;
inline constexpr int kMaxDockStripBorderRadius = 10;
inline const QString kDefaultThemeMode = QStringLiteral("dark");
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

inline QStringList getDockStripPresetColors()
{
    return {
        QStringLiteral("#FFB366"), // Light orange
        QStringLiteral("#D4956E"), // Warm beige
        QStringLiteral("#8B6F47"), // Brown
        QStringLiteral("#614D36"), // Dark brown
        QStringLiteral("#6B5D52")  // Grey brown
    };
}

struct ThemePalette {
    QString background;
    QString secondaryBackground;
    QString text;
    QString secondaryText;
    QString buttonBackground;
    QString buttonHover;
    QString buttonPressed;
    QString border;
};

inline ThemePalette getDarkThemePalette()
{
    return {
        QStringLiteral("#1E1E1E"),   // background
        QStringLiteral("#2D2D2D"),   // secondaryBackground
        QStringLiteral("#FFFFFF"),   // text
        QStringLiteral("#E0E0E0"),   // secondaryText
        QStringLiteral("#3D3D3D"),   // buttonBackground
        QStringLiteral("#0078D4"),   // buttonHover
        QStringLiteral("#005A9E"),   // buttonPressed
        QStringLiteral("#404040")    // border
    };
}

inline ThemePalette getLightThemePalette()
{
    return {
        QStringLiteral("#F5F5F5"),   // background
        QStringLiteral("#EBEBEB"),   // secondaryBackground
        QStringLiteral("#1E1E1E"),   // text
        QStringLiteral("#424242"),   // secondaryText
        QStringLiteral("#E0E0E0"),   // buttonBackground
        QStringLiteral("#0078D4"),   // buttonHover
        QStringLiteral("#005A9E"),   // buttonPressed
        QStringLiteral("#CCCCCC")    // border
    };
}
} // namespace AppSettings

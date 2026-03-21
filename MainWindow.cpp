#include "MainWindow.h"
#include "AppSettings.h"
#include "TrayIcon.h"
#include "CaptureTool.h"
#include <QApplication>
#include <QScreen>
#include <QCursor>
#include <QClipboard>
#include <QCryptographicHash>
#include <QMimeData>
#include <QDateTime>
#include <QDir>
#include <QDesktopServices>
#include <QEasingCurve>
#include <QByteArrayView>
#include <QFile>
#include <QFileInfo>
#include <QGraphicsDropShadowEffect>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QMenu>
#include <QPainter>
#include <QPainterPath>
#include <QSignalBlocker>
#include <QSettings>
#include <QSvgRenderer>
#include <QToolButton>
#include <QUrl>
#include <QWindow>

namespace {
constexpr qint64 kClipboardDuplicateWindowMs = 500;

QRect resolveAvailableGeometry(const QWidget *widget)
{
    if (widget) {
        if (QWindow *window = widget->windowHandle(); window && window->screen()) {
            return window->screen()->availableGeometry();
        }

        if (QScreen *screen = QApplication::screenAt(widget->frameGeometry().center())) {
            return screen->availableGeometry();
        }
    }

    if (QScreen *screen = QApplication::screenAt(QCursor::pos())) {
        return screen->availableGeometry();
    }

    if (QScreen *screen = QApplication::primaryScreen()) {
        return screen->availableGeometry();
    }

    return QRect();
}

QString normalizeThemeModeValue(QString mode)
{
    mode = mode.trimmed().toLower();
    if (mode != QStringLiteral("light")) {
        return QStringLiteral("dark");
    }
    return mode;
}

QIcon loadThemeAwareSvgIcon(const QString &resourcePath, const QColor &color)
{
    QFile file(resourcePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return QIcon(resourcePath);
    }

    QByteArray svgData = file.readAll();
    svgData.replace("currentColor", color.name(QColor::HexRgb).toUtf8());

    QSvgRenderer renderer(svgData);
    if (!renderer.isValid()) {
        return QIcon(resourcePath);
    }

    QIcon icon;
    const QList<int> sizes = {14, 16, 18, 20, 24};
    for (const int size : sizes) {
        QPixmap pixmap(size, size);
        pixmap.fill(Qt::transparent);
        QPainter painter(&pixmap);
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
        renderer.render(&painter, QRectF(0, 0, size, size));
        painter.end();
        icon.addPixmap(pixmap);
    }

    return icon;
}

QString controlsStyleSheet(const AppSettings::ThemePalette &palette)
{
    return QStringLiteral(
        "QPushButton { background-color: %1; color: %2; border: 1px solid %3; padding: 10px 12px; border-radius: 6px; text-align: center; font-family: 'Microsoft YaHei'; font-size: 14px; font-weight: 500; }"
        "QPushButton#captureActionButton { padding: 12px 12px; font-size: 17px; font-weight: 600; text-align: center; }"
        "QPushButton:hover { background-color: %4; color: #FFFFFF; }"
        "QPushButton:pressed { background-color: %5; color: #FFFFFF; }"
        "QToolButton#topActionButton { background-color: %1; color: %2; border: 1px solid %3; border-radius: 12px; min-width: 24px; min-height: 24px; max-width: 24px; max-height: 24px; font-family: 'Microsoft YaHei'; font-size: 13px; font-weight: 600; }"
        "QToolButton#topActionButton:hover { background-color: %4; color: #FFFFFF; }"
        "QToolButton#topActionButton:checked { background-color: %4; color: #FFFFFF; }"
        "QToolButton#topActionButton:pressed { background-color: %5; color: #FFFFFF; }"
        "QToolButton#topActionButton:disabled { background-color: %6; color: %7; }"
        "QWidget#historyDivider { background-color: %3; border: none; border-radius: 2px; }"
        "QListWidget { background-color: transparent; border: none; outline: none; }"
        "QListWidget::item { background-color: %6; margin-bottom: 8px; border-radius: 6px; }"
        "QListWidget::item:selected { background-color: %1; border: 1px solid %4; }"
        "QLabel#historyTitleLabel { color: %2; font-family: 'YouYuan', 'Microsoft YaHei UI', 'Microsoft YaHei'; font-weight: 700; font-size: 20px; qproperty-alignment: AlignHCenter|AlignVCenter; }"
        "QLabel#historyTimeLabel { background-color: %1; color: %4; border-radius: 4px; padding: 2px 6px; font-family: 'Microsoft YaHei'; font-weight: 700; font-size: 11px; }"
        "QLabel#historyTextLabel { color: %7; font-family: 'Microsoft YaHei'; font-size: 13px; }")
        .arg(palette.buttonBackground,
             palette.text,
             palette.border,
             palette.buttonHover,
             palette.buttonPressed,
             palette.secondaryBackground,
             palette.secondaryText);
}

QString expandedPanelStyleSheet(const AppSettings::ThemePalette &palette)
{
    return QStringLiteral(
               "#centralWidget { background-color: %1; border: 1.5px solid %2; border-radius: 10px; }")
               .arg(palette.secondaryBackground, palette.border) +
           controlsStyleSheet(palette);
}

QString dockStripStyleSheet(const AppSettings::ThemePalette &palette,
                            const QString &stripColor,
                            int stripBorderRadius)
{
    return QStringLiteral(
               "#centralWidget { background-color: %1; border: 1.5px solid %2; border-radius: %3px; }")
               .arg(stripColor)
               .arg(palette.border)
               .arg(stripBorderRadius) +
           controlsStyleSheet(palette);
}

bool savePixmapToConfiguredPath(const QPixmap &pixmap, QString *savedPath, QString *errorText)
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

    QString filePath = dir.filePath(QStringLiteral("%1.%2").arg(baseName, extension));
    int suffix = 1;
    while (QFileInfo::exists(filePath)) {
        filePath =
            dir.filePath(QStringLiteral("%1_%2.%3").arg(baseName).arg(suffix++).arg(extension));
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

QString normalizeTextForClipboardSignature(QString text)
{
    text.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
    text.replace(QLatin1Char('\r'), QLatin1Char('\n'));
    return text.trimmed();
}

QString clipboardSignatureForPixmap(const QPixmap &pixmap)
{
    if (pixmap.isNull()) {
        return QString();
    }

    QImage image = pixmap.toImage().convertToFormat(QImage::Format_ARGB32);
    if (image.isNull()) {
        return QString();
    }

    QCryptographicHash hash(QCryptographicHash::Sha256);
    hash.addData(QByteArrayView(reinterpret_cast<const char *>(image.constBits()),
                                image.sizeInBytes()));
    return QStringLiteral("img:%1:%2x%3")
        .arg(QString::fromLatin1(hash.result().toHex()))
        .arg(image.width())
        .arg(image.height());
}
} // namespace

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), isDocked(false), normalWidth(280), ignoreClipboardChange(false)
{
    captureTool = new CaptureTool(this);
    tray = new TrayIcon(this); 
    tray->setCaptureTool(captureTool); 
    connect(tray, &TrayIcon::settingsUpdated, this, &MainWindow::onSettingsUpdated);
    connect(tray, &TrayIcon::clearHistoryRequested, this, &MainWindow::clearClipboardHistory);
    connect(tray,
        &TrayIcon::settingsDialogVisibilityChanged,
        this,
        &MainWindow::onSettingsDialogVisibilityChanged);
    connect(tray, &TrayIcon::trayLeftClicked, this, &MainWindow::onTrayLeftClicked);
    connect(captureTool,
        &CaptureTool::regionCaptureStateChanged,
        this,
        &MainWindow::onRegionCaptureStateChanged);
    connect(captureTool,
        &CaptureTool::previewDialogStateChanged,
        this,
        &MainWindow::onPreviewDialogStateChanged);

    dockTriggerWidth = AppSettings::kDockTriggerWidth;
    dockTriggerHeight = AppSettings::kDockTriggerHeight;
    rightMargin = AppSettings::kSidebarRightMargin;
    {
        const QSettings settings = AppSettings::createSettings();
        sidebarPinned =
            settings.value(AppSettings::kSidebarPinned, AppSettings::kDefaultSidebarPinned).toBool();
        themeMode = normalizeThemeModeValue(
            settings.value(AppSettings::kThemeMode, AppSettings::kDefaultThemeMode).toString());
    }
    
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool);
    setAttribute(Qt::WA_TranslucentBackground);
    
    const QRect screenGeometry = resolveAvailableGeometry(this);
    if (screenGeometry.isValid()) {
        const int sidebarHeight = qMax(320, screenGeometry.height() - 100);
        const int x = screenGeometry.right() - rightMargin - normalWidth + 1;
        const int y = qBound(screenGeometry.top() + 10,
                             screenGeometry.top() + 50,
                             screenGeometry.bottom() - sidebarHeight + 1 - 10);
        setGeometry(x, y, normalWidth, sidebarHeight);
    }

    setMinimumWidth(0);
    setMaximumWidth(QWIDGETSIZE_MAX);
    setFixedWidth(normalWidth);

    setupUI();
    updatePinnedUi();

    animation = new QPropertyAnimation(this, "geometry", this);
    animation->setDuration(AppSettings::kSidebarAnimationDurationMs);
    animation->setEasingCurve(QEasingCurve::InOutCubic);
    connect(animation, &QPropertyAnimation::finished, this, [this]() {
        setDockContentVisible(!isDocked);
        updateDockMask();
    });
    
    hoverRevealTimer = new QTimer(this);
    hoverRevealTimer->setSingleShot(true);
    connect(hoverRevealTimer, &QTimer::timeout, this, [this]() {
        if (shouldSuppressSidebar() || sidebarPinned || !isDocked) {
            return;
        }
        if (geometry().contains(QCursor::pos())) {
            expandSidebar(true);
        }
    });

    leaveDockTimer = new QTimer(this);
    leaveDockTimer->setSingleShot(true);
    connect(leaveDockTimer, &QTimer::timeout, this, [this]() {
        if (shouldSuppressSidebar() || sidebarPinned || trayRevealHoldActive || isDocked) {
            return;
        }
        if (!geometry().contains(QCursor::pos())) {
            dockSidebar(false);
        }
    });

    edgeTimer = new QTimer(this);
    connect(edgeTimer, &QTimer::timeout, this, &MainWindow::checkEdgeDocking);
    edgeTimer->start(AppSettings::kEdgeCheckIntervalMs);

    trayRevealHoldTimer = new QTimer(this);
    trayRevealHoldTimer->setSingleShot(true);
    connect(trayRevealHoldTimer, &QTimer::timeout, this, [this]() {
        trayRevealHoldActive = false;
        if (shouldSuppressSidebar() || isDocked || sidebarPinned) {
            return;
        }
        if (!geometry().contains(QCursor::pos()) && leaveDockTimer) {
            leaveDockTimer->start(AppSettings::kSidebarAutoDockDelayMs);
        }
    });

    connect(QApplication::clipboard(), &QClipboard::dataChanged, this, &MainWindow::onClipboardChanged);
}

MainWindow::~MainWindow() {}

void MainWindow::showEvent(QShowEvent *event)
{
    QMainWindow::showEvent(event);

    if (!startupHoldApplied && !shouldSuppressSidebar()) {
        startupHoldApplied = true;
        revealSidebarWithHold();
    }
}

void MainWindow::setupUI()
{
    centralWidget = new QWidget(this);
    centralWidget->setObjectName("centralWidget");
    centralWidget->setStyleSheet(QString());

    auto *shadow = new QGraphicsDropShadowEffect(centralWidget);
    shadow->setBlurRadius(22);
    shadow->setOffset(-2, 0);
    shadow->setColor(QColor(0, 0, 0, 120));
    centralWidget->setGraphicsEffect(shadow);

    mainLayout = new QVBoxLayout(centralWidget);
    mainLayout->setContentsMargins(15, 20, 15, 20);

    QHBoxLayout *topBarLayout = new QHBoxLayout();
    topBarLayout->setContentsMargins(0, 0, 0, 0);
    topBarLayout->setSpacing(8);
    topBarLayout->addStretch();

    githubButton = new QToolButton(this);
    githubButton->setObjectName("topActionButton");
    githubButton->setIconSize(QSize(14, 14));
    githubButton->setToolTip(QStringLiteral("\u6253\u5f00 GitHub"));
    githubButton->setCursor(Qt::PointingHandCursor);
    connect(githubButton, &QToolButton::clicked, this, &MainWindow::onOpenGitHub);

    themeToggleButton = new QToolButton(this);
    themeToggleButton->setObjectName("topActionButton");
    themeToggleButton->setText(QString());
    themeToggleButton->setIconSize(QSize(14, 14));
    themeToggleButton->setToolTip(QStringLiteral("\u5207\u6362\u4e3b\u9898"));
    themeToggleButton->setCursor(Qt::PointingHandCursor);
    connect(themeToggleButton, &QToolButton::clicked, this, &MainWindow::onToggleTheme);

    pinButton = new QToolButton(this);
    pinButton->setObjectName("topActionButton");
    pinButton->setCheckable(true);
    pinButton->setText(QString());
    pinButton->setIcon(QIcon(QStringLiteral(":/icons/pin.svg")));
    pinButton->setIconSize(QSize(14, 14));
    pinButton->setCursor(Qt::PointingHandCursor);
    connect(pinButton, &QToolButton::toggled, this, &MainWindow::onTogglePinned);

    settingsButton = new QToolButton(this);
    settingsButton->setObjectName("topActionButton");
    settingsButton->setIconSize(QSize(14, 14));
    settingsButton->setToolTip(QStringLiteral("\u8bbe\u7f6e"));
    settingsButton->setCursor(Qt::PointingHandCursor);
    connect(settingsButton, &QToolButton::clicked, this, &MainWindow::onOpenSettings);

    exitButton = new QToolButton(this);
    exitButton->setObjectName("topActionButton");
    exitButton->setIconSize(QSize(14, 14));
    exitButton->setToolTip(QStringLiteral("\u9000\u51fa"));
    exitButton->setCursor(Qt::PointingHandCursor);
    connect(exitButton, &QToolButton::clicked, this, &MainWindow::onRequestExit);

    topBarLayout->addWidget(githubButton);
    topBarLayout->addWidget(themeToggleButton);
    topBarLayout->addWidget(pinButton);
    topBarLayout->addWidget(settingsButton);
    topBarLayout->addWidget(exitButton);
    mainLayout->addLayout(topBarLayout);
    mainLayout->addSpacing(8);

    // 区域截图按钮
    QPushButton *btnRegion = new QPushButton(QStringLiteral("\u533a\u57df\u622a\u56fe"), this);
    btnRegion->setObjectName("captureActionButton");
    QFont captureButtonFont(QStringLiteral("Microsoft YaHei"), 17, QFont::DemiBold);
    captureButtonFont.setLetterSpacing(QFont::AbsoluteSpacing, 0.8);
    btnRegion->setFont(captureButtonFont);
    connect(btnRegion, &QPushButton::clicked, this, [this]() {
        const QSettings settings = AppSettings::createSettings();
        const bool hideSidebar =
            settings.value(AppSettings::kHideSidebar, AppSettings::kDefaultHideSidebar).toBool();
        regionCaptureShouldHideSidebar = hideSidebar;

        if (hideSidebar) {
            if (!isDocked) {
                dockSidebar(false);
            }
            QTimer::singleShot(AppSettings::kPreCaptureDelayMs, this, [this]() {
                captureTool->captureRegion();
            });
            return;
        }

        captureTool->captureRegion();
    });
    mainLayout->addWidget(btnRegion);

    // 全屏截图按钮 (必定隐藏侧边栏)
    QPushButton *btnFull = new QPushButton(QStringLiteral("\u5168\u5c4f\u622a\u56fe"), this);
    btnFull->setObjectName("captureActionButton");
    btnFull->setFont(captureButtonFont);
    connect(btnFull, &QPushButton::clicked, this, [this]() {
        if (!isDocked) {
            dockSidebar(false);
        }
        QTimer::singleShot(AppSettings::kPreCaptureDelayMs, this, [this]() {
            captureTool->captureFullScreen();
        });
    });
    mainLayout->addWidget(btnFull);

    mainLayout->addSpacing(10);
    QWidget *historyDivider = new QWidget(this);
    historyDivider->setObjectName("historyDivider");
    historyDivider->setFixedSize(182, 3);
    mainLayout->addWidget(historyDivider, 0, Qt::AlignHCenter);
    mainLayout->addSpacing(4);

    QLabel *titleLabel = new QLabel(QStringLiteral("\u526a\u8d34\u677f"), this);
    titleLabel->setObjectName("historyTitleLabel");
    titleLabel->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(titleLabel);

    historyList = new QListWidget(this);
    historyList->setContextMenuPolicy(Qt::CustomContextMenu); 
    mainLayout->addWidget(historyList);

    connect(historyList, &QListWidget::itemClicked, this, &MainWindow::onItemClicked);
    connect(historyList, &QListWidget::customContextMenuRequested, this, &MainWindow::onCustomContextMenu);

    setCentralWidget(centralWidget);

    connect(captureTool, &CaptureTool::screenshotTaken, this, [this](const QPixmap &pixmap){
        ignoreClipboardChange = true;
        QApplication::clipboard()->setPixmap(pixmap);
        addClipboardItem(pixmap);
        QTimer::singleShot(100, this, [this](){ ignoreClipboardChange = false; });
    });

    applyTheme();
}

void MainWindow::onItemClicked(QListWidgetItem *item) { historyList->clearSelection(); }

void MainWindow::onCustomContextMenu(const QPoint &pos)
{
    QListWidgetItem *item = historyList->itemAt(pos);
    if (!item) return;

    historyList->setCurrentItem(item); 

    const QVariant data = item->data(Qt::UserRole);
    const bool isImageItem =
        data.typeId() == QMetaType::QPixmap || data.typeId() == QMetaType::Type::QPixmap ||
        data.canConvert<QPixmap>();

    QMenu menu(this);
    QAction *copyAction = menu.addAction(QStringLiteral("\u590d\u5236")); 
    QAction *saveAction = nullptr;
    if (isImageItem) {
        saveAction = menu.addAction(QStringLiteral("\u4fdd\u5b58"));
    }
    menu.addSeparator(); 
    QAction *delAction = menu.addAction(QStringLiteral("\u5220\u9664")); 
    
    QAction *selected = menu.exec(historyList->mapToGlobal(pos));
    if (selected == copyAction) {
        ignoreClipboardChange = true;
        if (data.canConvert<QPixmap>()) {
            QApplication::clipboard()->setPixmap(data.value<QPixmap>());
        } else {
            QApplication::clipboard()->setText(data.toString());
        }
        
        // 【关键修复】: 不直接 takeItem 然后 insert，因为这会销毁自定义 Widget！
        // 正确做法：读取数据 -> 彻底删除旧项 -> 使用数据重新生成一个新项在最顶端。
        int row = historyList->row(item);
        if (row != 0) {
            delete historyList->takeItem(row);
            addClipboardItem(data);
        }
        historyList->clearSelection(); 
        QTimer::singleShot(100, this, [this](){ ignoreClipboardChange = false; });
    } else if (saveAction && selected == saveAction) {
        const QPixmap pixmap = data.value<QPixmap>();
        QString savedPath;
        QString errorText;
        if (savePixmapToConfiguredPath(pixmap, &savedPath, &errorText)) {
            QMessageBox::information(this,
                                     QStringLiteral("\u4fdd\u5b58\u6210\u529f"),
                                     QStringLiteral("\u5df2\u4fdd\u5b58\u5230\uff1a\n%1")
                                         .arg(QDir::toNativeSeparators(savedPath)));
        } else {
            QMessageBox::warning(this,
                                 QStringLiteral("\u4fdd\u5b58\u5931\u8d25"),
                                 errorText.isEmpty()
                                     ? QStringLiteral("\u65e0\u6cd5\u4fdd\u5b58\u56fe\u7247\u6587\u4ef6\u3002")
                                     : errorText);
        }
    } else if (selected == delAction) {
        delete historyList->takeItem(historyList->row(item));
    }
}

void MainWindow::onClipboardChanged()
{
    if (ignoreClipboardChange) return;

    const QMimeData *mimeData = QApplication::clipboard()->mimeData();
    if (!mimeData) return;

    QVariant data;
    QString signature;

    if (mimeData->hasImage()) {
        QPixmap pix = qvariant_cast<QPixmap>(mimeData->imageData());
        if (pix.isNull()) {
            const QImage image = qvariant_cast<QImage>(mimeData->imageData());
            if (!image.isNull()) {
                pix = QPixmap::fromImage(image);
            }
        }

        if (pix.isNull()) {
            return;
        }

        data = pix;
        signature = clipboardSignatureForPixmap(pix);
    } else if (mimeData->hasText()) {
        const QString text = normalizeTextForClipboardSignature(mimeData->text());
        if (text.isEmpty()) {
            return;
        }

        data = text;
        signature = QStringLiteral("txt:%1").arg(text);
    } else {
        return;
    }

    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    if (!signature.isEmpty() && signature == lastClipboardSignature &&
        lastClipboardChangeAtMs > 0 && nowMs >= lastClipboardChangeAtMs &&
        (nowMs - lastClipboardChangeAtMs) <= kClipboardDuplicateWindowMs) {
        return;
    }

    addClipboardItem(data);
    if (!signature.isEmpty()) {
        lastClipboardSignature = signature;
        lastClipboardChangeAtMs = nowMs;
    }
}

void MainWindow::addClipboardItem(const QVariant &data)
{
    QListWidgetItem *item = new QListWidgetItem();
    item->setData(Qt::UserRole, data); 
    historyList->insertItem(0, item);

    QWidget *widget = new QWidget();
    widget->setStyleSheet("background: transparent;"); 
    QVBoxLayout *layout = new QVBoxLayout(widget);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(6);

    QString timeStr = QDateTime::currentDateTime().toString("HH:mm:ss");
    QLabel *timeLabel = new QLabel(timeStr);
    timeLabel->setObjectName("historyTimeLabel");
    timeLabel->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Maximum);
    layout->addWidget(timeLabel);

    if (data.typeId() == QMetaType::QPixmap || data.typeId() == QMetaType::Type::QPixmap) {
        QPixmap pix = data.value<QPixmap>();
        QLabel *imgLabel = new QLabel();
        imgLabel->setPixmap(pix.scaled(200, 120, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        layout->addWidget(imgLabel);
        item->setSizeHint(QSize(normalWidth - 40, 160)); 
    } else {
        QString text = data.toString();
        QLabel *textLabel = new QLabel(text.left(40).replace('\n', " ") + (text.length() > 40 ? "..." : ""));
        textLabel->setObjectName("historyTextLabel");
        layout->addWidget(textLabel);
        item->setSizeHint(QSize(normalWidth - 40, 70)); 
    }
    historyList->setItemWidget(item, widget);
    enforceHistoryLimit();
}

void MainWindow::enterEvent(QEnterEvent *event) {
    if (leaveDockTimer) {
        leaveDockTimer->stop();
    }

    if (trayRevealHoldActive && !isDocked && geometry().contains(QCursor::pos())) {
        trayRevealHoldActive = false;
        if (trayRevealHoldTimer) {
            trayRevealHoldTimer->stop();
        }
    }

    if (isDocked && !shouldSuppressSidebar() && !sidebarPinned && hoverRevealTimer) {
        hoverRevealTimer->start(AppSettings::kSidebarExpandDebounceMs);
    }
    QMainWindow::enterEvent(event);
}

void MainWindow::leaveEvent(QEvent *event)
{
    if (hoverRevealTimer) {
        hoverRevealTimer->stop();
    }

    if (!shouldSuppressSidebar() && !isDocked) {
        if (sidebarPinned || trayRevealHoldActive) {
            QMainWindow::leaveEvent(event);
            return;
        }

        if (leaveDockTimer && !geometry().contains(QCursor::pos())) {
            leaveDockTimer->start(AppSettings::kSidebarAutoDockDelayMs);
        }
    }
    QMainWindow::leaveEvent(event);
}

void MainWindow::checkEdgeDocking() {
    if (sidebarPinned || trayRevealHoldActive || shouldSuppressSidebar() || !animation ||
        animation->state() == QAbstractAnimation::Running) {
        return;
    }

    if (!isDocked && !geometry().contains(QCursor::pos()) && leaveDockTimer &&
        !leaveDockTimer->isActive()) {
        leaveDockTimer->start(AppSettings::kSidebarAutoDockDelayMs);
    }
}

void MainWindow::onSettingsUpdated()
{
    enforceHistoryLimit();
    if (isDocked) {
        dockSidebar(false);
    } else if (centralWidget) {
        const AppSettings::ThemePalette palette =
            isLightTheme() ? AppSettings::getLightThemePalette()
                           : AppSettings::getDarkThemePalette();
        centralWidget->setStyleSheet(expandedPanelStyleSheet(palette));
    }
}

void MainWindow::clearClipboardHistory()
{
    if (historyList) {
        historyList->clear();
    }
}

int MainWindow::currentHistoryLimit() const
{
    QSettings settings = AppSettings::createSettings();
    const int limit =
        settings.value(AppSettings::kHistoryMaxItems, AppSettings::kDefaultHistoryMaxItems).toInt();
    return AppSettings::normalizeHistoryMaxItems(limit);
}

void MainWindow::enforceHistoryLimit()
{
    if (!historyList) {
        return;
    }

    const int limit = currentHistoryLimit();
    while (historyList->count() > limit) {
        delete historyList->takeItem(historyList->count() - 1);
    }
}

void MainWindow::onSettingsDialogVisibilityChanged(bool visible)
{
    settingsDialogVisible = visible;
    applySuppressionState();
}

void MainWindow::onRegionCaptureStateChanged(bool active)
{
    regionCaptureActive = active;
    if (active) {
        const QSettings settings = AppSettings::createSettings();
        regionCaptureShouldHideSidebar =
            settings.value(AppSettings::kHideSidebar, AppSettings::kDefaultHideSidebar).toBool();
    } else {
        regionCaptureShouldHideSidebar = false;
    }
    applySuppressionState();
}

void MainWindow::onPreviewDialogStateChanged(bool visible)
{
    previewDialogVisible = visible;
    applySuppressionState();
}

void MainWindow::onOpenSettings()
{
    if (tray) {
        tray->openSettingsDialog();
    }
}

void MainWindow::onOpenGitHub()
{
    QDesktopServices::openUrl(QUrl(AppSettings::kGitHubUrl));
}

void MainWindow::onTogglePinned(bool checked)
{
    sidebarPinned = checked;

    QSettings settings = AppSettings::createSettings();
    settings.setValue(AppSettings::kSidebarPinned, sidebarPinned);

    updatePinnedUi();

    if (sidebarPinned) {
        trayRevealHoldActive = false;
        if (trayRevealHoldTimer) {
            trayRevealHoldTimer->stop();
        }
        if (leaveDockTimer) {
            leaveDockTimer->stop();
        }
        if (hoverRevealTimer) {
            hoverRevealTimer->stop();
        }

        if (!shouldSuppressSidebar() && isDocked) {
            expandSidebar(true);
        }
        return;
    }

    if (!shouldSuppressSidebar() && !isDocked && !geometry().contains(QCursor::pos()) &&
        leaveDockTimer) {
        leaveDockTimer->start(AppSettings::kSidebarAutoDockDelayMs);
    }
}

void MainWindow::onRequestExit()
{
    const QMessageBox::StandardButton result = QMessageBox::question(
        this,
        QStringLiteral("\u9000\u51fa\u786e\u8ba4"),
        QStringLiteral("\u786e\u5b9a\u9000\u51fa Words-Bin \u5417\uff1f"),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No);

    if (result == QMessageBox::Yes) {
        QApplication::quit();
    }
}

void MainWindow::updatePinnedUi()
{
    if (!pinButton) {
        return;
    }

    const QSignalBlocker blocker(pinButton);
    pinButton->setChecked(sidebarPinned);
    pinButton->setToolTip(sidebarPinned ? QStringLiteral("\u5df2\u56fa\u5b9a\u4fa7\u8fb9\u680f")
                                        : QStringLiteral("\u56fa\u5b9a\u4fa7\u8fb9\u680f"));
}

void MainWindow::onTrayLeftClicked()
{
    show();
    raise();
    activateWindow();

    if (!shouldSuppressSidebar()) {
        revealSidebarWithHold();
    }
}

void MainWindow::revealSidebarWithHold()
{
    const QRect screenGeometry = resolveAvailableGeometry(this);
    if (!screenGeometry.isValid()) {
        return;
    }

    setMinimumWidth(0);
    setMaximumWidth(QWIDGETSIZE_MAX);
    setMinimumHeight(0);
    setMaximumHeight(QWIDGETSIZE_MAX);
    setFixedWidth(normalWidth);
    const int expandedHeight = qMax(320, screenGeometry.height() - 100);
    setFixedHeight(expandedHeight);

    QRect rect = geometry();
    const int maxWidth = qMax(220, screenGeometry.width() - (rightMargin * 2));
    rect.setWidth(qMin(width(), maxWidth));
    rect.setHeight(expandedHeight);
    rect.setX(screenGeometry.right() - rightMargin - rect.width() + 1);

    const int minTop = screenGeometry.top() + 10;
    const int maxTop = screenGeometry.bottom() - rect.height() + 1 - 10;
    rect.moveTop(qBound(minTop, rect.y(), maxTop));

    if (animation) {
        animation->stop();
    }
    if (hoverRevealTimer) {
        hoverRevealTimer->stop();
    }
    if (leaveDockTimer) {
        leaveDockTimer->stop();
    }

    clearMask();
    if (centralWidget) {
        const AppSettings::ThemePalette palette =
            isLightTheme() ? AppSettings::getLightThemePalette()
                           : AppSettings::getDarkThemePalette();
        centralWidget->setStyleSheet(expandedPanelStyleSheet(palette));
    }
    setGeometry(rect);
    isDocked = false;
    setDockContentVisible(true);
    if (centralWidget && centralWidget->graphicsEffect()) {
        centralWidget->graphicsEffect()->setEnabled(true);
    }
    updateDockMask();

    trayRevealHoldActive = !sidebarPinned;
    if (trayRevealHoldTimer && trayRevealHoldActive) {
        trayRevealHoldTimer->start(AppSettings::kTrayRevealHoldMs);
    } else if (trayRevealHoldTimer) {
        trayRevealHoldTimer->stop();
    }
}

void MainWindow::setDockContentVisible(bool visible)
{
    if (!centralWidget) {
        return;
    }

    const QList<QWidget *> children =
        centralWidget->findChildren<QWidget *>(QString(), Qt::FindDirectChildrenOnly);
    for (QWidget *child : children) {
        if (child) {
            child->setVisible(visible);
        }
    }
}

bool MainWindow::shouldSuppressSidebar() const
{
    return settingsDialogVisible || previewDialogVisible ||
           (regionCaptureActive && regionCaptureShouldHideSidebar);
}

QRect MainWindow::dockTriggerRect() const
{
    const int triggerHeight = qMin(dockTriggerHeight, height());
    const int triggerTop = (height() - triggerHeight) / 2;
    return QRect(0, triggerTop, dockTriggerWidth, triggerHeight);
}

void MainWindow::updateDockMask()
{
    if (!isDocked) {
        clearMask();
        return;
    }

    const QSettings settings = AppSettings::createSettings();
    const int configuredRadius =
        settings.value(AppSettings::kDockStripBorderRadius, AppSettings::kDefaultDockStripBorderRadius)
            .toInt();
    const int safeRadius = qBound(0, configuredRadius, qMin(width(), height()) / 2);

    QPainterPath path;
    path.addRoundedRect(QRectF(0, 0, width(), height()), safeRadius, safeRadius);
    setMask(QRegion(path.toFillPolygon().toPolygon()));
}

void MainWindow::dockSidebar(bool animated)
{
    Q_UNUSED(animated);

    const QRect screenGeometry = resolveAvailableGeometry(this);
    if (!screenGeometry.isValid()) {
        return;
    }

    const QSettings settings = AppSettings::createSettings();
    const int stripWidth = settings.value(AppSettings::kDockStripWidth, AppSettings::kDefaultDockStripWidth).toInt();
    const int stripHeight = settings.value(AppSettings::kDockStripHeight, AppSettings::kDefaultDockStripHeight).toInt();
    const int stripBorderRadius = settings.value(AppSettings::kDockStripBorderRadius, AppSettings::kDefaultDockStripBorderRadius).toInt();

    dockTriggerWidth = qBound(AppSettings::kMinDockStripWidth, stripWidth, AppSettings::kMaxDockStripWidth);
    dockTriggerHeight = qBound(AppSettings::kMinDockStripHeight, stripHeight, AppSettings::kMaxDockStripHeight);
    const int safeStripRadius = qBound(0, stripBorderRadius, qMin(dockTriggerWidth, dockTriggerHeight) / 2);

    const AppSettings::ThemePalette palette =
        isLightTheme() ? AppSettings::getLightThemePalette() : AppSettings::getDarkThemePalette();
    const QString stripColor = palette.buttonBackground;

    // Apply dock strip style (color and border radius)
    if (centralWidget) {
        centralWidget->setStyleSheet(dockStripStyleSheet(palette, stripColor, safeStripRadius));
    }

    const int centerY = geometry().center().y();

    setMinimumWidth(0);
    setMaximumWidth(QWIDGETSIZE_MAX);
    setMinimumHeight(0);
    setMaximumHeight(QWIDGETSIZE_MAX);
    setFixedWidth(dockTriggerWidth);
    setFixedHeight(dockTriggerHeight);

    QRect targetRect = geometry();
    targetRect.setWidth(width());
    targetRect.setHeight(height());
    targetRect.setX(screenGeometry.right() - rightMargin - targetRect.width() + 1);
    targetRect.moveTop(centerY - (targetRect.height() / 2));

    const int minTop = screenGeometry.top() + 10;
    const int maxTop = screenGeometry.bottom() - targetRect.height() + 1 - 10;
    targetRect.moveTop(qBound(minTop, targetRect.y(), maxTop));

    if (animation) {
        animation->stop();
    }
    setGeometry(targetRect);
    isDocked = true;
    setDockContentVisible(false);
    if (centralWidget && centralWidget->graphicsEffect()) {
        centralWidget->graphicsEffect()->setEnabled(false);
    }
    updateDockMask();
}

void MainWindow::expandSidebar(bool force)
{
    const bool visuallyDocked = isDocked || (geometry().width() <= dockTriggerWidth + 2);
    if (!visuallyDocked || shouldSuppressSidebar()) {
        return;
    }

    if (!force && animation && animation->state() == QAbstractAnimation::Running) {
        return;
    }

    const QRect screenGeometry = resolveAvailableGeometry(this);
    if (!screenGeometry.isValid()) {
        return;
    }

    const int centerY = geometry().center().y();

    setMinimumWidth(0);
    setMaximumWidth(QWIDGETSIZE_MAX);
    setMinimumHeight(0);
    setMaximumHeight(QWIDGETSIZE_MAX);
    setFixedWidth(normalWidth);
    const int expandedHeight = qMax(320, screenGeometry.height() - 100);
    setFixedHeight(expandedHeight);

    QRect rect = geometry();
    rect.setWidth(width());
    rect.setHeight(expandedHeight);
    rect.setX(screenGeometry.right() - rightMargin - rect.width() + 1);
    rect.moveTop(centerY - (rect.height() / 2));

    const int minTop = screenGeometry.top() + 10;
    const int maxTop = screenGeometry.bottom() - rect.height() + 1 - 10;
    rect.moveTop(qBound(minTop, rect.y(), maxTop));

    clearMask();
    if (animation) {
        animation->stop();
    }
    if (centralWidget) {
        const AppSettings::ThemePalette palette =
            isLightTheme() ? AppSettings::getLightThemePalette()
                           : AppSettings::getDarkThemePalette();
        centralWidget->setStyleSheet(expandedPanelStyleSheet(palette));
    }

    if (force) {
        setGeometry(rect);
        isDocked = false;
        setDockContentVisible(true);
        if (centralWidget && centralWidget->graphicsEffect()) {
            centralWidget->graphicsEffect()->setEnabled(true);
        }
        return;
    }

    if (!animation) {
        setGeometry(rect);
        isDocked = false;
        setDockContentVisible(true);
        if (centralWidget && centralWidget->graphicsEffect()) {
            centralWidget->graphicsEffect()->setEnabled(true);
        }
        return;
    }

    animation->setEasingCurve(QEasingCurve::InOutCubic);
    animation->setStartValue(geometry());
    animation->setEndValue(rect);
    isDocked = false;
    setDockContentVisible(false);
    if (centralWidget && centralWidget->graphicsEffect()) {
        centralWidget->graphicsEffect()->setEnabled(true);
    }
    animation->start();
}

void MainWindow::applySuppressionState()
{
    if (shouldSuppressSidebar()) {
        if (hoverRevealTimer) {
            hoverRevealTimer->stop();
        }
        if (leaveDockTimer) {
            leaveDockTimer->stop();
        }
        trayRevealHoldActive = false;
        if (trayRevealHoldTimer) {
            trayRevealHoldTimer->stop();
        }
        dockSidebar(false);
        return;
    }

    if (sidebarPinned) {
        trayRevealHoldActive = false;
        if (trayRevealHoldTimer) {
            trayRevealHoldTimer->stop();
        }
        if (isDocked) {
            expandSidebar(true);
        }
        return;
    }

    updateDockMask();
}

void MainWindow::onToggleTheme()
{
    themeMode = isLightTheme() ? QStringLiteral("dark") : QStringLiteral("light");

    QSettings settings = AppSettings::createSettings();
    settings.setValue(AppSettings::kThemeMode, themeMode);
    settings.sync();

    applyTheme();
}

void MainWindow::applyTheme()
{
    themeMode = normalizeThemeModeValue(themeMode);
    updateTopBarIcons();
    updateThemeToggleUi();

    if (!centralWidget) {
        return;
    }

    const AppSettings::ThemePalette palette =
        isLightTheme() ? AppSettings::getLightThemePalette() : AppSettings::getDarkThemePalette();

    if (isDocked) {
        const QSettings settings = AppSettings::createSettings();
        const int stripBorderRadius = settings.value(AppSettings::kDockStripBorderRadius,
                                                     AppSettings::kDefaultDockStripBorderRadius)
                                        .toInt();
        const int safeStripRadius = qBound(0, stripBorderRadius, qMin(width(), height()) / 2);
        const QString stripColor = palette.buttonBackground;
        centralWidget->setStyleSheet(dockStripStyleSheet(palette, stripColor, safeStripRadius));
        updateDockMask();
    } else {
        centralWidget->setStyleSheet(expandedPanelStyleSheet(palette));
    }
}

void MainWindow::updateTopBarIcons()
{
    const QColor iconColor = isLightTheme() ? QColor(QStringLiteral("#202020"))
                                             : QColor(QStringLiteral("#F2F2F2"));

    if (githubButton) {
        githubButton->setIcon(loadThemeAwareSvgIcon(QStringLiteral(":/icons/github.svg"), iconColor));
    }

    if (pinButton) {
        pinButton->setIcon(loadThemeAwareSvgIcon(QStringLiteral(":/icons/pin.svg"), iconColor));
    }

    if (settingsButton) {
        settingsButton->setIcon(loadThemeAwareSvgIcon(QStringLiteral(":/icons/settings.svg"), iconColor));
    }

    if (exitButton) {
        exitButton->setIcon(loadThemeAwareSvgIcon(QStringLiteral(":/icons/shutdown.svg"), iconColor));
    }
}

bool MainWindow::isLightTheme() const
{
    return currentThemeMode() == QStringLiteral("light");
}

void MainWindow::updateThemeToggleUi()
{
    if (!themeToggleButton) {
        return;
    }

    if (isLightTheme()) {
        const QIcon icon = loadThemeAwareSvgIcon(QStringLiteral(":/icons/theme_light.svg"),
                                                 QColor(QStringLiteral("#202020")));
        themeToggleButton->setIcon(icon);
        themeToggleButton->setText(icon.isNull() ? QStringLiteral("\u25CB") : QString());
        themeToggleButton->setToolTip(
            QStringLiteral("\u5f53\u524d\uff1a\u6d45\u8272\u6a21\u5f0f\uff08\u70b9\u51fb\u5207\u6362\u6df1\u8272\uff09"));
    } else {
        const QIcon icon = loadThemeAwareSvgIcon(QStringLiteral(":/icons/theme_dark.svg"),
                                                 QColor(QStringLiteral("#F2F2F2")));
        themeToggleButton->setIcon(icon);
        themeToggleButton->setText(icon.isNull() ? QStringLiteral("\u263E") : QString());
        themeToggleButton->setToolTip(
            QStringLiteral("\u5f53\u524d\uff1a\u6df1\u8272\u6a21\u5f0f\uff08\u70b9\u51fb\u5207\u6362\u6d45\u8272\uff09"));
    }
}

QString MainWindow::currentThemeMode() const
{
    return normalizeThemeModeValue(themeMode);
}
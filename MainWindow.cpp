#include "MainWindow.h"
#include "AppSettings.h"
#include "TrayIcon.h"
#include "CaptureTool.h"
#include <QApplication>
#include <QScreen>
#include <QCursor>
#include <QClipboard>
#include <QMimeData>
#include <QDateTime>
#include <QDesktopServices>
#include <QEasingCurve>
#include <QGraphicsDropShadowEffect>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QMenu>
#include <QSignalBlocker>
#include <QSettings>
#include <QToolButton>
#include <QUrl>
#include <QWindow>

namespace {
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
        // 加载主题设置
        currentThemeMode = settings.value(AppSettings::kThemeMode, AppSettings::kDefaultThemeMode).toString();
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
    connect(animation, &QPropertyAnimation::finished, this, [this]() { updateDockMask(); });
    
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
    centralWidget->setStyleSheet(
        "#centralWidget { background-color: #2D2D2D; border: 1.5px solid #404040; border-radius: 10px; }"
        "QPushButton { background-color: #3D3D3D; color: white; border: none; padding: 12px; border-radius: 5px; text-align: left; }"
        "QPushButton:hover { background-color: #007AFF; }"
        "QToolButton#topActionButton { background-color: #3D3D3D; color: white; border: none; border-radius: 12px; min-width: 24px; min-height: 24px; max-width: 24px; max-height: 24px; font-weight: 600; }"
        "QToolButton#topActionButton:hover { background-color: #007AFF; }"
        "QToolButton#topActionButton:checked { background-color: #007AFF; color: white; }"
        "QToolButton#topActionButton:pressed { background-color: #005FCC; }"
        "QToolButton#topActionButton:disabled { background-color: #303030; color: #777777; }"
        "QListWidget { background-color: transparent; border: none; outline: none; }"
        "QListWidget::item { background-color: #383838; margin-bottom: 8px; border-radius: 6px; }"
        "QListWidget::item:selected { background-color: #4A4A4A; border: 1px solid #00E5FF; }"
    );

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

    QToolButton *githubButton = new QToolButton(this);
    githubButton->setObjectName("topActionButton");
    githubButton->setText(QStringLiteral("GH"));
    githubButton->setToolTip(QStringLiteral("GitHub"));
    githubButton->setCursor(Qt::PointingHandCursor);
    connect(githubButton, &QToolButton::clicked, this, &MainWindow::onOpenGitHub);

    themeToggleButton = new QToolButton(this);
    themeToggleButton->setObjectName("topActionButton");
    themeToggleButton->setText(currentThemeMode == QStringLiteral("dark") ? QStringLiteral("?") : QStringLiteral("?"));
    themeToggleButton->setToolTip(currentThemeMode == QStringLiteral("dark") ? QStringLiteral("浅色") : QStringLiteral("深色"));
    themeToggleButton->setCursor(Qt::PointingHandCursor);
    connect(themeToggleButton, &QToolButton::clicked, this, &MainWindow::onToggleTheme);

    pinButton = new QToolButton(this);
    pinButton->setObjectName("topActionButton");
    pinButton->setCheckable(true);
    pinButton->setText(QString::fromUtf8("\xF0\x9F\x93\x8C"));
    pinButton->setCursor(Qt::PointingHandCursor);
    connect(pinButton, &QToolButton::toggled, this, &MainWindow::onTogglePinned);

    QToolButton *settingsButton = new QToolButton(this);
    settingsButton->setObjectName("topActionButton");
    settingsButton->setText(QStringLiteral("\u2699"));
    settingsButton->setToolTip(QStringLiteral("\u8bbe\u7f6e"));
    settingsButton->setCursor(Qt::PointingHandCursor);
    connect(settingsButton, &QToolButton::clicked, this, &MainWindow::onOpenSettings);

    QToolButton *exitButton = new QToolButton(this);
    exitButton->setObjectName("topActionButton");
    exitButton->setText(QStringLiteral("\u00d7"));
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
    QLabel *titleLabel = new QLabel(QStringLiteral("\u526a\u8d34\u677f\u5386\u53f2"), this); 
    titleLabel->setStyleSheet("color: white; font-weight: bold;");
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
}

void MainWindow::onItemClicked(QListWidgetItem *item) { historyList->clearSelection(); }

void MainWindow::onCustomContextMenu(const QPoint &pos)
{
    QListWidgetItem *item = historyList->itemAt(pos);
    if (!item) return;

    historyList->setCurrentItem(item); 

    QMenu menu(this);
    QAction *copyAction = menu.addAction(QStringLiteral("\u590d\u5236")); 
    menu.addSeparator(); 
    QAction *delAction = menu.addAction(QStringLiteral("\u5220\u9664")); 
    
    QAction *selected = menu.exec(historyList->mapToGlobal(pos));
    if (selected == copyAction) {
        ignoreClipboardChange = true;
        QVariant data = item->data(Qt::UserRole);
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
    } else if (selected == delAction) {
        delete historyList->takeItem(historyList->row(item));
    }
}

void MainWindow::onClipboardChanged()
{
    if (ignoreClipboardChange) return;
    const QMimeData *mimeData = QApplication::clipboard()->mimeData();
    if (!mimeData) return;
    if (mimeData->hasImage()) {
        QPixmap pix = qvariant_cast<QPixmap>(mimeData->imageData());
        addClipboardItem(pix);
    } else if (mimeData->hasText()) {
        QString text = mimeData->text().trimmed();
        if (!text.isEmpty()) addClipboardItem(text);
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
    timeLabel->setStyleSheet("background-color: #555555; color: #00E5FF; border-radius: 4px; padding: 2px 6px; font-weight: bold; font-size: 11px;");
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
        textLabel->setStyleSheet("color: #E0E0E0; font-size: 13px;");
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

void MainWindow::onSettingsUpdated() { enforceHistoryLimit(); }

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
        QStringLiteral("\u786e\u5b9a\u9000\u51fa SnipLite \u5417\uff1f"),
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
    setGeometry(rect);
    isDocked = false;
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
    Q_UNUSED(isDocked);
    clearMask();
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
    const int colorIndex = settings.value(AppSettings::kDockStripColorIndex, AppSettings::kDefaultDockStripColorIndex).toInt();
    
    dockTriggerWidth = qBound(AppSettings::kMinDockStripWidth, stripWidth, AppSettings::kMaxDockStripWidth);
    dockTriggerHeight = qBound(AppSettings::kMinDockStripHeight, stripHeight, AppSettings::kMaxDockStripHeight);
    
    const QStringList presetColors = AppSettings::getDockStripPresetColors();
    const QString stripColor = (colorIndex >= 0 && colorIndex < presetColors.size()) 
                                 ? presetColors.at(colorIndex)
                                 : presetColors.first();
    
    // 应用dock strip的样式（颜色和圆角）
    centralWidget->setStyleSheet(
        QString("#centralWidget { background-color: %1; border: 1.5px solid #404040; border-radius: %2px; }")
            .arg(stripColor)
            .arg(stripBorderRadius)
        + "QPushButton { background-color: #3D3D3D; color: white; border: none; padding: 12px; border-radius: 5px; text-align: left; }"
          "QPushButton:hover { background-color: #007AFF; }"
          "QToolButton#topActionButton { background-color: #3D3D3D; color: white; border: none; border-radius: 12px; min-width: 24px; min-height: 24px; max-width: 24px; max-height: 24px; font-weight: 600; }"
          "QToolButton#topActionButton:hover { background-color: #007AFF; }"
          "QToolButton#topActionButton:checked { background-color: #007AFF; color: white; }"
          "QToolButton#topActionButton:pressed { background-color: #005FCC; }"
          "QToolButton#topActionButton:disabled { background-color: #303030; color: #777777; }"
          "QListWidget { background-color: transparent; border: none; outline: none; }"
          "QListWidget::item { background-color: #383838; margin-bottom: 8px; border-radius: 6px; }"
          "QListWidget::item:selected { background-color: #4A4A4A; border: 1px solid #00E5FF; }"
    );

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

    if (force) {
        setGeometry(rect);
        isDocked = false;
        if (centralWidget && centralWidget->graphicsEffect()) {
            centralWidget->graphicsEffect()->setEnabled(true);
        }
        return;
    }

    if (!animation) {
        setGeometry(rect);
        isDocked = false;
        if (centralWidget && centralWidget->graphicsEffect()) {
            centralWidget->graphicsEffect()->setEnabled(true);
        }
        return;
    }

    animation->setEasingCurve(QEasingCurve::InOutCubic);
    animation->setStartValue(geometry());
    animation->setEndValue(rect);
    isDocked = false;
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
    QString newTheme = (currentThemeMode == QStringLiteral("dark"))
                           ? QStringLiteral("light")
                           : QStringLiteral("dark");
    currentThemeMode = newTheme;
    
    QSettings settings = AppSettings::createSettings();
    settings.setValue(AppSettings::kThemeMode, newTheme);
    settings.sync();
    
    applyTheme(newTheme);
}

void MainWindow::applyTheme(const QString &mode)
{
    AppSettings::ThemePalette palette = (mode == QStringLiteral("light"))
                                            ? AppSettings::getLightThemePalette()
                                            : AppSettings::getDarkThemePalette();
    
    // 更新主题按钮的文字和工具提示
    if (themeToggleButton) {
        themeToggleButton->setText(mode == QStringLiteral("dark") ? QStringLiteral("?") : QStringLiteral("?"));
        themeToggleButton->setToolTip(mode == QStringLiteral("dark") ? QStringLiteral("浅色") : QStringLiteral("深色"));
    }
    
    // 应用全局样式表
    QString styleSheet = QString(
        "QMainWindow, QDialog { background-color: %1; color: %3; }"
        "QWidget { background-color: %1; color: %3; }"
        "QPushButton { background-color: %5; color: %3; border: none; padding: 8px; border-radius: 4px; }"
        "QPushButton:hover { background-color: %6; }"
        "QPushButton:pressed { background-color: %7; }"
        "QToolButton { background-color: %5; color: %3; border: none; }"
        "QToolButton:hover { background-color: %6; }"
        "QToolButton#topActionButton { background-color: %5; color: %3; border: none; border-radius: 6px; min-width: 24px; min-height: 24px; max-width: 24px; max-height: 24px; }"
        "QToolButton#topActionButton:hover { background-color: %6; }"
        "QToolButton#topActionButton:checked { background-color: %6; }"
        "QLineEdit, QComboBox, QSpinBox { background-color: %2; color: %3; border: 1px solid %8; border-radius: 4px; padding: 4px; }"
        "QLineEdit:focus, QComboBox:focus, QSpinBox:focus { border: 1px solid %6; }"
        "QCheckBox { color: %3; }"
        "QLabel { color: %3; }"
        "QListWidget { background-color: %2; border: none; }"
        "QListWidget::item { background-color: %2; color: %3; }"
        "QListWidget::item:selected { background-color: %6; }"
    ).arg(palette.background, palette.secondaryBackground, palette.text,
          palette.secondaryText, palette.buttonBackground, palette.buttonHover,
          palette.buttonPressed, palette.border);
    
    qApp->setStyleSheet(styleSheet);
    
    // 重新应用centralWidget的特定样式（dock strip颜色）
    if (isDocked) {
        dockSidebar(false);
    }
}
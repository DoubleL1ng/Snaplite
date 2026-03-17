#include "SettingsDialog.h"

#include "AppSettings.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QKeySequenceEdit>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSettings>
#include <QSpinBox>

namespace {
#ifdef Q_OS_WIN
constexpr auto kAutoStartRegPath = "HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run";
constexpr auto kAutoStartValueName = "SnipLite";
#endif
} // namespace

SettingsDialog::SettingsDialog(QWidget *parent) : QDialog(parent)
{
    buildUi();
    loadCurrentSettings();
}

QKeySequence SettingsDialog::captureHotkey() const
{
    QKeySequence sequence = hotkeyEdit->keySequence();
    if (sequence.isEmpty()) {
        sequence = QKeySequence(AppSettings::kDefaultCaptureHotkey);
    }
    return sequence;
}

QString SettingsDialog::savePath() const
{
    return AppSettings::normalizeSavePath(savePathEdit->text());
}

QString SettingsDialog::saveFormat() const
{
    return AppSettings::normalizeSaveFormat(saveFormatCombo->currentText());
}

bool SettingsDialog::hideSidebar() const { return hideSidebarCheck->isChecked(); }

int SettingsDialog::historyMaxItems() const
{
    return AppSettings::normalizeHistoryMaxItems(historyLimitSpin->value());
}

bool SettingsDialog::shouldClearHistory() const { return clearHistoryRequested; }

bool SettingsDialog::autoStartEnabled() const
{
#ifdef Q_OS_WIN
    return autoStartCheck->isChecked();
#else
    return false;
#endif
}

int SettingsDialog::dockStripWidth() const
{
    return dockStripWidthSpin->value();
}

int SettingsDialog::dockStripHeight() const
{
    return dockStripHeightSpin->value();
}

int SettingsDialog::dockStripBorderRadius() const
{
    return dockStripRadiusSpin->value();
}

int SettingsDialog::dockStripColorIndex() const
{
    return dockStripColorCombo->currentIndex();
}

void SettingsDialog::accept()
{
    const QString normalizedPath = AppSettings::normalizeSavePath(savePathEdit->text());
    QDir dir(normalizedPath);
    if (!dir.exists() && !dir.mkpath(QStringLiteral("."))) {
        QMessageBox::warning(this,
                             QStringLiteral("\u8bbe\u7f6e"),
                             QStringLiteral("\u4fdd\u5b58\u8def\u5f84\u4e0d\u53ef\u7528\uff0c\u8bf7\u9009\u62e9\u5176\u4ed6\u76ee\u5f55\u3002"));
        return;
    }

    savePathEdit->setText(normalizedPath);
    
    // 保存dock strip配置
    QSettings settings = AppSettings::createSettings();
    settings.setValue(AppSettings::kDockStripWidth, dockStripWidth());
    settings.setValue(AppSettings::kDockStripHeight, dockStripHeight());
    settings.setValue(AppSettings::kDockStripBorderRadius, dockStripBorderRadius());
    settings.setValue(AppSettings::kDockStripColorIndex, dockStripColorIndex());
    settings.sync();
    
    QDialog::accept();
}

void SettingsDialog::buildUi()
{
    setWindowTitle(QStringLiteral("SnipLite \u8bbe\u7f6e"));
    setModal(true);
    resize(520, 360);

    QFormLayout *layout = new QFormLayout(this);

    hotkeyEdit = new QKeySequenceEdit(this);
    layout->addRow(QStringLiteral("\u622a\u56fe\u5feb\u6377\u952e:"), hotkeyEdit);

    savePathEdit = new QLineEdit(this);
    QPushButton *browseButton = new QPushButton(QStringLiteral("\u6d4f\u89c8..."), this);
    QHBoxLayout *pathLayout = new QHBoxLayout();
    pathLayout->setContentsMargins(0, 0, 0, 0);
    pathLayout->addWidget(savePathEdit);
    pathLayout->addWidget(browseButton);
    layout->addRow(QStringLiteral("\u4fdd\u5b58\u8def\u5f84:"), pathLayout);

    connect(browseButton, &QPushButton::clicked, this, [this]() {
        const QString selectedDir = QFileDialog::getExistingDirectory(
            this,
            QStringLiteral("\u9009\u62e9\u4fdd\u5b58\u76ee\u5f55"),
            savePathEdit->text());
        if (!selectedDir.isEmpty()) {
            savePathEdit->setText(AppSettings::normalizeSavePath(selectedDir));
        }
    });

    saveFormatCombo = new QComboBox(this);
    saveFormatCombo->addItem(QStringLiteral("PNG"));
    saveFormatCombo->addItem(QStringLiteral("JPG"));
    layout->addRow(QStringLiteral("\u56fe\u7247\u683c\u5f0f:"), saveFormatCombo);

    hideSidebarCheck = new QCheckBox(QStringLiteral("\u533a\u57df\u622a\u56fe\u65f6\u9690\u85cf\u4fa7\u8fb9\u680f"), this);
    layout->addRow(QString(), hideSidebarCheck);

    historyLimitSpin = new QSpinBox(this);
    historyLimitSpin->setRange(AppSettings::kMinHistoryMaxItems, AppSettings::kMaxHistoryMaxItems);
    layout->addRow(QStringLiteral("\u5386\u53f2\u4e0a\u9650:"), historyLimitSpin);

    clearHistoryButton = new QPushButton(QStringLiteral("\u6e05\u7a7a\u526a\u8d34\u677f\u5386\u53f2"), this);
    layout->addRow(QString(), clearHistoryButton);
    connect(clearHistoryButton, &QPushButton::clicked, this, [this]() {
        clearHistoryRequested = true;
        clearHistoryButton->setText(QStringLiteral("\u5df2\u6807\u8bb0\uff1a\u4fdd\u5b58\u540e\u6e05\u7a7a\u5386\u53f2"));
    });

    // 添加分隔符
    layout->addRow(new QLabel(QStringLiteral("─────────────────────")));
    layout->addRow(new QLabel(QStringLiteral("<b>停靠条定制</b>")));
    
    // Dock strip width
    dockStripWidthSpin = new QSpinBox(this);
    dockStripWidthSpin->setRange(AppSettings::kMinDockStripWidth, AppSettings::kMaxDockStripWidth);
    dockStripWidthSpin->setSuffix(QStringLiteral(" px"));
    layout->addRow(QStringLiteral("宽度:"), dockStripWidthSpin);
    
    // Dock strip height
    dockStripHeightSpin = new QSpinBox(this);
    dockStripHeightSpin->setRange(AppSettings::kMinDockStripHeight, AppSettings::kMaxDockStripHeight);
    dockStripHeightSpin->setSuffix(QStringLiteral(" px"));
    layout->addRow(QStringLiteral("高度:"), dockStripHeightSpin);
    
    // Dock strip border radius
    dockStripRadiusSpin = new QSpinBox(this);
    dockStripRadiusSpin->setRange(AppSettings::kMinDockStripBorderRadius, AppSettings::kMaxDockStripBorderRadius);
    dockStripRadiusSpin->setSuffix(QStringLiteral(" px"));
    layout->addRow(QStringLiteral("圆角:"), dockStripRadiusSpin);
    
    // Dock strip color preset
    dockStripColorCombo = new QComboBox(this);
    const QStringList presetColors = AppSettings::getDockStripPresetColors();
    for (int i = 0; i < presetColors.size(); ++i) {
        const QString color = presetColors.at(i);
        dockStripColorCombo->addItem(color);
        // 设置下拉框显示样式颜色
        dockStripColorCombo->setItemData(i, QBrush(QColor(color)), Qt::BackgroundRole);
    }
    layout->addRow(QStringLiteral("颜色:"), dockStripColorCombo);

    autoStartCheck = new QCheckBox(QStringLiteral("\u5f00\u673a\u81ea\u52a8\u8fd0\u884c"), this);
#ifdef Q_OS_WIN
    layout->addRow(QString(), autoStartCheck);
#else
    autoStartCheck->setEnabled(false);
    autoStartCheck->setToolTip(QStringLiteral("\u4ec5 Windows \u53ef\u7528"));
    layout->addRow(QString(), autoStartCheck);
#endif

    QLabel *hintLabel = new QLabel(
        QStringLiteral("\u8bf4\u660e\uff1a\u70b9\u51fb\u9884\u89c8\u7a97\u53e3\u7684\u201c\u4fdd\u5b58\u201d\u6309\u94ae\u624d\u4f1a\u843d\u76d8\uff0c\u201c\u5b8c\u6210\u201d\u4ec5\u5199\u5165\u526a\u8d34\u677f\u548c\u5386\u53f2\u3002"),
        this);
    hintLabel->setWordWrap(true);
    layout->addRow(QString(), hintLabel);

    QDialogButtonBox *buttonBox =
        new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    layout->addWidget(buttonBox);

    connect(buttonBox, &QDialogButtonBox::accepted, this, &SettingsDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &SettingsDialog::reject);
}

void SettingsDialog::loadCurrentSettings()
{
    QSettings settings = AppSettings::createSettings();

    const QString hotkeyText =
        settings.value(AppSettings::kCaptureHotkey, AppSettings::kDefaultCaptureHotkey).toString();
    hotkeyEdit->setKeySequence(
        QKeySequence::fromString(hotkeyText, QKeySequence::PortableText));

    const QString currentPath =
        settings.value(AppSettings::kSavePath, AppSettings::defaultSavePath()).toString();
    savePathEdit->setText(AppSettings::normalizeSavePath(currentPath));

    const QString format = AppSettings::normalizeSaveFormat(
        settings.value(AppSettings::kSaveFormat, AppSettings::kDefaultSaveFormat).toString());
    saveFormatCombo->setCurrentText(format);

    hideSidebarCheck->setChecked(
        settings.value(AppSettings::kHideSidebar, AppSettings::kDefaultHideSidebar).toBool());

    const int historyLimit = AppSettings::normalizeHistoryMaxItems(
        settings.value(AppSettings::kHistoryMaxItems, AppSettings::kDefaultHistoryMaxItems)
            .toInt());
    historyLimitSpin->setValue(historyLimit);
    
    // 加载dock strip配置
    const int stripWidth = settings.value(AppSettings::kDockStripWidth, AppSettings::kDefaultDockStripWidth).toInt();
    const int stripHeight = settings.value(AppSettings::kDockStripHeight, AppSettings::kDefaultDockStripHeight).toInt();
    const int stripRadius = settings.value(AppSettings::kDockStripBorderRadius, AppSettings::kDefaultDockStripBorderRadius).toInt();
    const int colorIndex = settings.value(AppSettings::kDockStripColorIndex, AppSettings::kDefaultDockStripColorIndex).toInt();
    
    dockStripWidthSpin->setValue(stripWidth);
    dockStripHeightSpin->setValue(stripHeight);
    dockStripRadiusSpin->setValue(stripRadius);
    dockStripColorCombo->setCurrentIndex(qBound(0, colorIndex, AppSettings::getDockStripPresetColors().size() - 1));

#ifdef Q_OS_WIN
    QSettings bootSettings(QString::fromLatin1(kAutoStartRegPath), QSettings::NativeFormat);
    autoStartCheck->setChecked(bootSettings.contains(QString::fromLatin1(kAutoStartValueName)));
#else
    autoStartCheck->setChecked(false);
#endif
}

#include "SettingsDialog.h"

#include "AppSettings.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
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
constexpr auto kAutoStartValueName = "Words-Bin";
constexpr auto kLegacyAutoStartValueName = "SnipLite";
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
    bool ok = false;
    int value = historyLimitCombo->currentText().toInt(&ok);
    return ok ? AppSettings::normalizeHistoryMaxItems(value) : AppSettings::kDefaultHistoryMaxItems;
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
    
    // Save dock strip settings.
    QSettings settings = AppSettings::createSettings();
    settings.setValue(AppSettings::kDockStripWidth, dockStripWidth());
    settings.setValue(AppSettings::kDockStripHeight, dockStripHeight());
    settings.setValue(AppSettings::kDockStripBorderRadius, dockStripBorderRadius());
    settings.sync();
    
    QDialog::accept();
}

void SettingsDialog::buildUi()
{
    setWindowTitle(QStringLiteral("\u8bbe\u7f6e - Words-Bin"));
    setModal(true);
    resize(560, 520);

    QVBoxLayout *rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(14, 14, 14, 14);
    rootLayout->setSpacing(10);

    QGroupBox *generalGroup = new QGroupBox(QStringLiteral("\u622a\u56fe\u4e0e\u4fdd\u5b58"), this);
    QFormLayout *generalLayout = new QFormLayout(generalGroup);
    generalLayout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    generalLayout->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
    generalLayout->setFormAlignment(Qt::AlignTop);

    hotkeyEdit = new QKeySequenceEdit(this);
    generalLayout->addRow(QStringLiteral("\u622a\u56fe\u5feb\u6377\u952e:"), hotkeyEdit);

    savePathEdit = new QLineEdit(this);
    QPushButton *browseButton = new QPushButton(QStringLiteral("\u6d4f\u89c8..."), this);
    QHBoxLayout *pathLayout = new QHBoxLayout();
    pathLayout->setContentsMargins(0, 0, 0, 0);
    pathLayout->setSpacing(8);
    pathLayout->addWidget(savePathEdit, 1);
    pathLayout->addWidget(browseButton);
    generalLayout->addRow(QStringLiteral("\u4fdd\u5b58\u8def\u5f84:"), pathLayout);

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
    generalLayout->addRow(QStringLiteral("\u56fe\u7247\u683c\u5f0f:"), saveFormatCombo);

    hideSidebarCheck = new QCheckBox(QStringLiteral("\u533a\u57df\u622a\u56fe\u65f6\u9690\u85cf\u4fa7\u8fb9\u680f"), this);
    generalLayout->addRow(QString(), hideSidebarCheck);

    historyLimitCombo = new QComboBox(this);
    historyLimitCombo->addItem(QStringLiteral("30"));
    historyLimitCombo->addItem(QStringLiteral("50"));
    historyLimitCombo->addItem(QStringLiteral("100"));
    generalLayout->addRow(QStringLiteral("\u5386\u53f2\u4e0a\u9650:"), historyLimitCombo);

    clearHistoryButton = new QPushButton(QStringLiteral("\u6e05\u7a7a\u526a\u8d34\u677f\u5386\u53f2"), this);
    generalLayout->addRow(QString(), clearHistoryButton);
    connect(clearHistoryButton, &QPushButton::clicked, this, [this]() {
        clearHistoryRequested = true;
        clearHistoryButton->setText(QStringLiteral("\u5df2\u6807\u8bb0\uff1a\u4fdd\u5b58\u540e\u6e05\u7a7a\u5386\u53f2"));
    });

    QGroupBox *dockGroup = new QGroupBox(QStringLiteral("\u505c\u9760\u6761"), this);
    QGridLayout *dockLayout = new QGridLayout(dockGroup);
    dockLayout->setContentsMargins(10, 8, 10, 8);
    dockLayout->setHorizontalSpacing(10);
    dockLayout->setVerticalSpacing(8);

    dockStripWidthSpin = new QSpinBox(this);
    dockStripWidthSpin->setRange(AppSettings::kMinDockStripWidth, AppSettings::kMaxDockStripWidth);
    dockStripWidthSpin->setSuffix(QStringLiteral(" px"));
    dockStripWidthSpin->setButtonSymbols(QAbstractSpinBox::NoButtons);
    dockStripWidthSpin->setToolTip(QStringLiteral("\u53ef\u76f4\u63a5\u8f93\u5165\u6570\u5b57\u6216\u4f7f\u7528\u9f20\u6807\u6eda\u8f6e\u8c03\u6574"));

    dockStripHeightSpin = new QSpinBox(this);
    dockStripHeightSpin->setRange(AppSettings::kMinDockStripHeight, AppSettings::kMaxDockStripHeight);
    dockStripHeightSpin->setSuffix(QStringLiteral(" px"));
    dockStripHeightSpin->setButtonSymbols(QAbstractSpinBox::NoButtons);
    dockStripHeightSpin->setToolTip(QStringLiteral("\u53ef\u76f4\u63a5\u8f93\u5165\u6570\u5b57\u6216\u4f7f\u7528\u9f20\u6807\u6eda\u8f6e\u8c03\u6574"));

    dockStripRadiusSpin = new QSpinBox(this);
    dockStripRadiusSpin->setRange(AppSettings::kMinDockStripBorderRadius, AppSettings::kMaxDockStripBorderRadius);
    dockStripRadiusSpin->setSuffix(QStringLiteral(" px"));
    dockStripRadiusSpin->setButtonSymbols(QAbstractSpinBox::NoButtons);
    dockStripRadiusSpin->setToolTip(QStringLiteral("\u53ef\u76f4\u63a5\u8f93\u5165\u6570\u5b57\u6216\u4f7f\u7528\u9f20\u6807\u6eda\u8f6e\u8c03\u6574"));

    dockLayout->addWidget(new QLabel(QStringLiteral("\u5bbd\u5ea6:"), this), 0, 0);
    dockLayout->addWidget(dockStripWidthSpin, 0, 1);
    dockLayout->addWidget(new QLabel(QStringLiteral("\u9ad8\u5ea6:"), this), 0, 2);
    dockLayout->addWidget(dockStripHeightSpin, 0, 3);
    dockLayout->addWidget(new QLabel(QStringLiteral("\u5706\u89d2\u534a\u5f84:"), this), 1, 0);
    dockLayout->addWidget(dockStripRadiusSpin, 1, 1);
    dockLayout->setColumnStretch(1, 1);
    dockLayout->setColumnStretch(3, 1);

    QGroupBox *systemGroup = new QGroupBox(QStringLiteral("\u7cfb\u7edf"), this);
    QVBoxLayout *systemLayout = new QVBoxLayout(systemGroup);
    systemLayout->setContentsMargins(10, 8, 10, 8);
    systemLayout->setSpacing(8);

    autoStartCheck = new QCheckBox(QStringLiteral("\u5f00\u673a\u81ea\u52a8\u8fd0\u884c"), this);
#ifdef Q_OS_WIN
    systemLayout->addWidget(autoStartCheck);
#else
    autoStartCheck->setEnabled(false);
    autoStartCheck->setToolTip(QStringLiteral("仅 Windows 可用"));
    systemLayout->addWidget(autoStartCheck);
#endif

    QLabel *hintLabel = new QLabel(
        QStringLiteral("\u8bf4\u660e\uff1a\u70b9\u51fb\u9884\u89c8\u7a97\u53e3\u7684\u201c\u4fdd\u5b58\u201d\u6309\u94ae\u624d\u4f1a\u843d\u76d8\uff0c\u201c\u5b8c\u6210\u201d\u4ec5\u5199\u5165\u526a\u8d34\u677f\u548c\u5386\u53f2\u3002"),
        this);
    hintLabel->setWordWrap(true);
    hintLabel->setObjectName("hintLabel");
    systemLayout->addWidget(hintLabel);

    rootLayout->addWidget(generalGroup);
    rootLayout->addWidget(dockGroup);
    rootLayout->addWidget(systemGroup);
    rootLayout->addStretch(1);

    QDialogButtonBox *buttonBox =
        new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    rootLayout->addWidget(buttonBox);

    connect(buttonBox, &QDialogButtonBox::accepted, this, &SettingsDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &SettingsDialog::reject);

    applyTheme();
}

void SettingsDialog::applyTheme()
{
    // 设置面板始终使用浅色模式
    const AppSettings::ThemePalette palette = AppSettings::getLightThemePalette();

    // Apply palette for title bar and overall appearance
    QPalette dlgPalette;
    dlgPalette.setColor(QPalette::Window, QColor(palette.background));
    dlgPalette.setColor(QPalette::WindowText, QColor(palette.text));
    setPalette(dlgPalette);

    setStyleSheet(QStringLiteral(
                     "QDialog { background-color: %1; color: %2; }"
                     "QGroupBox { border: 1px solid %3; border-radius: 8px; margin-top: 10px; padding-top: 8px; font-weight: 600; color: %2; }"
                     "QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 4px; }"
                     "QLabel { color: %2; }"
                     "QLabel#hintLabel { color: %4; }"
                     "QLineEdit, QComboBox, QSpinBox, QKeySequenceEdit { background-color: %5; color: %2; border: 1px solid %3; border-radius: 6px; min-height: 30px; padding: 2px 8px; }"
                     "QSpinBox::up-button { display: none; }"
                     "QSpinBox::down-button { display: none; }"
                     "QComboBox::drop-down { border: none; width: 24px; }"
                     "QPushButton { background-color: %6; color: %2; border: 1px solid %3; border-radius: 6px; min-height: 30px; padding: 0 12px; }"
                     "QPushButton:hover { background-color: %7; color: #FFFFFF; }"
                     "QPushButton:pressed { background-color: %8; color: #FFFFFF; }"
                     "QCheckBox { color: %2; min-height: 24px; }"
                     "QDialogButtonBox QPushButton { min-width: 92px; }")
                     .arg(palette.background,
                          palette.text,
                          palette.border,
                          palette.secondaryText,
                          palette.secondaryBackground,
                          palette.buttonBackground,
                          palette.buttonHover,
                          palette.buttonPressed));
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
    const QString historyLimitText = QString::number(historyLimit);
    int comboIndex = historyLimitCombo->findText(historyLimitText);
    if (comboIndex >= 0) {
        historyLimitCombo->setCurrentIndex(comboIndex);
    } else {
        historyLimitCombo->setCurrentIndex(0);
    }
    
    // Load dock strip settings.
    const int stripWidth = settings.value(AppSettings::kDockStripWidth, AppSettings::kDefaultDockStripWidth).toInt();
    const int stripHeight = settings.value(AppSettings::kDockStripHeight, AppSettings::kDefaultDockStripHeight).toInt();
    const int stripRadius = settings.value(AppSettings::kDockStripBorderRadius, AppSettings::kDefaultDockStripBorderRadius).toInt();

    dockStripWidthSpin->setValue(stripWidth);
    dockStripHeightSpin->setValue(stripHeight);
    dockStripRadiusSpin->setValue(stripRadius);

#ifdef Q_OS_WIN
    QSettings bootSettings(QString::fromLatin1(kAutoStartRegPath), QSettings::NativeFormat);
    autoStartCheck->setChecked(
        bootSettings.contains(QString::fromLatin1(kAutoStartValueName)) ||
        bootSettings.contains(QString::fromLatin1(kLegacyAutoStartValueName)));
#else
    autoStartCheck->setChecked(false);
#endif
}

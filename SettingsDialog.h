#pragma once

#include <QDialog>
#include <QKeySequence>

class QCheckBox;
class QComboBox;
class QKeySequenceEdit;
class QLineEdit;
class QPushButton;
class QSpinBox;

class SettingsDialog : public QDialog
{
    Q_OBJECT
public:
    explicit SettingsDialog(QWidget *parent = nullptr);

    QKeySequence captureHotkey() const;
    QString savePath() const;
    QString saveFormat() const;
    bool hideSidebar() const;
    int historyMaxItems() const;
    bool shouldClearHistory() const;
    bool autoStartEnabled() const;

protected:
    void accept() override;

private:
    void buildUi();
    void loadCurrentSettings();

    QKeySequenceEdit *hotkeyEdit = nullptr;
    QLineEdit *savePathEdit = nullptr;
    QComboBox *saveFormatCombo = nullptr;
    QCheckBox *hideSidebarCheck = nullptr;
    QSpinBox *historyLimitSpin = nullptr;
    QPushButton *clearHistoryButton = nullptr;
    QCheckBox *autoStartCheck = nullptr;

    bool clearHistoryRequested = false;
};

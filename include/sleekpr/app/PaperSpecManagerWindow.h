#pragma once

#include "sleekpr/core/templates/PaperSpec.h"

#include <QString>
#include <QWidget>

#include <functional>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;
class QSpinBox;

namespace sleekpr::app {

class PaperSpecManagerWindow final : public QWidget
{
public:
    using PaperSpecsChangedCallback = std::function<void()>;

    explicit PaperSpecManagerWindow(
        QString paperSpecFilePath,
        PaperSpecsChangedCallback onPaperSpecsChanged,
        QWidget* parent = nullptr);

    void reloadFromDisk();

private:
    void buildUi();
    void refreshList(const QString& selectedId = QString());
    void loadSelectedSpec();
    void startNewSpec();
    void saveCurrentSpec();
    void deleteCurrentSpec();
    void loadSpecToForm(const sleekpr::core::PaperSpec& spec);
    sleekpr::core::PaperSpec specFromForm() const;
    void setFormEnabled(bool enabled);
    QString currentSpecId() const;
    void showStatus(const QString& message);
    void notifySpecsChanged();

    QString m_paperSpecFilePath;
    PaperSpecsChangedCallback m_onPaperSpecsChanged;
    bool m_loadingForm = false;

    QListWidget* m_specList = nullptr;
    QLineEdit* m_idEdit = nullptr;
    QLineEdit* m_nameEdit = nullptr;
    QComboBox* m_kindCombo = nullptr;
    QComboBox* m_orientationCombo = nullptr;
    QDoubleSpinBox* m_widthSpin = nullptr;
    QDoubleSpinBox* m_heightSpin = nullptr;
    QDoubleSpinBox* m_marginLeftSpin = nullptr;
    QDoubleSpinBox* m_marginTopSpin = nullptr;
    QDoubleSpinBox* m_marginRightSpin = nullptr;
    QDoubleSpinBox* m_marginBottomSpin = nullptr;
    QDoubleSpinBox* m_dpiSpin = nullptr;
    QCheckBox* m_gridEnabledCheck = nullptr;
    QSpinBox* m_gridRowsSpin = nullptr;
    QSpinBox* m_gridColumnsSpin = nullptr;
    QDoubleSpinBox* m_gridHorizontalGapSpin = nullptr;
    QDoubleSpinBox* m_gridVerticalGapSpin = nullptr;
    QPushButton* m_saveButton = nullptr;
    QPushButton* m_deleteButton = nullptr;
    QLabel* m_statusLabel = nullptr;
};

} // 命名空间 sleekpr::app

#pragma once

#include "sleekpr/core/templates/FieldPreset.h"

#include <QString>
#include <QWidget>

#include <functional>
#include <optional>

class QLabel;
class QLineEdit;
class QListWidget;
class QPlainTextEdit;
class QPushButton;

namespace sleekpr::app {

class FieldPresetManagerWindow final : public QWidget
{
public:
    using FieldPresetsChangedCallback = std::function<void()>;

    explicit FieldPresetManagerWindow(
        QString fieldPresetFilePath,
        FieldPresetsChangedCallback onFieldPresetsChanged,
        QWidget* parent = nullptr);

    void reloadFromDisk();

private:
    void buildUi();
    void refreshList(const QString& selectedId = QString());
    void loadSelectedPreset();
    void startNewPreset();
    void saveCurrentPreset();
    void deleteCurrentPreset();
    void loadPresetToForm(const sleekpr::core::FieldPreset& preset);
    std::optional<sleekpr::core::FieldPreset> presetFromForm(QString* errorMessage) const;
    void setFormEnabled(bool enabled);
    QString currentPresetId() const;
    void showStatus(const QString& message);
    void notifyPresetsChanged();

    QString m_fieldPresetFilePath;
    FieldPresetsChangedCallback m_onFieldPresetsChanged;
    bool m_loadingForm = false;

    QListWidget* m_presetList = nullptr;
    QLineEdit* m_idEdit = nullptr;
    QLineEdit* m_nameEdit = nullptr;
    QLineEdit* m_templateIdEdit = nullptr;
    QPlainTextEdit* m_valuesEdit = nullptr;
    QPushButton* m_saveButton = nullptr;
    QPushButton* m_deleteButton = nullptr;
    QLabel* m_statusLabel = nullptr;
};

} // 命名空间 sleekpr::app

#pragma once

#include "sleekpr/core/settings/FileSettingsStore.h"
#include "sleekpr/core/settings/PrintClientSettings.h"
#include "sleekpr/core/settings/TemplateElementCatalog.h"
#include "sleekpr/core/labels/LabelTemplateKey.h"

#include <QHash>
#include <QImage>
#include <QPoint>
#include <QRect>
#include <QSize>
#include <QSizeF>
#include <QString>
#include <QWidget>
#include <Qt>

#include <functional>
#include <optional>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;

namespace sleekpr::app {
class TemplatePreviewLabel;
}

namespace sleekpr::app {

class SettingsWindow final : public QWidget
{
public:
    using SettingsAppliedCallback = std::function<void(const sleekpr::core::PrintClientSettings&)>;

    explicit SettingsWindow(QString settingsPath, SettingsAppliedCallback onSettingsApplied, QWidget* parent = nullptr);

    // 每次从托盘打开设置窗口时重新读取本地文件，确保界面展示最新配置。
    void reloadFromDisk();

private:
    void buildUi();
    QWidget* createPrinterPanel();
    QWidget* createPreviewPanel();
    QWidget* createElementPanel();
    void populatePrinters();
    void populateElements();
    void selectFirstElement();
    void storeCurrentElementForm();
    void loadCurrentElementForm();
    void collectGeneralSettings();
    sleekpr::core::LabelTemplateKey currentLabelTemplateKey() const;
    QString currentTemplateOverrideKey() const;
    void applyWithoutSaving();
    void saveSettings();
    void resetCurrentElement();
    void resetAllElements();
    void addTemplateElement(sleekpr::core::TemplateElementType type);
    void deleteCurrentCustomElement();
    void refreshPreviewOnly();
    void applyPreviewDragDelta(const QPoint& pixelDelta);
    void applyPreviewKeyboardNudge(const QPoint& direction, Qt::KeyboardModifiers modifiers);
    void updatePreviewDraggingState();
    QSizeF currentElementSizeMm() const;
    QRect currentElementRectPx() const;
    bool canStartPreviewDragAt(const QPoint& position);
    std::optional<QString> elementKeyAtPreviewPosition(const QPoint& position) const;
    bool selectElementByKey(const QString& elementKey);
    QPointF snappedElementTopLeftMm(QPointF topLeftMm, QSizeF elementSizeMm) const;
    void drawSelectionOverlay(QImage& image) const;
    void renderPreview(const sleekpr::core::PrintClientSettings& settings);
    QImage createPreviewImage(const sleekpr::core::PrintClientSettings& settings) const;
    bool isCurrentCustomElement() const;
    sleekpr::core::TemplateElement* mutableCustomElement(const QString& elementKey);
    const sleekpr::core::TemplateElement* customElement(const QString& elementKey) const;
    void storeCustomElementForm(sleekpr::core::TemplateElement& element) const;
    void loadCustomElementForm(const sleekpr::core::TemplateElement& element);
    void updateCustomElementControlState();
    sleekpr::core::TemplateElementOverride formOverrideForCurrentElement() const;
    sleekpr::core::TemplateElementOverride displayedOverrideForElement(const sleekpr::core::TemplateElementDefinition& element) const;
    bool formMatchesDefault(const sleekpr::core::TemplateElementOverride& formValue, const sleekpr::core::TemplateElementDefinition& element) const;

    sleekpr::core::FileSettingsStore m_settingsStore;
    SettingsAppliedCallback m_onSettingsApplied;
    sleekpr::core::PrintClientSettings m_settings;
    QList<sleekpr::core::TemplateElementDefinition> m_elements;
    QString m_currentElementKey;
    QString m_currentTemplateOverrideKey = QStringLiteral("default");
    bool m_loadingForm = false;

    QComboBox* m_templateCombo = nullptr;
    QComboBox* m_printerCombo = nullptr;
    QDoubleSpinBox* m_offsetXSpin = nullptr;
    QDoubleSpinBox* m_offsetYSpin = nullptr;
    TemplatePreviewLabel* m_previewLabel = nullptr;
    QLabel* m_statusLabel = nullptr;
    QCheckBox* m_snapToGridCheck = nullptr;
    QSize m_previewImageSize;
    QListWidget* m_elementList = nullptr;
    QDoubleSpinBox* m_elementXSpin = nullptr;
    QDoubleSpinBox* m_elementYSpin = nullptr;
    QDoubleSpinBox* m_elementWidthSpin = nullptr;
    QDoubleSpinBox* m_elementHeightSpin = nullptr;
    QComboBox* m_customElementTypeCombo = nullptr;
    QLineEdit* m_customElementTextEdit = nullptr;
    QComboBox* m_customElementFieldCombo = nullptr;
    QDoubleSpinBox* m_fontSizeSpin = nullptr;
    QCheckBox* m_boldCheck = nullptr;
    QPushButton* m_deleteCustomElementButton = nullptr;
};

} // 命名空间 sleekpr::app

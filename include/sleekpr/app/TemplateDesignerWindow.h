#pragma once

#include "sleekpr/core/settings/PrintClientSettings.h"
#include "sleekpr/core/native/NativeDrawCommand.h"

#include <QList>
#include <QPoint>
#include <QString>
#include <Qt>
#include <QWidget>

#include <functional>

class QLabel;
class QDoubleSpinBox;
class QLineEdit;
class QListWidget;
class QPushButton;

namespace sleekpr::core {
struct TemplateElement;
struct TemplateLayer;
}

namespace sleekpr::app {

class TemplatePreviewLabel;

class TemplateDesignerWindow final : public QWidget
{
public:
    using SettingsChangedCallback = std::function<void(const sleekpr::core::PrintClientSettings&)>;

    explicit TemplateDesignerWindow(
        sleekpr::core::PrintClientSettings settings,
        SettingsChangedCallback onSettingsChanged,
        QString templateLibraryDirectoryPath = QString(),
        QWidget* parent = nullptr);

    bool exportTemplateToFile(const QString& path) const;
    bool importTemplateFromFile(const QString& path);

private:
    void buildUi();
    void ensureCurrentTemplateDocument();
    void addLayer();
    void deleteCurrentLayer();
    void toggleCurrentLayerLocked();
    void toggleCurrentLayerVisible();
    void moveCurrentLayerUp();
    void moveCurrentLayerDown();
    void addFixedTextElement();
    void addBoundFieldElement();
    void addQrCodeElement();
    void addRectangleElement();
    void deleteCurrentElement();
    void toggleCurrentElementLocked();
    void toggleCurrentElementVisible();
    void moveCurrentElementUp();
    void moveCurrentElementDown();
    void saveTemplateVersion();
    void restoreActiveTemplateVersion();
    void saveCurrentTemplateToLibrary();
    void loadCurrentTemplateFromLibrary();
    void importTemplateWithDialog();
    void exportTemplateWithDialog();
    void saveDeviceProfile();
    void refreshLayerList();
    void refreshElementList();
    void refreshPreview();
    void refreshAll();
    void notifySettingsChanged();
    bool selectLayer(const QString& layerId);
    bool selectElement(const QString& elementId);
    bool selectElementInCurrentList(const QString& elementId);
    bool canEditElement(const QString& elementId) const;
    bool loadTemplateDocumentFromFile(const QString& path, sleekpr::core::TemplateDocument* document, QString* errorMessage) const;
    void applyImportedTemplateDocument(const sleekpr::core::TemplateDocument& document);
    QString currentLayerId() const;
    QString currentElementId() const;
    sleekpr::core::TemplateLayer* currentLayer();
    const sleekpr::core::TemplateLayer* currentLayer() const;
    sleekpr::core::TemplateElement* currentElement();
    void addElement(sleekpr::core::TemplateElement element);
    void moveSelectedElementByPixels(QPoint delta);
    void nudgeSelectedElement(QPoint direction, Qt::KeyboardModifiers modifiers);

    sleekpr::core::PrintClientSettings m_settings;
    SettingsChangedCallback m_onSettingsChanged;
    QString m_templateLibraryDirectoryPath;
    QString m_templateKey = QStringLiteral("default");
    QListWidget* m_layerList = nullptr;
    QListWidget* m_elementList = nullptr;
    TemplatePreviewLabel* m_previewLabel = nullptr;
    QLabel* m_statusLabel = nullptr;
    QLineEdit* m_deviceProfilePrinterEdit = nullptr;
    QDoubleSpinBox* m_deviceProfileDpiSpin = nullptr;
    QDoubleSpinBox* m_deviceProfileScaleXSpin = nullptr;
    QDoubleSpinBox* m_deviceProfileScaleYSpin = nullptr;
    QDoubleSpinBox* m_deviceProfileOffsetXSpin = nullptr;
    QDoubleSpinBox* m_deviceProfileOffsetYSpin = nullptr;
    QList<sleekpr::core::NativeDrawCommand> m_previewCommands;
};

} // 命名空间 sleekpr::app

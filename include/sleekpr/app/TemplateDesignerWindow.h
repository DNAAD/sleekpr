#pragma once

#include "sleekpr/core/settings/PrintClientSettings.h"
#include "sleekpr/core/native/NativeDrawCommand.h"

#include <QComboBox>
#include <QList>
#include <QPoint>
#include <QSizeF>
#include <QString>
#include <Qt>
#include <QWidget>

#include <functional>

class QLabel;
class QCheckBox;
class QDoubleSpinBox;
class QLineEdit;
class QListWidget;
class QPushButton;

namespace sleekpr::core {
struct TemplateElement;
struct TemplateLayer;
struct TableElement;
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
    ~TemplateDesignerWindow() override
    {
        if (m_paperSpecCombo != nullptr) {
            m_paperSpecCombo->disconnect();
            delete m_paperSpecCombo;
            m_paperSpecCombo = nullptr;
        }
    }

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
    void addTableElement();
    void deleteCurrentElement();
    void toggleCurrentElementLocked();
    void toggleCurrentElementVisible();
    void moveCurrentElementUp();
    void moveCurrentElementDown();
    void saveTemplateVersion();
    void restoreActiveTemplateVersion();
    void saveCurrentTemplateToLibrary();
    void loadCurrentTemplateFromLibrary();
    void createTemplateInLibrary();
    void deleteSelectedTemplateFromLibrary();
    void loadSelectedTemplateFromLibrary();
    void loadTemplateFromLibraryById(const QString& templateId);
    void importTemplateWithDialog();
    void exportTemplateWithDialog();
    void saveDeviceProfile();
    void refreshTemplateLibraryList();
    void refreshPaperSpecSelector();
    void refreshLayerList();
    void refreshElementList();
    void refreshTablePropertyEditor();
    void refreshPreview();
    void refreshAll();
    void applySelectedPaperSpec();
    void notifySettingsChanged();
    bool selectLayer(const QString& layerId);
    bool selectElement(const QString& elementId);
    bool selectElementInCurrentList(const QString& elementId);
    bool canEditElement(const QString& elementId) const;
    bool currentSelectionIsTable() const;
    bool loadTemplateDocumentFromFile(const QString& path, sleekpr::core::TemplateDocument* document, QString* errorMessage) const;
    void applyImportedTemplateDocument(const sleekpr::core::TemplateDocument& document);
    QString paperSpecFilePath() const;
    QSizeF currentPaperSizeMm() const;
    QString currentLayerId() const;
    QString currentElementId() const;
    sleekpr::core::TemplateLayer* currentLayer();
    const sleekpr::core::TemplateLayer* currentLayer() const;
    sleekpr::core::TemplateElement* currentElement();
    sleekpr::core::TableElement* currentTable();
    void addElement(sleekpr::core::TemplateElement element);
    void addTable(sleekpr::core::TableElement table);
    void applyCurrentTableProperties();
    void moveSelectedElementByPixels(QPoint delta);
    void nudgeSelectedElement(QPoint direction, Qt::KeyboardModifiers modifiers);

    sleekpr::core::PrintClientSettings m_settings;
    SettingsChangedCallback m_onSettingsChanged;
    QString m_templateLibraryDirectoryPath;
    QString m_templateKey = QStringLiteral("default");
    QComboBox* m_paperSpecCombo = nullptr;
    QLineEdit* m_templateLibraryNameEdit = nullptr;
    QListWidget* m_templateLibraryList = nullptr;
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
    QLineEdit* m_tableDisplayNameEdit = nullptr;
    QLineEdit* m_tableDataPathEdit = nullptr;
    QDoubleSpinBox* m_tableXSpin = nullptr;
    QDoubleSpinBox* m_tableYSpin = nullptr;
    QDoubleSpinBox* m_tableWidthSpin = nullptr;
    QDoubleSpinBox* m_tableHeightSpin = nullptr;
    QDoubleSpinBox* m_tableHeaderHeightSpin = nullptr;
    QDoubleSpinBox* m_tableDetailHeightSpin = nullptr;
    QCheckBox* m_tableRepeatHeaderCheck = nullptr;
    QCheckBox* m_tableDrawBordersCheck = nullptr;
    QLineEdit* m_tableColumnsEdit = nullptr;
    QList<sleekpr::core::NativeDrawCommand> m_previewCommands;
};

} // 命名空间 sleekpr::app

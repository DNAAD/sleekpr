#pragma once

#include "sleekpr/app/TemplateDesignerPresenter.h"
#include "sleekpr/app/TemplateDesignerState.h"
#include "sleekpr/app/TemplateTableCanvasEditor.h"
#include "sleekpr/core/settings/PrintClientSettings.h"
#include "sleekpr/core/native/NativeDrawCommand.h"
#include "sleekpr/core/native/NativePrintDrawingPlan.h"
#include "sleekpr/core/templates/PaperSpec.h"
#include "sleekpr/core/templates/TemplateRenderContext.h"

#include <QComboBox>
#include <QJsonObject>
#include <QList>
#include <QPoint>
#include <QSizeF>
#include <QString>
#include <Qt>
#include <QWidget>

#include <functional>
#include <optional>

class QLabel;
class QCheckBox;
class QDoubleSpinBox;
class QEvent;
class QLineEdit;
class QListWidget;
class QListWidgetItem;
class QObject;
class QPlainTextEdit;
class QPushButton;
class QTimer;

namespace sleekpr::core {
struct TemplateElement;
struct TemplateLayer;
struct TableElement;
}

namespace sleekpr::app {

class TemplatePreviewLabel;
class TemplateInspectorPanel;

class TemplateDesignerWindow final : public QWidget
{
public:
    using SettingsChangedCallback = std::function<void(const sleekpr::core::PrintClientSettings&)>;
    using OpenPaperSpecManagerCallback = std::function<void()>;
    using OpenFieldPresetManagerCallback = std::function<void()>;
    using PrePrintCallback = std::function<bool(const sleekpr::core::NativePrintDrawingPlan&, const QString&)>;

    explicit TemplateDesignerWindow(
        sleekpr::core::PrintClientSettings settings,
        SettingsChangedCallback onSettingsChanged,
        QString templateLibraryDirectoryPath = QString(),
        QWidget* parent = nullptr);
    TemplateDesignerWindow(
        sleekpr::core::PrintClientSettings settings,
        SettingsChangedCallback onSettingsChanged,
        QString templateLibraryDirectoryPath,
        PrePrintCallback prePrintCallback,
        QWidget* parent = nullptr);
    ~TemplateDesignerWindow() override = default;

    bool exportTemplateToFile(const QString& path) const;
    bool importTemplateFromFile(const QString& path);
    void setOpenPaperSpecManagerCallback(OpenPaperSpecManagerCallback callback);
    void setOpenFieldPresetManagerCallback(OpenFieldPresetManagerCallback callback);
    void reloadPaperSpecsFromDisk();

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    struct TableColumnResizeDrag
    {
        QString tableId;
        int leftColumnIndex = -1;
    };

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
    void addArrayGridElement();
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
    void duplicateSelectedTemplateFromLibrary();
    void renameSelectedTemplateFromLibrary();
    void loadTemplateFromLibraryById(const QString& templateId);
    void importTemplateWithDialog();
    void exportTemplateWithDialog();
    void prePrintCurrentTemplate();
    void saveSampleDataFromEditor();
    void undoCurrentTemplateChange();
    void redoCurrentTemplateChange();
    void calculateDeviceCalibrationScale();
    void saveDeviceProfile();
    void refreshTemplateLibraryList();
    void refreshPaperSpecSelector();
    void refreshLayerList();
    void refreshElementList();
    void refreshElementPropertyEditor();
    void refreshTablePropertyEditor();
    void refreshPreview();
    void schedulePreviewRefresh(int delayMs);
    void scheduleSelectionPropertyRefresh(int delayMs);
    void scheduleSettingsChanged(int delayMs);
    void refreshSampleDataEditor();
    QJsonObject sampleDataFromEditor(bool* ok = nullptr, QString* errorMessage = nullptr) const;
    bool syncSampleDataFromEditor(bool* ok = nullptr, QString* errorMessage = nullptr);
    sleekpr::core::TemplateRenderContext sampleRenderContext(bool* ok = nullptr, QString* errorMessage = nullptr) const;
    void refreshAll();
    void applySelectedPaperSpec();
    void notifySettingsChanged();
    bool selectLayer(const QString& layerId);
    bool selectElement(const QString& elementId);
    bool selectElementInCurrentList(const QString& elementId);
    bool selectTemplateInLibraryList(const QString& templateId);
    bool canEditElement(const QString& elementId) const;
    bool currentSelectionIsTable() const;
    void renameElementFromListItem(QListWidgetItem* item);
    void beginRenameCurrentElement();
    void applyElementListOrderFromList();
    bool loadTemplateDocumentFromFile(const QString& path, sleekpr::core::TemplateDocument* document, QString* errorMessage) const;
    void applyImportedTemplateDocument(const sleekpr::core::TemplateDocument& document);
    QString paperSpecFilePath() const;
    std::optional<sleekpr::core::PaperSpec> currentPaperSpec() const;
    QSizeF currentPaperSizeMm() const;
    sleekpr::core::NativePrintDrawingPlan createCurrentPrintPlan(
        const sleekpr::core::TemplateRenderContext& renderContext,
        const QString& printerName) const;
    QString currentLayerId() const;
    QString currentElementId() const;
    sleekpr::core::TemplateLayer* currentLayer();
    const sleekpr::core::TemplateLayer* currentLayer() const;
    sleekpr::core::TemplateElement* currentElement();
    sleekpr::core::TableElement* currentTable();
    void addElement(sleekpr::core::TemplateElement element);
    void addTable(sleekpr::core::TableElement table);
    bool applyCurrentElementProperties(bool lightweightRefresh = false);
    bool applyCurrentTableProperties(bool lightweightRefresh = false);
    void scheduleElementAutoApply(int delayMs);
    void scheduleTableAutoApply(int delayMs);
    void applyPendingElementAutoApply();
    void applyPendingTableAutoApply();
    bool isElementAutoApplyEditor(QObject* watched) const;
    bool isTableAutoApplyEditor(QObject* watched) const;
    void updateSampleDataErrorLabel(const QString& errorMessage);
    void refreshInspectorTabForCurrentSelection();
    void resetDocumentHistory();
    void rememberCurrentDocumentHistory();
    void updateHistoryButtons();
    void moveSelectedElementByPixels(QPoint delta);
    void resizeTableColumnByPixels(QPoint delta);
    std::optional<TableCanvasHit> tableCanvasHitAt(QPoint position) const;
    void editTableCanvasCellAt(QPoint position);
    void showTableCanvasContextMenu(QPoint position, QPoint globalPosition);
    bool editTableCanvasColumnText(const TableCanvasHit& hit);
    bool applyTableCanvasMutation(
        const TableCanvasHit& hit,
        const std::function<bool(sleekpr::core::TableElement*)>& mutation,
        const QString& statusText);
    void nudgeSelectedElement(QPoint direction, Qt::KeyboardModifiers modifiers);
    std::optional<TableColumnResizeDrag> tableColumnResizeDragAt(QPoint position) const;
    bool currentSelectionContainsCanvasPosition(QPoint position) const;

    sleekpr::core::PrintClientSettings m_settings;
    SettingsChangedCallback m_onSettingsChanged;
    OpenPaperSpecManagerCallback m_openPaperSpecManager;
    OpenFieldPresetManagerCallback m_openFieldPresetManager;
    PrePrintCallback m_prePrintCallback;
    QString m_templateLibraryDirectoryPath;
    QString m_templateKey = QStringLiteral("default");
    QComboBox* m_paperSpecCombo = nullptr;
    QComboBox* m_rulerPrecisionCombo = nullptr;
    QComboBox* m_zoomCombo = nullptr;
    QCheckBox* m_designAidCheck = nullptr;
    QPushButton* m_undoButton = nullptr;
    QPushButton* m_redoButton = nullptr;
    QLineEdit* m_templateLibrarySearchEdit = nullptr;
    QLineEdit* m_templateLibraryNameEdit = nullptr;
    QListWidget* m_templateLibraryList = nullptr;
    QListWidget* m_layerList = nullptr;
    QListWidget* m_elementList = nullptr;
    TemplateInspectorPanel* m_inspectorPanel = nullptr;
    TemplatePreviewLabel* m_previewLabel = nullptr;
    QLabel* m_statusLabel = nullptr;
    QLabel* m_sampleDataErrorLabel = nullptr;
    QPlainTextEdit* m_sampleDataEdit = nullptr;
    QLineEdit* m_deviceProfilePrinterEdit = nullptr;
    QDoubleSpinBox* m_deviceProfileDpiSpin = nullptr;
    QDoubleSpinBox* m_deviceProfileScaleXSpin = nullptr;
    QDoubleSpinBox* m_deviceProfileScaleYSpin = nullptr;
    QDoubleSpinBox* m_deviceProfileOffsetXSpin = nullptr;
    QDoubleSpinBox* m_deviceProfileOffsetYSpin = nullptr;
    QDoubleSpinBox* m_deviceCalibrationExpectedWidthSpin = nullptr;
    QDoubleSpinBox* m_deviceCalibrationActualWidthSpin = nullptr;
    QDoubleSpinBox* m_deviceCalibrationExpectedHeightSpin = nullptr;
    QDoubleSpinBox* m_deviceCalibrationActualHeightSpin = nullptr;
    QList<sleekpr::core::NativeDrawCommand> m_previewCommands;
    TemplateDesignerPresenter m_presenter;
    TemplateDesignerState m_state;
    QTimer* m_elementAutoApplyTimer = nullptr;
    QTimer* m_tableAutoApplyTimer = nullptr;
    QTimer* m_previewRefreshTimer = nullptr;
    QTimer* m_propertyRefreshTimer = nullptr;
    QTimer* m_settingsChangedTimer = nullptr;
    double m_previewZoomFactor = 1.0;
    bool m_updatingPropertyEditors = false;
    bool m_applyingElementListOrder = false;
    std::optional<TableColumnResizeDrag> m_tableColumnResizeDrag;
};

} // 命名空间 sleekpr::app

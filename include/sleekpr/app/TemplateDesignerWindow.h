#pragma once

#include "sleekpr/app/TemplateDesignerPresenter.h"
#include "sleekpr/app/TemplateDesignerState.h"
#include "sleekpr/app/TemplateTableCanvasEditor.h"
#include "sleekpr/app/TemplateTablePaginationOverlay.h"
#include "sleekpr/core/settings/PrintClientSettings.h"
#include "sleekpr/core/settings/SettingsChangeSet.h"
#include "sleekpr/core/settings/SettingsPersistenceResult.h"
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
#include <QStringList>
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
class QShowEvent;
class QResizeEvent;
class QCloseEvent;
class QHideEvent;

namespace sleekpr::core {
struct TemplateElement;
struct TemplateLayer;
struct TableElement;
}

namespace sleekpr::infrastructure { struct PreviewWorkResult; }

namespace sleekpr::app {

class TemplateCanvasEditor;
class LatestPreviewQueue;
class SettingsPersistenceNotice;

class TemplatePreviewLabel;
class TemplateInspectorPanel;
class TemplateWorkspacePanel;

class TemplateDesignerWindow final : public QWidget
{
public:
    using SettingsChangedCallback = std::function<void(const sleekpr::core::PrintClientSettings&)>;
    using SettingsSaveHandler = std::function<core::SettingsWriteResult(const core::SettingsChangeSet&)>;
    using SettingsReloadHandler = std::function<core::SettingsReadResult()>;
    using SettingsRestoreHandler = std::function<core::SettingsWriteResult(const QByteArray&)>;
    void setSettingsSaveHandler(SettingsSaveHandler handler);
    void setSettingsReloadHandler(SettingsReloadHandler handler);
    void setPersistenceSnapshot(const core::SettingsReadResult& snapshot);
    void setSettingsRestoreHandler(SettingsRestoreHandler handler);
    bool hasUnsavedPersistenceChanges() const;
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
    ~TemplateDesignerWindow() override;

    bool exportTemplateToFile(const QString& path) const;
    bool importTemplateFromFile(const QString& path);
    void setOpenPaperSpecManagerCallback(OpenPaperSpecManagerCallback callback);
    void setOpenFieldPresetManagerCallback(OpenFieldPresetManagerCallback callback);
    void reloadPaperSpecsFromDisk();

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
    void showEvent(QShowEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void closeEvent(QCloseEvent* event) override;
    void hideEvent(QHideEvent* event) override;

private:
    struct TableColumnResizeDrag
    {
        QString tableId;
        int leftColumnIndex = -1;
    };

    void buildUi();
    void setupPersistenceNotice();
    bool persistDraft();
    void reloadPersistedSettings();
    void applyPersistenceReload(const core::SettingsReadResult& snapshot);
    void showPersistenceFailure(const core::SettingsWriteResult& result);
    void setupCanvasEditor();
    void applyCanvasSelection(const QStringList& ids);
    void syncCanvasSelectionFromList();
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
    void refreshPreview(bool reusePlan = false);
    void setupPreviewQueue();
    void publishPreview(const infrastructure::PreviewWorkResult& result);
    void fitPreviewToCanvas();
    void scheduleCanvasFit();
    void flushPendingPropertyEdits();
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
    bool elementIdIsTable(const QString& elementId) const;
    bool canvasElementOverridesTable(const QString& elementId, const QString& tableId) const;
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
    void updateTableCanvasSelectionAt(QPoint position);
    void refreshTableCanvasFloatingActions();
    void handleTableCanvasFloatingAction(const QString& actionId);
    void handleCanvasClick(QPoint position, Qt::KeyboardModifiers modifiers);
    bool handleTableCanvasShortcut(int key, Qt::KeyboardModifiers modifiers);
    std::optional<QString> canvasElementHitAt(QPoint position) const;
    std::optional<TableCanvasHit> tableCanvasHitAt(QPoint position) const;
    std::optional<TableCanvasSelection> tableCanvasSelectionFromHits(const TableCanvasHit& anchor, const TableCanvasHit& target) const;
    bool selectTableCanvasCell(const TableCanvasHit& hit);
    bool selectCurrentTableCanvasRows();
    bool selectCurrentTableCanvasColumns();
    bool mergeCurrentTableCanvasSelection();
    bool splitCurrentTableCanvasSelection();
    bool setTableCanvasSelection(const TableCanvasSelection& selection, const QString& statusText);
    std::optional<TableCanvasSelection> expandedTableCanvasSelection(bool expandColumns, bool expandRows) const;
    void editTableCanvasCellAt(QPoint position);
    void showTableCanvasContextMenu(QPoint position, QPoint globalPosition);
    bool editTableCanvasColumnText(const TableCanvasHit& hit);
    void focusTablePaginationPreview(int pageNumber, int firstRowIndex);
    bool applyTableCanvasMutation(
        const TableCanvasHit& hit,
        const std::function<bool(sleekpr::core::TableElement*)>& mutation,
        const QString& statusText);
    bool applyTableCanvasSelectionMutation(
        const TableCanvasSelection& selection,
        const std::function<bool(sleekpr::core::TableElement*)>& mutation,
        const QString& statusText);
    void nudgeSelectedElement(QPoint direction, Qt::KeyboardModifiers modifiers);
    std::optional<TableColumnResizeDrag> tableColumnResizeDragAt(QPoint position) const;
    bool currentSelectionContainsCanvasPosition(QPoint position) const;

    sleekpr::core::PrintClientSettings m_settings;
    SettingsChangedCallback m_onSettingsChanged;
    SettingsSaveHandler m_settingsSaveHandler;
    SettingsReloadHandler m_settingsReloadHandler;
    SettingsRestoreHandler m_settingsRestoreHandler;
    core::SettingsReadResult m_persistenceBaseline;
    std::optional<core::SettingsWriteResult> m_persistenceConflict;
    SettingsPersistenceNotice* m_persistenceNotice = nullptr;
    bool m_persistenceDirty = false;
    LatestPreviewQueue* m_previewQueue = nullptr;
    std::optional<core::NativePrintDrawingPlan> m_cachedPreviewPlan;
    bool m_previewDirty = true;
    bool m_previewPlanDirty = true;
    static constexpr double kDesignerInteractivePreviewDpi = 152.4;
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
    TemplateWorkspacePanel* m_workspacePanel = nullptr;
    TemplatePreviewLabel* m_previewLabel = nullptr;
    TemplateCanvasEditor* m_canvasEditor = nullptr;
    bool m_syncingCanvasSelection = false;
    QLabel* m_statusLabel = nullptr;
    QLabel* m_sampleDataErrorLabel = nullptr;
    QPlainTextEdit* m_sampleDataEdit = nullptr;
    bool m_sampleDataDraftDirty = false;
    QString m_sampleDataDraftTemplateKey;
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
    QList<TablePaginationOverlay> m_tablePaginationOverlays;
    TemplateDesignerPresenter m_presenter;
    TemplateDesignerState m_state;
    QTimer* m_elementAutoApplyTimer = nullptr;
    QTimer* m_tableAutoApplyTimer = nullptr;
    QTimer* m_previewRefreshTimer = nullptr;
    QTimer* m_propertyRefreshTimer = nullptr;
    QTimer* m_settingsChangedTimer = nullptr;
    double m_previewZoomFactor = 1.0;
    double m_previewBaseDpi = 0.0;
    bool m_fitToCanvas = true;
    bool m_updatingCanvasZoom = false;
    bool m_canvasFitQueued = false;
    int m_fitZoomIndex = -1;
    bool m_updatingPropertyEditors = false;
    bool m_applyingElementListOrder = false;
    std::optional<TableColumnResizeDrag> m_tableColumnResizeDrag;
    std::optional<TableCanvasHit> m_tableCanvasSelectionAnchor;
    std::optional<TableCanvasSelection> m_tableCanvasSelection;
    QPoint m_tableCanvasSelectionDragPosition;
};

} // 命名空间 sleekpr::app

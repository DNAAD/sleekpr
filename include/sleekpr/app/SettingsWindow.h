#pragma once

#include "sleekpr/app/SettingsPersistenceService.h"
#include "sleekpr/app/ApplicationExitParticipant.h"
#include <QPointer>
#include <QMap>
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
class QEvent;
class QLabel;
class QLineEdit;
class QListWidget;
class QPlainTextEdit;
class QPushButton;
class QScrollArea;
class QSpinBox;
class QStackedWidget;
class QShowEvent;
class QHideEvent;

namespace sleekpr::app {
class TemplatePreviewLabel;
class SettingsPersistenceNotice;
class LatestPreviewQueue;
}

namespace sleekpr::app {

class SettingsWindow final : public QWidget
{
public:
    using SettingsAppliedCallback = std::function<void(const sleekpr::core::PrintClientSettings&)>;
    using OpenTemplateDesignerCallback = std::function<void()>;
    using OpenPrintWorkbenchCallback = std::function<void()>;
    using OpenPaperSpecManagerCallback = std::function<void()>;
    using OpenFieldPresetManagerCallback = std::function<void()>;

    explicit SettingsWindow(QString settingsPath, SettingsPersistenceService& persistence, SettingsAppliedCallback onSettingsApplied, QWidget* parent = nullptr);
    SettingsWindow(
        QString settingsPath,
        SettingsPersistenceService& persistence,
        SettingsAppliedCallback onSettingsApplied,
        QString templateLibraryDirectoryPath,
        QWidget* parent = nullptr);
    ~SettingsWindow() override;

    // 每次从托盘打开设置窗口时重新读取本地文件，确保界面展示最新配置。
    void reloadFromDisk();
    void requestExternalRefresh(bool includeDevices = false);
    bool hasUnsavedPersistenceChanges() const;
    void flushPersistenceEdits();
    void saveForExit();
    void setExitPaused(bool);
    ExitDraftState exitDraftState() const;
    void setOpenTemplateDesignerCallback(OpenTemplateDesignerCallback callback);
    void setOpenPrintWorkbenchCallback(OpenPrintWorkbenchCallback callback);
    void setOpenPaperSpecManagerCallback(OpenPaperSpecManagerCallback callback);
    void setOpenFieldPresetManagerCallback(OpenFieldPresetManagerCallback callback);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;

private:
    void buildUi();
    QWidget* createWorkbenchNavigation();
    QWidget* createOverviewPanel();
    QWidget* createPrinterPanel();
    QWidget* createPreviewPanel();
    QWidget* createElementPanel();
    void fitPreviewToViewport();
    void refreshOverview();
    void populateTemplateCombo(const QString& preferredTemplateKey = QString());
    void populatePrinters();
    void populateElements();
    void selectFirstElement();
    void storeCurrentElementForm();
    void loadCurrentElementForm();
    void collectGeneralSettings();
    core::PrintClientSettings readGeneralSettingsForm() const;
    void confirmPersistenceReload();
    void reloadSettings(bool refreshDevices);
    void applyPersistenceSnapshot(const core::SettingsReadResult&, bool refreshDevices);
    void setupPersistence();
    void refreshPersistenceState();
    bool hasLocalSettingsDraft() const;
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
    void setupPreviewQueue();
    bool isCurrentCustomElement() const;
    sleekpr::core::TemplateElement* mutableCustomElement(const QString& elementKey);
    const sleekpr::core::TemplateElement* customElement(const QString& elementKey) const;
    void storeCustomElementForm(sleekpr::core::TemplateElement& element) const;
    void loadCustomElementForm(const sleekpr::core::TemplateElement& element);
    void updateCustomElementControlState();
    sleekpr::core::TemplateElementOverride formOverrideForCurrentElement() const;
    sleekpr::core::TemplateElementOverride displayedOverrideForElement(const sleekpr::core::TemplateElementDefinition& element) const;
    bool formMatchesDefault(const sleekpr::core::TemplateElementOverride& formValue, const sleekpr::core::TemplateElementDefinition& element) const;

    QString m_settingsPath;
    QPointer<SettingsPersistenceService> m_persistenceService;
    SettingsSessionId m_persistenceSession = 0;
    quint64 m_persistenceOutcomeSerial = 0;
    quint64 m_runtimeApplySequence = 0;
    quint64 m_modelEditRevision = 0, m_requestedModelRevision = 0;
    bool m_persistenceInitialized = false, m_exitPaused = false, m_enabledBeforeExit = true;
    bool m_persistenceLoading = false, m_persistenceWasEnabled = true;
    bool m_pendingDeviceRefresh = true;
    core::PrintClientSettings m_expectedSettings;
    std::optional<core::PrintClientSettings> m_lastRequestedForm;
    struct SaveContext { core::PrintClientSettings form; quint64 runtimeSequence; quint64 modelRevision; };
    QMap<quint64, SaveContext> m_saveContexts;
    QLabel* m_persistenceStateLabel = nullptr;
    LatestPreviewQueue* m_previewQueue = nullptr;
    bool m_previewNeedsRender = true;
    core::PrintClientSettings m_formBaseline;
    SettingsPersistenceNotice* m_persistenceNotice = nullptr;
    SettingsAppliedCallback m_onSettingsApplied;
    OpenTemplateDesignerCallback m_openTemplateDesigner;
    OpenPrintWorkbenchCallback m_openPrintWorkbench;
    OpenPaperSpecManagerCallback m_openPaperSpecManager;
    OpenFieldPresetManagerCallback m_openFieldPresetManager;
    sleekpr::core::PrintClientSettings m_settings;
    QList<sleekpr::core::TemplateElementDefinition> m_elements;
    QString m_currentElementKey;
    QString m_currentTemplateOverrideKey = QStringLiteral("default");
    QString m_templateLibraryDirectoryPath;
    QHash<QString, sleekpr::core::TemplateDocument> m_libraryTemplateDocuments;
    bool m_loadingForm = false;
    bool m_externalRefreshPending = false;
    bool m_externalDevicesPending = false;

    QStackedWidget* m_settingsPages = nullptr;
    QPushButton* m_printNavButton = nullptr;
    QPushButton* m_securityNavButton = nullptr;
    QComboBox* m_templateCombo = nullptr;
    QComboBox* m_printerCombo = nullptr;
    QPlainTextEdit* m_allowedOriginsEdit = nullptr;
    QSpinBox* m_httpMaxHeaderKbSpin = nullptr;
    QSpinBox* m_httpMaxBodyMbSpin = nullptr;
    QSpinBox* m_httpMaxPreviewBatchItemsSpin = nullptr;
    QSpinBox* m_httpMaxPreviewPagesSpin = nullptr;
    QSpinBox* m_httpMaxPreviewResponseMbSpin = nullptr;
    QDoubleSpinBox* m_offsetXSpin = nullptr;
    QDoubleSpinBox* m_offsetYSpin = nullptr;
    TemplatePreviewLabel* m_previewLabel = nullptr;
    QScrollArea* m_previewScrollArea = nullptr;
    bool m_fittingPreview = false;
    QLabel* m_statusLabel = nullptr;
    QLabel* m_serviceStatusLabel = nullptr;
    QLabel* m_printerStatusLabel = nullptr;
    QLabel* m_templateStatusLabel = nullptr;
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

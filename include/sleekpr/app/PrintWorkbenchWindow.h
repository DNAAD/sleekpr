#pragma once

#include "sleekpr/core/settings/PrintClientSettings.h"
#include "sleekpr/infrastructure/printing/TemplatePrintService.h"

#include <QHash>
#include <QWidget>
#include <functional>
#include <optional>

class QComboBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;
class QSpinBox;
class QTimer;

namespace sleekpr::infrastructure { class LabelPrintEngine; struct PreviewWorkResult; }

namespace sleekpr::app {

class PrintFieldEditorPanel;
class PrintPreviewPanel;
class PrintPreflightPanel;
class LatestPreviewQueue;

class PrintWorkbenchWindow final : public QWidget
{
public:
    using PrinterNamesProvider = std::function<QStringList()>;
    explicit PrintWorkbenchWindow(QString settingsPath, sleekpr::infrastructure::LabelPrintEngine* printEngine,
        PrinterNamesProvider printers = {}, QWidget* parent = nullptr);
    ~PrintWorkbenchWindow() override;
    void reloadFromDisk();
    void requestExternalRefresh();
    void refreshFieldDisplayNames();
    void setOpenDesignerCallback(std::function<void()> callback);
    void setOpenSettingsCallback(std::function<void()> callback);
    void setOpenFieldPresetManagerCallback(std::function<void()> callback);
    bool hasActivePrintSubmission() const;
    void setExitPaused(bool);
    void setSubmissionStateChangedCallback(std::function<void()> callback);
    bool saveCurrentValuesAsPreset(const QString& name, QString* errorMessage = nullptr);

protected:
    bool event(QEvent* event) override;

private:
    void buildUi();
    void populateTemplates(const QString& preferredKey);
    void selectTemplate();
    void populatePrinters();
    void schedulePreparation();
    void scheduleJobCheck();
    void prepareNow();
    void setupPreviewQueue();
    void publishPreparedPreview(const infrastructure::PreviewWorkResult& result);
    void printCurrent();
    void updatePrintState();
    void showStatus(const QString& text, const QString& role = {});
    bool rememberDraft();
    void applyPreset();
    void populatePresets(const sleekpr::core::TemplateDocument& document);
    void savePresetWithDialog();
    void refreshPreflight(const QString& jobError);
    QString preflightMessage(const core::PrintPreflightIssue& issue) const;
    void locatePreflightIssue(const core::PrintPreflightIssue& issue);
    QString selectedTemplateKey() const;

    QString m_settingsPath;
    sleekpr::infrastructure::TemplatePrintService m_service;
    sleekpr::infrastructure::LabelPrintEngine* m_printEngine;
    PrinterNamesProvider m_printersProvider;
    sleekpr::core::PrintClientSettings m_settings;
    QList<sleekpr::infrastructure::TemplatePrintChoice> m_templates;
    QList<sleekpr::core::FieldPreset> m_presets;
    QHash<QString, QJsonObject> m_drafts;
    QString m_activeTemplateKey;
    QString m_loadError;
    std::optional<sleekpr::infrastructure::PreparedTemplatePrint> m_prepared;
    std::optional<infrastructure::TemplatePrintSnapshot> m_preparedSnapshot;
    std::optional<infrastructure::TemplatePrintSnapshot> m_pendingSnapshot;
    LatestPreviewQueue* m_prepareQueue = nullptr;
    QListWidget* m_templateList = nullptr;
    QLineEdit* m_search = nullptr;
    PrintFieldEditorPanel* m_fields = nullptr;
    PrintPreviewPanel* m_preview = nullptr;
    PrintPreflightPanel* m_preflight = nullptr;
    core::PrintPreflightReport m_preflightReport;
    bool m_preflightPending = true;
    QComboBox* m_printers = nullptr;
    QComboBox* m_presetsCombo = nullptr;
    QSpinBox* m_copies = nullptr;
    QLabel* m_status = nullptr;
    QLabel* m_summary = nullptr;
    QLabel* m_jobSummary = nullptr;
    QPushButton* m_printButton = nullptr;
    QPushButton* m_refreshButton = nullptr;
    QPushButton* m_savePresetButton = nullptr;
    QPushButton* m_managePresetsButton = nullptr;
    QWidget* m_templatePanel = nullptr;
    QWidget* m_dataPanel = nullptr;
    QTimer* m_prepareTimer = nullptr;
    QTimer* m_jobCheckTimer = nullptr;
    std::function<void()> m_openDesigner;
    std::function<void()> m_openSettings;
    std::function<void()> m_openFieldPresetManager;
    bool m_loading = false;
    bool m_printing = false;
    bool m_exitPaused = false, m_enabledBeforeExit = true;
    bool m_resumeJobCheckAfterExit = false;
    std::function<void()> m_submissionStateChanged;
    bool m_nameRefreshQueued = false;
    bool m_externalRefreshPending = false;
    bool m_prepareOnShow = false;
};

} // 命名空间 sleekpr::app

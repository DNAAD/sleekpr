#include "sleekpr/app/TemplateDesignerWindow.h"

#include "sleekpr/app/TemplateDesignerFactory.h"
#include "sleekpr/app/TemplateDragCoordinateMapper.h"
#include "sleekpr/app/TemplateElementHitTester.h"
#include "sleekpr/app/TemplateInspectorPanel.h"
#include "sleekpr/app/TemplateLayerPanel.h"
#include "sleekpr/app/TemplatePreviewLabel.h"
#include "sleekpr/app/TemplateWorkspacePanel.h"
#include "sleekpr/core/labels/LabelRenderPlanner.h"
#include "sleekpr/core/native/NativeLabelDrawingPlanner.h"
#include "sleekpr/core/settings/PrinterSelectionResolver.h"
#include "sleekpr/core/settings/TemplateElement.h"
#include "sleekpr/core/templates/DefaultPaperSpecs.h"
#include "sleekpr/core/templates/DeviceProfileResolver.h"
#include "sleekpr/core/templates/PaperSpecStore.h"
#include "sleekpr/core/templates/TemplateDocumentFactory.h"
#include "sleekpr/core/templates/TemplateDocumentEditModel.h"
#include "sleekpr/core/templates/TemplateDocumentJson.h"
#include "sleekpr/core/templates/TemplateDocumentRenderer.h"
#include "sleekpr/core/templates/TemplateDocumentValidator.h"
#include "sleekpr/core/templates/TemplateLibraryStore.h"
#include "sleekpr/infrastructure/preview/LabelPreviewImageRenderer.h"
#include "sleekpr/infrastructure/preview/PreviewLabelFactory.h"
#include "sleekpr/infrastructure/printing/QtLabelPrintEngine.h"

#include <QAbstractItemModel>
#include <QAction>
#include <QApplication>
#include <QDateTime>
#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QEvent>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMessageBox>
#include <QPixmap>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRectF>
#include <QSaveFile>
#include <QSignalBlocker>
#include <QSplitter>
#include <QStringList>
#include <QTabWidget>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>
#include <optional>
#include <utility>

namespace sleekpr::app {

namespace {

constexpr auto kDefaultPaperSpecId = "label-80x30";
constexpr int kControlAutoApplyDelayMs = 80;
constexpr int kPreviewRefreshDelayMs = 16;
constexpr int kSettingsChangedDelayMs = 240;
constexpr double kDesignerInteractivePreviewDpi = 152.4;
constexpr double kColumnResizeHandleTolerancePx = 6.0;
constexpr double kColumnResizeMinimumWidthMm = 2.0;

struct JsonTextPosition
{
    int line = 1;
    int column = 1;
};

class ScopedBoolFlag
{
public:
    explicit ScopedBoolFlag(bool& target)
        : m_target(target)
        , m_previous(target)
    {
        m_target = true;
    }

    ~ScopedBoolFlag()
    {
        m_target = m_previous;
    }

private:
    bool& m_target;
    bool m_previous = false;
};

QSizeF defaultPaperSizeMm()
{
    return QSizeF(80.0, 30.0);
}

double designerColumnFlexWeight(const sleekpr::core::TableColumn& column)
{
    return column.flexWeight > 0.0 ? column.flexWeight : 1.0;
}

QList<double> resolvedDesignerColumnWidths(const sleekpr::core::TableElement& table)
{
    QList<double> widths;
    widths.reserve(table.columns.size());

    double fixedTotal = 0.0;
    double flexWeightTotal = 0.0;
    for (const auto& column : table.columns) {
        if (column.widthMode == sleekpr::core::TableColumnWidthMode::Flex) {
            flexWeightTotal += designerColumnFlexWeight(column);
        } else {
            fixedTotal += std::max(0.0, column.widthMm);
        }
    }

    const auto remainingWidth = std::max(0.0, table.width - fixedTotal);
    for (const auto& column : table.columns) {
        if (column.widthMode == sleekpr::core::TableColumnWidthMode::Flex && flexWeightTotal > 0.0) {
            widths.append(std::max(column.minWidthMm, remainingWidth * designerColumnFlexWeight(column) / flexWeightTotal));
        } else {
            widths.append(std::max(column.minWidthMm, column.widthMm));
        }
    }
    return widths;
}

JsonTextPosition jsonTextPositionAtOffset(const QString& text, qsizetype byteOffset)
{
    const auto bytes = text.toUtf8();
    const auto safeOffset = std::clamp<qsizetype>(byteOffset, 0, bytes.size());
    const auto before = QString::fromUtf8(bytes.left(safeOffset));
    const auto lastNewline = before.lastIndexOf(QLatin1Char('\n'));
    return JsonTextPosition{
        static_cast<int>(before.count(QLatin1Char('\n')) + 1),
        static_cast<int>(lastNewline < 0 ? before.size() + 1 : before.size() - lastNewline),
    };
}

QString sampleDataJson(const QJsonObject& sampleData)
{
    return QString::fromUtf8(QJsonDocument(sampleData).toJson(QJsonDocument::Indented));
}

QString defaultSampleDataJson()
{
    return sampleDataJson(TemplateDesignerFactory::createDefaultSampleData());
}

QString paperSpecDisplayName(const sleekpr::core::PaperSpec& spec)
{
    const auto name = spec.name.trimmed().isEmpty() ? spec.id : spec.name.trimmed();
    return QString::fromUtf8("%1 (%2, %3x%4 mm)")
        .arg(name)
        .arg(spec.id)
        .arg(spec.widthMm, 0, 'f', 1)
        .arg(spec.heightMm, 0, 'f', 1);
}

QString elementDisplayName(const sleekpr::core::TemplateElement& element, int index)
{
    const auto baseName = element.displayName.trimmed().isEmpty()
        ? QString::fromUtf8("%1 %2").arg(TemplateDesignerFactory::elementTypeName(element.type)).arg(index + 1)
        : element.displayName.trimmed();
    QStringList suffixes;
    if (!element.visible) {
        suffixes.append(QString::fromUtf8("隐藏"));
    }
    if (element.locked) {
        suffixes.append(QString::fromUtf8("锁定"));
    }
    return suffixes.isEmpty() ? baseName : QStringLiteral("%1 (%2)").arg(baseName, suffixes.join(QStringLiteral(", ")));
}

QString tableDisplayName(const sleekpr::core::TableElement& table, int index)
{
    const auto baseName = table.displayName.trimmed().isEmpty()
        ? QString::fromUtf8("表格 %1").arg(index + 1)
        : table.displayName.trimmed();
    QStringList suffixes;
    if (!table.visible) {
        suffixes.append(QString::fromUtf8("隐藏"));
    }
    if (table.locked) {
        suffixes.append(QString::fromUtf8("锁定"));
    }
    return suffixes.isEmpty() ? baseName : QStringLiteral("%1 (%2)").arg(baseName, suffixes.join(QStringLiteral(", ")));
}

bool templateMatchesLibrarySearch(
    const sleekpr::core::TemplateDocument& document,
    const QString& templateId,
    const QString& query)
{
    const auto normalizedQuery = query.trimmed();
    if (normalizedQuery.isEmpty()) {
        return true;
    }

    const QStringList searchableParts = {
        templateId,
        document.id,
        document.name,
        document.templateKey,
        document.category,
    };
    return std::any_of(searchableParts.cbegin(), searchableParts.cend(), [&normalizedQuery](const QString& part) {
        return part.contains(normalizedQuery, Qt::CaseInsensitive);
    });
}

} // 匿名命名空间

TemplateDesignerWindow::TemplateDesignerWindow(
    sleekpr::core::PrintClientSettings settings,
    SettingsChangedCallback onSettingsChanged,
    QString templateLibraryDirectoryPath,
    QWidget* parent)
    : TemplateDesignerWindow(
        std::move(settings),
        std::move(onSettingsChanged),
        std::move(templateLibraryDirectoryPath),
        nullptr,
        parent)
{
}

TemplateDesignerWindow::TemplateDesignerWindow(
    sleekpr::core::PrintClientSettings settings,
    SettingsChangedCallback onSettingsChanged,
    QString templateLibraryDirectoryPath,
    PrePrintCallback prePrintCallback,
    QWidget* parent)
    : QWidget(parent)
    , m_settings(std::move(settings))
    , m_onSettingsChanged(std::move(onSettingsChanged))
    , m_prePrintCallback(std::move(prePrintCallback))
    , m_templateLibraryDirectoryPath(std::move(templateLibraryDirectoryPath))
{
    setWindowTitle(QString::fromUtf8("sleekpr 模板设计器"));
    resize(1180, 680);
    buildUi();
    ensureCurrentTemplateDocument();
    refreshSampleDataEditor();
    refreshPaperSpecSelector();
    refreshTemplateLibraryList();
    refreshLayerList();
    refreshElementList();
    refreshPreview();
    resetDocumentHistory();
}

bool TemplateDesignerWindow::eventFilter(QObject* watched, QEvent* event)
{
    if (event != nullptr && event->type() == QEvent::FocusOut) {
        // 属性编辑控件失焦时立即提交，避免用户切换目标后还要等待防抖计时器。
        if (isElementAutoApplyEditor(watched)) {
            applyPendingElementAutoApply();
        } else if (isTableAutoApplyEditor(watched)) {
            applyPendingTableAutoApply();
        }
    }

    return QWidget::eventFilter(watched, event);
}

bool TemplateDesignerWindow::exportTemplateToFile(const QString& path) const
{
    if (path.trimmed().isEmpty()) {
        return false;
    }

    const auto documentIt = m_settings.templateDocuments.constFind(m_templateKey);
    if (documentIt == m_settings.templateDocuments.constEnd()) {
        return false;
    }

    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }

    const auto json = sleekpr::core::TemplateDocumentJson::toJson(documentIt.value());
    const auto payload = QJsonDocument(json).toJson(QJsonDocument::Indented);
    if (file.write(payload) != payload.size()) {
        return false;
    }
    return file.commit();
}

bool TemplateDesignerWindow::importTemplateFromFile(const QString& path)
{
    sleekpr::core::TemplateDocument document;
    QString errorMessage;
    if (!loadTemplateDocumentFromFile(path, &document, &errorMessage)) {
        m_statusLabel->setText(errorMessage.isEmpty() ? QString::fromUtf8("导入模板失败") : errorMessage);
        return false;
    }

    applyImportedTemplateDocument(document);
    m_statusLabel->setText(QString::fromUtf8("已导入模板"));
    notifySettingsChanged();
    return true;
}

void TemplateDesignerWindow::setOpenPaperSpecManagerCallback(OpenPaperSpecManagerCallback callback)
{
    m_openPaperSpecManager = std::move(callback);
}

void TemplateDesignerWindow::setOpenFieldPresetManagerCallback(OpenFieldPresetManagerCallback callback)
{
    m_openFieldPresetManager = std::move(callback);
}

void TemplateDesignerWindow::reloadPaperSpecsFromDisk()
{
    refreshPaperSpecSelector();
    refreshPreview();
}

void TemplateDesignerWindow::buildUi()
{
    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    auto* shell = new QWidget(this);
    shell->setObjectName(QStringLiteral("designerWorkbenchShell"));
    auto* shellLayout = new QHBoxLayout(shell);
    shellLayout->setContentsMargins(12, 12, 12, 12);
    shellLayout->setSpacing(12);

    auto* layerPanel = new TemplateLayerPanel(this);
    m_templateLibrarySearchEdit = layerPanel->templateLibrarySearchEdit();
    m_templateLibraryNameEdit = layerPanel->templateLibraryNameEdit();
    m_templateLibraryList = layerPanel->templateLibraryList();
    auto* refreshLibraryButton = layerPanel->refreshLibraryButton();
    auto* loadSelectedLibraryButton = layerPanel->loadSelectedLibraryButton();
    auto* createLibraryButton = layerPanel->createLibraryButton();
    auto* deleteLibraryButton = layerPanel->deleteLibraryButton();
    auto* duplicateLibraryButton = layerPanel->duplicateLibraryButton();
    auto* renameLibraryButton = layerPanel->renameLibraryButton();
    auto* loadFromLibraryButton = layerPanel->loadCurrentLibraryButton();
    m_layerList = layerPanel->layerList();
    auto* addLayerButton = layerPanel->addLayerButton();
    auto* deleteLayerButton = layerPanel->deleteLayerButton();
    auto* lockLayerButton = layerPanel->lockLayerButton();
    auto* hideLayerButton = layerPanel->hideLayerButton();
    auto* moveLayerUpButton = layerPanel->moveLayerUpButton();
    auto* moveLayerDownButton = layerPanel->moveLayerDownButton();
    auto* saveVersionButton = layerPanel->saveVersionButton();
    auto* restoreVersionButton = layerPanel->restoreVersionButton();

    auto* workspacePanel = new TemplateWorkspacePanel(defaultSampleDataJson(), this);
    auto* saveToLibraryButton = workspacePanel->saveToLibraryButton();
    auto* prePrintTemplateButton = workspacePanel->prePrintTemplateButton();
    auto* importTemplateButton = workspacePanel->importTemplateButton();
    auto* exportTemplateButton = workspacePanel->exportTemplateButton();
    m_undoButton = workspacePanel->undoButton();
    m_redoButton = workspacePanel->redoButton();
    m_zoomCombo = workspacePanel->zoomCombo();
    m_rulerPrecisionCombo = workspacePanel->rulerPrecisionCombo();
    m_designAidCheck = workspacePanel->designAidCheck();
    m_paperSpecCombo = workspacePanel->paperSpecCombo();
    auto* openPaperSpecManagerButton = workspacePanel->openPaperSpecManagerButton();
    auto* openFieldPresetManagerButton = workspacePanel->openFieldPresetManagerButton();
    m_previewLabel = workspacePanel->previewLabel();
    auto* saveSampleDataButton = workspacePanel->saveSampleDataButton();
    m_sampleDataEdit = workspacePanel->sampleDataEdit();
    connect(m_sampleDataEdit, &QPlainTextEdit::destroyed, this, [this] { m_sampleDataEdit = nullptr; });
    m_sampleDataErrorLabel = workspacePanel->sampleDataErrorLabel();
    m_statusLabel = workspacePanel->statusLabel();
    m_previewRefreshTimer = new QTimer(this);
    m_previewRefreshTimer->setObjectName(QStringLiteral("designerPreviewRefreshTimer"));
    m_previewRefreshTimer->setSingleShot(true);
    m_previewRefreshTimer->setInterval(kPreviewRefreshDelayMs);
    connect(m_previewRefreshTimer, &QTimer::timeout, this, [this] { refreshPreview(); });
    m_propertyRefreshTimer = new QTimer(this);
    m_propertyRefreshTimer->setObjectName(QStringLiteral("designerPropertyRefreshTimer"));
    m_propertyRefreshTimer->setSingleShot(true);
    m_propertyRefreshTimer->setInterval(kPreviewRefreshDelayMs);
    connect(m_propertyRefreshTimer, &QTimer::timeout, this, [this] {
        if (currentSelectionIsTable()) {
            refreshTablePropertyEditor();
            return;
        }
        refreshElementPropertyEditor();
    });
    m_settingsChangedTimer = new QTimer(this);
    m_settingsChangedTimer->setObjectName(QStringLiteral("designerSettingsChangedTimer"));
    m_settingsChangedTimer->setSingleShot(true);
    m_settingsChangedTimer->setInterval(kSettingsChangedDelayMs);
    connect(m_settingsChangedTimer, &QTimer::timeout, this, [this] { notifySettingsChanged(); });

    auto* inspectorPanel = new TemplateInspectorPanel(this);
    m_inspectorPanel = inspectorPanel;
    connect(m_inspectorPanel, &TemplateInspectorPanel::destroyed, this, [this] { m_inspectorPanel = nullptr; });
    m_elementList = inspectorPanel->elementList();
    auto* renameElementAction = inspectorPanel->renameElementAction();
    auto* deleteElementAction = inspectorPanel->deleteElementAction();
    auto* lockElementAction = inspectorPanel->lockElementAction();
    auto* hideElementAction = inspectorPanel->hideElementAction();
    auto* moveElementUpAction = inspectorPanel->moveElementUpAction();
    auto* moveElementDownAction = inspectorPanel->moveElementDownAction();
    auto* addFixedTextButton = inspectorPanel->addFixedTextButton();
    auto* addBoundFieldButton = inspectorPanel->addBoundFieldButton();
    auto* addQrCodeButton = inspectorPanel->addQrCodeButton();
    auto* addRectangleButton = inspectorPanel->addRectangleButton();
    auto* addTableButton = inspectorPanel->addTableButton();
    auto* addArrayGridButton = inspectorPanel->addArrayGridButton();
    auto* deleteElementButton = inspectorPanel->deleteElementButton();
    auto* lockElementButton = inspectorPanel->lockElementButton();
    auto* hideElementButton = inspectorPanel->hideElementButton();
    auto* moveElementUpButton = inspectorPanel->moveElementUpButton();
    auto* moveElementDownButton = inspectorPanel->moveElementDownButton();
    m_deviceProfilePrinterEdit = inspectorPanel->deviceProfilePrinterEdit();
    m_deviceProfileDpiSpin = inspectorPanel->deviceProfileDpiSpin();
    m_deviceProfileScaleXSpin = inspectorPanel->deviceProfileScaleXSpin();
    m_deviceProfileScaleYSpin = inspectorPanel->deviceProfileScaleYSpin();
    m_deviceProfileOffsetXSpin = inspectorPanel->deviceProfileOffsetXSpin();
    m_deviceProfileOffsetYSpin = inspectorPanel->deviceProfileOffsetYSpin();
    m_deviceCalibrationExpectedWidthSpin = inspectorPanel->deviceCalibrationExpectedWidthSpin();
    m_deviceCalibrationActualWidthSpin = inspectorPanel->deviceCalibrationActualWidthSpin();
    m_deviceCalibrationExpectedHeightSpin = inspectorPanel->deviceCalibrationExpectedHeightSpin();
    m_deviceCalibrationActualHeightSpin = inspectorPanel->deviceCalibrationActualHeightSpin();
    auto* calculateDeviceCalibrationButton = inspectorPanel->calculateDeviceCalibrationButton();
    auto* saveDeviceProfileButton = inspectorPanel->saveDeviceProfileButton();

    auto* mainSplitter = new QSplitter(Qt::Horizontal, shell);
    mainSplitter->setObjectName(QStringLiteral("designerMainSplitter"));
    mainSplitter->setChildrenCollapsible(false);
    mainSplitter->addWidget(layerPanel);
    mainSplitter->addWidget(workspacePanel);
    mainSplitter->addWidget(inspectorPanel);
    mainSplitter->setStretchFactor(0, 0);
    mainSplitter->setStretchFactor(1, 1);
    mainSplitter->setStretchFactor(2, 0);
    mainSplitter->setSizes({260, 760, 340});
    shellLayout->addWidget(mainSplitter, 1);
    rootLayout->addWidget(shell, 1);

    connect(addLayerButton, &QPushButton::clicked, this, [this] { addLayer(); });
    connect(deleteLayerButton, &QPushButton::clicked, this, [this] { deleteCurrentLayer(); });
    connect(lockLayerButton, &QPushButton::clicked, this, [this] { toggleCurrentLayerLocked(); });
    connect(hideLayerButton, &QPushButton::clicked, this, [this] { toggleCurrentLayerVisible(); });
    connect(moveLayerUpButton, &QPushButton::clicked, this, [this] { moveCurrentLayerUp(); });
    connect(moveLayerDownButton, &QPushButton::clicked, this, [this] { moveCurrentLayerDown(); });
    connect(saveVersionButton, &QPushButton::clicked, this, [this] { saveTemplateVersion(); });
    connect(restoreVersionButton, &QPushButton::clicked, this, [this] { restoreActiveTemplateVersion(); });
    connect(importTemplateButton, &QPushButton::clicked, this, [this] { importTemplateWithDialog(); });
    connect(exportTemplateButton, &QPushButton::clicked, this, [this] { exportTemplateWithDialog(); });
    connect(prePrintTemplateButton, &QPushButton::clicked, this, [this] { prePrintCurrentTemplate(); });
    connect(saveToLibraryButton, &QPushButton::clicked, this, [this] { saveCurrentTemplateToLibrary(); });
    connect(loadFromLibraryButton, &QPushButton::clicked, this, [this] { loadCurrentTemplateFromLibrary(); });
    connect(m_paperSpecCombo, &QComboBox::currentIndexChanged, this, [this] { applySelectedPaperSpec(); });
    connect(openPaperSpecManagerButton, &QPushButton::clicked, this, [this] {
        if (m_openPaperSpecManager) {
            m_openPaperSpecManager();
        }
    });
    connect(openFieldPresetManagerButton, &QPushButton::clicked, this, [this] {
        if (m_openFieldPresetManager) {
            m_openFieldPresetManager();
        }
    });
    connect(refreshLibraryButton, &QPushButton::clicked, this, [this] { refreshTemplateLibraryList(); });
    connect(loadSelectedLibraryButton, &QPushButton::clicked, this, [this] { loadSelectedTemplateFromLibrary(); });
    connect(createLibraryButton, &QPushButton::clicked, this, [this] { createTemplateInLibrary(); });
    connect(deleteLibraryButton, &QPushButton::clicked, this, [this] { deleteSelectedTemplateFromLibrary(); });
    connect(duplicateLibraryButton, &QPushButton::clicked, this, [this] { duplicateSelectedTemplateFromLibrary(); });
    connect(renameLibraryButton, &QPushButton::clicked, this, [this] { renameSelectedTemplateFromLibrary(); });
    connect(m_templateLibrarySearchEdit, &QLineEdit::textChanged, this, [this] { refreshTemplateLibraryList(); });
    connect(m_templateLibraryList, &QListWidget::currentItemChanged, this, [this](QListWidgetItem* current) {
        if (current == nullptr) {
            return;
        }

        loadTemplateFromLibraryById(current->data(Qt::UserRole).toString());
    });
    connect(m_templateLibraryList, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem*) { loadSelectedTemplateFromLibrary(); });
    connect(addFixedTextButton, &QPushButton::clicked, this, [this] { addFixedTextElement(); });
    connect(addBoundFieldButton, &QPushButton::clicked, this, [this] { addBoundFieldElement(); });
    connect(addQrCodeButton, &QPushButton::clicked, this, [this] { addQrCodeElement(); });
    connect(addRectangleButton, &QPushButton::clicked, this, [this] { addRectangleElement(); });
    connect(addTableButton, &QPushButton::clicked, this, [this] { addTableElement(); });
    connect(addArrayGridButton, &QPushButton::clicked, this, [this] { addArrayGridElement(); });
    connect(deleteElementButton, &QPushButton::clicked, this, [this] { deleteCurrentElement(); });
    connect(lockElementButton, &QPushButton::clicked, this, [this] { toggleCurrentElementLocked(); });
    connect(hideElementButton, &QPushButton::clicked, this, [this] { toggleCurrentElementVisible(); });
    connect(moveElementUpButton, &QPushButton::clicked, this, [this] { moveCurrentElementUp(); });
    connect(moveElementDownButton, &QPushButton::clicked, this, [this] { moveCurrentElementDown(); });
    connect(renameElementAction, &QAction::triggered, this, [this] { beginRenameCurrentElement(); });
    connect(deleteElementAction, &QAction::triggered, this, [this] { deleteCurrentElement(); });
    connect(lockElementAction, &QAction::triggered, this, [this] { toggleCurrentElementLocked(); });
    connect(hideElementAction, &QAction::triggered, this, [this] { toggleCurrentElementVisible(); });
    connect(moveElementUpAction, &QAction::triggered, this, [this] { moveCurrentElementUp(); });
    connect(moveElementDownAction, &QAction::triggered, this, [this] { moveCurrentElementDown(); });
    connect(m_elementList, &QListWidget::itemChanged, this, [this](QListWidgetItem* item) {
        renameElementFromListItem(item);
    });
    connect(m_elementList->model(), &QAbstractItemModel::rowsMoved, this, [this] {
        applyElementListOrderFromList();
    });
    m_elementAutoApplyTimer = new QTimer(this);
    m_elementAutoApplyTimer->setSingleShot(true);
    m_elementAutoApplyTimer->setInterval(kControlAutoApplyDelayMs);
    connect(m_elementAutoApplyTimer, &QTimer::timeout, this, [this] { applyCurrentElementProperties(true); });
    m_tableAutoApplyTimer = new QTimer(this);
    m_tableAutoApplyTimer->setSingleShot(true);
    m_tableAutoApplyTimer->setInterval(kControlAutoApplyDelayMs);
    connect(m_tableAutoApplyTimer, &QTimer::timeout, this, [this] { applyCurrentTableProperties(true); });
    connect(inspectorPanel, &TemplateInspectorPanel::elementPropertiesEdited, this, [this](int delayMs) {
        scheduleElementAutoApply(delayMs);
    });
    connect(inspectorPanel, &TemplateInspectorPanel::tablePropertiesEdited, this, [this](int delayMs) {
        scheduleTableAutoApply(delayMs);
    });
    connect(inspectorPanel, &TemplateInspectorPanel::elementPropertiesApplyRequested, this, [this] {
        applyCurrentElementProperties();
    });
    connect(inspectorPanel, &TemplateInspectorPanel::tablePropertiesApplyRequested, this, [this] {
        applyCurrentTableProperties();
    });
    connect(inspectorPanel, &TemplateInspectorPanel::elementPropertiesEditingFinished, this, [this] {
        applyPendingElementAutoApply();
    });
    connect(inspectorPanel, &TemplateInspectorPanel::tablePropertiesEditingFinished, this, [this] {
        applyPendingTableAutoApply();
    });
    inspectorPanel->installPropertyEditorEventFilter(this);
    connect(qApp, &QApplication::focusChanged, this, [this](QWidget* previous, QWidget*) {
        if (m_inspectorPanel != nullptr && m_inspectorPanel->isElementPropertyEditor(previous)) {
            applyPendingElementAutoApply();
            return;
        }

        if (m_inspectorPanel != nullptr && m_inspectorPanel->isTablePropertyEditor(previous)) {
            applyPendingTableAutoApply();
        }
    });
    connect(calculateDeviceCalibrationButton, &QPushButton::clicked, this, [this] { calculateDeviceCalibrationScale(); });
    connect(saveDeviceProfileButton, &QPushButton::clicked, this, [this] { saveDeviceProfile(); });
    connect(saveSampleDataButton, &QPushButton::clicked, this, [this] { saveSampleDataFromEditor(); });
    connect(m_undoButton, &QPushButton::clicked, this, [this] { undoCurrentTemplateChange(); });
    connect(m_redoButton, &QPushButton::clicked, this, [this] { redoCurrentTemplateChange(); });
    connect(m_zoomCombo, &QComboBox::currentIndexChanged, this, [this](int index) {
        if (m_zoomCombo == nullptr || index < 0) {
            return;
        }
        m_previewZoomFactor = std::max(0.1, m_zoomCombo->itemData(index).toDouble());
        refreshPreview();
    });
    connect(m_rulerPrecisionCombo, &QComboBox::currentIndexChanged, this, [this](int index) {
        if (m_previewLabel == nullptr || m_rulerPrecisionCombo == nullptr || index < 0) {
            return;
        }
        m_previewLabel->setRulerPrecisionMm(m_rulerPrecisionCombo->itemData(index).toDouble());
    });
    connect(m_designAidCheck, &QCheckBox::toggled, this, [this](bool checked) {
        if (m_previewLabel != nullptr) {
            m_previewLabel->setDesignAidVisible(checked);
        }
    });
    connect(m_sampleDataEdit, &QPlainTextEdit::textChanged, this, [this] {
        bool sampleDataOk = true;
        QString sampleDataErrorMessage;
        syncSampleDataFromEditor(&sampleDataOk, &sampleDataErrorMessage);
        updateSampleDataErrorLabel(sampleDataOk ? QString() : sampleDataErrorMessage);
        refreshPreview();
        if (!sampleDataOk && m_statusLabel != nullptr) {
            m_statusLabel->setText(sampleDataErrorMessage);
        }
    });
    connect(m_layerList, &QListWidget::currentRowChanged, this, [this] {
        refreshElementList();
        refreshPreview();
    });
    connect(m_elementList, &QListWidget::currentRowChanged, this, [this] {
        refreshElementPropertyEditor();
        refreshTablePropertyEditor();
        refreshInspectorTabForCurrentSelection();
        refreshPreview();
    });
    connect(m_previewLabel, &TemplatePreviewLabel::destroyed, this, [this] { m_previewLabel = nullptr; });
    m_previewLabel->setDragStartCallback([this](QPoint position) {
        m_tableColumnResizeDrag.reset();
        if (auto resizeDrag = tableColumnResizeDragAt(position); resizeDrag.has_value()) {
            if (!canEditElement(resizeDrag->tableId)) {
                return false;
            }
            selectElement(resizeDrag->tableId);
            m_tableColumnResizeDrag = resizeDrag;
            return true;
        }

        // 已选元素内部的拖拽优先作用在当前选中项，避免重叠元素抢占命中导致用户误以为拖出了副本。
        if (currentSelectionContainsCanvasPosition(position)) {
            return true;
        }

        const auto paperSize = currentPaperSizeMm();
        const auto hit = TemplateElementHitTester().hitTest(
            m_previewCommands,
            paperSize,
            m_previewLabel->printableImageSizePx(),
            m_previewLabel->mapToPrintableImagePx(position));
        if (!hit.has_value() || !canEditElement(hit.value())) {
            return false;
        }
        selectElement(hit.value());
        return true;
    });
    m_previewLabel->setDragDeltaCallback([this](QPoint delta) {
        if (m_tableColumnResizeDrag.has_value()) {
            resizeTableColumnByPixels(delta);
            return;
        }
        moveSelectedElementByPixels(delta);
    });
    m_previewLabel->setKeyboardNudgeCallback([this](QPoint direction, Qt::KeyboardModifiers modifiers) {
        nudgeSelectedElement(direction, modifiers);
    });
}

void TemplateDesignerWindow::ensureCurrentTemplateDocument()
{
    if (m_settings.templateDocuments.contains(m_templateKey)) {
        return;
    }

    const auto labelItem = sleekpr::infrastructure::PreviewLabelFactory::createDemoLabel(
        sleekpr::core::LabelTemplateKey::Default80x30);
    const auto labelPlan = sleekpr::core::LabelRenderPlanner().createPlan(labelItem);
    const auto drawingPlan = sleekpr::core::NativeLabelDrawingPlanner().createPlan(
        labelPlan,
        m_settings.labelOffset,
        m_settings.templateOverrides.value(m_templateKey),
        QList<sleekpr::core::TemplateElement>{});

    // 首次打开设计器只在内存中启动完整模板文档；旧版自定义元素必须原样迁移，避免绑定字段和动态二维码被演示数据固化。
    m_settings.templateDocuments[m_templateKey] = sleekpr::core::TemplateDocumentFactory::fromDrawingPlan(
        m_templateKey,
        QString::fromUtf8("默认标签"),
        drawingPlan,
        m_settings.templateElements.value(m_templateKey));
    m_settings.templateDocuments[m_templateKey].paperSpecId = QString::fromLatin1(kDefaultPaperSpecId);
    m_settings.templateDocuments[m_templateKey].sampleData = TemplateDesignerFactory::createDefaultSampleData();
}

void TemplateDesignerWindow::addLayer()
{
    ensureCurrentTemplateDocument();
    auto& document = m_settings.templateDocuments[m_templateKey];

    const auto layerId = TemplateDesignerFactory::createLayerId();
    if (!sleekpr::core::TemplateDocumentEditModel::addLayer(document, layerId, TemplateDesignerFactory::nextLayerName(document.layers.size()))) {
        m_statusLabel->setText(QString::fromUtf8("新增图层失败"));
        return;
    }

    refreshLayerList();
    selectLayer(layerId);
    refreshElementList();
    refreshPreview();
    m_statusLabel->setText(QString::fromUtf8("已新增图层"));
    notifySettingsChanged();
}

void TemplateDesignerWindow::deleteCurrentLayer()
{
    ensureCurrentTemplateDocument();
    auto& document = m_settings.templateDocuments[m_templateKey];
    const auto layerId = currentLayerId();
    const auto* layer = currentLayer();
    if (layer == nullptr || !layer->elements.isEmpty() || document.layers.size() <= 1) {
        m_statusLabel->setText(QString::fromUtf8("只能直接删除空图层"));
        return;
    }

    if (sleekpr::core::TemplateDocumentEditModel::deleteLayer(document, layerId)) {
        refreshAll();
        m_statusLabel->setText(QString::fromUtf8("已删除图层"));
        notifySettingsChanged();
    }
}

void TemplateDesignerWindow::toggleCurrentLayerLocked()
{
    ensureCurrentTemplateDocument();
    auto& document = m_settings.templateDocuments[m_templateKey];
    auto* layer = currentLayer();
    if (layer == nullptr) {
        return;
    }
    const auto layerId = layer->id;
    if (sleekpr::core::TemplateDocumentEditModel::setLayerLocked(document, layerId, !layer->locked)) {
        refreshAll();
        selectLayer(layerId);
        m_statusLabel->setText(QString::fromUtf8("已切换图层锁定状态"));
        notifySettingsChanged();
    }
}

void TemplateDesignerWindow::toggleCurrentLayerVisible()
{
    ensureCurrentTemplateDocument();
    auto& document = m_settings.templateDocuments[m_templateKey];
    auto* layer = currentLayer();
    if (layer == nullptr) {
        return;
    }
    const auto layerId = layer->id;
    if (sleekpr::core::TemplateDocumentEditModel::setLayerVisible(document, layerId, !layer->visible)) {
        refreshAll();
        selectLayer(layerId);
        m_statusLabel->setText(QString::fromUtf8("已切换图层显示状态"));
        notifySettingsChanged();
    }
}

void TemplateDesignerWindow::moveCurrentLayerUp()
{
    ensureCurrentTemplateDocument();
    auto& document = m_settings.templateDocuments[m_templateKey];
    const auto layerId = currentLayerId();
    if (sleekpr::core::TemplateDocumentEditModel::moveLayerUp(document, layerId)) {
        refreshAll();
        selectLayer(layerId);
        notifySettingsChanged();
    }
}

void TemplateDesignerWindow::moveCurrentLayerDown()
{
    ensureCurrentTemplateDocument();
    auto& document = m_settings.templateDocuments[m_templateKey];
    const auto layerId = currentLayerId();
    if (sleekpr::core::TemplateDocumentEditModel::moveLayerDown(document, layerId)) {
        refreshAll();
        selectLayer(layerId);
        notifySettingsChanged();
    }
}

void TemplateDesignerWindow::addFixedTextElement()
{
    addElement(TemplateDesignerFactory::createElement(sleekpr::core::TemplateElementType::FixedText));
}

void TemplateDesignerWindow::addBoundFieldElement()
{
    addElement(TemplateDesignerFactory::createElement(sleekpr::core::TemplateElementType::BoundField));
}

void TemplateDesignerWindow::addQrCodeElement()
{
    addElement(TemplateDesignerFactory::createElement(sleekpr::core::TemplateElementType::QrCode));
}

void TemplateDesignerWindow::addRectangleElement()
{
    addElement(TemplateDesignerFactory::createElement(sleekpr::core::TemplateElementType::Rectangle));
}

void TemplateDesignerWindow::addTableElement()
{
    addTable(TemplateDesignerFactory::createTable());
}

void TemplateDesignerWindow::addArrayGridElement()
{
    addElement(TemplateDesignerFactory::createElement(sleekpr::core::TemplateElementType::ArrayGrid));
}

void TemplateDesignerWindow::deleteCurrentElement()
{
    ensureCurrentTemplateDocument();
    auto& document = m_settings.templateDocuments[m_templateKey];
    const auto elementId = currentElementId();
    if (currentSelectionIsTable()) {
        if (sleekpr::core::TemplateDocumentEditModel::deleteTable(document, elementId)) {
            refreshAll();
            notifySettingsChanged();
        }
        return;
    }
    if (sleekpr::core::TemplateDocumentEditModel::deleteElement(document, elementId)) {
        refreshAll();
        notifySettingsChanged();
    }
}

void TemplateDesignerWindow::toggleCurrentElementLocked()
{
    ensureCurrentTemplateDocument();
    auto& document = m_settings.templateDocuments[m_templateKey];
    if (currentSelectionIsTable()) {
        auto* table = currentTable();
        if (table == nullptr) {
            return;
        }
        const auto tableId = table->id;
        if (sleekpr::core::TemplateDocumentEditModel::setTableLocked(document, tableId, !table->locked)) {
            refreshAll();
            selectElement(tableId);
            notifySettingsChanged();
        }
        return;
    }

    auto* element = currentElement();
    if (element == nullptr) {
        return;
    }
    const auto elementId = element->id;
    if (sleekpr::core::TemplateDocumentEditModel::setElementLocked(document, elementId, !element->locked)) {
        refreshAll();
        selectElement(elementId);
        notifySettingsChanged();
    }
}

void TemplateDesignerWindow::toggleCurrentElementVisible()
{
    ensureCurrentTemplateDocument();
    auto& document = m_settings.templateDocuments[m_templateKey];
    if (currentSelectionIsTable()) {
        auto* table = currentTable();
        if (table == nullptr) {
            return;
        }
        const auto tableId = table->id;
        if (sleekpr::core::TemplateDocumentEditModel::setTableVisible(document, tableId, !table->visible)) {
            refreshAll();
            selectElement(tableId);
            notifySettingsChanged();
        }
        return;
    }

    auto* element = currentElement();
    if (element == nullptr) {
        return;
    }
    const auto elementId = element->id;
    if (sleekpr::core::TemplateDocumentEditModel::setElementVisible(document, elementId, !element->visible)) {
        refreshAll();
        selectElement(elementId);
        notifySettingsChanged();
    }
}

void TemplateDesignerWindow::moveCurrentElementUp()
{
    ensureCurrentTemplateDocument();
    auto& document = m_settings.templateDocuments[m_templateKey];
    const auto elementId = currentElementId();
    if (sleekpr::core::TemplateDocumentEditModel::moveElementUp(document, elementId)) {
        refreshAll();
        selectElement(elementId);
        notifySettingsChanged();
    }
}

void TemplateDesignerWindow::moveCurrentElementDown()
{
    ensureCurrentTemplateDocument();
    auto& document = m_settings.templateDocuments[m_templateKey];
    const auto elementId = currentElementId();
    if (sleekpr::core::TemplateDocumentEditModel::moveElementDown(document, elementId)) {
        refreshAll();
        selectElement(elementId);
        notifySettingsChanged();
    }
}

void TemplateDesignerWindow::saveTemplateVersion()
{
    ensureCurrentTemplateDocument();
    auto& document = m_settings.templateDocuments[m_templateKey];
    const auto now = QDateTime::currentDateTime();
    const auto baseId = now.toMSecsSinceEpoch();

    for (int attempt = 0; attempt < 100; ++attempt) {
        const auto versionId = QStringLiteral("version-%1").arg(baseId + attempt);
        if (sleekpr::core::TemplateDocumentEditModel::saveVersion(
                document,
                versionId,
                now.toString(QString::fromUtf8("yyyy-MM-dd HH:mm:ss")),
                now.toString(Qt::ISODateWithMs),
                QString::fromUtf8("设计器保存"))) {
            m_statusLabel->setText(QString::fromUtf8("已保存模板版本"));
            notifySettingsChanged();
            return;
        }
    }

    m_statusLabel->setText(QString::fromUtf8("保存模板版本失败"));
}

void TemplateDesignerWindow::restoreActiveTemplateVersion()
{
    ensureCurrentTemplateDocument();
    auto& document = m_settings.templateDocuments[m_templateKey];
    auto versionId = document.activeVersionId;
    if (versionId.isEmpty() && !document.versions.isEmpty()) {
        versionId = document.versions.last().id;
    }

    if (sleekpr::core::TemplateDocumentEditModel::restoreVersion(document, versionId)) {
        refreshAll();
        m_statusLabel->setText(QString::fromUtf8("已恢复模板版本"));
        notifySettingsChanged();
        return;
    }

    m_statusLabel->setText(QString::fromUtf8("没有可恢复的模板版本"));
}

void TemplateDesignerWindow::saveCurrentTemplateToLibrary()
{
    ensureCurrentTemplateDocument();
    if (m_templateLibraryDirectoryPath.trimmed().isEmpty()) {
        m_statusLabel->setText(QString::fromUtf8("未配置模板库目录"));
        return;
    }

    bool sampleDataOk = true;
    QString sampleDataErrorMessage;
    if (!syncSampleDataFromEditor(&sampleDataOk, &sampleDataErrorMessage)) {
        m_statusLabel->setText(sampleDataErrorMessage);
        return;
    }

    QString errorMessage;
    const auto& document = m_settings.templateDocuments[m_templateKey];
    // 设计器只负责发起保存，模板 id 安全性、目录创建和原子写入都交给模板库存储层。
    if (!sleekpr::core::TemplateLibraryStore(m_templateLibraryDirectoryPath).saveTemplate(document, &errorMessage)) {
        m_statusLabel->setText(errorMessage.isEmpty() ? QString::fromUtf8("保存到模板库失败") : errorMessage);
        return;
    }

    m_statusLabel->setText(QString::fromUtf8("已保存到模板库"));
    refreshTemplateLibraryList();
    if (m_templateLibraryList != nullptr) {
        for (int row = 0; row < m_templateLibraryList->count(); ++row) {
            if (m_templateLibraryList->item(row)->data(Qt::UserRole).toString() == document.id) {
                m_templateLibraryList->setCurrentRow(row);
                break;
            }
        }
    }
}

void TemplateDesignerWindow::loadCurrentTemplateFromLibrary()
{
    loadTemplateFromLibraryById(QStringLiteral("template-") + m_templateKey);
}

void TemplateDesignerWindow::createTemplateInLibrary()
{
    ensureCurrentTemplateDocument();
    if (m_templateLibraryDirectoryPath.trimmed().isEmpty()) {
        m_statusLabel->setText(QString::fromUtf8("未配置模板库目录"));
        return;
    }

    const auto requestedName = m_templateLibraryNameEdit == nullptr
        ? QString()
        : m_templateLibraryNameEdit->text().trimmed();
    const auto& currentDocument = m_settings.templateDocuments[m_templateKey];
    const auto document = TemplateDesignerFactory::createBlankDocument(
        TemplateDesignerFactory::createTemplateId(),
        requestedName.isEmpty() ? QString::fromUtf8("未命名模板") : requestedName,
        m_templateKey,
        currentDocument.paperSpecId,
        currentDocument.deviceProfiles);

    QString errorMessage;
    // 新建模板必须清空画布内容，只保留纸张和校准上下文，避免把上一张标签误当成新模板。
    if (!sleekpr::core::TemplateLibraryStore(m_templateLibraryDirectoryPath).saveTemplate(document, &errorMessage)) {
        m_statusLabel->setText(errorMessage.isEmpty() ? QString::fromUtf8("新建模板失败") : errorMessage);
        return;
    }

    applyImportedTemplateDocument(document);
    refreshTemplateLibraryList();
    if (m_templateLibraryNameEdit != nullptr) {
        m_templateLibraryNameEdit->clear();
    }
    if (m_templateLibraryList != nullptr) {
        for (int row = 0; row < m_templateLibraryList->count(); ++row) {
            if (m_templateLibraryList->item(row)->data(Qt::UserRole).toString() == document.id) {
                m_templateLibraryList->setCurrentRow(row);
                break;
            }
        }
    }
    m_statusLabel->setText(QString::fromUtf8("已新建模板：%1").arg(document.name));
    notifySettingsChanged();
}

void TemplateDesignerWindow::deleteSelectedTemplateFromLibrary()
{
    if (m_templateLibraryDirectoryPath.trimmed().isEmpty()) {
        m_statusLabel->setText(QString::fromUtf8("未配置模板库目录"));
        return;
    }
    if (m_templateLibraryList == nullptr || m_templateLibraryList->currentItem() == nullptr) {
        m_statusLabel->setText(QString::fromUtf8("请先选择模板库中的模板"));
        return;
    }

    const auto templateId = m_templateLibraryList->currentItem()->data(Qt::UserRole).toString();
    if (templateId.trimmed().isEmpty()) {
        m_statusLabel->setText(QString::fromUtf8("模板 id 不能为空"));
        return;
    }

    // 删除只影响模板库文件；当前画布内容保留，避免误删后界面立即丢失正在编辑的版式。
    if (!sleekpr::core::TemplateLibraryStore(m_templateLibraryDirectoryPath).removeTemplate(templateId)) {
        m_statusLabel->setText(QString::fromUtf8("删除模板失败"));
        return;
    }

    refreshTemplateLibraryList();
    m_statusLabel->setText(QString::fromUtf8("已删除模板：%1").arg(templateId));
}

void TemplateDesignerWindow::duplicateSelectedTemplateFromLibrary()
{
    if (m_templateLibraryDirectoryPath.trimmed().isEmpty()) {
        m_statusLabel->setText(QString::fromUtf8("未配置模板库目录"));
        return;
    }
    if (m_templateLibraryList == nullptr || m_templateLibraryList->currentItem() == nullptr) {
        m_statusLabel->setText(QString::fromUtf8("请先选择模板库中的模板"));
        return;
    }

    const auto sourceTemplateId = m_templateLibraryList->currentItem()->data(Qt::UserRole).toString();
    if (sourceTemplateId.trimmed().isEmpty()) {
        m_statusLabel->setText(QString::fromUtf8("模板 id 不能为空"));
        return;
    }

    sleekpr::core::TemplateLibraryStore store(m_templateLibraryDirectoryPath);
    QString errorMessage;
    auto document = store.loadTemplate(sourceTemplateId, &errorMessage);
    if (!document.has_value()) {
        m_statusLabel->setText(errorMessage.isEmpty() ? QString::fromUtf8("读取模板失败") : errorMessage);
        return;
    }

    const auto sourceName = document->name.trimmed().isEmpty() ? sourceTemplateId : document->name.trimmed();
    document->id = TemplateDesignerFactory::createTemplateId();
    document->name = QString::fromUtf8("%1 副本").arg(sourceName);

    // 复制只产生一个新的模板库文件，不切换当前画布，避免覆盖用户正在编辑的内容。
    if (!store.saveTemplate(document.value(), &errorMessage)) {
        m_statusLabel->setText(errorMessage.isEmpty() ? QString::fromUtf8("复制模板失败") : errorMessage);
        return;
    }

    refreshTemplateLibraryList();
    selectTemplateInLibraryList(document->id);
    m_statusLabel->setText(QString::fromUtf8("已复制模板：%1").arg(document->name));
}

void TemplateDesignerWindow::renameSelectedTemplateFromLibrary()
{
    if (m_templateLibraryDirectoryPath.trimmed().isEmpty()) {
        m_statusLabel->setText(QString::fromUtf8("未配置模板库目录"));
        return;
    }
    if (m_templateLibraryList == nullptr || m_templateLibraryList->currentItem() == nullptr) {
        m_statusLabel->setText(QString::fromUtf8("请先选择模板库中的模板"));
        return;
    }

    const auto templateId = m_templateLibraryList->currentItem()->data(Qt::UserRole).toString();
    const auto requestedName = m_templateLibraryNameEdit == nullptr
        ? QString()
        : m_templateLibraryNameEdit->text().trimmed();
    if (templateId.trimmed().isEmpty()) {
        m_statusLabel->setText(QString::fromUtf8("模板 id 不能为空"));
        return;
    }
    if (requestedName.isEmpty()) {
        m_statusLabel->setText(QString::fromUtf8("请输入模板名称"));
        return;
    }

    sleekpr::core::TemplateLibraryStore store(m_templateLibraryDirectoryPath);
    QString errorMessage;
    auto document = store.loadTemplate(templateId, &errorMessage);
    if (!document.has_value()) {
        m_statusLabel->setText(errorMessage.isEmpty() ? QString::fromUtf8("读取模板失败") : errorMessage);
        return;
    }

    document->name = requestedName;
    // 重命名保留模板 id，外部接口和历史引用不会因为显示名变化而失效。
    if (!store.saveTemplate(document.value(), &errorMessage)) {
        m_statusLabel->setText(errorMessage.isEmpty() ? QString::fromUtf8("重命名模板失败") : errorMessage);
        return;
    }

    refreshTemplateLibraryList();
    selectTemplateInLibraryList(templateId);
    m_statusLabel->setText(QString::fromUtf8("已重命名模板：%1").arg(requestedName));
}

void TemplateDesignerWindow::loadSelectedTemplateFromLibrary()
{
    if (m_templateLibraryList == nullptr || m_templateLibraryList->currentItem() == nullptr) {
        m_statusLabel->setText(QString::fromUtf8("请先选择模板库中的模板"));
        return;
    }

    loadTemplateFromLibraryById(m_templateLibraryList->currentItem()->data(Qt::UserRole).toString());
}

void TemplateDesignerWindow::loadTemplateFromLibraryById(const QString& templateId)
{
    if (m_templateLibraryDirectoryPath.trimmed().isEmpty()) {
        m_statusLabel->setText(QString::fromUtf8("未配置模板库目录"));
        return;
    }
    if (templateId.trimmed().isEmpty()) {
        m_statusLabel->setText(QString::fromUtf8("模板 id 不能为空"));
        return;
    }

    QString errorMessage;
    const auto loadedDocument = sleekpr::core::TemplateLibraryStore(m_templateLibraryDirectoryPath)
                                    .loadTemplate(templateId, &errorMessage);
    if (!loadedDocument.has_value()) {
        m_statusLabel->setText(errorMessage.isEmpty() ? QString::fromUtf8("从模板库加载失败") : errorMessage);
        return;
    }

    // 加载后仍写回当前 settings 模板槽位，保持旧设置结构和预览刷新链路兼容。
    applyImportedTemplateDocument(loadedDocument.value());
    const auto displayName = loadedDocument->name.trimmed().isEmpty() ? loadedDocument->id : loadedDocument->name.trimmed();
    m_statusLabel->setText(QString::fromUtf8("已从模板库加载：%1").arg(displayName));
    notifySettingsChanged();
}

void TemplateDesignerWindow::refreshTemplateLibraryList()
{
    if (m_templateLibraryList == nullptr) {
        return;
    }

    const auto selectedId = m_templateLibraryList->currentItem() == nullptr
        ? QString()
        : m_templateLibraryList->currentItem()->data(Qt::UserRole).toString();
    const auto wasBlocked = m_templateLibraryList->blockSignals(true);
    m_templateLibraryList->clear();

    if (!m_templateLibraryDirectoryPath.trimmed().isEmpty()) {
        const sleekpr::core::TemplateLibraryStore store(m_templateLibraryDirectoryPath);
        const auto searchQuery = m_templateLibrarySearchEdit == nullptr
            ? QString()
            : m_templateLibrarySearchEdit->text().trimmed();
        for (const auto& templateId : store.templateIds()) {
            const auto loadedDocument = store.loadTemplate(templateId);
            if (!loadedDocument.has_value() && !searchQuery.isEmpty()
                && !templateId.contains(searchQuery, Qt::CaseInsensitive)) {
                continue;
            }
            if (loadedDocument.has_value()
                && !templateMatchesLibrarySearch(loadedDocument.value(), templateId, searchQuery)) {
                continue;
            }
            const auto displayName = loadedDocument.has_value() && !loadedDocument->name.trimmed().isEmpty()
                ? QStringLiteral("%1 (%2)").arg(loadedDocument->name.trimmed(), templateId)
                : templateId;
            auto* item = new QListWidgetItem(displayName, m_templateLibraryList);
            // 列表显示可搜索、可重命名，但真正加载必须始终使用稳定模板 id。
            item->setData(Qt::UserRole, templateId);
        }
    }

    bool selected = false;
    if (!selectedId.isEmpty()) {
        for (int row = 0; row < m_templateLibraryList->count(); ++row) {
            if (m_templateLibraryList->item(row)->data(Qt::UserRole).toString() == selectedId) {
                m_templateLibraryList->setCurrentRow(row);
                selected = true;
                break;
            }
        }
    }
    if (!selected && m_templateLibraryList->count() > 0) {
        m_templateLibraryList->setCurrentRow(0);
    }

    m_templateLibraryList->blockSignals(wasBlocked);
}

void TemplateDesignerWindow::importTemplateWithDialog()
{
    QFileDialog dialog(this, QString::fromUtf8("导入模板"));
    dialog.setAcceptMode(QFileDialog::AcceptOpen);
    dialog.setFileMode(QFileDialog::ExistingFile);
    dialog.setNameFilter(QString::fromUtf8("sleekpr 模板 (*.sleekpr-template.json *.json)"));
    if (dialog.exec() != QDialog::Accepted || dialog.selectedFiles().isEmpty()) {
        return;
    }

    sleekpr::core::TemplateDocument document;
    QString errorMessage;
    if (!loadTemplateDocumentFromFile(dialog.selectedFiles().first(), &document, &errorMessage)) {
        QMessageBox::warning(this, QString::fromUtf8("导入模板失败"), errorMessage);
        m_statusLabel->setText(errorMessage);
        return;
    }

    const auto summary = QString::fromUtf8("模板：%1\n图层：%2\n版本：%3\n\n是否替换当前模板？")
        .arg(document.name.trimmed().isEmpty() ? document.id : document.name)
        .arg(document.layers.size())
        .arg(document.versions.size());
    if (QMessageBox::question(this, QString::fromUtf8("确认导入模板"), summary) != QMessageBox::Yes) {
        return;
    }

    applyImportedTemplateDocument(document);
    m_statusLabel->setText(QString::fromUtf8("已导入模板"));
    notifySettingsChanged();
}

void TemplateDesignerWindow::exportTemplateWithDialog()
{
    ensureCurrentTemplateDocument();
    QFileDialog dialog(this, QString::fromUtf8("导出模板"));
    dialog.setAcceptMode(QFileDialog::AcceptSave);
    dialog.setDefaultSuffix(QStringLiteral("sleekpr-template.json"));
    dialog.setNameFilter(QString::fromUtf8("sleekpr 模板 (*.sleekpr-template.json *.json)"));
    dialog.selectFile(QStringLiteral("%1.sleekpr-template.json").arg(m_templateKey));
    if (dialog.exec() != QDialog::Accepted || dialog.selectedFiles().isEmpty()) {
        return;
    }

    if (exportTemplateToFile(dialog.selectedFiles().first())) {
        m_statusLabel->setText(QString::fromUtf8("已导出模板"));
        return;
    }

    QMessageBox::warning(this, QString::fromUtf8("导出模板失败"), QString::fromUtf8("模板文件写入失败，请检查路径权限。"));
    m_statusLabel->setText(QString::fromUtf8("导出模板失败"));
}

void TemplateDesignerWindow::prePrintCurrentTemplate()
{
    ensureCurrentTemplateDocument();

    bool sampleDataOk = true;
    QString sampleDataErrorMessage;
    const auto renderContext = sampleRenderContext(&sampleDataOk, &sampleDataErrorMessage);
    if (!sampleDataOk) {
        if (m_statusLabel != nullptr) {
            m_statusLabel->setText(sampleDataErrorMessage);
        }
        return;
    }

    const auto requestedPrinter = m_deviceProfilePrinterEdit == nullptr ? QString() : m_deviceProfilePrinterEdit->text();
    const auto printerName = sleekpr::core::PrinterSelectionResolver().resolve(m_settings, requestedPrinter);
    const auto printPlan = createCurrentPrintPlan(renderContext, printerName);

    bool printed = false;
    if (m_prePrintCallback) {
        printed = m_prePrintCallback(printPlan, printerName);
    } else {
        // 设计器预打印必须复用 Qt 原生打印引擎，避免出现一套预览坐标、一套真实打印坐标。
        sleekpr::infrastructure::QtLabelPrintEngine printEngine;
        printed = printEngine.print(printPlan, printerName);
    }

    if (m_statusLabel != nullptr) {
        m_statusLabel->setText(printed
                ? QString::fromUtf8("预打印已发送")
                : QString::fromUtf8("预打印失败，请检查打印机或模板数据"));
    }
}

void TemplateDesignerWindow::saveSampleDataFromEditor()
{
    bool sampleDataOk = true;
    QString sampleDataErrorMessage;
    if (!syncSampleDataFromEditor(&sampleDataOk, &sampleDataErrorMessage)) {
        updateSampleDataErrorLabel(sampleDataErrorMessage);
        if (m_statusLabel != nullptr) {
            m_statusLabel->setText(sampleDataErrorMessage);
        }
        return;
    }

    updateSampleDataErrorLabel(QString());
    refreshPreview();
    if (m_statusLabel != nullptr) {
        m_statusLabel->setText(QString::fromUtf8("已保存模拟数据"));
    }
}

void TemplateDesignerWindow::undoCurrentTemplateChange()
{
    const auto document = m_state.undoDocument();
    if (!document.has_value()) {
        updateHistoryButtons();
        return;
    }

    m_state.beginDocumentHistoryRestore();
    m_settings.templateDocuments[m_templateKey] = document.value();
    refreshAll();
    notifySettingsChanged();
    m_state.endDocumentHistoryRestore();
    updateHistoryButtons();
    if (m_statusLabel != nullptr) {
        m_statusLabel->setText(QString::fromUtf8("已撤销"));
    }
}

void TemplateDesignerWindow::redoCurrentTemplateChange()
{
    const auto document = m_state.redoDocument();
    if (!document.has_value()) {
        updateHistoryButtons();
        return;
    }

    m_state.beginDocumentHistoryRestore();
    m_settings.templateDocuments[m_templateKey] = document.value();
    refreshAll();
    notifySettingsChanged();
    m_state.endDocumentHistoryRestore();
    updateHistoryButtons();
    if (m_statusLabel != nullptr) {
        m_statusLabel->setText(QString::fromUtf8("已重做"));
    }
}

void TemplateDesignerWindow::calculateDeviceCalibrationScale()
{
    if (m_deviceCalibrationExpectedWidthSpin == nullptr
        || m_deviceCalibrationActualWidthSpin == nullptr
        || m_deviceCalibrationExpectedHeightSpin == nullptr
        || m_deviceCalibrationActualHeightSpin == nullptr
        || m_deviceProfileScaleXSpin == nullptr
        || m_deviceProfileScaleYSpin == nullptr) {
        return;
    }

    const auto expectedWidth = m_deviceCalibrationExpectedWidthSpin->value();
    const auto actualWidth = m_deviceCalibrationActualWidthSpin->value();
    const auto expectedHeight = m_deviceCalibrationExpectedHeightSpin->value();
    const auto actualHeight = m_deviceCalibrationActualHeightSpin->value();
    if (expectedWidth <= 0.0 || actualWidth <= 0.0 || expectedHeight <= 0.0 || actualHeight <= 0.0) {
        m_statusLabel->setText(QString::fromUtf8("校准尺寸必须大于 0"));
        return;
    }

    // 打印结果偏大时 scale 小于 1，偏小时 scale 大于 1，渲染层会统一应用该缩放。
    const auto scaleX = expectedWidth / actualWidth;
    const auto scaleY = expectedHeight / actualHeight;
    if (scaleX < m_deviceProfileScaleXSpin->minimum()
        || scaleX > m_deviceProfileScaleXSpin->maximum()
        || scaleY < m_deviceProfileScaleYSpin->minimum()
        || scaleY > m_deviceProfileScaleYSpin->maximum()) {
        m_statusLabel->setText(QString::fromUtf8("测量结果超出当前缩放范围"));
        return;
    }

    m_deviceProfileScaleXSpin->setValue(scaleX);
    m_deviceProfileScaleYSpin->setValue(scaleY);
    m_statusLabel->setText(QString::fromUtf8("已按测量尺寸计算缩放"));
}

void TemplateDesignerWindow::saveDeviceProfile()
{
    ensureCurrentTemplateDocument();
    auto& document = m_settings.templateDocuments[m_templateKey];
    const auto printerName = m_deviceProfilePrinterEdit->text().trimmed();

    sleekpr::core::DeviceProfile profile;
    profile.id = printerName.isEmpty()
        ? QStringLiteral("profile-default")
        : TemplateDesignerFactory::createDeviceProfileId();
    profile.printerName = printerName;
    profile.dpi = m_deviceProfileDpiSpin->value();
    profile.scaleX = m_deviceProfileScaleXSpin->value();
    profile.scaleY = m_deviceProfileScaleYSpin->value();
    profile.offsetX = m_deviceProfileOffsetXSpin->value();
    profile.offsetY = m_deviceProfileOffsetYSpin->value();
    profile.notes = QString::fromUtf8("模板设计器校准");
    if (m_deviceCalibrationExpectedWidthSpin != nullptr
        && m_deviceCalibrationExpectedHeightSpin != nullptr
        && m_deviceCalibrationActualWidthSpin != nullptr
        && m_deviceCalibrationActualHeightSpin != nullptr) {
        profile.notes = QString::fromUtf8("校准测量：期望 %1x%2 mm，实际 %3x%4 mm")
                            .arg(m_deviceCalibrationExpectedWidthSpin->value(), 0, 'f', 3)
                            .arg(m_deviceCalibrationExpectedHeightSpin->value(), 0, 'f', 3)
                            .arg(m_deviceCalibrationActualWidthSpin->value(), 0, 'f', 3)
                            .arg(m_deviceCalibrationActualHeightSpin->value(), 0, 'f', 3);
    }

    auto profileIt = std::find_if(document.deviceProfiles.begin(), document.deviceProfiles.end(), [&printerName](const auto& item) {
        return item.printerName.trimmed().compare(printerName, Qt::CaseInsensitive) == 0;
    });
    if (profileIt != document.deviceProfiles.end()) {
        // 同一打印机的校准只能保留一份；替换时保留原 id，避免 settings.json 中 profile 标识抖动。
        profile.id = profileIt->id;
        *profileIt = profile;
    } else {
        document.deviceProfiles.append(profile);
    }

    refreshPreview();
    m_statusLabel->setText(QString::fromUtf8("已保存设备校准"));
    notifySettingsChanged();
}

void TemplateDesignerWindow::refreshLayerList()
{
    ensureCurrentTemplateDocument();
    const auto& document = m_settings.templateDocuments[m_templateKey];
    const auto selectedLayerId = currentLayerId();

    const auto wasBlocked = m_layerList->blockSignals(true);
    m_layerList->clear();
    for (int index = 0; index < document.layers.size(); ++index) {
        const auto& layer = document.layers[index];
        auto displayName = layer.name.trimmed().isEmpty()
            ? TemplateDesignerFactory::nextLayerName(index)
            : layer.name.trimmed();
        QStringList suffixes;
        if (!layer.visible) {
            suffixes.append(QString::fromUtf8("隐藏"));
        }
        if (layer.locked) {
            suffixes.append(QString::fromUtf8("锁定"));
        }
        if (!suffixes.isEmpty()) {
            displayName = QStringLiteral("%1 (%2)").arg(displayName, suffixes.join(QStringLiteral(", ")));
        }
        auto* item = new QListWidgetItem(displayName, m_layerList);
        item->setData(Qt::UserRole, layer.id);
    }

    if (!selectLayer(selectedLayerId) && m_layerList->count() > 0) {
        m_layerList->setCurrentRow(0);
    }
    m_layerList->blockSignals(wasBlocked);
}

void TemplateDesignerWindow::refreshElementList()
{
    ensureCurrentTemplateDocument();
    auto selectedElementId = currentElementId();
    const auto* layer = currentLayer();

    const auto wasBlocked = m_elementList->blockSignals(true);
    m_elementList->clear();
    if (layer == nullptr) {
        m_elementList->blockSignals(wasBlocked);
        return;
    }

    const auto selectedItemInCurrentLayer =
        std::any_of(layer->elements.begin(), layer->elements.end(), [&selectedElementId](const auto& element) {
            return element.id == selectedElementId;
        })
        || std::any_of(layer->tables.begin(), layer->tables.end(), [&selectedElementId](const auto& table) {
               return table.id == selectedElementId;
           });
    if (!selectedItemInCurrentLayer) {
        selectedElementId.clear();
    }

    QList<sleekpr::core::TemplateElement> elements = layer->elements;
    std::stable_sort(elements.begin(), elements.end(), [](const auto& left, const auto& right) {
        return left.zIndex < right.zIndex;
    });

    for (int index = 0; index < elements.size(); ++index) {
        const auto& element = elements[index];
        auto* item = new QListWidgetItem(elementDisplayName(element, index), m_elementList);
        item->setData(Qt::UserRole, element.id);
        item->setData(Qt::UserRole + 1, TemplateDesignerFactory::elementTypeKey(element.type));
        item->setFlags(item->flags() | Qt::ItemIsEditable | Qt::ItemIsDragEnabled | Qt::ItemIsDropEnabled);
    }

    QList<sleekpr::core::TableElement> tables = layer->tables;
    std::stable_sort(tables.begin(), tables.end(), [](const auto& left, const auto& right) {
        return left.zIndex < right.zIndex;
    });

    for (int index = 0; index < tables.size(); ++index) {
        const auto& table = tables[index];
        auto* item = new QListWidgetItem(tableDisplayName(table, index), m_elementList);
        item->setData(Qt::UserRole, table.id);
        item->setData(Qt::UserRole + 1, QStringLiteral("table"));
        item->setFlags(item->flags() | Qt::ItemIsEditable | Qt::ItemIsDragEnabled | Qt::ItemIsDropEnabled);
    }

    // 刷新元素列表时只在当前图层内恢复选择，避免跨图层选择再次触发列表刷新。
    if (!selectElementInCurrentList(selectedElementId) && m_elementList->count() > 0) {
        m_elementList->setCurrentRow(0);
    }
    m_elementList->blockSignals(wasBlocked);
    refreshElementPropertyEditor();
    refreshTablePropertyEditor();
}

void TemplateDesignerWindow::refreshElementPropertyEditor()
{
    if (m_inspectorPanel == nullptr) {
        return;
    }

    ScopedBoolFlag updating(m_updatingPropertyEditors);
    const auto* element = currentElement();
    if (element == nullptr || currentSelectionIsTable()) {
        m_inspectorPanel->clearElementProperties();
        return;
    }

    m_inspectorPanel->setElementProperties(m_presenter.elementPropertyModel(*element, canEditElement(element->id)));
}

void TemplateDesignerWindow::refreshTablePropertyEditor()
{
    if (m_inspectorPanel == nullptr) {
        return;
    }

    // 表格属性面板只服务当前选中的表格；选中普通元素时清空，避免误把普通元素写成表格配置。
    ScopedBoolFlag updating(m_updatingPropertyEditors);
    const auto* table = currentTable();
    if (table == nullptr) {
        m_inspectorPanel->clearTableProperties();
        return;
    }

    m_inspectorPanel->setTableProperties(m_presenter.tablePropertyModel(*table, canEditElement(table->id)));
}

void TemplateDesignerWindow::refreshPaperSpecSelector()
{
    if (m_paperSpecCombo == nullptr) {
        return;
    }

    ensureCurrentTemplateDocument();
    auto& document = m_settings.templateDocuments[m_templateKey];
    if (document.paperSpecId.trimmed().isEmpty()) {
        document.paperSpecId = QString::fromLatin1(kDefaultPaperSpecId);
    }

    QString errorMessage;
    auto specs = paperSpecFilePath().trimmed().isEmpty()
        ? QList<sleekpr::core::PaperSpec>{}
        : sleekpr::core::PaperSpecStore(paperSpecFilePath()).paperSpecs(&errorMessage);

    const auto hasDefaultSpec = std::any_of(specs.cbegin(), specs.cend(), [](const auto& spec) {
        return spec.id == QString::fromLatin1(kDefaultPaperSpecId);
    });
    if (!hasDefaultSpec) {
        sleekpr::core::PaperSpec defaultSpec;
        defaultSpec.id = QString::fromLatin1(kDefaultPaperSpecId);
        defaultSpec.name = QString::fromUtf8("80x30 标签");
        defaultSpec.widthMm = defaultPaperSizeMm().width();
        defaultSpec.heightMm = defaultPaperSizeMm().height();
        defaultSpec.defaultDpi = 203.0;
        specs.prepend(defaultSpec);
    }

    const QSignalBlocker blocker(m_paperSpecCombo);
    m_paperSpecCombo->clear();
    for (const auto& spec : specs) {
        m_paperSpecCombo->addItem(paperSpecDisplayName(spec), spec.id);
    }

    auto selectedIndex = m_paperSpecCombo->findData(document.paperSpecId);
    if (selectedIndex < 0) {
        selectedIndex = m_paperSpecCombo->findData(QString::fromLatin1(kDefaultPaperSpecId));
    }
    if (selectedIndex >= 0) {
        m_paperSpecCombo->setCurrentIndex(selectedIndex);
    }

    if (!errorMessage.trimmed().isEmpty() && m_statusLabel != nullptr) {
        m_statusLabel->setText(errorMessage);
    }
}

void TemplateDesignerWindow::refreshPreview()
{
    ensureCurrentTemplateDocument();
    if (m_previewLabel == nullptr) {
        return;
    }
    if (m_previewRefreshTimer != nullptr && m_previewRefreshTimer->isActive()) {
        m_previewRefreshTimer->stop();
    }

    const auto labelItem = sleekpr::infrastructure::PreviewLabelFactory::createDemoLabel(
        sleekpr::core::LabelTemplateKey::Default80x30);
    auto labelPlan = sleekpr::core::LabelRenderPlanner().createPlan(labelItem);
    const auto paperSize = currentPaperSizeMm();
    labelPlan.paperSize = sleekpr::core::LabelPaperSize(paperSize.width(), paperSize.height());
    const auto& document = m_settings.templateDocuments[m_templateKey];
    const auto printerName = m_deviceProfilePrinterEdit == nullptr ? QString() : m_deviceProfilePrinterEdit->text();
    const auto profile = sleekpr::core::DeviceProfileResolver::resolve(document.deviceProfiles, printerName);
    bool sampleDataOk = true;
    QString sampleDataErrorMessage;
    const auto renderContext = sampleRenderContext(&sampleDataOk, &sampleDataErrorMessage);
    const auto drawingPlan = sleekpr::core::TemplateDocumentRenderer().render(
        document,
        labelPlan,
        m_settings.labelOffset,
        profile,
        renderContext);
    m_previewCommands = drawingPlan.commands;

    const auto previewDpi = drawingPlan.renderDpi > 0.0
        ? std::min(drawingPlan.renderDpi, kDesignerInteractivePreviewDpi)
        : kDesignerInteractivePreviewDpi;
    const auto image = sleekpr::infrastructure::LabelPreviewImageRenderer().renderImage(drawingPlan, previewDpi);
    auto previewPixmap = QPixmap::fromImage(image);
    if (std::abs(m_previewZoomFactor - 1.0) > 0.0001) {
        const auto scaledSize = QSize(
            std::max(1, static_cast<int>(std::round(image.width() * m_previewZoomFactor))),
            std::max(1, static_cast<int>(std::round(image.height() * m_previewZoomFactor))));
        previewPixmap = previewPixmap.scaled(scaledSize, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    }
    m_previewLabel->setPixmap(previewPixmap);
    m_previewLabel->setRulerPaperSizeMm(paperSize);
    m_previewLabel->setDesignAidCommands(m_previewCommands);
    m_previewLabel->setDesignAidSelectedElementKey(currentElementId());
    const auto previewOrigin = m_previewLabel->printableImageOriginPx();
    // 预览控件尺寸包含外置标尺边距，打印图层本身仍保持渲染图原始比例，避免标签变形。
    m_previewLabel->setFixedSize(previewPixmap.width() + previewOrigin.x(), previewPixmap.height() + previewOrigin.y());

    const auto validation = sleekpr::core::TemplateDocumentValidator().validate(document, paperSize);
    if (!sampleDataOk && m_statusLabel != nullptr) {
        updateSampleDataErrorLabel(sampleDataErrorMessage);
        m_statusLabel->setText(sampleDataErrorMessage);
    } else if (validation.hasErrors() && m_statusLabel != nullptr) {
        updateSampleDataErrorLabel(QString());
        m_statusLabel->setText(QString::fromUtf8("模板校验失败：%1").arg(validation.firstErrorMessage()));
    } else {
        updateSampleDataErrorLabel(QString());
    }
}

void TemplateDesignerWindow::refreshAll()
{
    const auto layerId = currentLayerId();
    const auto elementId = currentElementId();
    refreshSampleDataEditor();
    refreshPaperSpecSelector();
    refreshLayerList();
    selectLayer(layerId);
    refreshElementList();
    selectElement(elementId);
    refreshPreview();
}

void TemplateDesignerWindow::schedulePreviewRefresh(int delayMs)
{
    if (m_previewRefreshTimer == nullptr) {
        refreshPreview();
        return;
    }

    // 自动应用可能在短时间内连续回写多个字段，预览渲染统一合并到队列尾部执行。
    m_previewRefreshTimer->start(std::max(0, delayMs));
}

void TemplateDesignerWindow::scheduleSelectionPropertyRefresh(int delayMs)
{
    if (m_propertyRefreshTimer == nullptr) {
        if (currentSelectionIsTable()) {
            refreshTablePropertyEditor();
            return;
        }
        refreshElementPropertyEditor();
        return;
    }

    // 拖动过程中只合并刷新当前属性页，避免每个鼠标事件都重写整组编辑控件。
    m_propertyRefreshTimer->start(std::max(0, delayMs));
}

void TemplateDesignerWindow::scheduleSettingsChanged(int delayMs)
{
    if (m_settingsChangedTimer == nullptr) {
        notifySettingsChanged();
        return;
    }

    // 拖拽和键盘微调会高频修改坐标，设置回调与撤销快照统一合并，避免每一帧都序列化模板。
    m_settingsChangedTimer->start(std::max(0, delayMs));
}

void TemplateDesignerWindow::applySelectedPaperSpec()
{
    if (m_paperSpecCombo == nullptr || m_paperSpecCombo->currentIndex() < 0) {
        return;
    }

    ensureCurrentTemplateDocument();
    const auto paperSpecId = m_paperSpecCombo->currentData().toString().trimmed();
    if (paperSpecId.isEmpty()) {
        return;
    }

    auto& document = m_settings.templateDocuments[m_templateKey];
    if (document.paperSpecId == paperSpecId) {
        return;
    }

    document.paperSpecId = paperSpecId;
    refreshPreview();
    if (m_statusLabel != nullptr) {
        m_statusLabel->setText(QString::fromUtf8("已切换纸张规格"));
    }
    notifySettingsChanged();
}

void TemplateDesignerWindow::refreshSampleDataEditor()
{
    ensureCurrentTemplateDocument();
    if (m_sampleDataEdit == nullptr) {
        return;
    }

    const auto& document = m_settings.templateDocuments[m_templateKey];
    const auto sampleData = document.sampleData.isEmpty()
        ? TemplateDesignerFactory::createDefaultSampleData()
        : document.sampleData;
    const QSignalBlocker blocker(m_sampleDataEdit);
    m_sampleDataEdit->setPlainText(sampleDataJson(sampleData));
    updateSampleDataErrorLabel(QString());
}

void TemplateDesignerWindow::notifySettingsChanged()
{
    if (m_settingsChangedTimer != nullptr && m_settingsChangedTimer->isActive()) {
        m_settingsChangedTimer->stop();
    }
    if (m_onSettingsChanged) {
        m_onSettingsChanged(m_settings);
    }
    if (!m_state.isRestoringDocumentHistory()) {
        rememberCurrentDocumentHistory();
    } else {
        updateHistoryButtons();
    }
}

bool TemplateDesignerWindow::loadTemplateDocumentFromFile(
    const QString& path,
    sleekpr::core::TemplateDocument* document,
    QString* errorMessage) const
{
    if (document == nullptr || path.trimmed().isEmpty()) {
        if (errorMessage != nullptr) {
            *errorMessage = QString::fromUtf8("模板路径不能为空");
        }
        return false;
    }

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (errorMessage != nullptr) {
            *errorMessage = QString::fromUtf8("模板文件读取失败");
        }
        return false;
    }

    QJsonParseError parseError;
    const auto json = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !json.isObject()) {
        if (errorMessage != nullptr) {
            *errorMessage = QString::fromUtf8("模板 JSON 格式无效");
        }
        return false;
    }

    QString validationError;
    if (!sleekpr::core::TemplateDocumentJson::validateForImport(json.object(), &validationError)) {
        if (errorMessage != nullptr) {
            *errorMessage = validationError;
        }
        return false;
    }

    *document = sleekpr::core::TemplateDocumentJson::fromJson(json.object());
    return true;
}

void TemplateDesignerWindow::applyImportedTemplateDocument(const sleekpr::core::TemplateDocument& document)
{
    auto documentToApply = document;
    // 设计器总是编辑当前模板槽位，导入文件的 templateKey 只作为来源信息校验，不改变 settings 的映射键。
    documentToApply.templateKey = m_templateKey;
    m_settings.templateDocuments[m_templateKey] = documentToApply;
    refreshAll();
}

QString TemplateDesignerWindow::paperSpecFilePath() const
{
    if (m_templateLibraryDirectoryPath.trimmed().isEmpty()) {
        return QString();
    }

    return QFileInfo(m_templateLibraryDirectoryPath).absoluteDir().filePath(QStringLiteral("paper-specs.json"));
}

QSizeF TemplateDesignerWindow::currentPaperSizeMm() const
{
    const auto spec = currentPaperSpec();
    if (spec.has_value()) {
        return QSizeF(spec->widthMm, spec->heightMm);
    }

    return defaultPaperSizeMm();
}

std::optional<sleekpr::core::PaperSpec> TemplateDesignerWindow::currentPaperSpec() const
{
    const_cast<TemplateDesignerWindow*>(this)->ensureCurrentTemplateDocument();
    const auto documentIt = m_settings.templateDocuments.constFind(m_templateKey);
    auto paperSpecId = documentIt == m_settings.templateDocuments.constEnd()
        ? QString::fromLatin1(kDefaultPaperSpecId)
        : documentIt.value().paperSpecId.trimmed();
    if (paperSpecId.isEmpty()) {
        paperSpecId = QString::fromLatin1(kDefaultPaperSpecId);
    }

    const auto path = paperSpecFilePath();
    if (!path.trimmed().isEmpty()) {
        const auto spec = sleekpr::core::PaperSpecStore(path).loadPaperSpec(paperSpecId);
        if (spec.has_value() && spec->widthMm > 0.0 && spec->heightMm > 0.0) {
            return spec;
        }
    }

    return sleekpr::core::builtInPaperSpec(paperSpecId);
}

sleekpr::core::NativePrintDrawingPlan TemplateDesignerWindow::createCurrentPrintPlan(
    const sleekpr::core::TemplateRenderContext& renderContext,
    const QString& printerName) const
{
    const_cast<TemplateDesignerWindow*>(this)->ensureCurrentTemplateDocument();

    const auto labelItem = sleekpr::infrastructure::PreviewLabelFactory::createDemoLabel(
        sleekpr::core::LabelTemplateKey::Default80x30);
    auto labelPlan = sleekpr::core::LabelRenderPlanner().createPlan(labelItem);
    const auto paperSpec = currentPaperSpec();
    const auto paperSize = paperSpec.has_value()
        ? QSizeF(paperSpec->widthMm, paperSpec->heightMm)
        : currentPaperSizeMm();
    labelPlan.paperSize = sleekpr::core::LabelPaperSize(paperSize.width(), paperSize.height());

    const auto documentIt = m_settings.templateDocuments.constFind(m_templateKey);
    if (documentIt == m_settings.templateDocuments.constEnd()) {
        return {};
    }

    auto labelOffset = m_settings.labelOffset;
    if (paperSpec.has_value()) {
        // 预打印与 HTTP 模板打印保持一致：纸张左/上边距合入整体偏移，不改写模板内元素坐标。
        labelOffset.x += paperSpec->marginLeftMm;
        labelOffset.y += paperSpec->marginTopMm;
    }

    auto profile = sleekpr::core::DeviceProfileResolver::resolve(documentIt.value().deviceProfiles, printerName);
    if (paperSpec.has_value() && (documentIt.value().deviceProfiles.isEmpty() || profile.printerName.trimmed().isEmpty())) {
        // 没有专用打印机 profile 时使用纸张规格默认 DPI，避免小标签按错误 DPI 渲染后被打印驱动缩放发糊。
        profile.dpi = paperSpec->defaultDpi;
    }

    return sleekpr::core::TemplateDocumentRenderer().renderPrint(
        documentIt.value(),
        labelPlan,
        labelOffset,
        profile,
        renderContext);
}

QJsonObject TemplateDesignerWindow::sampleDataFromEditor(bool* ok, QString* errorMessage) const
{
    if (ok != nullptr) {
        *ok = true;
    }
    if (errorMessage != nullptr) {
        errorMessage->clear();
    }

    if (m_sampleDataEdit == nullptr) {
        return {};
    }

    const auto sampleDataText = m_sampleDataEdit->toPlainText().trimmed();
    if (sampleDataText.isEmpty()) {
        return {};
    }

    QJsonParseError parseError;
    const auto document = QJsonDocument::fromJson(sampleDataText.toUtf8(), &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        const auto position = jsonTextPositionAtOffset(sampleDataText, parseError.offset);
        if (ok != nullptr) {
            *ok = false;
        }
        if (errorMessage != nullptr) {
            *errorMessage = QString::fromUtf8("模拟数据 JSON 解析失败（第 %1 行，第 %2 列）：%3")
                                .arg(position.line)
                                .arg(position.column)
                                .arg(parseError.errorString());
        }
        return {};
    }

    if (!document.isObject()) {
        if (ok != nullptr) {
            *ok = false;
        }
        if (errorMessage != nullptr) {
            *errorMessage = QString::fromUtf8("模拟数据 JSON 解析失败：根节点必须是对象");
        }
        return {};
    }

    return document.object();
}

bool TemplateDesignerWindow::syncSampleDataFromEditor(bool* ok, QString* errorMessage)
{
    ensureCurrentTemplateDocument();
    bool parseOk = true;
    QString parseErrorMessage;
    const auto sampleData = sampleDataFromEditor(&parseOk, &parseErrorMessage);
    if (ok != nullptr) {
        *ok = parseOk;
    }
    if (errorMessage != nullptr) {
        *errorMessage = parseErrorMessage;
    }
    if (!parseOk) {
        return false;
    }

    auto& document = m_settings.templateDocuments[m_templateKey];
    if (document.sampleData != sampleData) {
        // 样例数据跟随模板保存，但只参与设计器预览；正式打印仍由接口 values 提供真实数据。
        document.sampleData = sampleData;
        notifySettingsChanged();
    }
    return true;
}

sleekpr::core::TemplateRenderContext TemplateDesignerWindow::sampleRenderContext(bool* ok, QString* errorMessage) const
{
    sleekpr::core::TemplateRenderContext context;
    bool parseOk = true;
    QString parseErrorMessage;
    const auto sampleData = sampleDataFromEditor(&parseOk, &parseErrorMessage);
    if (ok != nullptr) {
        *ok = parseOk;
    }
    if (errorMessage != nullptr) {
        *errorMessage = parseErrorMessage;
    }
    if (!parseOk) {
        return context;
    }

    // 设计器预览只读取模板绑定的样例数据；正式打印链路不会读取 sampleData。
    context.values = sampleData;
    return context;
}

bool TemplateDesignerWindow::selectLayer(const QString& layerId)
{
    if (layerId.isEmpty()) {
        return false;
    }

    for (int row = 0; row < m_layerList->count(); ++row) {
        if (m_layerList->item(row)->data(Qt::UserRole).toString() == layerId) {
            m_layerList->setCurrentRow(row);
            return true;
        }
    }
    return false;
}

bool TemplateDesignerWindow::selectElement(const QString& elementId)
{
    if (elementId.isEmpty()) {
        return false;
    }

    auto& document = m_settings.templateDocuments[m_templateKey];
    QString targetLayerId;
    for (const auto& layer : document.layers) {
        for (const auto& element : layer.elements) {
            if (element.id == elementId) {
                targetLayerId = layer.id;
                break;
            }
        }
        for (const auto& table : layer.tables) {
            if (table.id == elementId) {
                targetLayerId = layer.id;
                break;
            }
        }
        if (!targetLayerId.isEmpty()) {
            break;
        }
    }

    if (!targetLayerId.isEmpty() && targetLayerId != currentLayerId()) {
        selectLayer(targetLayerId);
        refreshElementList();
    }

    return selectElementInCurrentList(elementId);
}

bool TemplateDesignerWindow::selectElementInCurrentList(const QString& elementId)
{
    if (elementId.isEmpty()) {
        return false;
    }

    for (int row = 0; row < m_elementList->count(); ++row) {
        if (m_elementList->item(row)->data(Qt::UserRole).toString() == elementId) {
            m_elementList->setCurrentRow(row);
            refreshInspectorTabForCurrentSelection();
            return true;
        }
    }
    return false;
}

bool TemplateDesignerWindow::selectTemplateInLibraryList(const QString& templateId)
{
    if (m_templateLibraryList == nullptr || templateId.trimmed().isEmpty()) {
        return false;
    }

    for (int row = 0; row < m_templateLibraryList->count(); ++row) {
        if (m_templateLibraryList->item(row)->data(Qt::UserRole).toString() == templateId) {
            m_templateLibraryList->setCurrentRow(row);
            return true;
        }
    }
    return false;
}

bool TemplateDesignerWindow::canEditElement(const QString& elementId) const
{
    const_cast<TemplateDesignerWindow*>(this)->ensureCurrentTemplateDocument();
    const auto& document = m_settings.templateDocuments[m_templateKey];
    for (const auto& layer : document.layers) {
        for (const auto& element : layer.elements) {
            if (element.id == elementId) {
                return layer.visible && !layer.locked && element.visible && !element.locked;
            }
        }
        for (const auto& table : layer.tables) {
            if (table.id == elementId) {
                return layer.visible && !layer.locked && table.visible && !table.locked;
            }
        }
    }
    return false;
}

bool TemplateDesignerWindow::currentSelectionIsTable() const
{
    if (m_elementList == nullptr || m_elementList->currentItem() == nullptr) {
        return false;
    }
    return m_elementList->currentItem()->data(Qt::UserRole + 1).toString() == QStringLiteral("table");
}

void TemplateDesignerWindow::renameElementFromListItem(QListWidgetItem* item)
{
    if (item == nullptr || m_elementList == nullptr || m_applyingElementListOrder) {
        return;
    }

    ensureCurrentTemplateDocument();
    const auto itemId = item->data(Qt::UserRole).toString();
    const auto itemType = item->data(Qt::UserRole + 1).toString();
    const auto displayName = item->text().trimmed();
    if (itemId.trimmed().isEmpty() || displayName.isEmpty()) {
        refreshElementList();
        selectElement(itemId);
        return;
    }

    auto& document = m_settings.templateDocuments[m_templateKey];
    for (auto& layer : document.layers) {
        if (itemType == QStringLiteral("table")) {
            for (auto& table : layer.tables) {
                if (table.id == itemId && !layer.locked && !table.locked) {
                    table.displayName = displayName;
                    refreshElementList();
                    selectElement(itemId);
                    if (m_statusLabel != nullptr) {
                        m_statusLabel->setText(QString::fromUtf8("已重命名元素"));
                    }
                    notifySettingsChanged();
                    return;
                }
            }
            continue;
        }

        for (auto& element : layer.elements) {
            if (element.id == itemId && !layer.locked && !element.locked) {
                element.displayName = displayName;
                refreshElementList();
                selectElement(itemId);
                if (m_statusLabel != nullptr) {
                    m_statusLabel->setText(QString::fromUtf8("已重命名元素"));
                }
                notifySettingsChanged();
                return;
            }
        }
    }

    refreshElementList();
    selectElement(itemId);
}

void TemplateDesignerWindow::beginRenameCurrentElement()
{
    if (m_elementList == nullptr || m_elementList->currentItem() == nullptr) {
        return;
    }
    m_elementList->editItem(m_elementList->currentItem());
}

void TemplateDesignerWindow::applyElementListOrderFromList()
{
    if (m_elementList == nullptr || m_applyingElementListOrder) {
        return;
    }

    ScopedBoolFlag applying(m_applyingElementListOrder);
    ensureCurrentTemplateDocument();
    auto* layer = currentLayer();
    if (layer == nullptr || layer->locked) {
        refreshElementList();
        return;
    }

    QList<QString> elementOrder;
    QList<QString> tableOrder;
    // 当前渲染模型中普通元素和表格分别维护层内顺序，拖拽排序也按这两个集合分别写回。
    for (int row = 0; row < m_elementList->count(); ++row) {
        const auto* item = m_elementList->item(row);
        if (item == nullptr) {
            continue;
        }
        const auto itemId = item->data(Qt::UserRole).toString();
        if (item->data(Qt::UserRole + 1).toString() == QStringLiteral("table")) {
            tableOrder.append(itemId);
        } else {
            elementOrder.append(itemId);
        }
    }

    const auto reorderElements = [&elementOrder](QList<sleekpr::core::TemplateElement>& elements) {
        QList<sleekpr::core::TemplateElement> reordered;
        for (const auto& id : elementOrder) {
            const auto it = std::find_if(elements.begin(), elements.end(), [&id](const auto& element) {
                return element.id == id;
            });
            if (it != elements.end()) {
                reordered.append(*it);
            }
        }
        for (const auto& element : elements) {
            const auto alreadyAdded = std::any_of(reordered.begin(), reordered.end(), [&element](const auto& current) {
                return current.id == element.id;
            });
            if (!alreadyAdded) {
                reordered.append(element);
            }
        }
        const bool sameOrder = reordered.size() == elements.size()
            && std::equal(reordered.begin(), reordered.end(), elements.begin(), [](const auto& left, const auto& right) {
                   return left.id == right.id;
               });
        const bool changed = !sameOrder;
        if (!changed) {
            return false;
        }
        elements = reordered;
        for (int index = 0; index < elements.size(); ++index) {
            elements[index].zIndex = index;
        }
        return true;
    };

    const auto reorderTables = [&tableOrder](QList<sleekpr::core::TableElement>& tables) {
        QList<sleekpr::core::TableElement> reordered;
        for (const auto& id : tableOrder) {
            const auto it = std::find_if(tables.begin(), tables.end(), [&id](const auto& table) {
                return table.id == id;
            });
            if (it != tables.end()) {
                reordered.append(*it);
            }
        }
        for (const auto& table : tables) {
            const auto alreadyAdded = std::any_of(reordered.begin(), reordered.end(), [&table](const auto& current) {
                return current.id == table.id;
            });
            if (!alreadyAdded) {
                reordered.append(table);
            }
        }
        const bool sameOrder = reordered.size() == tables.size()
            && std::equal(reordered.begin(), reordered.end(), tables.begin(), [](const auto& left, const auto& right) {
                   return left.id == right.id;
               });
        const bool changed = !sameOrder;
        if (!changed) {
            return false;
        }
        tables = reordered;
        for (int index = 0; index < tables.size(); ++index) {
            tables[index].zIndex = index;
        }
        return true;
    };

    const auto selectedElementId = currentElementId();
    const bool changed = reorderElements(layer->elements) || reorderTables(layer->tables);
    if (!changed) {
        return;
    }

    refreshPreview();
    selectElement(selectedElementId);
    if (m_statusLabel != nullptr) {
        m_statusLabel->setText(QString::fromUtf8("已调整元素顺序"));
    }
    notifySettingsChanged();
}

void TemplateDesignerWindow::refreshInspectorTabForCurrentSelection()
{
    auto* inspectorTabs = findChild<QTabWidget*>(QStringLiteral("designerInspectorTabs"));
    if (inspectorTabs == nullptr) {
        return;
    }

    QString targetTabObjectName;
    if (currentTable() != nullptr) {
        targetTabObjectName = QStringLiteral("designerTableInspectorTab");
    } else if (const auto* element = currentElement(); element != nullptr) {
        // 画板或图层列表选中对象后，右侧检查器自动跳到该元素最常用的配置页。
        targetTabObjectName = element->type == sleekpr::core::TemplateElementType::ArrayGrid
            ? QStringLiteral("designerArrayGridInspectorTab")
            : QStringLiteral("designerElementInspectorTab");
    }

    if (targetTabObjectName.isEmpty()) {
        return;
    }

    if (auto* targetTab = findChild<QWidget*>(targetTabObjectName); targetTab != nullptr) {
        inspectorTabs->setCurrentWidget(targetTab);
    }
}

QString TemplateDesignerWindow::currentLayerId() const
{
    if (m_layerList == nullptr || m_layerList->currentItem() == nullptr) {
        return QString();
    }
    return m_layerList->currentItem()->data(Qt::UserRole).toString();
}

QString TemplateDesignerWindow::currentElementId() const
{
    if (m_elementList == nullptr || m_elementList->currentItem() == nullptr) {
        return QString();
    }
    return m_elementList->currentItem()->data(Qt::UserRole).toString();
}

sleekpr::core::TemplateLayer* TemplateDesignerWindow::currentLayer()
{
    ensureCurrentTemplateDocument();
    auto& document = m_settings.templateDocuments[m_templateKey];
    const auto layerId = currentLayerId();
    for (auto& layer : document.layers) {
        if (layer.id == layerId) {
            return &layer;
        }
    }
    return nullptr;
}

const sleekpr::core::TemplateLayer* TemplateDesignerWindow::currentLayer() const
{
    const_cast<TemplateDesignerWindow*>(this)->ensureCurrentTemplateDocument();
    const auto& document = m_settings.templateDocuments[m_templateKey];
    const auto layerId = currentLayerId();
    for (const auto& layer : document.layers) {
        if (layer.id == layerId) {
            return &layer;
        }
    }
    return nullptr;
}

sleekpr::core::TemplateElement* TemplateDesignerWindow::currentElement()
{
    auto* layer = currentLayer();
    if (layer == nullptr) {
        return nullptr;
    }

    const auto elementId = currentElementId();
    for (auto& element : layer->elements) {
        if (element.id == elementId) {
            return &element;
        }
    }
    return nullptr;
}

sleekpr::core::TableElement* TemplateDesignerWindow::currentTable()
{
    auto* layer = currentLayer();
    if (layer == nullptr) {
        return nullptr;
    }

    const auto tableId = currentElementId();
    for (auto& table : layer->tables) {
        if (table.id == tableId) {
            return &table;
        }
    }
    return nullptr;
}

void TemplateDesignerWindow::addElement(sleekpr::core::TemplateElement element)
{
    ensureCurrentTemplateDocument();
    auto& document = m_settings.templateDocuments[m_templateKey];
    const auto layerId = currentLayerId();
    auto* layer = currentLayer();
    if (layer == nullptr) {
        return;
    }
    element.zIndex = layer->elements.size();
    const auto elementId = element.id;
    if (!sleekpr::core::TemplateDocumentEditModel::addElement(document, layerId, element)) {
        m_statusLabel->setText(QString::fromUtf8("当前图层不可编辑"));
        return;
    }

    refreshAll();
    selectElement(elementId);
    m_statusLabel->setText(QString::fromUtf8("已新增元素"));
    notifySettingsChanged();
}

void TemplateDesignerWindow::addTable(sleekpr::core::TableElement table)
{
    ensureCurrentTemplateDocument();
    auto& document = m_settings.templateDocuments[m_templateKey];
    const auto layerId = currentLayerId();
    auto* layer = currentLayer();
    if (layer == nullptr) {
        return;
    }
    table.zIndex = layer->tables.size();
    const auto tableId = table.id;
    if (!sleekpr::core::TemplateDocumentEditModel::addTable(document, layerId, table)) {
        m_statusLabel->setText(QString::fromUtf8("当前图层不可编辑"));
        return;
    }

    refreshAll();
    selectElement(tableId);
    m_statusLabel->setText(QString::fromUtf8("已新增表格"));
    notifySettingsChanged();
}

bool TemplateDesignerWindow::applyCurrentElementProperties(bool lightweightRefresh)
{
    auto* element = currentElement();
    if (element == nullptr || currentSelectionIsTable() || m_inspectorPanel == nullptr) {
        m_statusLabel->setText(QString::fromUtf8("请先选择固定文本或绑定字段"));
        return false;
    }
    const auto canEdit = canEditElement(element->id);
    if (!canEdit) {
        m_statusLabel->setText(QString::fromUtf8("当前元素不可编辑"));
        return false;
    }

    const auto elementId = element->id;
    auto model = m_inspectorPanel->elementProperties();
    model.elementId = elementId;
    model.canEdit = canEdit;
    const auto result = m_presenter.applyElementProperties(*element, model);
    if (!result.errorMessage.isEmpty()) {
        m_statusLabel->setText(result.errorMessage);
        return false;
    }
    if (!result.changed) {
        return false;
    }

    // 自动应用只刷新画布，避免重建列表和属性面板导致输入卡顿；手动应用保留完整刷新。
    if (lightweightRefresh) {
        schedulePreviewRefresh(kPreviewRefreshDelayMs);
    } else {
        refreshAll();
        selectElement(elementId);
    }
    m_statusLabel->setText(QString::fromUtf8("已更新元素属性"));
    notifySettingsChanged();
    return true;
}

bool TemplateDesignerWindow::applyCurrentTableProperties(bool lightweightRefresh)
{
    auto* table = currentTable();
    if (table == nullptr || m_inspectorPanel == nullptr) {
        m_statusLabel->setText(QString::fromUtf8("请先选择表格"));
        return false;
    }
    const auto canEdit = canEditElement(table->id);
    if (!canEdit) {
        m_statusLabel->setText(QString::fromUtf8("当前表格不可编辑"));
        return false;
    }

    auto& document = m_settings.templateDocuments[m_templateKey];
    const auto tableId = table->id;
    auto model = m_inspectorPanel->tableProperties();
    model.tableId = tableId;
    model.canEdit = canEdit;
    const auto result = m_presenter.applyTableProperties(document, tableId, model);
    if (!result.errorMessage.isEmpty()) {
        m_statusLabel->setText(result.errorMessage);
        return false;
    }
    if (!result.changed) {
        return false;
    }

    // 自动应用表格属性时同样只刷新画布，减少列配置和尺寸微调时的 UI 抖动。
    if (lightweightRefresh) {
        schedulePreviewRefresh(kPreviewRefreshDelayMs);
    } else {
        refreshAll();
        selectElement(tableId);
    }
    m_statusLabel->setText(QString::fromUtf8("已更新表格属性"));
    notifySettingsChanged();
    return true;
}

void TemplateDesignerWindow::scheduleElementAutoApply(int delayMs)
{
    if (m_updatingPropertyEditors
        || m_inspectorPanel == nullptr
        || currentSelectionIsTable()
        || currentElement() == nullptr
        || m_elementAutoApplyTimer == nullptr) {
        return;
    }
    m_elementAutoApplyTimer->start(std::max(0, delayMs));
}

void TemplateDesignerWindow::scheduleTableAutoApply(int delayMs)
{
    if (m_updatingPropertyEditors
        || m_inspectorPanel == nullptr
        || !currentSelectionIsTable()
        || currentTable() == nullptr
        || m_tableAutoApplyTimer == nullptr) {
        return;
    }
    m_tableAutoApplyTimer->start(std::max(0, delayMs));
}

void TemplateDesignerWindow::applyPendingElementAutoApply()
{
    if (m_elementAutoApplyTimer == nullptr || !m_elementAutoApplyTimer->isActive()) {
        return;
    }
    m_elementAutoApplyTimer->stop();
    applyCurrentElementProperties(true);
}

void TemplateDesignerWindow::applyPendingTableAutoApply()
{
    if (m_tableAutoApplyTimer == nullptr || !m_tableAutoApplyTimer->isActive()) {
        return;
    }
    m_tableAutoApplyTimer->stop();
    applyCurrentTableProperties(true);
}

bool TemplateDesignerWindow::isElementAutoApplyEditor(QObject* watched) const
{
    return m_inspectorPanel != nullptr && m_inspectorPanel->isElementPropertyEditor(watched);
}

bool TemplateDesignerWindow::isTableAutoApplyEditor(QObject* watched) const
{
    return m_inspectorPanel != nullptr && m_inspectorPanel->isTablePropertyEditor(watched);
}

void TemplateDesignerWindow::updateSampleDataErrorLabel(const QString& errorMessage)
{
    if (m_sampleDataErrorLabel == nullptr) {
        return;
    }

    const auto normalized = errorMessage.trimmed();
    m_sampleDataErrorLabel->setText(normalized);
    m_sampleDataErrorLabel->setVisible(!normalized.isEmpty());
}

void TemplateDesignerWindow::resetDocumentHistory()
{
    ensureCurrentTemplateDocument();
    m_state.resetDocumentHistory(m_settings.templateDocuments[m_templateKey]);
    updateHistoryButtons();
}

void TemplateDesignerWindow::rememberCurrentDocumentHistory()
{
    if (m_state.isRestoringDocumentHistory()) {
        updateHistoryButtons();
        return;
    }

    ensureCurrentTemplateDocument();
    m_state.rememberDocument(m_settings.templateDocuments[m_templateKey]);
    updateHistoryButtons();
}

void TemplateDesignerWindow::updateHistoryButtons()
{
    if (m_undoButton != nullptr) {
        m_undoButton->setEnabled(m_state.canUndo());
    }
    if (m_redoButton != nullptr) {
        m_redoButton->setEnabled(m_state.canRedo());
    }
}

std::optional<TemplateDesignerWindow::TableColumnResizeDrag> TemplateDesignerWindow::tableColumnResizeDragAt(QPoint position) const
{
    if (m_previewLabel == nullptr) {
        return std::nullopt;
    }

    const auto paperSize = currentPaperSizeMm();
    const auto previewSize = m_previewLabel->printableImageSizePx();
    if (paperSize.isEmpty() || previewSize.isEmpty()) {
        return std::nullopt;
    }

    const auto imagePosition = m_previewLabel->mapToPrintableImagePx(position);
    const auto millimetersPerPixelX = paperSize.width() / previewSize.width();
    const auto millimetersPerPixelY = paperSize.height() / previewSize.height();
    const QPointF positionMm(imagePosition.x() * millimetersPerPixelX, imagePosition.y() * millimetersPerPixelY);
    const auto toleranceMm = std::max(0.5, kColumnResizeHandleTolerancePx * millimetersPerPixelX);

    const auto& document = m_settings.templateDocuments.value(m_templateKey);
    for (const auto& layer : document.layers) {
        if (!layer.visible || layer.locked) {
            continue;
        }
        for (const auto& table : layer.tables) {
            if (!table.visible || table.locked || table.columns.size() < 2) {
                continue;
            }
            if (positionMm.y() < table.y || positionMm.y() > table.y + table.height) {
                continue;
            }
            if (positionMm.x() < table.x || positionMm.x() > table.x + table.width) {
                continue;
            }

            const auto widths = resolvedDesignerColumnWidths(table);
            double boundaryX = table.x;
            for (int columnIndex = 0; columnIndex < widths.size() - 1; ++columnIndex) {
                boundaryX += widths[columnIndex];
                if (std::abs(positionMm.x() - boundaryX) <= toleranceMm) {
                    return TableColumnResizeDrag{table.id, columnIndex};
                }
            }
        }
    }

    return std::nullopt;
}

bool TemplateDesignerWindow::currentSelectionContainsCanvasPosition(QPoint position) const
{
    if (m_previewLabel == nullptr) {
        return false;
    }

    const auto selectedId = currentElementId();
    if (selectedId.trimmed().isEmpty() || !canEditElement(selectedId)) {
        return false;
    }

    const auto paperSize = currentPaperSizeMm();
    const auto previewSize = m_previewLabel->printableImageSizePx();
    if (paperSize.isEmpty() || previewSize.isEmpty()) {
        return false;
    }

    const auto imagePosition = m_previewLabel->mapToPrintableImagePx(position);
    const QPointF positionMm(
        imagePosition.x() * paperSize.width() / previewSize.width(),
        imagePosition.y() * paperSize.height() / previewSize.height());

    const auto containsPosition = [&positionMm](double x, double y, double width, double height) {
        return QRectF(x, y, width, height).adjusted(-0.2, -0.2, 0.2, 0.2).contains(positionMm);
    };

    const auto& document = m_settings.templateDocuments.value(m_templateKey);
    for (const auto& layer : document.layers) {
        for (const auto& element : layer.elements) {
            if (element.id == selectedId) {
                return containsPosition(element.x, element.y, element.width, element.height);
            }
        }
        for (const auto& table : layer.tables) {
            if (table.id == selectedId) {
                return containsPosition(table.x, table.y, table.width, table.height);
            }
        }
    }

    return false;
}

void TemplateDesignerWindow::moveSelectedElementByPixels(QPoint delta)
{
    ensureCurrentTemplateDocument();
    if (currentSelectionIsTable()) {
        auto* table = currentTable();
        if (table == nullptr || m_previewLabel == nullptr) {
            return;
        }

        const TemplateDragCoordinateMapper mapper(currentPaperSizeMm(), m_previewLabel->printableImageSizePx());
        const auto tableId = table->id;
        const auto moved = mapper.movedTopLeftMm(
            QPointF(table->x, table->y),
            delta,
            QSizeF(table->width, table->height));

        auto& document = m_settings.templateDocuments[m_templateKey];
        if (sleekpr::core::TemplateDocumentEditModel::moveTable(document, tableId, moved.x(), moved.y())) {
            scheduleSelectionPropertyRefresh(kPreviewRefreshDelayMs);
            schedulePreviewRefresh(kPreviewRefreshDelayMs);
            scheduleSettingsChanged(kSettingsChangedDelayMs);
        }
        return;
    }

    auto* element = currentElement();
    if (element == nullptr || m_previewLabel == nullptr) {
        return;
    }

    const TemplateDragCoordinateMapper mapper(currentPaperSizeMm(), m_previewLabel->printableImageSizePx());
    const auto elementId = element->id;
    const auto moved = mapper.movedTopLeftMm(
        QPointF(element->x, element->y),
        delta,
        QSizeF(element->width, element->height));

    auto& document = m_settings.templateDocuments[m_templateKey];
    if (sleekpr::core::TemplateDocumentEditModel::moveElement(document, elementId, moved.x(), moved.y())) {
        scheduleSelectionPropertyRefresh(kPreviewRefreshDelayMs);
        schedulePreviewRefresh(kPreviewRefreshDelayMs);
        scheduleSettingsChanged(kSettingsChangedDelayMs);
    }
}

void TemplateDesignerWindow::resizeTableColumnByPixels(QPoint delta)
{
    if (!m_tableColumnResizeDrag.has_value() || m_previewLabel == nullptr || delta.x() == 0) {
        return;
    }

    ensureCurrentTemplateDocument();
    if (currentElementId() != m_tableColumnResizeDrag->tableId) {
        selectElement(m_tableColumnResizeDrag->tableId);
    }

    auto* table = currentTable();
    if (table == nullptr || table->id != m_tableColumnResizeDrag->tableId) {
        return;
    }

    const auto leftIndex = m_tableColumnResizeDrag->leftColumnIndex;
    const auto rightIndex = leftIndex + 1;
    if (leftIndex < 0 || rightIndex >= table->columns.size()) {
        return;
    }

    const auto paperSize = currentPaperSizeMm();
    const auto previewSize = m_previewLabel->printableImageSizePx();
    if (paperSize.isEmpty() || previewSize.isEmpty()) {
        return;
    }

    const auto widths = resolvedDesignerColumnWidths(*table);
    if (widths.size() != table->columns.size()) {
        return;
    }

    const auto deltaMm = delta.x() * paperSize.width() / previewSize.width();
    const auto combinedWidth = widths[leftIndex] + widths[rightIndex];
    const auto minLeft = std::max(kColumnResizeMinimumWidthMm, table->columns[leftIndex].minWidthMm);
    const auto minRight = std::max(kColumnResizeMinimumWidthMm, table->columns[rightIndex].minWidthMm);
    if (combinedWidth <= minLeft + minRight) {
        return;
    }

    const auto nextLeftWidth = std::clamp(widths[leftIndex] + deltaMm, minLeft, combinedWidth - minRight);
    const auto nextRightWidth = combinedWidth - nextLeftWidth;
    if (std::abs(nextLeftWidth - table->columns[leftIndex].widthMm) < 0.001
        && std::abs(nextRightWidth - table->columns[rightIndex].widthMm) < 0.001
        && table->columns[leftIndex].widthMode == sleekpr::core::TableColumnWidthMode::Fixed
        && table->columns[rightIndex].widthMode == sleekpr::core::TableColumnWidthMode::Fixed) {
        return;
    }

    // 画布列宽拖拽表示用户要固定当前相邻两列的毫米宽度，避免弹性列继续覆盖拖拽结果。
    table->columns[leftIndex].widthMode = sleekpr::core::TableColumnWidthMode::Fixed;
    table->columns[rightIndex].widthMode = sleekpr::core::TableColumnWidthMode::Fixed;
    table->columns[leftIndex].widthMm = nextLeftWidth;
    table->columns[rightIndex].widthMm = nextRightWidth;

    scheduleSelectionPropertyRefresh(kPreviewRefreshDelayMs);
    schedulePreviewRefresh(kPreviewRefreshDelayMs);
    scheduleSettingsChanged(kSettingsChangedDelayMs);
    if (m_statusLabel != nullptr) {
        m_statusLabel->setText(QString::fromUtf8("已调整表格列宽"));
    }
}

void TemplateDesignerWindow::nudgeSelectedElement(QPoint direction, Qt::KeyboardModifiers modifiers)
{
    if (currentSelectionIsTable()) {
        auto* table = currentTable();
        if (table == nullptr) {
            return;
        }

        const auto step = modifiers.testFlag(Qt::ShiftModifier) ? 1.0 : 0.1;
        auto& document = m_settings.templateDocuments[m_templateKey];
        const auto tableId = table->id;
        if (sleekpr::core::TemplateDocumentEditModel::moveTable(
                document,
                tableId,
                std::max(0.0, table->x + direction.x() * step),
                std::max(0.0, table->y + direction.y() * step))) {
            scheduleSelectionPropertyRefresh(kPreviewRefreshDelayMs);
            schedulePreviewRefresh(kPreviewRefreshDelayMs);
            scheduleSettingsChanged(kSettingsChangedDelayMs);
        }
        return;
    }

    auto* element = currentElement();
    if (element == nullptr) {
        return;
    }

    const auto step = modifiers.testFlag(Qt::ShiftModifier) ? 1.0 : 0.1;
    auto& document = m_settings.templateDocuments[m_templateKey];
    const auto elementId = element->id;
    if (sleekpr::core::TemplateDocumentEditModel::moveElement(
            document,
            elementId,
            std::max(0.0, element->x + direction.x() * step),
            std::max(0.0, element->y + direction.y() * step))) {
        scheduleSelectionPropertyRefresh(kPreviewRefreshDelayMs);
        schedulePreviewRefresh(kPreviewRefreshDelayMs);
        scheduleSettingsChanged(kSettingsChangedDelayMs);
    }
}

} // 命名空间 sleekpr::app

// Hallmark 设计复核：理念5、层级5、实现4、场景5、克制5、差异4；沿用原生标签工作区主题。
#include "sleekpr/app/PrintWorkbenchWindow.h"

#include "sleekpr/app/AppTheme.h"
#include "sleekpr/app/PrintFieldEditorPanel.h"
#include "sleekpr/app/PrintPreviewPanel.h"
#include "sleekpr/app/PrintPreflightPanel.h"
#include "sleekpr/app/TemplateFieldNames.h"
#include "sleekpr/core/printing/PrintCopyPlanner.h"
#include "sleekpr/core/native/NativePlanJsonSerializer.h"
#include "sleekpr/core/settings/FileSettingsStore.h"
#include "sleekpr/core/templates/TemplateRenderContextBuilder.h"
#include "sleekpr/core/templates/TemplateDocumentJson.h"
#include "sleekpr/infrastructure/printing/LabelPrintEngine.h"

#include <QComboBox>
#include <QDateTime>
#include <QEvent>
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QInputDialog>
#include <QLineEdit>
#include <QListWidget>
#include <QPrinterInfo>
#include <QPushButton>
#include <QScopedValueRollback>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QSplitter>
#include <QStyle>
#include <QTimer>
#include <QUuid>
#include <QVBoxLayout>

#include <algorithm>
#include <utility>

namespace sleekpr::app {

namespace {

bool samePreparedPlan(const sleekpr::infrastructure::PreparedTemplatePrint& left,
    const sleekpr::infrastructure::PreparedTemplatePrint& right)
{
    const auto printDescription = [](sleekpr::core::TemplateDocument document) {
        // 显示名称不影响输出，不能仅因改名要求再次确认；字段规则和实际绘制命令仍完整比较。
        document.fieldDisplayNames.clear();
        for (auto& field : document.fieldSchema) field.displayName.clear();
        return sleekpr::core::TemplateDocumentJson::toJson(document);
    };
    if (printDescription(left.document) != printDescription(right.document)
        || left.printPlan.renderDpi != right.printPlan.renderDpi || left.printPlan.pages.size() != right.printPlan.pages.size()) return false;
    const sleekpr::core::NativePlanJsonSerializer serializer;
    for (int index = 0; index < left.printPlan.pages.size(); ++index) {
        auto first = left.printPlan.firstPageAsLabelPlan();
        auto second = right.printPlan.firstPageAsLabelPlan();
        first.commands = left.printPlan.pages[index].commands;
        second.commands = right.printPlan.pages[index].commands;
        if (serializer.toJson(first) != serializer.toJson(second)) return false;
    }
    return true;
}

} // 匿名命名空间

PrintWorkbenchWindow::PrintWorkbenchWindow(QString settingsPath,
    sleekpr::infrastructure::LabelPrintEngine* printEngine, PrinterNamesProvider printers, QWidget* parent)
    : QWidget(parent), m_settingsPath(std::move(settingsPath)), m_service(m_settingsPath), m_printEngine(printEngine),
      m_printersProvider(std::move(printers))
{
    if (!m_printersProvider) m_printersProvider = [] { return QPrinterInfo::availablePrinterNames(); };
    setWindowTitle(QString::fromUtf8("sleekpr 打印工作台"));
    setMinimumSize(980, 640);
    resize(1280, 800);
    buildUi();
    reloadFromDisk();
}

void PrintWorkbenchWindow::setOpenDesignerCallback(std::function<void()> callback) { m_openDesigner = std::move(callback); }
void PrintWorkbenchWindow::setOpenSettingsCallback(std::function<void()> callback) { m_openSettings = std::move(callback); }

void PrintWorkbenchWindow::setOpenFieldPresetManagerCallback(std::function<void()> callback)
{
    m_openFieldPresetManager = std::move(callback);
    m_managePresetsButton->setEnabled(bool(m_openFieldPresetManager));
}

void PrintWorkbenchWindow::buildUi()
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);
    auto* header = new QWidget(this);
    header->setObjectName(QStringLiteral("workbenchHeader"));
    auto* headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(16, 10, 16, 10);
    auto* title = new QLabel(QString::fromUtf8("打印工作台"), header);
    title->setObjectName(QStringLiteral("workbenchTitle"));
    headerLayout->addWidget(title);
    headerLayout->addStretch();
    m_refreshButton = new QPushButton(header);
    m_refreshButton->setObjectName(QStringLiteral("workbenchRefreshButton"));
    configureIconButton(m_refreshButton, QStyle::SP_BrowserReload, QString::fromUtf8("刷新模板和打印机"));
    auto* designer = new QPushButton(QString::fromUtf8("标签设计器"), header);
    designer->setObjectName(QStringLiteral("workbenchOpenDesignerButton"));
    auto* settings = new QPushButton(header);
    settings->setObjectName(QStringLiteral("workbenchOpenSettingsButton"));
    configureIconButton(settings, QStyle::SP_FileDialogDetailedView, QString::fromUtf8("打印设置"));
    headerLayout->addWidget(m_refreshButton);
    headerLayout->addWidget(designer);
    headerLayout->addWidget(settings);
    root->addWidget(header);
    auto* splitter = new QSplitter(Qt::Horizontal, this);
    splitter->setObjectName(QStringLiteral("workbenchSplitter"));
    splitter->setChildrenCollapsible(false);
    m_templatePanel = new QWidget(splitter);
    m_templatePanel->setObjectName(QStringLiteral("workbenchTemplatePanel"));
    m_templatePanel->setMinimumWidth(180);
    m_templatePanel->setMaximumWidth(260);
    auto* library = new QVBoxLayout(m_templatePanel);
    library->setContentsMargins(12, 14, 12, 12);
    library->setSpacing(10);
    library->addWidget(new QLabel(QString::fromUtf8("模板"), m_templatePanel));
    m_search = new QLineEdit(m_templatePanel);
    m_search->setObjectName(QStringLiteral("workbenchTemplateSearch"));
    m_search->setPlaceholderText(QString::fromUtf8("搜索模板"));
    m_search->setClearButtonEnabled(true);
    library->addWidget(m_search);
    m_templateList = new QListWidget(m_templatePanel);
    m_templateList->setObjectName(QStringLiteral("workbenchTemplateList"));
    m_templateList->setTextElideMode(Qt::ElideRight);
    m_templateList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    library->addWidget(m_templateList, 1);
    auto* center = new QWidget(splitter);
    center->setMinimumWidth(300);
    auto* centerLayout = new QVBoxLayout(center);
    centerLayout->setContentsMargins(0, 0, 0, 0);
    centerLayout->setSpacing(0);
    m_preview = new PrintPreviewPanel(center);
    m_preflight = new PrintPreflightPanel(center);
    centerLayout->addWidget(m_preview, 1);
    centerLayout->addWidget(m_preflight);
    connect(m_preflight, &PrintPreflightPanel::issueActivated, this, [this](const auto& issue) { locatePreflightIssue(issue); });
    m_dataPanel = new QWidget(splitter);
    m_dataPanel->setObjectName(QStringLiteral("workbenchDataPanel"));
    m_dataPanel->setMinimumWidth(300);
    m_dataPanel->setMaximumWidth(360);
    auto* data = new QVBoxLayout(m_dataPanel);
    data->setContentsMargins(14, 14, 14, 14);
    data->setSpacing(10);
    data->addWidget(new QLabel(QString::fromUtf8("标签数据"), m_dataPanel));
    m_presetsCombo = new QComboBox(m_dataPanel);
    m_presetsCombo->setObjectName(QStringLiteral("workbenchPresetCombo"));
    m_presetsCombo->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    m_presetsCombo->setMinimumContentsLength(8);
    m_presetsCombo->setToolTip(QString::fromUtf8("字段预设"));
    auto* presetTools = new QHBoxLayout();
    presetTools->setContentsMargins(0, 0, 0, 0);
    presetTools->setSpacing(4);
    presetTools->addWidget(m_presetsCombo, 1);
    m_savePresetButton = new QPushButton(m_dataPanel);
    m_savePresetButton->setObjectName(QStringLiteral("workbenchSavePresetButton"));
    configureIconButton(m_savePresetButton, QStyle::SP_DialogSaveButton, QString::fromUtf8("将当前数据保存为字段预设"));
    m_managePresetsButton = new QPushButton(m_dataPanel);
    m_managePresetsButton->setObjectName(QStringLiteral("workbenchManagePresetsButton"));
    configureIconButton(m_managePresetsButton, QStyle::SP_FileDialogContentsView, QString::fromUtf8("管理字段预设"));
    m_managePresetsButton->setEnabled(false);
    presetTools->addWidget(m_savePresetButton);
    presetTools->addWidget(m_managePresetsButton);
    data->addLayout(presetTools);
    m_fields = new PrintFieldEditorPanel(m_dataPanel);
    data->addWidget(m_fields, 1);
    auto* separator = new QFrame(m_dataPanel);
    separator->setFrameShape(QFrame::HLine);
    data->addWidget(separator);
    auto* printOptions = new QFormLayout();
    printOptions->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    m_printers = new QComboBox(m_dataPanel);
    m_printers->setObjectName(QStringLiteral("workbenchPrinterCombo"));
    m_printers->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    m_printers->setMinimumContentsLength(8);
    m_copies = new QSpinBox(m_dataPanel);
    m_copies->setObjectName(QStringLiteral("workbenchCopiesSpin"));
    m_copies->setRange(1, sleekpr::core::PrintCopyPlanner::MaxCopies);
    m_copies->setValue(1);
    m_copies->setSuffix(QString::fromUtf8(" 份"));
    printOptions->addRow(QString::fromUtf8("打印机"), m_printers);
    printOptions->addRow(QString::fromUtf8("份数"), m_copies);
    data->addLayout(printOptions);
    m_summary = new QLabel(m_dataPanel);
    m_summary->setObjectName(QStringLiteral("workbenchPaperSummary"));
    m_summary->setWordWrap(true);
    data->addWidget(m_summary);
    m_jobSummary = new QLabel(m_dataPanel);
    m_jobSummary->setObjectName(QStringLiteral("workbenchJobSummary"));
    data->addWidget(m_jobSummary);
    m_printButton = new QPushButton(QString::fromUtf8("打印"), m_dataPanel);
    m_printButton->setObjectName(QStringLiteral("workbenchPrintButton"));
    m_printButton->setProperty("buttonRole", QStringLiteral("primary"));
    m_printButton->setMinimumHeight(36);
    data->addWidget(m_printButton);
    splitter->setSizes({210, 700, 330});
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);
    splitter->setStretchFactor(2, 0);
    root->addWidget(splitter, 1);
    m_status = new QLabel(this);
    m_status->setObjectName(QStringLiteral("workbenchStatusLabel"));
    m_status->setTextFormat(Qt::PlainText);
    m_status->setWordWrap(true);
    m_status->setContentsMargins(12, 8, 12, 8);
    root->addWidget(m_status);
    m_status->hide();
    m_prepareTimer = new QTimer(this);
    m_prepareTimer->setSingleShot(true);
    connect(m_prepareTimer, &QTimer::timeout, this, [this] { prepareNow(); });
    m_jobCheckTimer = new QTimer(this);
    m_jobCheckTimer->setSingleShot(true);
    connect(m_jobCheckTimer, &QTimer::timeout, this, [this] {
        if (m_prepareTimer->isActive()) return;
        m_preflightPending = false;
        updatePrintState();
    });
    connect(m_fields, &PrintFieldEditorPanel::valuesEdited, this, [this] { schedulePreparation(); });
    connect(m_templateList, &QListWidget::currentRowChanged, this, [this] { if (!m_loading) selectTemplate(); });
    connect(m_search, &QLineEdit::textChanged, this, [this](const QString& search) {
        QListWidgetItem* firstVisible = nullptr;
        for (int row = 0; row < m_templateList->count(); ++row) {
            auto* item = m_templateList->item(row);
            item->setHidden(!item->text().contains(search.trimmed(), Qt::CaseInsensitive)
                && !item->data(Qt::UserRole + 1).toString().contains(search.trimmed(), Qt::CaseInsensitive));
            if (!item->isHidden() && firstVisible == nullptr) firstVisible = item;
        }
        // 搜索过滤后不能继续打印列表中已不可见的旧选中模板。
        if (m_templateList->currentItem() == nullptr || m_templateList->currentItem()->isHidden()) {
            m_templateList->setCurrentItem(firstVisible);
        }
    });
    connect(m_presetsCombo, &QComboBox::currentIndexChanged, this, [this] { if (!m_loading) applyPreset(); });
    connect(m_printers, &QComboBox::currentIndexChanged, this, [this] { schedulePreparation(); });
    connect(m_copies, &QSpinBox::valueChanged, this, [this] { scheduleJobCheck(); });
    connect(m_refreshButton, &QPushButton::clicked, this, [this] { reloadFromDisk(); });
    connect(m_printButton, &QPushButton::clicked, this, [this] { printCurrent(); });
    connect(m_savePresetButton, &QPushButton::clicked, this, [this] { savePresetWithDialog(); });
    connect(m_managePresetsButton, &QPushButton::clicked, this, [this] {
        if (m_openFieldPresetManager) m_openFieldPresetManager();
    });
    connect(designer, &QPushButton::clicked, this, [this] { if (m_openDesigner) m_openDesigner(); });
    connect(settings, &QPushButton::clicked, this, [this] { if (m_openSettings) m_openSettings(); });
}

QString PrintWorkbenchWindow::selectedTemplateKey() const
{
    const auto* item = m_templateList->currentItem();
    return item == nullptr ? QString() : item->data(Qt::UserRole).toString();
}

bool PrintWorkbenchWindow::rememberDraft()
{
    bool valid = false;
    QString error;
    const auto values = m_fields->values(&valid, &error);
    if (!valid && !m_activeTemplateKey.isEmpty()) {
        showStatus(QString::fromUtf8("切换或刷新未执行，请先修复输入；当前草稿已保留"), QStringLiteral("warning"));
        return false;
    }
    if (valid && !m_activeTemplateKey.isEmpty()) m_drafts.insert(m_activeTemplateKey, values);
    return true;
}

void PrintWorkbenchWindow::reloadFromDisk()
{
    if (m_printing) return;
    if (!rememberDraft()) return;
    const auto preferred = selectedTemplateKey();
    QScopedValueRollback<bool> loading(m_loading, true);
    m_settings = sleekpr::core::FileSettingsStore(m_settingsPath).load();
    QString error;
    m_templates = m_service.templates(m_settings, &error);
    m_loadError = error;
    QString presetError;
    m_presets = m_service.fieldPresets(&presetError);
    if (!presetError.isEmpty()) m_loadError += QString::fromUtf8(" 字段预设读取失败：%1").arg(presetError);
    populatePrinters();
    populateTemplates(preferred);
    selectTemplate();
}

void PrintWorkbenchWindow::populateTemplates(const QString& preferredKey)
{
    const QSignalBlocker blocker(m_templateList);
    m_templateList->clear();
    int selected = 0;
    for (int index = 0; index < m_templates.size(); ++index) {
        const auto& choice = m_templates[index];
        const auto name = choice.document.name.isEmpty() ? choice.document.id : choice.document.name;
        const auto source = choice.key.contains(QStringLiteral("library:")) ? QString::fromUtf8("模板库") : QString::fromUtf8("配置");
        auto* item = new QListWidgetItem(QString::fromUtf8("%1（%2）").arg(name, source), m_templateList);
        item->setData(Qt::UserRole, choice.key);
        item->setData(Qt::UserRole + 1, QStringLiteral("%1 %2 %3").arg(choice.document.id, choice.document.templateKey, choice.key));
        item->setToolTip(QStringLiteral("%1\n%2").arg(item->text(), choice.key));
        if (choice.key == preferredKey) selected = index;
        const auto query = m_search->text().trimmed();
        item->setHidden(!item->text().contains(query, Qt::CaseInsensitive)
            && !item->data(Qt::UserRole + 1).toString().contains(query, Qt::CaseInsensitive));
    }
    if (m_templateList->count() > 0) m_templateList->setCurrentRow(selected);
    if (m_templateList->currentItem() != nullptr && m_templateList->currentItem()->isHidden()) {
        m_templateList->setCurrentItem(nullptr);
        for (int row = 0; row < m_templateList->count(); ++row) {
            if (!m_templateList->item(row)->isHidden()) { m_templateList->setCurrentRow(row); break; }
        }
    }
}

void PrintWorkbenchWindow::refreshFieldDisplayNames()
{
    if (m_fields == nullptr || m_printing || m_activeTemplateKey.isEmpty()) return;
    const auto latestSettings = sleekpr::core::FileSettingsStore(m_settingsPath).load();
    const auto document = m_service.findTemplate(latestSettings, m_activeTemplateKey);
    if (!document.has_value()) return;
    m_settings = latestSettings;
    for (auto& choice : m_templates) {
        if (choice.key == m_activeTemplateKey) { choice.document = *document; break; }
    }
    m_fields->refreshDisplayNames(*document);
    for (auto& issue : m_preflightReport.issues) {
        if (issue.code == QStringLiteral("INVALID_VALUES")) m_fields->values(nullptr, &issue.message);
    }
    updatePrintState();
}

bool PrintWorkbenchWindow::event(QEvent* event)
{
    const auto handled = QWidget::event(event);
    if (event->type() == QEvent::WindowActivate && m_fields != nullptr && !m_nameRefreshQueued) {
        m_nameRefreshQueued = true;
        QTimer::singleShot(0, this, [this] {
            m_nameRefreshQueued = false;
            refreshFieldDisplayNames();
        });
    }
    return handled;
}

void PrintWorkbenchWindow::populatePrinters()
{
    const QSignalBlocker blocker(m_printers);
    auto preferred = m_printers->currentData().toString();
    if (preferred.isEmpty()) preferred = m_settings.defaultPrinter;
    const auto names = m_printersProvider();
    m_printers->clear();
    for (const auto& name : names) m_printers->addItem(name, name);
    if (names.isEmpty()) {
        m_printers->addItem(QString::fromUtf8("无可用打印机"), QString());
    } else if (preferred.isEmpty()) {
        const auto systemDefault = QPrinterInfo::defaultPrinterName();
        m_printers->setCurrentIndex(std::max(0, m_printers->findData(systemDefault)));
    } else {
        // 配置中的设备消失时必须让用户重新选择，不能静默把任务发给另一台设备。
        const auto index = m_printers->findData(preferred);
        if (index < 0) m_printers->insertItem(0, QString::fromUtf8("请选择打印机"), QString());
        m_printers->setCurrentIndex(index < 0 ? 0 : index);
    }
}

void PrintWorkbenchWindow::selectTemplate()
{
    if (m_printing) return;
    if (!rememberDraft()) {
        const QSignalBlocker blocker(m_templateList);
        for (int row = 0; row < m_templateList->count(); ++row) {
            if (m_templateList->item(row)->data(Qt::UserRole).toString() == m_activeTemplateKey) {
                m_templateList->setCurrentRow(row);
                break;
            }
        }
        return;
    }
    const auto key = selectedTemplateKey();
    const auto it = std::find_if(m_templates.cbegin(), m_templates.cend(), [&key](const auto& choice) { return choice.key == key; });
    m_activeTemplateKey = key;
    if (it == m_templates.cend()) {
        m_prepared.reset();
        m_preview->clear();
        m_summary->clear();
        m_preflightPending = false;
        m_preflightReport = {};
        m_preflightReport.issues.append({core::PrintPreflightSeverity::Error, QStringLiteral("TEMPLATE_NOT_SELECTED"),
            QString::fromUtf8("请选择可用模板")});
        if (!m_loadError.isEmpty()) m_preflightReport.issues.append({core::PrintPreflightSeverity::Error,
            QStringLiteral("CONFIGURATION_ERROR"), m_loadError.trimmed()});
        showStatus({});
        updatePrintState();
        return;
    }
    QScopedValueRollback<bool> loading(m_loading, true);
    populatePresets(it->document);
    m_fields->setValues({});
    m_fields->setDocument(it->document);
    if (m_drafts.contains(key)) {
        m_fields->setValues(m_drafts.value(key));
    } else {
        QJsonObject presetValues;
        for (const auto& preset : m_presets) if (preset.id == m_presetsCombo->currentData().toString()) presetValues = preset.values;
        const auto context = sleekpr::core::TemplateRenderContextBuilder::build(it->document.fieldSchema, presetValues, {});
        m_fields->setValues(context.values);
    }
    prepareNow();
    refreshFieldDisplayNames();
}

void PrintWorkbenchWindow::populatePresets(const sleekpr::core::TemplateDocument& document)
{
    const QSignalBlocker blocker(m_presetsCombo);
    const auto previous = m_presetsCombo->currentData().toString();
    m_presetsCombo->clear();
    m_presetsCombo->addItem(QString::fromUtf8("无字段预设"), QString());
    for (const auto& preset : m_presets) {
        const auto target = preset.templateId.trimmed();
        bool compatible = target.isEmpty() || target == m_activeTemplateKey;
        if (!compatible) {
            const auto resolved = m_service.findTemplate(m_settings, target);
            compatible = resolved.has_value() && sleekpr::core::TemplateDocumentJson::toJson(*resolved)
                == sleekpr::core::TemplateDocumentJson::toJson(document);
        }
        // 只提供属于当前模板或显式通用的预设，避免客户、金额等同名字段跨模板串用。
        if (compatible) m_presetsCombo->addItem(preset.name.isEmpty() ? preset.id : preset.name, preset.id);
    }
    m_presetsCombo->setCurrentIndex(std::max(0, m_presetsCombo->findData(previous)));
}

void PrintWorkbenchWindow::applyPreset()
{
    m_drafts.remove(m_activeTemplateKey);
    const auto key = m_activeTemplateKey;
    m_activeTemplateKey.clear();
    selectTemplate();
    m_activeTemplateKey = key;
}

void PrintWorkbenchWindow::schedulePreparation()
{
    if (m_loading || m_printing) return;
    m_jobCheckTimer->stop();
    // 输入一变就废弃旧计划，不能在防抖等待期间误打印上一版数据。
    m_prepared.reset();
    m_preflightReport = {};
    m_preflightPending = true;
    m_preflight->setPending();
    m_printButton->setEnabled(false);
    m_savePresetButton->setEnabled(false);
    m_preview->clear();
    showStatus({});
    m_prepareTimer->start(180);
}

void PrintWorkbenchWindow::scheduleJobCheck()
{
    if (m_loading || m_printing) return;
    // 份数不改变基础版式；仅合并任务级检查，避免按键连发时反复渲染标签。
    m_preflightPending = true;
    m_preflight->setPending();
    m_preview->clearElementHighlight();
    m_printButton->setEnabled(false);
    m_jobSummary->clear();
    showStatus({});
    m_jobCheckTimer->start(180);
}

void PrintWorkbenchWindow::prepareNow()
{
    m_prepareTimer->stop();
    m_jobCheckTimer->stop();
    m_prepared.reset();
    m_preflightReport = {};
    m_preflightPending = false;
    // 校验信息只由检查面板展示，状态栏留给保存、提交和切换失败等操作结果。
    showStatus({});
    m_summary->clear();
    if (m_activeTemplateKey.isEmpty()) {
        m_preflightReport.issues.append({core::PrintPreflightSeverity::Error, QStringLiteral("TEMPLATE_NOT_SELECTED"),
            QString::fromUtf8("请选择可用模板")});
        if (!m_loadError.isEmpty()) m_preflightReport.issues.append({core::PrintPreflightSeverity::Error,
            QStringLiteral("CONFIGURATION_ERROR"), m_loadError.trimmed()});
        updatePrintState();
        return;
    }
    if (!m_loadError.isEmpty()) {
        m_preflightReport.issues.append({core::PrintPreflightSeverity::Error, QStringLiteral("CONFIGURATION_ERROR"), m_loadError.trimmed()});
        m_preview->clear();
        updatePrintState();
        return;
    }
    bool valid = true;
    QString error;
    const auto values = m_fields->values(&valid, &error);
    if (!valid) {
        const auto fieldKey = m_fields->invalidFieldKey();
        m_preflightReport.issues.append({core::PrintPreflightSeverity::Error, QStringLiteral("INVALID_VALUES"), error,
            fieldKey.isEmpty() ? core::PrintPreflightTarget::Json : core::PrintPreflightTarget::Field, fieldKey});
        m_preview->clear();
        updatePrintState();
        return;
    }
    sleekpr::infrastructure::TemplatePrintInput input;
    input.templateId = m_activeTemplateKey;
    input.values = values;
    input.fieldPresetId = m_presetsCombo->currentData().toString();
    input.printerName = m_printers->currentData().toString();
    input.requireCompleteValues = true;
    QString code;
    m_prepared = m_service.prepare(m_settings, input, &code, &error, &m_preflightReport);
    if (!m_prepared.has_value()) {
        m_preview->clear();
        m_summary->clear();
    } else {
        m_preview->setPlan(m_prepared->printPlan);
        const auto& plan = m_prepared->printPlan;
        m_summary->setText(QString::fromUtf8("%1 × %2 mm · %3 DPI").arg(plan.paperSize.widthMm()).arg(plan.paperSize.heightMm()).arg(plan.renderDpi));
    }
    updatePrintState();
}

void PrintWorkbenchWindow::updatePrintState()
{
    const auto printerAvailable = !m_printers->currentData().toString().isEmpty();
    QString error;
    const auto expanded = m_prepared.has_value()
        ? sleekpr::core::PrintCopyPlanner::repeat(m_prepared->printPlan, m_copies->value(), &error)
        : std::optional<sleekpr::core::NativePrintDrawingPlan>{};
    m_printButton->setEnabled(!m_printing && !m_preflightPending && printerAvailable && m_printEngine != nullptr && expanded.has_value());
    m_savePresetButton->setEnabled(!m_printing && m_prepared.has_value());
    m_jobSummary->setText(expanded.has_value()
            ? QString::fromUtf8("%1 份，共 %2 页").arg(m_copies->value()).arg(expanded->pages.size()) : QString());
    refreshPreflight(error);
}

void PrintWorkbenchWindow::refreshPreflight(const QString& jobError)
{
    if (m_preflightPending) { m_preflight->setPending(); return; }
    auto report = m_preflightReport;
    if (m_printers->currentData().toString().isEmpty()) {
        report.issues.append({core::PrintPreflightSeverity::Error, QStringLiteral("PRINTER_UNAVAILABLE"),
            QString::fromUtf8("请选择可用打印机"), core::PrintPreflightTarget::Printer});
    }
    if (m_printEngine == nullptr) {
        report.issues.append({core::PrintPreflightSeverity::Error, QStringLiteral("PRINT_ENGINE_UNAVAILABLE"),
            QString::fromUtf8("打印引擎不可用")});
    }
    // 份数规划沿用按钮状态检查的结果，不能为了填列表再复制一次整份打印计划。
    if (!jobError.isEmpty()) {
        report.issues.append({core::PrintPreflightSeverity::Error, QStringLiteral("PRINT_JOB_TOO_LARGE"), jobError,
            core::PrintPreflightTarget::Copies});
    }
    for (auto& issue : report.issues) issue.message = preflightMessage(issue);
    m_preflight->setReport(report);
}

QString PrintWorkbenchWindow::preflightMessage(const core::PrintPreflightIssue& issue) const
{
    const core::TemplateDocument* document = m_preflightReport.document.has_value() ? &*m_preflightReport.document : nullptr;
    for (const auto& choice : m_templates) {
        if (choice.key == m_activeTemplateKey) { document = &choice.document; break; }
    }
    if (document == nullptr) return issue.message;
    // 仅转换展示副本，结构化定位键和服务层的原始错误契约保持不变。
    if (issue.inputIssue.has_value()) {
        return issue.inputIssue->messageForDisplayNames(TemplateFieldNames::resolve(*document, issue.fieldKey),
            TemplateFieldNames::resolve(*document, issue.inputIssue->childFieldKey));
    }
    if (issue.code == QStringLiteral("FIELD_INTENTIONALLY_EMPTY")) {
        return QString::fromUtf8("字段 %1 已明确留空").arg(TemplateFieldNames::resolve(*document, issue.fieldKey));
    }
    return issue.message;
}

void PrintWorkbenchWindow::locatePreflightIssue(const core::PrintPreflightIssue& issue)
{
    if (m_preflightPending || m_printing) return;
    m_preview->clearElementHighlight();
    if (!issue.elementIds.isEmpty() && m_preflightReport.document.has_value()) {
        const auto& report = m_preflightReport;
        if (!m_prepared.has_value()) {
            // 错误数据不能生成正式预览；诊断只展示当前版式外框，不能复用旧标签内容。
            m_preview->setDiagnosticDocument(*report.document, report.paperSizeMm,
                report.offsetXmm, report.offsetYmm, report.scaleX, report.scaleY);
        }
        m_preview->setElementHighlight(*report.document, issue.elementIds,
            report.offsetXmm, report.offsetYmm, report.scaleX, report.scaleY);
    }
    switch (issue.target) {
    case core::PrintPreflightTarget::Field: m_fields->focusField(issue.fieldKey); break;
    case core::PrintPreflightTarget::Json: m_fields->focusJsonError(issue.line, issue.column); break;
    case core::PrintPreflightTarget::Printer: m_printers->setFocus(Qt::OtherFocusReason); break;
    case core::PrintPreflightTarget::Copies: m_copies->setFocus(Qt::OtherFocusReason); break;
    case core::PrintPreflightTarget::Element: m_preview->setFocus(Qt::OtherFocusReason); break;
    case core::PrintPreflightTarget::General: break;
    }
}

void PrintWorkbenchWindow::printCurrent()
{
    if (m_printing) return;
    // 点击时再次同步解析输入，打印使用本次不可变计划，不使用旧预览快照。
    const auto previous = m_prepared;
    m_settings = sleekpr::core::FileSettingsStore(m_settingsPath).load();
    prepareNow();
    if (!m_printButton->isEnabled() || !m_prepared.has_value()) return;
    if (previous.has_value() && !samePreparedPlan(*previous, *m_prepared)) {
        // 文件可能已被设计器或接口修改；先展示新输出，让用户核对后再次点击，避免打印未见过的版式。
        showStatus(QString::fromUtf8("模板或设备参数已更新，请核对新预览后再次打印"), QStringLiteral("warning"));
        return;
    }
    QString error;
    const auto expanded = sleekpr::core::PrintCopyPlanner::repeat(m_prepared->printPlan, m_copies->value(), &error);
    if (!expanded.has_value()) { refreshPreflight(error); return; }
    const auto printer = m_printers->currentData().toString();
    const auto copies = m_copies->value();
    m_printing = true;
    m_printButton->setEnabled(false);
    m_templatePanel->setEnabled(false);
    m_dataPanel->setEnabled(false);
    m_refreshButton->setEnabled(false);
    showStatus(QString::fromUtf8("正在提交打印任务"));
    QTimer::singleShot(0, this, [this, plan = *expanded, printer, copies] {
        const auto submitted = m_printEngine != nullptr && m_printEngine->print(plan, printer);
        m_printing = false;
        m_templatePanel->setEnabled(true);
        m_dataPanel->setEnabled(true);
        m_refreshButton->setEnabled(true);
        updatePrintState();
        // 引擎成功只代表驱动接收任务，不宣称已物理出纸；失败也不自动重试，防止重复标签。
        showStatus(submitted ? QString::fromUtf8("已向 %1 提交 %2 份，共 %3 页").arg(printer).arg(copies).arg(plan.pages.size())
                             : QString::fromUtf8("打印提交失败，可能已部分出纸，请检查设备后再决定是否重试"),
            submitted ? QString() : QStringLiteral("danger"));
    });
}

void PrintWorkbenchWindow::showStatus(const QString& text, const QString& role)
{
    m_status->setText(text);
    m_status->setVisible(!text.isEmpty());
    m_status->setProperty("statusRole", role);
    m_status->style()->unpolish(m_status);
    m_status->style()->polish(m_status);
}

bool PrintWorkbenchWindow::saveCurrentValuesAsPreset(const QString& name, QString* errorMessage)
{
    if (errorMessage != nullptr) errorMessage->clear();
    const auto fail = [this, errorMessage](const QString& message) {
        if (errorMessage != nullptr) *errorMessage = message;
        showStatus(message, QStringLiteral("danger"));
        return false;
    };
    if (m_printing) return fail(QString::fromUtf8("打印提交期间不能保存预设"));
    if (name.trimmed().isEmpty()) return fail(QString::fromUtf8("请填写字段预设名称"));
    prepareNow();
    if (!m_prepared.has_value()) {
        QStringList errors;
        for (const auto& issue : m_preflightReport.issues) {
            if (issue.severity == core::PrintPreflightSeverity::Error) errors.append(preflightMessage(issue));
        }
        // 返回错误不依赖状态栏文字，界面已经由检查面板给出详细原因。
        if (errorMessage != nullptr) *errorMessage = errors.isEmpty() ? QString::fromUtf8("当前数据未通过打印前检查")
            : errors.join(QString::fromUtf8("；"));
        return false;
    }

    sleekpr::core::FieldPreset preset;
    preset.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    preset.name = name.trimmed();
    // 使用当前来源明确的选择 key，避免同名模板或重复 id 把客户、金额等数据绑定到另一张标签。
    preset.templateId = m_activeTemplateKey;
    preset.values = m_prepared->context.values;
    preset.updatedAt = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
    QString error;
    if (!m_service.saveFieldPreset(preset, &error)) return fail(error);
    m_presets = m_service.fieldPresets(&error);
    if (!error.isEmpty()) return fail(error);
    populatePresets(m_prepared->document);
    {
        const QSignalBlocker blocker(m_presetsCombo);
        m_presetsCombo->setCurrentIndex(m_presetsCombo->findData(preset.id));
    }
    prepareNow();
    showStatus(QString::fromUtf8("已保存字段预设：%1").arg(preset.name));
    return true;
}

void PrintWorkbenchWindow::savePresetWithDialog()
{
    if (m_printing) return;
    bool accepted = false;
    const auto name = QInputDialog::getText(this, QString::fromUtf8("保存字段预设"),
        QString::fromUtf8("预设名称"), QLineEdit::Normal, QString(), &accepted);
    if (accepted) saveCurrentValuesAsPreset(name);
}

} // 命名空间 sleekpr::app

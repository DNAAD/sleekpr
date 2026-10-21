#include "sleekpr/app/SettingsWindow.h"
#include "sleekpr/app/BuildInfoDialog.h"
#include "sleekpr/app/SettingsPersistenceNotice.h"

#include "sleekpr/app/AppTheme.h"
#include "sleekpr/core/labels/LabelRenderPlanner.h"
#include "sleekpr/core/labels/TemplateOverrideKey.h"
#include "sleekpr/core/native/NativeLabelDrawingPlanner.h"
#include "sleekpr/core/printing/LabelPaperSize.h"
#include "sleekpr/core/settings/SettingsEditModel.h"
#include "sleekpr/core/templates/DeviceProfileResolver.h"
#include "sleekpr/core/templates/TemplateDocumentRenderer.h"
#include "sleekpr/core/templates/TemplateLibraryStore.h"
#include "sleekpr/core/templates/TemplateRenderContext.h"
#include "sleekpr/app/TemplateDragCoordinateMapper.h"
#include "sleekpr/app/TemplateElementHitTester.h"
#include "sleekpr/app/TemplatePreviewLabel.h"
#include "sleekpr/http/LocalHttpLimits.h"
#include "sleekpr/infrastructure/preview/LabelPreviewImageRenderer.h"
#include "sleekpr/infrastructure/preview/LabelPreviewService.h"
#include "sleekpr/infrastructure/preview/PreviewLabelFactory.h"

#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QEvent>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMessageBox>
#include <QPainter>
#include <QPen>
#include <QPixmap>
#include <QPlainTextEdit>
#include <QPrinterInfo>
#include <QPushButton>
#include <QScrollArea>
#include <QSet>
#include <QSignalBlocker>
#include <QSizePolicy>
#include <QSpinBox>
#include <QSplitter>
#include <QStackedWidget>
#include <QUuid>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>
#include <utility>

namespace sleekpr::app {

namespace {

QDoubleSpinBox* millimeterSpinBox(double minimum = -50.0, double maximum = 100.0)
{
    auto* spinBox = new QDoubleSpinBox;
    spinBox->setRange(minimum, maximum);
    spinBox->setDecimals(2);
    spinBox->setSingleStep(0.10);
    spinBox->setSuffix(QString::fromUtf8(" mm"));
    return spinBox;
}

QSpinBox* localHttpLimitSpinBox(int minimum, int maximum, int singleStep, const QString& suffix)
{
    auto* spinBox = new QSpinBox;
    spinBox->setRange(minimum, maximum);
    spinBox->setSingleStep(singleStep);
    spinBox->setSuffix(suffix);
    spinBox->setMinimumWidth(96);
    spinBox->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    return spinBox;
}

QWidget* localHttpLimitField(const QString& title, QSpinBox* spinBox)
{
    auto* field = new QWidget;
    auto* layout = new QVBoxLayout(field);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);

    auto* titleLabel = new QLabel(title, field);
    titleLabel->setBuddy(spinBox);
    layout->addWidget(titleLabel);
    layout->addWidget(spinBox);
    return field;
}

int bytesToWholeUnit(qsizetype bytes, qsizetype unit)
{
    const auto positiveBytes = std::max(bytes, unit);
    return static_cast<int>((positiveBytes + unit - 1) / unit);
}

qsizetype kibibytesToBytes(int value)
{
    return static_cast<qsizetype>(value) * 1024;
}

qsizetype mebibytesToBytes(int value)
{
    return static_cast<qsizetype>(value) * 1024 * 1024;
}

bool sameDouble(double left, double right)
{
    return std::abs(left - right) < 0.001;
}

constexpr double gridSnapStepMm = 0.5;
constexpr double keyboardNudgeStepMm = 0.1;
constexpr double keyboardLargeNudgeStepMm = 1.0;
constexpr int elementKindRole = Qt::UserRole + 1;

double snapToGrid(double value)
{
    return std::round(value / gridSnapStepMm) * gridSnapStepMm;
}

QString templateElementTypeName(sleekpr::core::TemplateElementType type)
{
    switch (type) {
    case sleekpr::core::TemplateElementType::FixedText:
        return QString::fromUtf8("固定文本");
    case sleekpr::core::TemplateElementType::BoundField:
        return QString::fromUtf8("绑定字段");
    case sleekpr::core::TemplateElementType::QrCode:
        return QString::fromUtf8("二维码");
    case sleekpr::core::TemplateElementType::Rectangle:
        return QString::fromUtf8("矩形");
    }
    return QString::fromUtf8("固定文本");
}

QString customElementDisplayName(const sleekpr::core::TemplateElement& element, int index)
{
    if (!element.displayName.trimmed().isEmpty()) {
        return element.displayName.trimmed();
    }
    return QString::fromUtf8("自定义%1 %2").arg(templateElementTypeName(element.type)).arg(index + 1);
}

sleekpr::core::TemplateElementType typeFromCombo(const QComboBox* combo)
{
    bool ok = false;
    const int rawType = combo->currentData().toInt(&ok);
    return static_cast<sleekpr::core::TemplateElementType>(
        ok ? rawType : static_cast<int>(sleekpr::core::TemplateElementType::FixedText));
}

} // 匿名命名空间

static QString templateDocumentComboText(const sleekpr::core::TemplateDocument& document, const QString& templateId)
{
    const auto name = document.name.trimmed().isEmpty() ? templateId : document.name.trimmed();
    const auto category = document.category.trimmed();
    if (!category.isEmpty() && category != QStringLiteral("label")) {
        return QStringLiteral("%1 (%2)").arg(name, category);
    }
    return name;
}

SettingsWindow::SettingsWindow(QString settingsPath, SettingsPersistenceService& persistence, SettingsAppliedCallback onSettingsApplied, QWidget* parent)
    : SettingsWindow(std::move(settingsPath), persistence, std::move(onSettingsApplied), QString(), parent)
{
}

SettingsWindow::SettingsWindow(
    QString settingsPath,
    SettingsPersistenceService& persistence,
    SettingsAppliedCallback onSettingsApplied,
    QString templateLibraryDirectoryPath,
    QWidget* parent)
    : QWidget(parent)
    , m_settingsPath(settingsPath)
    , m_persistenceService(&persistence)
    , m_onSettingsApplied(std::move(onSettingsApplied))
    , m_templateLibraryDirectoryPath(std::move(templateLibraryDirectoryPath))
{
    setWindowTitle(QString::fromUtf8("sleekpr 设置"));
    resize(1100, 740);
    buildUi();
    setupPreviewQueue();
    setupPersistence();
}

void SettingsWindow::setOpenTemplateDesignerCallback(OpenTemplateDesignerCallback callback)
{
    m_openTemplateDesigner = std::move(callback);
}

void SettingsWindow::setOpenPrintWorkbenchCallback(OpenPrintWorkbenchCallback callback)
{
    m_openPrintWorkbench = std::move(callback);
}

void SettingsWindow::setOpenPaperSpecManagerCallback(OpenPaperSpecManagerCallback callback)
{
    m_openPaperSpecManager = std::move(callback);
}

void SettingsWindow::setOpenFieldPresetManagerCallback(OpenFieldPresetManagerCallback callback)
{
    m_openFieldPresetManager = std::move(callback);
}

void SettingsWindow::reloadFromDisk()
{
    reloadSettings(true);
}

void SettingsWindow::reloadSettings(bool refreshDevices)
{
    m_pendingDeviceRefresh = m_pendingDeviceRefresh || refreshDevices;
    if (m_persistenceService) m_persistenceService->requestReload(m_persistenceSession);
}

void SettingsWindow::applyPersistenceSnapshot(const core::SettingsReadResult& snapshot, bool refreshDevices)
{
    const auto selectedTemplateKey = currentTemplateOverrideKey();
    m_loadingForm = true;
    m_persistenceNotice->clear();
    m_settings = snapshot.settings;
    populateTemplateCombo(selectedTemplateKey);
    m_currentElementKey.clear();
    if (refreshDevices || m_printerCombo->count() == 0) populatePrinters();

    m_offsetXSpin->setValue(m_settings.labelOffset.x);
    m_offsetYSpin->setValue(m_settings.labelOffset.y);
    m_allowedOriginsEdit->setPlainText(m_settings.allowedOrigins.join(QStringLiteral("\n")));
    const auto httpLimits = sleekpr::http::LocalHttpLimits::fromSettings(m_settings);
    m_httpMaxHeaderKbSpin->setValue(bytesToWholeUnit(httpLimits.maxHeaderBytes, 1024));
    m_httpMaxBodyMbSpin->setValue(bytesToWholeUnit(httpLimits.maxContentLengthBytes, 1024 * 1024));
    m_httpMaxPreviewBatchItemsSpin->setValue(httpLimits.maxPreviewBatchItems);
    m_httpMaxPreviewPagesSpin->setValue(httpLimits.maxPreviewPages);
    m_httpMaxPreviewResponseMbSpin->setValue(bytesToWholeUnit(httpLimits.maxPreviewResponseBytes, 1024 * 1024));
    m_formBaseline = readGeneralSettingsForm();
    m_loadingForm = false;

    renderPreview(m_settings);
    refreshOverview();
}

void SettingsWindow::buildUi()
{
    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    auto* shell = new QWidget(this);
    shell->setObjectName(QStringLiteral("settingsWorkbenchShell"));
    auto* shellLayout = new QHBoxLayout(shell);
    shellLayout->setContentsMargins(0, 0, 0, 0);
    shellLayout->setSpacing(0);

    shellLayout->addWidget(createWorkbenchNavigation(), 0);

    auto* contentPanel = new QWidget(shell);
    contentPanel->setObjectName(QStringLiteral("settingsContentPanel"));
    auto* contentLayout = new QVBoxLayout(contentPanel);
    contentLayout->setContentsMargins(20, 20, 20, 16);
    contentLayout->setSpacing(16);

    auto* splitter = new QSplitter(Qt::Horizontal, contentPanel);
    splitter->setObjectName(QStringLiteral("settingsWorkspaceSplitter"));
    splitter->setChildrenCollapsible(false);
    splitter->addWidget(createPrinterPanel());
    splitter->addWidget(createPreviewPanel());
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);
    splitter->setSizes({320, 540});
    contentLayout->addWidget(createOverviewPanel());
    contentLayout->addWidget(splitter, 1);

    auto* footer = new QWidget(contentPanel);
    footer->setObjectName(QStringLiteral("settingsFooter"));
    auto* buttonLayout = new QHBoxLayout(footer);
    buttonLayout->setContentsMargins(0, 12, 0, 0);
    buttonLayout->setSpacing(8);
    auto* saveButton = new QPushButton(QString::fromUtf8("保存"));
    auto* applyButton = new QPushButton(QString::fromUtf8("应用"));
    auto* refreshButton = new QPushButton;
    auto* closeButton = new QPushButton(QString::fromUtf8("关闭"));

    saveButton->setObjectName(QStringLiteral("saveSettingsButton"));
    saveButton->setProperty("buttonRole", QStringLiteral("primary"));
    applyButton->setObjectName(QStringLiteral("applySettingsButton"));
    applyButton->setToolTip(QString::fromUtf8("应用当前设置，但不保存到文件"));
    refreshButton->setObjectName(QStringLiteral("refreshSettingsPreviewButton"));
    configureIconButton(refreshButton, QStyle::SP_BrowserReload, QString::fromUtf8("刷新预览"));
    closeButton->setObjectName(QStringLiteral("closeSettingsButton"));
    buttonLayout->addWidget(saveButton);
    buttonLayout->addWidget(applyButton);
    buttonLayout->addWidget(refreshButton);
    buttonLayout->addStretch();
    auto* versionButton = new QPushButton(footer);
    versionButton->setObjectName(QStringLiteral("settingsBuildInfoButton"));
    configureIconButton(versionButton, QStyle::SP_MessageBoxInformation, QString::fromUtf8("版本与诊断"));
    buttonLayout->addWidget(versionButton);
    connect(versionButton, &QPushButton::clicked, this, [this] { BuildInfoDialog(m_settingsPath, this).exec(); });
    buttonLayout->addWidget(closeButton);
    contentLayout->addWidget(footer);

    shellLayout->addWidget(contentPanel, 1);
    rootLayout->addWidget(shell, 1);
    m_persistenceStateLabel = new QLabel(this);
    m_persistenceStateLabel->setObjectName(QStringLiteral("persistenceStateLabel"));
    rootLayout->addWidget(m_persistenceStateLabel);
    m_persistenceNotice = new SettingsPersistenceNotice(this);
    rootLayout->addWidget(m_persistenceNotice);
    m_persistenceNotice->setActions([this] { confirmPersistenceReload(); }, [this] {
        if (m_persistenceService) m_persistenceService->retry(m_persistenceSession);
    }, {}, [this] {
        if (QMessageBox::question(this, QString::fromUtf8("恢复备份"), QString::fromUtf8("确认恢复最近有效备份并放弃当前草稿？"),
                QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes) return;
        if (m_persistenceService) m_persistenceService->requestRestore(m_persistenceSession, m_persistenceNotice->observedFailureDigest());
    });

    connect(saveButton, &QPushButton::clicked, this, [this] { saveSettings(); });
    connect(applyButton, &QPushButton::clicked, this, [this] { applyWithoutSaving(); });
    connect(refreshButton, &QPushButton::clicked, this, [this] { refreshPreviewOnly(); });
    connect(closeButton, &QPushButton::clicked, this, [this] { close(); });
}

QWidget* SettingsWindow::createWorkbenchNavigation()
{
    auto* panel = new QWidget(this);
    panel->setObjectName(QStringLiteral("settingsNavigationRail"));
    panel->setFixedWidth(176);
    auto* layout = new QVBoxLayout(panel);
    layout->setContentsMargins(12, 24, 12, 16);
    layout->setSpacing(8);

    auto* titleLabel = new QLabel(QStringLiteral("sleekpr"), panel);
    titleLabel->setObjectName(QStringLiteral("settingsBrandTitle"));
    auto* subtitleLabel = new QLabel(QString::fromUtf8("本地打印工作台"), panel);
    subtitleLabel->setObjectName(QStringLiteral("settingsBrandSubtitle"));
    auto* hintLabel = new QLabel(QStringLiteral("127.0.0.1:37122"), panel);
    hintLabel->setObjectName(QStringLiteral("settingsNavigationHint"));

    m_printNavButton = new QPushButton(QString::fromUtf8("打印设置"), panel);
    m_printNavButton->setObjectName(QStringLiteral("settingsPrintNavButton"));
    m_printNavButton->setProperty("buttonRole", QStringLiteral("nav"));
    m_printNavButton->setCheckable(true);
    m_securityNavButton = new QPushButton(QString::fromUtf8("接口安全"), panel);
    m_securityNavButton->setObjectName(QStringLiteral("settingsSecurityNavButton"));
    m_securityNavButton->setProperty("buttonRole", QStringLiteral("nav"));
    m_securityNavButton->setCheckable(true);

    // 页面导航保持互斥；切页只改变可见性，不收集或重载尚未保存的设置。
    auto* pageNavigation = new QButtonGroup(panel);
    pageNavigation->setExclusive(true);
    pageNavigation->addButton(m_printNavButton);
    pageNavigation->addButton(m_securityNavButton);
    m_printNavButton->setChecked(true);

    auto* openDesignerButton = new QPushButton(QString::fromUtf8("标签设计器"), panel);
    openDesignerButton->setObjectName(QStringLiteral("openTemplateDesignerButton"));
    openDesignerButton->setProperty("buttonRole", QStringLiteral("nav"));
    auto* openPaperSpecManagerButton = new QPushButton(QString::fromUtf8("纸张规格"), panel);
    openPaperSpecManagerButton->setObjectName(QStringLiteral("openPaperSpecManagerButton"));
    openPaperSpecManagerButton->setProperty("buttonRole", QStringLiteral("nav"));
    auto* openFieldPresetManagerButton = new QPushButton(QString::fromUtf8("字段预设"), panel);
    openFieldPresetManagerButton->setObjectName(QStringLiteral("openFieldPresetManagerButton"));
    openFieldPresetManagerButton->setProperty("buttonRole", QStringLiteral("nav"));

    layout->addWidget(titleLabel);
    layout->addWidget(subtitleLabel);
    layout->addSpacing(24);
    auto* workbench = new QPushButton(QString::fromUtf8("打印工作台"), panel);
    workbench->setObjectName(QStringLiteral("openPrintWorkbenchButton"));
    workbench->setProperty("buttonRole", QStringLiteral("nav"));
    layout->addWidget(workbench);
    layout->addWidget(m_printNavButton);
    layout->addWidget(m_securityNavButton);
    layout->addSpacing(16);
    layout->addWidget(openDesignerButton);
    layout->addWidget(openPaperSpecManagerButton);
    layout->addWidget(openFieldPresetManagerButton);
    layout->addStretch(1);
    layout->addWidget(hintLabel);

    connect(m_printNavButton, &QPushButton::toggled, this, [this](bool checked) {
        if (checked && m_settingsPages != nullptr) {
            m_settingsPages->setCurrentIndex(0);
        }
    });
    connect(m_securityNavButton, &QPushButton::toggled, this, [this](bool checked) {
        if (checked && m_settingsPages != nullptr) {
            m_settingsPages->setCurrentIndex(1);
        }
    });
    connect(openDesignerButton, &QPushButton::clicked, this, [this] {
        if (m_openTemplateDesigner) {
            m_openTemplateDesigner();
        }
    });
    connect(workbench, &QPushButton::clicked, this, [this] {
        if (m_openPrintWorkbench) m_openPrintWorkbench();
    });
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

    return panel;
}

QWidget* SettingsWindow::createOverviewPanel()
{
    auto* panel = new QWidget(this);
    panel->setObjectName(QStringLiteral("settingsOverviewPanel"));
    auto* layout = new QGridLayout(panel);
    layout->setContentsMargins(0, 0, 0, 12);
    layout->setHorizontalSpacing(16);
    layout->setVerticalSpacing(8);

    auto* titleLabel = new QLabel(QString::fromUtf8("设置"), panel);
    titleLabel->setObjectName(QStringLiteral("settingsOverviewTitle"));
    m_serviceStatusLabel = new QLabel(panel);
    m_serviceStatusLabel->setObjectName(QStringLiteral("settingsServiceStatusLabel"));
    m_printerStatusLabel = new QLabel(panel);
    m_printerStatusLabel->setObjectName(QStringLiteral("settingsPrinterStatusLabel"));
    m_templateStatusLabel = new QLabel(panel);
    m_templateStatusLabel->setObjectName(QStringLiteral("settingsTemplateStatusLabel"));
    // 长打印机名和模板名不参与窗口最小宽度计算，完整内容保留在悬浮提示中。
    m_printerStatusLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    m_templateStatusLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);

    layout->addWidget(titleLabel, 0, 0, 1, 3);
    layout->addWidget(m_serviceStatusLabel, 1, 0);
    layout->addWidget(m_printerStatusLabel, 1, 1);
    layout->addWidget(m_templateStatusLabel, 1, 2);
    layout->setColumnStretch(1, 1);
    layout->setColumnStretch(2, 1);

    refreshOverview();
    return panel;
}

void SettingsWindow::refreshOverview()
{
    if (m_serviceStatusLabel != nullptr) {
        // 设置窗口创建早于 HTTP 监听，此处只展示地址，不推断服务是否运行。
        m_serviceStatusLabel->setText(QStringLiteral("http://127.0.0.1:37122"));
    }

    if (m_printerStatusLabel != nullptr) {
        const auto printerName = m_settings.defaultPrinter.trimmed().isEmpty()
            ? QString::fromUtf8("系统默认打印机")
            : m_settings.defaultPrinter.trimmed();
        m_printerStatusLabel->setText(QString::fromUtf8("默认打印机：%1").arg(printerName));
        m_printerStatusLabel->setToolTip(m_printerStatusLabel->text());
    }

    if (m_templateStatusLabel != nullptr) {
        const auto templateName = (m_templateCombo != nullptr && m_templateCombo->currentIndex() >= 0)
            ? m_templateCombo->currentText()
            : QString::fromUtf8("默认标签");
        m_templateStatusLabel->setText(QString::fromUtf8("当前模板：%1").arg(templateName));
        m_templateStatusLabel->setToolTip(m_templateStatusLabel->text());
    }
}

QWidget* SettingsWindow::createPrinterPanel()
{
    m_settingsPages = new QStackedWidget(this);
    m_settingsPages->setObjectName(QStringLiteral("settingsConfigurationStack"));
    m_settingsPages->setFixedWidth(320);

    auto* printPage = new QWidget(m_settingsPages);
    printPage->setObjectName(QStringLiteral("settingsPrintPage"));
    auto* printLayout = new QVBoxLayout(printPage);
    printLayout->setContentsMargins(16, 16, 16, 16);
    printLayout->setSpacing(12);
    auto* printTitle = new QLabel(QString::fromUtf8("打印设置"), printPage);
    printTitle->setObjectName(QStringLiteral("settingsPrintTitle"));
    printLayout->addWidget(printTitle);
    printLayout->addSpacing(8);

    auto* templateTitle = new QLabel(QString::fromUtf8("标签模板"), printPage);
    m_templateCombo = new QComboBox(printPage);
    m_templateCombo->setObjectName(QStringLiteral("settingsTemplateCombo"));
    m_templateCombo->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    m_templateCombo->setMinimumContentsLength(12);
    templateTitle->setBuddy(m_templateCombo);
    populateTemplateCombo();
    printLayout->addWidget(templateTitle);
    printLayout->addWidget(m_templateCombo);
    printLayout->addSpacing(8);

    auto* printerTitle = new QLabel(QString::fromUtf8("打印机"), printPage);
    auto* printerLayout = new QHBoxLayout;
    printerLayout->setSpacing(8);
    m_printerCombo = new QComboBox(printPage);
    m_printerCombo->setObjectName(QStringLiteral("settingsPrinterCombo"));
    m_printerCombo->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    m_printerCombo->setMinimumContentsLength(12);
    printerTitle->setBuddy(m_printerCombo);
    auto* refreshPrintersButton = new QPushButton(printPage);
    refreshPrintersButton->setObjectName(QStringLiteral("refreshSettingsPrintersButton"));
    configureIconButton(refreshPrintersButton, QStyle::SP_BrowserReload, QString::fromUtf8("刷新打印机"));
    printerLayout->addWidget(m_printerCombo);
    printerLayout->addWidget(refreshPrintersButton);
    printLayout->addWidget(printerTitle);
    printLayout->addLayout(printerLayout);
    printLayout->addSpacing(8);

    printLayout->addWidget(new QLabel(QString::fromUtf8("整体偏移"), printPage));
    auto* offsetLayout = new QGridLayout;
    offsetLayout->setHorizontalSpacing(12);
    offsetLayout->setVerticalSpacing(4);
    offsetLayout->setColumnStretch(0, 1);
    offsetLayout->setColumnStretch(1, 1);
    m_offsetXSpin = millimeterSpinBox();
    m_offsetYSpin = millimeterSpinBox();
    m_offsetXSpin->setObjectName(QStringLiteral("settingsOffsetXSpin"));
    m_offsetYSpin->setObjectName(QStringLiteral("settingsOffsetYSpin"));
    m_offsetXSpin->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_offsetYSpin->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    auto* offsetXTitle = new QLabel(QString::fromUtf8("水平 X"), printPage);
    auto* offsetYTitle = new QLabel(QString::fromUtf8("垂直 Y"), printPage);
    offsetXTitle->setBuddy(m_offsetXSpin);
    offsetYTitle->setBuddy(m_offsetYSpin);
    offsetLayout->addWidget(offsetXTitle, 0, 0);
    offsetLayout->addWidget(offsetYTitle, 0, 1);
    offsetLayout->addWidget(m_offsetXSpin, 1, 0);
    offsetLayout->addWidget(m_offsetYSpin, 1, 1);
    printLayout->addLayout(offsetLayout);
    printLayout->addStretch();

    auto* securityPage = new QWidget(m_settingsPages);
    securityPage->setObjectName(QStringLiteral("settingsSecurityPage"));
    auto* securityLayout = new QVBoxLayout(securityPage);
    securityLayout->setContentsMargins(16, 16, 16, 16);
    securityLayout->setSpacing(8);
    auto* securityTitle = new QLabel(QString::fromUtf8("接口安全"), securityPage);
    securityTitle->setObjectName(QStringLiteral("settingsSecurityTitle"));
    securityLayout->addWidget(securityTitle);
    securityLayout->addSpacing(8);

    const auto originsTooltip = QString::fromUtf8(
        "允许访问本地服务的网页 Origin，每行一个；留空时只接受没有 Origin 的本机调用。");
    auto* allowedOriginsTitle = new QLabel(QString::fromUtf8("允许的 Origin"), securityPage);
    allowedOriginsTitle->setToolTip(originsTooltip);
    m_allowedOriginsEdit = new QPlainTextEdit(securityPage);
    m_allowedOriginsEdit->setObjectName(QStringLiteral("allowedOriginsEdit"));
    m_allowedOriginsEdit->setPlaceholderText(QString::fromUtf8("https://manager.example.com"));
    m_allowedOriginsEdit->setToolTip(originsTooltip);
    m_allowedOriginsEdit->setFixedHeight(96);
    allowedOriginsTitle->setBuddy(m_allowedOriginsEdit);
    securityLayout->addWidget(allowedOriginsTitle);
    securityLayout->addWidget(m_allowedOriginsEdit);

    const auto limitsTooltip = QString::fromUtf8(
        "在收包和预览响应阶段生效；调大前请确认 Web 端确实需要。");
    auto* limitTitle = new QLabel(QString::fromUtf8("高级上限"), securityPage);
    limitTitle->setObjectName(QStringLiteral("settingsHttpLimitsTitle"));
    limitTitle->setToolTip(limitsTooltip);
    auto* limitsGridPanel = new QWidget(securityPage);
    limitsGridPanel->setObjectName(QStringLiteral("settingsHttpLimitsGrid"));
    limitsGridPanel->setToolTip(limitsTooltip);
    auto* limitLayout = new QGridLayout(limitsGridPanel);
    limitLayout->setContentsMargins(0, 0, 0, 0);
    limitLayout->setHorizontalSpacing(10);
    limitLayout->setVerticalSpacing(8);
    limitLayout->setColumnStretch(0, 1);
    limitLayout->setColumnStretch(1, 1);
    m_httpMaxHeaderKbSpin = localHttpLimitSpinBox(
        sleekpr::http::LocalHttpLimits::kMinMaxHeaderBytes / 1024,
        sleekpr::http::LocalHttpLimits::kMaxMaxHeaderBytes / 1024,
        4,
        QStringLiteral(" KB"));
    m_httpMaxHeaderKbSpin->setObjectName(QStringLiteral("settingsMaxHeaderKbSpin"));
    m_httpMaxBodyMbSpin = localHttpLimitSpinBox(
        sleekpr::http::LocalHttpLimits::kMinMaxContentLengthBytes / (1024 * 1024),
        sleekpr::http::LocalHttpLimits::kMaxMaxContentLengthBytes / (1024 * 1024),
        1,
        QStringLiteral(" MB"));
    m_httpMaxBodyMbSpin->setObjectName(QStringLiteral("settingsMaxBodyMbSpin"));
    m_httpMaxPreviewBatchItemsSpin = localHttpLimitSpinBox(
        sleekpr::http::LocalHttpLimits::kMinMaxPreviewBatchItems,
        sleekpr::http::LocalHttpLimits::kMaxMaxPreviewBatchItems,
        5,
        QString::fromUtf8(" 条"));
    m_httpMaxPreviewBatchItemsSpin->setObjectName(QStringLiteral("settingsMaxPreviewBatchItemsSpin"));
    m_httpMaxPreviewPagesSpin = localHttpLimitSpinBox(
        sleekpr::http::LocalHttpLimits::kMinMaxPreviewPages,
        sleekpr::http::LocalHttpLimits::kMaxMaxPreviewPages,
        10,
        QString::fromUtf8(" 页"));
    m_httpMaxPreviewPagesSpin->setObjectName(QStringLiteral("settingsMaxPreviewPagesSpin"));
    m_httpMaxPreviewResponseMbSpin = localHttpLimitSpinBox(
        sleekpr::http::LocalHttpLimits::kMinMaxPreviewResponseBytes / (1024 * 1024),
        sleekpr::http::LocalHttpLimits::kMaxMaxPreviewResponseBytes / (1024 * 1024),
        1,
        QStringLiteral(" MB"));
    m_httpMaxPreviewResponseMbSpin->setObjectName(QStringLiteral("settingsMaxPreviewResponseMbSpin"));
    limitLayout->addWidget(localHttpLimitField(QString::fromUtf8("请求头"), m_httpMaxHeaderKbSpin), 0, 0);
    limitLayout->addWidget(localHttpLimitField(QString::fromUtf8("请求体"), m_httpMaxBodyMbSpin), 0, 1);
    limitLayout->addWidget(localHttpLimitField(QString::fromUtf8("批量预览"), m_httpMaxPreviewBatchItemsSpin), 1, 0);
    limitLayout->addWidget(localHttpLimitField(QString::fromUtf8("预览页数"), m_httpMaxPreviewPagesSpin), 1, 1);
    limitLayout->addWidget(localHttpLimitField(QString::fromUtf8("响应体"), m_httpMaxPreviewResponseMbSpin), 2, 0);

    auto* resetLimitsButton = new QPushButton(QString::fromUtf8("恢复默认上限"), securityPage);
    resetLimitsButton->setObjectName(QStringLiteral("resetHttpLimitsButton"));
    securityLayout->addSpacing(8);
    securityLayout->addWidget(limitTitle);
    securityLayout->addWidget(limitsGridPanel);
    securityLayout->addWidget(resetLimitsButton, 0, Qt::AlignLeft);
    securityLayout->addStretch();

    // 两页共用原设置控件和保存路径，隐藏的安全页也保留用户正在编辑的值。
    m_settingsPages->addWidget(printPage);
    m_settingsPages->addWidget(securityPage);
    m_settingsPages->setCurrentIndex(0);

    connect(refreshPrintersButton, &QPushButton::clicked, this, [this] {
        collectGeneralSettings();
        populatePrinters();
        refreshOverview();
    });
    connect(m_templateCombo, &QComboBox::currentIndexChanged, this, [this] {
        m_currentTemplateOverrideKey = m_templateCombo->currentData().toString();
        m_currentElementKey.clear();
        renderPreview(m_settings);
        refreshOverview();
    });
    connect(resetLimitsButton, &QPushButton::clicked, this, [this] {
        m_httpMaxHeaderKbSpin->setValue(sleekpr::http::LocalHttpLimits::kDefaultMaxHeaderBytes / 1024);
        m_httpMaxBodyMbSpin->setValue(sleekpr::http::LocalHttpLimits::kDefaultMaxContentLengthBytes / (1024 * 1024));
        m_httpMaxPreviewBatchItemsSpin->setValue(sleekpr::http::LocalHttpLimits::kDefaultMaxPreviewBatchItems);
        m_httpMaxPreviewPagesSpin->setValue(sleekpr::http::LocalHttpLimits::kDefaultMaxPreviewPages);
        m_httpMaxPreviewResponseMbSpin->setValue(sleekpr::http::LocalHttpLimits::kDefaultMaxPreviewResponseBytes / (1024 * 1024));
    });

    return m_settingsPages;
}

QWidget* SettingsWindow::createPreviewPanel()
{
    auto* panel = new QWidget(this);
    panel->setObjectName(QStringLiteral("settingsPreviewPanel"));
    panel->setMinimumWidth(0);
    panel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    auto* layout = new QVBoxLayout(panel);
    layout->setContentsMargins(16, 16, 0, 0);
    layout->setSpacing(12);

    auto* header = new QHBoxLayout;
    auto* title = new QLabel(QString::fromUtf8("打印预览"), panel);
    title->setObjectName(QStringLiteral("settingsPreviewTitle"));
    m_statusLabel = new QLabel(panel);
    m_statusLabel->setObjectName(QStringLiteral("settingsPreviewStatusLabel"));
    m_statusLabel->setText(QString::fromUtf8("预览尺寸：80mm x 30mm"));
    m_statusLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_statusLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    header->addWidget(title);
    header->addWidget(m_statusLabel, 1);
    layout->addLayout(header);

    m_previewLabel = new TemplatePreviewLabel(panel);
    m_previewLabel->setObjectName(QStringLiteral("settingsPreviewLabel"));
    m_previewLabel->setAlignment(Qt::AlignCenter);
    m_previewLabel->setScaledContents(true);
    m_previewLabel->setDraggingEnabled(false);

    m_previewScrollArea = new QScrollArea(panel);
    m_previewScrollArea->setObjectName(QStringLiteral("settingsPreviewScrollArea"));
    m_previewScrollArea->setFrameShape(QFrame::NoFrame);
    m_previewScrollArea->setWidgetResizable(false);
    m_previewScrollArea->setAlignment(Qt::AlignCenter);
    m_previewScrollArea->setWidget(m_previewLabel);
    m_previewScrollArea->viewport()->setObjectName(QStringLiteral("settingsPreviewViewport"));
    m_previewScrollArea->viewport()->installEventFilter(this);
    m_previewLabel->installEventFilter(this);
    layout->addWidget(m_previewScrollArea, 1);

    return panel;
}

bool SettingsWindow::eventFilter(QObject* watched, QEvent* event)
{
    if (m_previewScrollArea != nullptr
        && (watched == m_previewScrollArea->viewport() || watched == m_previewLabel)
        && (event->type() == QEvent::Resize || event->type() == QEvent::Show)) {
        fitPreviewToViewport();
    }
    return QWidget::eventFilter(watched, event);
}

void SettingsWindow::fitPreviewToViewport()
{
    if (m_fittingPreview || m_previewScrollArea == nullptr || m_previewLabel == nullptr
        || m_previewLabel->pixmap().isNull()) {
        return;
    }

    // 只调整预览控件的逻辑尺寸，原始图像和正式打印使用的像素、毫米数据均不改变。
    const auto imageSize = m_previewLabel->pixmap().deviceIndependentSize().toSize();
    const auto availableSize = (m_previewScrollArea->viewport()->size() - QSize(32, 32)).expandedTo(QSize(1, 1));
    const auto fittedSize = imageSize.scaled(availableSize.boundedTo(imageSize), Qt::KeepAspectRatio);
    if (m_previewLabel->size() != fittedSize) {
        m_fittingPreview = true;
        m_previewLabel->setFixedSize(fittedSize);
        m_fittingPreview = false;
    }
}

QWidget* SettingsWindow::createElementPanel()
{
    auto* panel = new QWidget(this);
    auto* layout = new QVBoxLayout(panel);
    layout->setContentsMargins(10, 0, 0, 0);

    auto* addTitle = new QLabel(QString::fromUtf8("新增元素"), panel);
    auto* addFirstRow = new QHBoxLayout;
    auto* addSecondRow = new QHBoxLayout;
    auto* addFixedTextButton = new QPushButton(QString::fromUtf8("固定文本"), panel);
    auto* addBoundFieldButton = new QPushButton(QString::fromUtf8("绑定字段"), panel);
    auto* addQrCodeButton = new QPushButton(QString::fromUtf8("二维码"), panel);
    auto* addRectangleButton = new QPushButton(QString::fromUtf8("矩形"), panel);
    addFixedTextButton->setObjectName(QStringLiteral("addFixedTextButton"));
    addBoundFieldButton->setObjectName(QStringLiteral("addBoundFieldButton"));
    addQrCodeButton->setObjectName(QStringLiteral("addQrCodeButton"));
    addRectangleButton->setObjectName(QStringLiteral("addRectangleButton"));
    addFirstRow->addWidget(addFixedTextButton);
    addFirstRow->addWidget(addBoundFieldButton);
    addSecondRow->addWidget(addQrCodeButton);
    addSecondRow->addWidget(addRectangleButton);
    layout->addWidget(addTitle);
    layout->addLayout(addFirstRow);
    layout->addLayout(addSecondRow);

    m_elementList = new QListWidget(panel);
    m_elementList->setMinimumWidth(180);
    layout->addWidget(m_elementList, 1);

    auto* editGroup = new QGroupBox(QString::fromUtf8("元素"));
    auto* editLayout = new QFormLayout(editGroup);
    m_customElementTypeCombo = new QComboBox(editGroup);
    m_customElementTypeCombo->setObjectName(QStringLiteral("customElementTypeCombo"));
    m_customElementTypeCombo->addItem(QString::fromUtf8("固定文本"), static_cast<int>(sleekpr::core::TemplateElementType::FixedText));
    m_customElementTypeCombo->addItem(QString::fromUtf8("绑定字段"), static_cast<int>(sleekpr::core::TemplateElementType::BoundField));
    m_customElementTypeCombo->addItem(QString::fromUtf8("二维码"), static_cast<int>(sleekpr::core::TemplateElementType::QrCode));
    m_customElementTypeCombo->addItem(QString::fromUtf8("矩形"), static_cast<int>(sleekpr::core::TemplateElementType::Rectangle));
    m_customElementTextEdit = new QLineEdit(editGroup);
    m_customElementTextEdit->setObjectName(QStringLiteral("customElementTextEdit"));
    m_customElementFieldCombo = new QComboBox(editGroup);
    m_customElementFieldCombo->setObjectName(QStringLiteral("customElementFieldCombo"));
    m_customElementFieldCombo->addItem(QString::fromUtf8("编号"), QStringLiteral("identifierText"));
    m_customElementFieldCombo->addItem(QString::fromUtf8("商品名称"), QStringLiteral("productName"));
    m_customElementFieldCombo->addItem(QString::fromUtf8("成品重"), QStringLiteral("finishedWeightText"));
    m_customElementFieldCombo->addItem(QString::fromUtf8("总件重"), QStringLiteral("roughWeightText"));
    m_customElementFieldCombo->addItem(QString::fromUtf8("销售码"), QStringLiteral("salesCode"));
    m_customElementFieldCombo->addItem(QString::fromUtf8("执行标准"), QStringLiteral("standardText"));
    m_customElementFieldCombo->addItem(QString::fromUtf8("地址"), QStringLiteral("addressText"));
    m_customElementFieldCombo->addItem(QString::fromUtf8("二维码内容"), QStringLiteral("qrPayload"));
    m_customElementFieldCombo->addItem(QString::fromUtf8("标签重量"), QStringLiteral("tagWeightText"));
    m_customElementFieldCombo->addItem(QString::fromUtf8("底部备注"), QStringLiteral("footerText"));
    m_elementXSpin = millimeterSpinBox();
    m_elementYSpin = millimeterSpinBox();
    m_elementWidthSpin = millimeterSpinBox(0.1, 100.0);
    m_elementHeightSpin = millimeterSpinBox(0.1, 100.0);
    m_elementXSpin->setObjectName(QStringLiteral("elementXSpin"));
    m_elementYSpin->setObjectName(QStringLiteral("elementYSpin"));
    m_elementWidthSpin->setObjectName(QStringLiteral("elementWidthSpin"));
    m_elementHeightSpin->setObjectName(QStringLiteral("elementHeightSpin"));
    m_fontSizeSpin = new QDoubleSpinBox(editGroup);
    m_fontSizeSpin->setRange(1.0, 36.0);
    m_fontSizeSpin->setDecimals(1);
    m_fontSizeSpin->setSingleStep(0.1);
    m_fontSizeSpin->setSuffix(QString::fromUtf8(" pt"));
    m_boldCheck = new QCheckBox(QString::fromUtf8("加粗"), editGroup);

    m_deleteCustomElementButton = new QPushButton(QString::fromUtf8("删除自定义元素"), editGroup);
    m_deleteCustomElementButton->setObjectName(QStringLiteral("deleteCustomElementButton"));

    editLayout->addRow(QString::fromUtf8("类型"), m_customElementTypeCombo);
    editLayout->addRow(QString::fromUtf8("内容"), m_customElementTextEdit);
    editLayout->addRow(QString::fromUtf8("绑定字段"), m_customElementFieldCombo);
    editLayout->addRow(QString::fromUtf8("X"), m_elementXSpin);
    editLayout->addRow(QString::fromUtf8("Y"), m_elementYSpin);
    editLayout->addRow(QString::fromUtf8("宽"), m_elementWidthSpin);
    editLayout->addRow(QString::fromUtf8("高"), m_elementHeightSpin);
    editLayout->addRow(QString::fromUtf8("字号"), m_fontSizeSpin);
    editLayout->addRow(QString(), m_boldCheck);
    editLayout->addRow(QString(), m_deleteCustomElementButton);
    layout->addWidget(editGroup);

    connect(addFixedTextButton, &QPushButton::clicked, this, [this] { addTemplateElement(sleekpr::core::TemplateElementType::FixedText); });
    connect(addBoundFieldButton, &QPushButton::clicked, this, [this] { addTemplateElement(sleekpr::core::TemplateElementType::BoundField); });
    connect(addQrCodeButton, &QPushButton::clicked, this, [this] { addTemplateElement(sleekpr::core::TemplateElementType::QrCode); });
    connect(addRectangleButton, &QPushButton::clicked, this, [this] { addTemplateElement(sleekpr::core::TemplateElementType::Rectangle); });
    connect(m_deleteCustomElementButton, &QPushButton::clicked, this, [this] { deleteCurrentCustomElement(); });
    connect(m_elementList, &QListWidget::currentRowChanged, this, [this](int row) {
        if (m_loadingForm) {
            return;
        }
        storeCurrentElementForm();
        m_currentElementKey = row >= 0 ? m_elementList->item(row)->data(Qt::UserRole).toString() : QString();
        loadCurrentElementForm();
        renderPreview(m_settings);
    });
    connect(m_customElementTypeCombo, &QComboBox::currentIndexChanged, this, [this] {
        if (m_loadingForm || !isCurrentCustomElement()) {
            return;
        }
        storeCurrentElementForm();
        loadCurrentElementForm();
        renderPreview(m_settings);
    });
    connect(m_customElementFieldCombo, &QComboBox::currentIndexChanged, this, [this] {
        if (m_loadingForm || !isCurrentCustomElement()) {
            return;
        }
        storeCurrentElementForm();
        renderPreview(m_settings);
    });
    connect(m_customElementTextEdit, &QLineEdit::editingFinished, this, [this] {
        if (m_loadingForm || !isCurrentCustomElement()) {
            return;
        }
        storeCurrentElementForm();
        renderPreview(m_settings);
    });
    const auto persistCustomSize = [this](double) {
        if (m_loadingForm || !isCurrentCustomElement()) {
            return;
        }
        storeCurrentElementForm();
        renderPreview(m_settings);
    };
    connect(m_elementWidthSpin, &QDoubleSpinBox::valueChanged, this, persistCustomSize);
    connect(m_elementHeightSpin, &QDoubleSpinBox::valueChanged, this, persistCustomSize);

    return panel;
}

void SettingsWindow::populatePrinters()
{
    const auto selectedPrinter = m_settings.defaultPrinter;
    m_printerCombo->clear();
    m_printerCombo->addItem(QString::fromUtf8("使用系统默认打印机"), QString());

    for (const auto& printer : QPrinterInfo::availablePrinters()) {
        const auto printerName = printer.printerName();
        m_printerCombo->addItem(printerName, printerName);
    }

    const auto selectedIndex = m_printerCombo->findData(selectedPrinter);
    m_printerCombo->setCurrentIndex(selectedIndex >= 0 ? selectedIndex : 0);
}

void SettingsWindow::populateTemplateCombo(const QString& preferredTemplateKey)
{
    if (m_templateCombo == nullptr) {
        return;
    }

    const auto selectedKey = preferredTemplateKey.trimmed().isEmpty()
        ? currentTemplateOverrideKey()
        : preferredTemplateKey.trimmed();
    const QSignalBlocker blocker(m_templateCombo);
    m_templateCombo->clear();
    m_libraryTemplateDocuments.clear();

    QSet<QString> addedKeys;
    auto addItem = [this, &addedKeys](const QString& text, const QString& key) {
        if (key.trimmed().isEmpty() || addedKeys.contains(key)) {
            return;
        }
        m_templateCombo->addItem(text, key);
        addedKeys.insert(key);
    };
    auto addDocumentItem = [&addItem](const QString& templateId, const sleekpr::core::TemplateDocument& document) {
        addItem(templateDocumentComboText(document, templateId), templateId);
    };

    addItem(QString::fromUtf8("默认标签"), QStringLiteral("default"));
    addItem(QString::fromUtf8("银标签 factoryNo=25003"), QStringLiteral("silver"));

    for (auto it = m_settings.templateDocuments.constBegin(); it != m_settings.templateDocuments.constEnd(); ++it) {
        addDocumentItem(it.key(), it.value());
    }

    if (!m_templateLibraryDirectoryPath.trimmed().isEmpty()) {
        const sleekpr::core::TemplateLibraryStore templateStore(m_templateLibraryDirectoryPath);
        for (const auto& templateId : templateStore.templateIds()) {
            const auto document = templateStore.loadTemplate(templateId);
            if (!document.has_value()) {
                continue;
            }
            // 设置页下拉只引用模板库文档，不把它们隐式写入 settings.json。
            m_libraryTemplateDocuments.insert(templateId, document.value());
            addDocumentItem(templateId, document.value());
        }
    }

    int selectedIndex = m_templateCombo->findData(selectedKey);
    if (selectedIndex < 0) {
        selectedIndex = m_templateCombo->findData(QStringLiteral("default"));
    }
    m_templateCombo->setCurrentIndex(selectedIndex >= 0 ? selectedIndex : 0);
    m_currentTemplateOverrideKey = m_templateCombo->currentData().toString();
}

void SettingsWindow::populateElements()
{
    const auto labelPlan = sleekpr::core::LabelRenderPlanner().createPlan(
        sleekpr::infrastructure::PreviewLabelFactory::createDemoLabel(currentLabelTemplateKey()));
    const auto drawingPlan = sleekpr::core::NativeLabelDrawingPlanner().createPlan(labelPlan);
    m_elements = sleekpr::core::TemplateElementCatalog::fromDrawingPlan(drawingPlan);

    m_loadingForm = true;
    m_elementList->clear();
    for (const auto& element : m_elements) {
        auto* item = new QListWidgetItem(element.displayName, m_elementList);
        item->setData(Qt::UserRole, element.key);
        item->setData(elementKindRole, QStringLiteral("default"));
    }
    const auto customElements = m_settings.templateElements.value(currentTemplateOverrideKey());
    for (int index = 0; index < customElements.size(); ++index) {
        const auto& element = customElements[index];
        auto* item = new QListWidgetItem(customElementDisplayName(element, index), m_elementList);
        item->setData(Qt::UserRole, element.id);
        item->setData(elementKindRole, QStringLiteral("custom"));
    }
    m_loadingForm = false;
}

void SettingsWindow::selectFirstElement()
{
    if (m_elementList->count() <= 0) {
        return;
    }

    // 首次选中元素时表单尚未加载，必须阻断列表信号，避免把控件默认值写成元素覆盖。
    m_loadingForm = true;
    m_elementList->setCurrentRow(0);
    m_loadingForm = false;
    m_currentElementKey = m_elementList->item(0)->data(Qt::UserRole).toString();
    loadCurrentElementForm();
    updatePreviewDraggingState();
}

void SettingsWindow::storeCurrentElementForm()
{
    if (!m_loadingForm) ++m_modelEditRevision;
    if (m_currentElementKey.isEmpty()) {
        return;
    }

    if (auto* custom = mutableCustomElement(m_currentElementKey)) {
        storeCustomElementForm(*custom);
        return;
    }

    const auto element = sleekpr::core::TemplateElementCatalog::find(m_elements, m_currentElementKey);
    if (!element.has_value()) {
        return;
    }

    const auto formValue = formOverrideForCurrentElement();
    auto& templateOverrides = m_settings.templateOverrides[currentTemplateOverrideKey()];
    if (formMatchesDefault(formValue, element.value())) {
        templateOverrides.remove(m_currentElementKey);
        return;
    }

    // 只保存与默认值不同的元素覆盖，保持 settings.json 轻量并便于人工排查。
    templateOverrides[m_currentElementKey] = formValue;
}

void SettingsWindow::loadCurrentElementForm()
{
    if (const auto* custom = customElement(m_currentElementKey)) {
        loadCustomElementForm(*custom);
        return;
    }

    const auto element = sleekpr::core::TemplateElementCatalog::find(m_elements, m_currentElementKey);
    if (!element.has_value()) {
        return;
    }

    const auto value = displayedOverrideForElement(element.value());
    m_loadingForm = true;
    m_elementXSpin->setValue(value.x.value_or(0.0));
    m_elementYSpin->setValue(value.y.value_or(0.0));
    m_fontSizeSpin->setEnabled(element->defaultValue.fontSizePt.has_value());
    m_boldCheck->setEnabled(element->defaultValue.bold.has_value());
    m_fontSizeSpin->setValue(value.fontSizePt.value_or(1.0));
    m_boldCheck->setChecked(value.bold.value_or(false));
    const auto elementSize = currentElementSizeMm();
    m_elementWidthSpin->setValue(elementSize.width());
    m_elementHeightSpin->setValue(elementSize.height());
    m_elementWidthSpin->setEnabled(false);
    m_elementHeightSpin->setEnabled(false);
    m_customElementTypeCombo->setCurrentIndex(-1);
    m_customElementTypeCombo->setEnabled(false);
    m_customElementTextEdit->clear();
    m_customElementTextEdit->setEnabled(false);
    m_customElementFieldCombo->setCurrentIndex(-1);
    m_customElementFieldCombo->setEnabled(false);
    m_deleteCustomElementButton->setEnabled(false);
    m_loadingForm = false;
    updatePreviewDraggingState();
}

void SettingsWindow::collectGeneralSettings()
{
    const auto form = readGeneralSettingsForm();
    const auto& saved = m_expectedSettings;
    // 显示的默认上限不等于用户显式修改，恢复原值也必须恢复到保存基线。
    const auto edited = [](const auto& value, const auto& shown, const auto& stored) { return value == shown ? stored : value; };
    m_settings.defaultPrinter = edited(form.defaultPrinter, m_formBaseline.defaultPrinter, saved.defaultPrinter);
    m_settings.labelOffset.x = edited(form.labelOffset.x, m_formBaseline.labelOffset.x, saved.labelOffset.x);
    m_settings.labelOffset.y = edited(form.labelOffset.y, m_formBaseline.labelOffset.y, saved.labelOffset.y);
    m_settings.allowedOrigins = edited(form.allowedOrigins, m_formBaseline.allowedOrigins, saved.allowedOrigins);
    const auto& value = form.localHttpLimits;
    const auto& shown = m_formBaseline.localHttpLimits;
    const auto& stored = saved.localHttpLimits;
    m_settings.localHttpLimits.maxHeaderBytes = edited(value.maxHeaderBytes, shown.maxHeaderBytes, stored.maxHeaderBytes);
    m_settings.localHttpLimits.maxContentLengthBytes = edited(value.maxContentLengthBytes, shown.maxContentLengthBytes, stored.maxContentLengthBytes);
    m_settings.localHttpLimits.maxPreviewBatchItems = edited(value.maxPreviewBatchItems, shown.maxPreviewBatchItems, stored.maxPreviewBatchItems);
    m_settings.localHttpLimits.maxPreviewPages = edited(value.maxPreviewPages, shown.maxPreviewPages, stored.maxPreviewPages);
    m_settings.localHttpLimits.maxPreviewResponseBytes = edited(value.maxPreviewResponseBytes, shown.maxPreviewResponseBytes, stored.maxPreviewResponseBytes);
}

core::PrintClientSettings SettingsWindow::readGeneralSettingsForm() const
{
    core::PrintClientSettings form;
    form.defaultPrinter = m_printerCombo->currentData().toString();
    form.labelOffset = sleekpr::core::LabelOffset{
        m_offsetXSpin->value(),
        m_offsetYSpin->value(),
    };

    QStringList allowedOrigins;
    const auto originLines = m_allowedOriginsEdit->toPlainText().split(QLatin1Char('\n'));
    for (const auto& line : originLines) {
        const auto origin = line.trimmed();
        if (!origin.isEmpty() && !allowedOrigins.contains(origin)) {
            allowedOrigins.append(origin);
        }
    }
    form.allowedOrigins = allowedOrigins;
    form.localHttpLimits.maxHeaderBytes = kibibytesToBytes(m_httpMaxHeaderKbSpin->value());
    form.localHttpLimits.maxContentLengthBytes = mebibytesToBytes(m_httpMaxBodyMbSpin->value());
    form.localHttpLimits.maxPreviewBatchItems = m_httpMaxPreviewBatchItemsSpin->value();
    form.localHttpLimits.maxPreviewPages = m_httpMaxPreviewPagesSpin->value();
    form.localHttpLimits.maxPreviewResponseBytes = mebibytesToBytes(m_httpMaxPreviewResponseMbSpin->value());
    return form;
}

sleekpr::core::LabelTemplateKey SettingsWindow::currentLabelTemplateKey() const
{
    return sleekpr::core::labelTemplateKeyFromOverrideKey(currentTemplateOverrideKey());
}

QString SettingsWindow::currentTemplateOverrideKey() const
{
    return m_currentTemplateOverrideKey.isEmpty() ? QStringLiteral("default") : m_currentTemplateOverrideKey;
}


void SettingsWindow::resetCurrentElement()
{
    if (!m_loadingForm) ++m_modelEditRevision;
    if (m_currentElementKey.isEmpty()) {
        return;
    }
    if (isCurrentCustomElement()) {
        m_statusLabel->setText(QString::fromUtf8("自定义元素请使用删除按钮，尚未保存"));
        return;
    }

    collectGeneralSettings();
    sleekpr::core::SettingsEditModel::resetElement(m_settings, currentTemplateOverrideKey(), m_currentElementKey);
    loadCurrentElementForm();
    renderPreview(m_settings);
    if (m_onSettingsApplied) {
        m_onSettingsApplied(m_settings);
    }
    m_statusLabel->setText(QString::fromUtf8("已重置当前元素，尚未保存"));
}

void SettingsWindow::resetAllElements()
{
    if (!m_loadingForm) ++m_modelEditRevision;
    collectGeneralSettings();
    sleekpr::core::SettingsEditModel::resetAllElements(m_settings, currentTemplateOverrideKey());
    loadCurrentElementForm();
    renderPreview(m_settings);
    if (m_onSettingsApplied) {
        m_onSettingsApplied(m_settings);
    }
    m_statusLabel->setText(QString::fromUtf8("已重置全部元素，尚未保存"));
}

void SettingsWindow::addTemplateElement(sleekpr::core::TemplateElementType type)
{
    if (!m_loadingForm) ++m_modelEditRevision;
    storeCurrentElementForm();
    collectGeneralSettings();

    sleekpr::core::TemplateElement element;
    element.id = QStringLiteral("custom_%1").arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
    element.type = type;
    element.displayName = templateElementTypeName(type);
    element.x = 5.0;
    element.y = 5.0;
    element.fontSizePt = 4.0;

    switch (type) {
    case sleekpr::core::TemplateElementType::FixedText:
        element.width = 18.0;
        element.height = 3.0;
        element.text = QString::fromUtf8("固定文本");
        break;
    case sleekpr::core::TemplateElementType::BoundField:
        element.width = 22.0;
        element.height = 3.0;
        element.fieldKey = QStringLiteral("productName");
        break;
    case sleekpr::core::TemplateElementType::QrCode:
        element.width = 8.0;
        element.height = 8.0;
        element.fieldKey = QStringLiteral("identifierText");
        break;
    case sleekpr::core::TemplateElementType::Rectangle:
        element.width = 15.0;
        element.height = 5.0;
        break;
    }

    m_settings.templateElements[currentTemplateOverrideKey()].append(element);
    populateElements();
    selectElementByKey(element.id);
    renderPreview(m_settings);
    m_statusLabel->setText(QString::fromUtf8("已新增自定义元素，尚未保存"));
}

void SettingsWindow::deleteCurrentCustomElement()
{
    if (!m_loadingForm) ++m_modelEditRevision;
    if (!isCurrentCustomElement()) {
        return;
    }

    collectGeneralSettings();
    const auto elementKey = m_currentElementKey;
    const int previousRow = m_elementList->currentRow();
    auto& elements = m_settings.templateElements[currentTemplateOverrideKey()];
    for (int index = 0; index < elements.size(); ++index) {
        if (elements[index].id == elementKey) {
            elements.removeAt(index);
            break;
        }
    }

    m_currentElementKey.clear();
    populateElements();
    if (m_elementList->count() > 0) {
        const int nextRow = std::clamp(previousRow, 0, m_elementList->count() - 1);
        m_loadingForm = true;
        m_elementList->setCurrentRow(nextRow);
        m_loadingForm = false;
        m_currentElementKey = m_elementList->item(nextRow)->data(Qt::UserRole).toString();
        loadCurrentElementForm();
    }
    renderPreview(m_settings);
    m_statusLabel->setText(QString::fromUtf8("已删除自定义元素，尚未保存"));
}

void SettingsWindow::refreshPreviewOnly()
{
    const auto selectedTemplateKey = currentTemplateOverrideKey();
    collectGeneralSettings();
    populateTemplateCombo(selectedTemplateKey);
    renderPreview(m_settings);
}

void SettingsWindow::applyPreviewDragDelta(const QPoint& pixelDelta)
{
    if (!m_loadingForm) ++m_modelEditRevision;
    if (m_currentElementKey.isEmpty() || m_previewImageSize.isEmpty()) {
        return;
    }

    const auto elementSize = currentElementSizeMm();
    if (elementSize.isEmpty()) {
        return;
    }

    const auto paperSize = sleekpr::core::LabelPaperSize::create80x30();
    const TemplateDragCoordinateMapper mapper(
        QSizeF(paperSize.widthMm(), paperSize.heightMm()),
        m_previewImageSize);
    const auto nextTopLeft = mapper.movedTopLeftMm(
        QPointF(m_elementXSpin->value(), m_elementYSpin->value()),
        pixelDelta,
        elementSize);
    const auto snappedTopLeft = snappedElementTopLeftMm(nextTopLeft, elementSize);

    // 拖拽编辑只改变当前选中元素的毫米坐标，字号、加粗和整体偏移仍由原控件负责。
    m_elementXSpin->setValue(snappedTopLeft.x());
    m_elementYSpin->setValue(snappedTopLeft.y());
    storeCurrentElementForm();
    collectGeneralSettings();
    renderPreview(m_settings);
    if (m_onSettingsApplied) {
        m_onSettingsApplied(m_settings);
    }
    m_statusLabel->setText(QString::fromUtf8("已拖动当前元素，尚未保存"));
}

void SettingsWindow::applyPreviewKeyboardNudge(const QPoint& direction, Qt::KeyboardModifiers modifiers)
{
    if (!m_loadingForm) ++m_modelEditRevision;
    if (m_currentElementKey.isEmpty() || direction.isNull()) {
        return;
    }

    const auto elementSize = currentElementSizeMm();
    if (elementSize.isEmpty()) {
        return;
    }

    const double stepMm = modifiers.testFlag(Qt::ShiftModifier)
        ? keyboardLargeNudgeStepMm
        : keyboardNudgeStepMm;
    const QPointF requestedTopLeft(
        m_elementXSpin->value() + direction.x() * stepMm,
        m_elementYSpin->value() + direction.y() * stepMm);
    const auto nextTopLeft = snappedElementTopLeftMm(requestedTopLeft, elementSize);

    // 键盘微调复用拖拽的表单写入路径，确保预览、应用回调和未保存状态完全一致。
    m_elementXSpin->setValue(nextTopLeft.x());
    m_elementYSpin->setValue(nextTopLeft.y());
    storeCurrentElementForm();
    collectGeneralSettings();
    renderPreview(m_settings);
    if (m_onSettingsApplied) {
        m_onSettingsApplied(m_settings);
    }
    m_statusLabel->setText(QString::fromUtf8("已微调当前元素，尚未保存"));
}

void SettingsWindow::updatePreviewDraggingState()
{
    if (m_previewLabel == nullptr) {
        return;
    }

    // 设置窗口只负责正式预览，元素编辑统一进入模板设计器处理。
    m_previewLabel->setDraggingEnabled(false);
}

QSizeF SettingsWindow::currentElementSizeMm() const
{
    if (m_currentElementKey.isEmpty()) {
        return QSizeF();
    }

    if (const auto* custom = customElement(m_currentElementKey)) {
        return QSizeF(custom->width, custom->height);
    }

    const auto labelPlan = sleekpr::core::LabelRenderPlanner().createPlan(
        sleekpr::infrastructure::PreviewLabelFactory::createDemoLabel(currentLabelTemplateKey()));
    const auto drawingPlan = sleekpr::core::NativeLabelDrawingPlanner().createPlan(labelPlan);
    for (const auto& command : drawingPlan.commands) {
        if (command.elementKey == m_currentElementKey) {
            // 元素尺寸只用于编辑器边界限制，不写入 settings.json，也不改变领域模板定义。
            return QSizeF(command.width, command.height);
        }
    }
    return QSizeF();
}

QRect SettingsWindow::currentElementRectPx() const
{
    if (m_currentElementKey.isEmpty() || m_previewImageSize.isEmpty()) {
        return QRect();
    }

    const auto elementSizeMm = currentElementSizeMm();
    if (elementSizeMm.isEmpty()) {
        return QRect();
    }

    const auto paperSize = sleekpr::core::LabelPaperSize::create80x30();
    const double scaleX = m_previewImageSize.width() / paperSize.widthMm();
    const double scaleY = m_previewImageSize.height() / paperSize.heightMm();
    const double xMm = m_elementXSpin->value() + m_settings.labelOffset.x;
    const double yMm = m_elementYSpin->value() + m_settings.labelOffset.y;

    // 命中区域按当前预览图中的真实像素位置计算，整体偏移同样参与换算。
    return QRect(
        QPoint(static_cast<int>(std::round(xMm * scaleX)), static_cast<int>(std::round(yMm * scaleY))),
        QSize(
            static_cast<int>(std::round(elementSizeMm.width() * scaleX)),
            static_cast<int>(std::round(elementSizeMm.height() * scaleY))));
}

bool SettingsWindow::canStartPreviewDragAt(const QPoint& position)
{
    storeCurrentElementForm();
    collectGeneralSettings();

    const auto elementKey = elementKeyAtPreviewPosition(position);
    if (!elementKey.has_value()) {
        return false;
    }

    // 鼠标按下命中任意已有元素时，先同步右侧列表选中项，再允许后续拖拽移动它。
    return selectElementByKey(elementKey.value());
}

std::optional<QString> SettingsWindow::elementKeyAtPreviewPosition(const QPoint& position) const
{
    if (m_previewImageSize.isEmpty()) {
        return std::nullopt;
    }

    const auto labelPlan = sleekpr::core::LabelRenderPlanner().createPlan(
        sleekpr::infrastructure::PreviewLabelFactory::createDemoLabel(currentLabelTemplateKey()));
    const auto drawingPlan = sleekpr::core::NativeLabelDrawingPlanner().createPlan(
        labelPlan,
        m_settings.labelOffset,
        m_settings.templateOverrides.value(currentTemplateOverrideKey()),
        m_settings.templateElements.value(currentTemplateOverrideKey()));
    const auto paperSize = sleekpr::core::LabelPaperSize::create80x30();
    return TemplateElementHitTester().hitTest(
        drawingPlan.commands,
        QSizeF(paperSize.widthMm(), paperSize.heightMm()),
        m_previewImageSize,
        position);
}

bool SettingsWindow::selectElementByKey(const QString& elementKey)
{
    if (elementKey.isEmpty()) {
        return false;
    }
    if (elementKey == m_currentElementKey) {
        return true;
    }

    for (int row = 0; row < m_elementList->count(); ++row) {
        if (m_elementList->item(row)->data(Qt::UserRole).toString() == elementKey) {
            m_elementList->setCurrentRow(row);
            return true;
        }
    }
    return false;
}

QPointF SettingsWindow::snappedElementTopLeftMm(QPointF topLeftMm, QSizeF elementSizeMm) const
{
    const auto paperSize = sleekpr::core::LabelPaperSize::create80x30();
    const double maxX = std::max(0.0, paperSize.widthMm() - elementSizeMm.width());
    const double maxY = std::max(0.0, paperSize.heightMm() - elementSizeMm.height());

    double nextX = topLeftMm.x();
    double nextY = topLeftMm.y();
    if (m_snapToGridCheck != nullptr && m_snapToGridCheck->isChecked()) {
        // 网格吸附只影响编辑器落点，不改变模板默认坐标和 settings.json 结构。
        nextX = snapToGrid(nextX);
        nextY = snapToGrid(nextY);
    }

    return QPointF(
        std::clamp(nextX, 0.0, maxX),
        std::clamp(nextY, 0.0, maxY));
}

void SettingsWindow::drawSelectionOverlay(QImage& image) const
{
    const auto rect = currentElementRectPx();
    if (rect.isNull()) {
        return;
    }

    QPainter painter(&image);
    QPen pen(QColor(37, 99, 235));
    pen.setWidth(2);
    pen.setStyle(Qt::DashLine);
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);
    painter.drawRect(rect.adjusted(1, 1, -1, -1));
}

bool SettingsWindow::isCurrentCustomElement() const
{
    return customElement(m_currentElementKey) != nullptr;
}

sleekpr::core::TemplateElement* SettingsWindow::mutableCustomElement(const QString& elementKey)
{
    auto& elements = m_settings.templateElements[currentTemplateOverrideKey()];
    for (auto& element : elements) {
        if (element.id == elementKey) {
            return &element;
        }
    }
    return nullptr;
}

const sleekpr::core::TemplateElement* SettingsWindow::customElement(const QString& elementKey) const
{
    const auto templateIt = m_settings.templateElements.constFind(currentTemplateOverrideKey());
    if (templateIt == m_settings.templateElements.constEnd()) {
        return nullptr;
    }
    for (const auto& element : templateIt.value()) {
        if (element.id == elementKey) {
            return &element;
        }
    }
    return nullptr;
}

void SettingsWindow::storeCustomElementForm(sleekpr::core::TemplateElement& element) const
{
    element.type = typeFromCombo(m_customElementTypeCombo);
    element.displayName = templateElementTypeName(element.type);
    element.x = m_elementXSpin->value();
    element.y = m_elementYSpin->value();
    element.width = m_elementWidthSpin->value();
    element.height = m_elementHeightSpin->value();
    element.fontSizePt = m_fontSizeSpin->value();
    element.bold = m_boldCheck->isChecked();

    element.text.clear();
    element.fieldKey.clear();
    element.payload.clear();
    switch (element.type) {
    case sleekpr::core::TemplateElementType::FixedText:
        element.text = m_customElementTextEdit->text();
        break;
    case sleekpr::core::TemplateElementType::BoundField:
        element.fieldKey = m_customElementFieldCombo->currentData().toString();
        break;
    case sleekpr::core::TemplateElementType::QrCode:
        element.payload = m_customElementTextEdit->text();
        element.fieldKey = m_customElementFieldCombo->currentData().toString();
        break;
    case sleekpr::core::TemplateElementType::Rectangle:
        break;
    }
}

void SettingsWindow::loadCustomElementForm(const sleekpr::core::TemplateElement& element)
{
    m_loadingForm = true;
    const int typeIndex = m_customElementTypeCombo->findData(static_cast<int>(element.type));
    m_customElementTypeCombo->setCurrentIndex(typeIndex >= 0 ? typeIndex : 0);
    const int fieldIndex = m_customElementFieldCombo->findData(element.fieldKey);
    m_customElementFieldCombo->setCurrentIndex(fieldIndex >= 0 ? fieldIndex : 0);
    m_customElementTextEdit->setText(element.type == sleekpr::core::TemplateElementType::QrCode ? element.payload : element.text);
    m_elementXSpin->setValue(element.x);
    m_elementYSpin->setValue(element.y);
    m_elementWidthSpin->setValue(element.width);
    m_elementHeightSpin->setValue(element.height);
    m_fontSizeSpin->setValue(element.fontSizePt);
    m_boldCheck->setChecked(element.bold);
    m_loadingForm = false;

    updateCustomElementControlState();
    updatePreviewDraggingState();
}

void SettingsWindow::updateCustomElementControlState()
{
    const bool custom = isCurrentCustomElement();
    const auto type = custom ? typeFromCombo(m_customElementTypeCombo) : sleekpr::core::TemplateElementType::FixedText;
    const bool textEnabled =
        custom &&
        (type == sleekpr::core::TemplateElementType::FixedText || type == sleekpr::core::TemplateElementType::QrCode);
    const bool fieldEnabled =
        custom &&
        (type == sleekpr::core::TemplateElementType::BoundField || type == sleekpr::core::TemplateElementType::QrCode);
    const bool fontEnabled =
        custom &&
        (type == sleekpr::core::TemplateElementType::FixedText || type == sleekpr::core::TemplateElementType::BoundField);

    m_customElementTypeCombo->setEnabled(custom);
    m_customElementTextEdit->setEnabled(textEnabled);
    m_customElementFieldCombo->setEnabled(fieldEnabled);
    m_elementWidthSpin->setEnabled(custom);
    m_elementHeightSpin->setEnabled(custom);
    m_fontSizeSpin->setEnabled(fontEnabled);
    m_boldCheck->setEnabled(fontEnabled);
    m_deleteCustomElementButton->setEnabled(custom);
}

sleekpr::core::TemplateElementOverride SettingsWindow::formOverrideForCurrentElement() const
{
    sleekpr::core::TemplateElementOverride value;
    value.x = m_elementXSpin->value();
    value.y = m_elementYSpin->value();

    // 二维码等非文本元素禁用字体控件，保存时也不写入字体覆盖。
    if (m_fontSizeSpin->isEnabled()) {
        value.fontSizePt = m_fontSizeSpin->value();
    }
    if (m_boldCheck->isEnabled()) {
        value.bold = m_boldCheck->isChecked();
    }

    return value;
}

sleekpr::core::TemplateElementOverride SettingsWindow::displayedOverrideForElement(const sleekpr::core::TemplateElementDefinition& element) const
{
    const auto templateOverrides = m_settings.templateOverrides.value(currentTemplateOverrideKey());
    const auto overrideValue = templateOverrides.value(element.key);

    sleekpr::core::TemplateElementOverride value = element.defaultValue;
    if (overrideValue.x.has_value()) {
        value.x = overrideValue.x;
    }
    if (overrideValue.y.has_value()) {
        value.y = overrideValue.y;
    }
    if (overrideValue.fontSizePt.has_value()) {
        value.fontSizePt = overrideValue.fontSizePt;
    }
    if (overrideValue.bold.has_value()) {
        value.bold = overrideValue.bold;
    }
    return value;
}

bool SettingsWindow::formMatchesDefault(
    const sleekpr::core::TemplateElementOverride& formValue,
    const sleekpr::core::TemplateElementDefinition& element) const
{
    const auto& defaultValue = element.defaultValue;
    const bool positionMatches =
        sameDouble(formValue.x.value_or(0.0), defaultValue.x.value_or(0.0)) &&
        sameDouble(formValue.y.value_or(0.0), defaultValue.y.value_or(0.0));
    const bool fontMatches =
        formValue.fontSizePt.has_value() == defaultValue.fontSizePt.has_value() &&
        (!formValue.fontSizePt.has_value() || sameDouble(formValue.fontSizePt.value(), defaultValue.fontSizePt.value()));
    const bool boldMatches =
        formValue.bold.has_value() == defaultValue.bold.has_value() &&
        (!formValue.bold.has_value() || formValue.bold.value() == defaultValue.bold.value());

    return positionMatches && fontMatches && boldMatches;
}

} // 命名空间 sleekpr::app

#include "sleekpr/app/SettingsWindow.h"

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
#include "sleekpr/infrastructure/preview/LabelPreviewImageRenderer.h"
#include "sleekpr/infrastructure/preview/LabelPreviewService.h"
#include "sleekpr/infrastructure/preview/PreviewLabelFactory.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
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
#include <QSplitter>
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

static QImage renderTemplateDocumentPreviewImage(
    const sleekpr::core::TemplateDocument& document,
    const sleekpr::core::PrintClientSettings& settings,
    const QString& fallbackTemplateKey)
{
    const auto rawTemplateKey = document.templateKey.trimmed().isEmpty() ? fallbackTemplateKey : document.templateKey.trimmed();
    const auto labelPlan = sleekpr::core::LabelRenderPlanner().createPlan(
        sleekpr::infrastructure::PreviewLabelFactory::createDemoLabel(
            sleekpr::core::labelTemplateKeyFromOverrideKey(rawTemplateKey)));

    sleekpr::core::TemplateRenderContext previewContext;
    // 设置页只做正式预览：模板绑定的 sampleData 用于模拟接口数据，真实打印仍由接口传入值覆盖。
    previewContext.values = document.sampleData;
    const auto profile = sleekpr::core::DeviceProfileResolver::resolve(document.deviceProfiles, settings.defaultPrinter);
    const auto drawingPlan = sleekpr::core::TemplateDocumentRenderer().render(
        document,
        labelPlan,
        settings.labelOffset,
        profile,
        previewContext);
    return sleekpr::infrastructure::LabelPreviewImageRenderer().renderImage(drawingPlan, drawingPlan.renderDpi);
}

SettingsWindow::SettingsWindow(QString settingsPath, SettingsAppliedCallback onSettingsApplied, QWidget* parent)
    : SettingsWindow(std::move(settingsPath), std::move(onSettingsApplied), QString(), parent)
{
}

SettingsWindow::SettingsWindow(
    QString settingsPath,
    SettingsAppliedCallback onSettingsApplied,
    QString templateLibraryDirectoryPath,
    QWidget* parent)
    : QWidget(parent)
    , m_settingsStore(std::move(settingsPath))
    , m_onSettingsApplied(std::move(onSettingsApplied))
    , m_templateLibraryDirectoryPath(std::move(templateLibraryDirectoryPath))
{
    setWindowTitle(QString::fromUtf8("sleekpr 设置"));
    resize(1320, 680);
    buildUi();
    reloadFromDisk();
}

void SettingsWindow::setOpenTemplateDesignerCallback(OpenTemplateDesignerCallback callback)
{
    m_openTemplateDesigner = std::move(callback);
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
    const auto selectedTemplateKey = currentTemplateOverrideKey();
    m_settings = m_settingsStore.load();
    populateTemplateCombo(selectedTemplateKey);
    m_currentElementKey.clear();
    populatePrinters();

    m_offsetXSpin->setValue(m_settings.labelOffset.x);
    m_offsetYSpin->setValue(m_settings.labelOffset.y);
    m_allowedOriginsEdit->setPlainText(m_settings.allowedOrigins.join(QStringLiteral("\n")));

    renderPreview(m_settings);
}

void SettingsWindow::buildUi()
{
    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(14, 14, 14, 14);
    rootLayout->setSpacing(10);

    auto* splitter = new QSplitter(Qt::Horizontal, this);
    splitter->addWidget(createPrinterPanel());
    splitter->addWidget(createPreviewPanel());
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);
    splitter->setSizes({260, 1040});
    rootLayout->addWidget(splitter, 1);

    auto* buttonLayout = new QHBoxLayout;
    auto* saveButton = new QPushButton(QString::fromUtf8("保存"));
    auto* applyButton = new QPushButton(QString::fromUtf8("应用但不保存"));
    auto* refreshButton = new QPushButton(QString::fromUtf8("刷新预览"));
    auto* openDesignerButton = new QPushButton(QString::fromUtf8("模板设计器"));
    auto* closeButton = new QPushButton(QString::fromUtf8("关闭"));

    auto* openPaperSpecManagerButton = new QPushButton(QString::fromUtf8("纸张规格"));
    auto* openFieldPresetManagerButton = new QPushButton(QString::fromUtf8("字段预设"));

    saveButton->setObjectName(QStringLiteral("saveSettingsButton"));
    openDesignerButton->setObjectName(QStringLiteral("openTemplateDesignerButton"));
    openPaperSpecManagerButton->setObjectName(QStringLiteral("openPaperSpecManagerButton"));
    openFieldPresetManagerButton->setObjectName(QStringLiteral("openFieldPresetManagerButton"));
    buttonLayout->addWidget(saveButton);
    buttonLayout->addWidget(applyButton);
    buttonLayout->addWidget(refreshButton);
    buttonLayout->addWidget(openDesignerButton);
    buttonLayout->addWidget(openPaperSpecManagerButton);
    buttonLayout->addWidget(openFieldPresetManagerButton);
    buttonLayout->addStretch();
    buttonLayout->addWidget(closeButton);
    rootLayout->addLayout(buttonLayout);

    connect(saveButton, &QPushButton::clicked, this, [this] { saveSettings(); });
    connect(applyButton, &QPushButton::clicked, this, [this] { applyWithoutSaving(); });
    connect(refreshButton, &QPushButton::clicked, this, [this] { refreshPreviewOnly(); });
    connect(openDesignerButton, &QPushButton::clicked, this, [this] {
        if (m_openTemplateDesigner) {
            m_openTemplateDesigner();
        }
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
    connect(closeButton, &QPushButton::clicked, this, [this] { close(); });
}

QWidget* SettingsWindow::createPrinterPanel()
{
    auto* panel = new QWidget(this);
    auto* layout = new QVBoxLayout(panel);
    layout->setContentsMargins(0, 0, 10, 0);

    auto* templateGroup = new QGroupBox(QString::fromUtf8("模板"));
    auto* templateLayout = new QVBoxLayout(templateGroup);
    m_templateCombo = new QComboBox(templateGroup);
    m_templateCombo->setObjectName(QStringLiteral("settingsTemplateCombo"));
    populateTemplateCombo();
    templateLayout->addWidget(m_templateCombo);

    auto* printerGroup = new QGroupBox(QString::fromUtf8("打印机"));
    auto* printerLayout = new QVBoxLayout(printerGroup);
    m_printerCombo = new QComboBox(printerGroup);
    auto* refreshPrintersButton = new QPushButton(QString::fromUtf8("刷新打印机"));
    printerLayout->addWidget(m_printerCombo);
    printerLayout->addWidget(refreshPrintersButton);

    auto* offsetGroup = new QGroupBox(QString::fromUtf8("整体偏移"));
    auto* offsetLayout = new QFormLayout(offsetGroup);
    m_offsetXSpin = millimeterSpinBox();
    m_offsetYSpin = millimeterSpinBox();
    offsetLayout->addRow(QString::fromUtf8("X"), m_offsetXSpin);
    offsetLayout->addRow(QString::fromUtf8("Y"), m_offsetYSpin);

    auto* securityGroup = new QGroupBox(QString::fromUtf8("本地接口安全"));
    auto* securityLayout = new QVBoxLayout(securityGroup);
    auto* allowedOriginsHint = new QLabel(
        QString::fromUtf8("允许访问本地服务的网页 Origin，每行一个；留空时只接受没有 Origin 的本机调用。"),
        securityGroup);
    allowedOriginsHint->setWordWrap(true);
    m_allowedOriginsEdit = new QPlainTextEdit(securityGroup);
    m_allowedOriginsEdit->setObjectName(QStringLiteral("allowedOriginsEdit"));
    m_allowedOriginsEdit->setPlaceholderText(QString::fromUtf8("https://manager.example.com"));
    m_allowedOriginsEdit->setFixedHeight(82);
    securityLayout->addWidget(allowedOriginsHint);
    securityLayout->addWidget(m_allowedOriginsEdit);

    layout->addWidget(templateGroup);
    layout->addWidget(printerGroup);
    layout->addWidget(offsetGroup);
    layout->addWidget(securityGroup);
    layout->addStretch();

    connect(refreshPrintersButton, &QPushButton::clicked, this, [this] {
        collectGeneralSettings();
        populatePrinters();
    });
    connect(m_templateCombo, &QComboBox::currentIndexChanged, this, [this] {
        m_currentTemplateOverrideKey = m_templateCombo->currentData().toString();
        m_currentElementKey.clear();
        renderPreview(m_settings);
    });

    return panel;
}

QWidget* SettingsWindow::createPreviewPanel()
{
    auto* panel = new QWidget(this);
    auto* layout = new QVBoxLayout(panel);
    layout->setContentsMargins(0, 0, 0, 0);

    m_statusLabel = new QLabel(panel);
    m_statusLabel->setText(QString::fromUtf8("预览尺寸：80mm x 30mm"));
    layout->addWidget(m_statusLabel);

    m_previewLabel = new TemplatePreviewLabel(panel);
    m_previewLabel->setObjectName(QStringLiteral("settingsPreviewLabel"));
    m_previewLabel->setAlignment(Qt::AlignCenter);
    m_previewLabel->setMinimumSize(945, 354);
    m_previewLabel->setStyleSheet(QStringLiteral("QLabel { background: white; border: 1px solid #111827; }"));
    m_previewLabel->setDraggingEnabled(false);

    auto* scrollArea = new QScrollArea(panel);
    scrollArea->setWidgetResizable(true);
    scrollArea->setAlignment(Qt::AlignCenter);
    scrollArea->setWidget(m_previewLabel);
    layout->addWidget(scrollArea, 1);

    return panel;
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
    m_settings.defaultPrinter = m_printerCombo->currentData().toString();
    m_settings.labelOffset = sleekpr::core::LabelOffset{
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
    m_settings.allowedOrigins = allowedOrigins;
}

sleekpr::core::LabelTemplateKey SettingsWindow::currentLabelTemplateKey() const
{
    return sleekpr::core::labelTemplateKeyFromOverrideKey(currentTemplateOverrideKey());
}

QString SettingsWindow::currentTemplateOverrideKey() const
{
    return m_currentTemplateOverrideKey.isEmpty() ? QStringLiteral("default") : m_currentTemplateOverrideKey;
}

void SettingsWindow::applyWithoutSaving()
{
    collectGeneralSettings();
    renderPreview(m_settings);
    if (m_onSettingsApplied) {
        m_onSettingsApplied(m_settings);
    }
    m_statusLabel->setText(QString::fromUtf8("已应用到预览，尚未保存"));
}

void SettingsWindow::saveSettings()
{
    collectGeneralSettings();
    if (!m_settingsStore.save(m_settings)) {
        QMessageBox::warning(this, QString::fromUtf8("保存失败"), QString::fromUtf8("无法写入设置文件"));
        return;
    }

    renderPreview(m_settings);
    if (m_onSettingsApplied) {
        m_onSettingsApplied(m_settings);
    }
    m_statusLabel->setText(QString::fromUtf8("已保存"));
}

void SettingsWindow::resetCurrentElement()
{
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

void SettingsWindow::renderPreview(const sleekpr::core::PrintClientSettings& settings)
{
    const auto image = createPreviewImage(settings);
    if (image.isNull()) {
        m_previewImageSize = QSize();
        m_previewLabel->setText(QString::fromUtf8("预览图生成失败"));
        updatePreviewDraggingState();
        return;
    }

    m_previewImageSize = image.size();
    m_previewLabel->setPixmap(QPixmap::fromImage(image));
    m_previewLabel->setFixedSize(image.size());
    updatePreviewDraggingState();
}

QImage SettingsWindow::createPreviewImage(const sleekpr::core::PrintClientSettings& settings) const
{
    const auto selectedTemplateKey = currentTemplateOverrideKey();
    const auto settingsDocumentIt = settings.templateDocuments.constFind(selectedTemplateKey);
    if (settingsDocumentIt != settings.templateDocuments.constEnd()) {
        return renderTemplateDocumentPreviewImage(settingsDocumentIt.value(), settings, selectedTemplateKey);
    }

    const auto libraryDocumentIt = m_libraryTemplateDocuments.constFind(selectedTemplateKey);
    if (libraryDocumentIt != m_libraryTemplateDocuments.constEnd()) {
        return renderTemplateDocumentPreviewImage(libraryDocumentIt.value(), settings, selectedTemplateKey);
    }

    return sleekpr::infrastructure::LabelPreviewService().renderDemoPreview(settings, currentLabelTemplateKey());
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

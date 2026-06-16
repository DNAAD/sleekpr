#include "sleekpr/app/TemplateDesignerWindow.h"

#include "sleekpr/app/TemplateDragCoordinateMapper.h"
#include "sleekpr/app/TemplateElementHitTester.h"
#include "sleekpr/app/TemplatePreviewLabel.h"
#include "sleekpr/core/labels/LabelRenderPlanner.h"
#include "sleekpr/core/native/NativeLabelDrawingPlanner.h"
#include "sleekpr/core/settings/TemplateElement.h"
#include "sleekpr/core/templates/DeviceProfileResolver.h"
#include "sleekpr/core/templates/TemplateDocumentFactory.h"
#include "sleekpr/core/templates/TemplateDocumentEditModel.h"
#include "sleekpr/core/templates/TemplateDocumentJson.h"
#include "sleekpr/core/templates/TemplateDocumentRenderer.h"
#include "sleekpr/core/templates/TemplateLibraryStore.h"
#include "sleekpr/infrastructure/preview/LabelPreviewImageRenderer.h"
#include "sleekpr/infrastructure/preview/PreviewLabelFactory.h"

#include <QDateTime>
#include <QDoubleSpinBox>
#include <QFile>
#include <QFileDialog>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMessageBox>
#include <QPixmap>
#include <QPushButton>
#include <QSaveFile>
#include <QSizePolicy>
#include <QUuid>
#include <QVBoxLayout>

#include <algorithm>
#include <utility>

namespace sleekpr::app {

namespace {

QString generatedLayerId()
{
    return QStringLiteral("layer-%1").arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
}

QString generatedElementId(const QString& prefix)
{
    return QStringLiteral("%1-%2").arg(prefix, QUuid::createUuid().toString(QUuid::WithoutBraces));
}

QString nextLayerName(int layerCount)
{
    return QString::fromUtf8("图层 %1").arg(layerCount + 1);
}

QString elementTypeKey(sleekpr::core::TemplateElementType type)
{
    switch (type) {
    case sleekpr::core::TemplateElementType::FixedText:
        return QStringLiteral("fixedText");
    case sleekpr::core::TemplateElementType::BoundField:
        return QStringLiteral("boundField");
    case sleekpr::core::TemplateElementType::QrCode:
        return QStringLiteral("qrCode");
    case sleekpr::core::TemplateElementType::Rectangle:
        return QStringLiteral("rectangle");
    }
    return QStringLiteral("fixedText");
}

QString elementTypeName(sleekpr::core::TemplateElementType type)
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

QString elementDisplayName(const sleekpr::core::TemplateElement& element, int index)
{
    const auto baseName = element.displayName.trimmed().isEmpty()
        ? QString::fromUtf8("%1 %2").arg(elementTypeName(element.type)).arg(index + 1)
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

QPushButton* createButton(const QString& text, const QString& objectName, QWidget* parent)
{
    auto* button = new QPushButton(text, parent);
    button->setObjectName(objectName);
    return button;
}

QDoubleSpinBox* createProfileSpinBox(
    const QString& objectName,
    double minimum,
    double maximum,
    double value,
    QWidget* parent)
{
    auto* spinBox = new QDoubleSpinBox(parent);
    spinBox->setObjectName(objectName);
    spinBox->setRange(minimum, maximum);
    spinBox->setDecimals(3);
    spinBox->setSingleStep(0.01);
    spinBox->setValue(value);
    return spinBox;
}

sleekpr::core::TemplateElement createDefaultElement(sleekpr::core::TemplateElementType type)
{
    sleekpr::core::TemplateElement element;
    element.type = type;
    element.visible = true;
    element.locked = false;
    element.x = 5.0;
    element.y = 5.0;
    element.width = 14.0;
    element.height = 4.0;
    element.fontSizePt = 4.0;
    element.bold = false;

    switch (type) {
    case sleekpr::core::TemplateElementType::FixedText:
        element.id = generatedElementId(QStringLiteral("fixedText"));
        element.displayName = QString::fromUtf8("固定文本");
        element.text = QString::fromUtf8("固定文本");
        break;
    case sleekpr::core::TemplateElementType::BoundField:
        element.id = generatedElementId(QStringLiteral("boundField"));
        element.displayName = QString::fromUtf8("绑定字段");
        element.fieldKey = QStringLiteral("productName");
        break;
    case sleekpr::core::TemplateElementType::QrCode:
        element.id = generatedElementId(QStringLiteral("qrCode"));
        element.displayName = QString::fromUtf8("二维码");
        element.fieldKey = QStringLiteral("qrPayload");
        element.width = 8.0;
        element.height = 8.0;
        break;
    case sleekpr::core::TemplateElementType::Rectangle:
        element.id = generatedElementId(QStringLiteral("rectangle"));
        element.displayName = QString::fromUtf8("矩形");
        element.width = 16.0;
        element.height = 6.0;
        break;
    }

    return element;
}

} // 匿名命名空间

TemplateDesignerWindow::TemplateDesignerWindow(
    sleekpr::core::PrintClientSettings settings,
    SettingsChangedCallback onSettingsChanged,
    QString templateLibraryDirectoryPath,
    QWidget* parent)
    : QWidget(parent)
    , m_settings(std::move(settings))
    , m_onSettingsChanged(std::move(onSettingsChanged))
    , m_templateLibraryDirectoryPath(std::move(templateLibraryDirectoryPath))
{
    setWindowTitle(QString::fromUtf8("sleekpr 模板设计器"));
    resize(1180, 680);
    buildUi();
    ensureCurrentTemplateDocument();
    refreshLayerList();
    refreshElementList();
    refreshPreview();
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

void TemplateDesignerWindow::buildUi()
{
    auto* rootLayout = new QHBoxLayout(this);
    rootLayout->setContentsMargins(14, 14, 14, 14);
    rootLayout->setSpacing(12);

    auto* layerPanel = new QWidget(this);
    auto* layerLayout = new QVBoxLayout(layerPanel);
    layerLayout->setContentsMargins(0, 0, 8, 0);

    auto* titleLabel = new QLabel(QString::fromUtf8("图层"), layerPanel);
    auto* addLayerButton = createButton(QString::fromUtf8("新增图层"), QStringLiteral("addLayerButton"), layerPanel);
    auto* deleteLayerButton = createButton(QString::fromUtf8("删除图层"), QStringLiteral("deleteLayerButton"), layerPanel);
    auto* lockLayerButton = createButton(QString::fromUtf8("锁定图层"), QStringLiteral("lockLayerButton"), layerPanel);
    auto* hideLayerButton = createButton(QString::fromUtf8("隐藏图层"), QStringLiteral("hideLayerButton"), layerPanel);
    auto* moveLayerUpButton = createButton(QString::fromUtf8("上移图层"), QStringLiteral("moveLayerUpButton"), layerPanel);
    auto* moveLayerDownButton = createButton(QString::fromUtf8("下移图层"), QStringLiteral("moveLayerDownButton"), layerPanel);
    auto* saveVersionButton = createButton(QString::fromUtf8("保存版本"), QStringLiteral("saveTemplateVersionButton"), layerPanel);
    auto* restoreVersionButton = createButton(QString::fromUtf8("恢复版本"), QStringLiteral("restoreTemplateVersionButton"), layerPanel);
    auto* importTemplateButton = createButton(QString::fromUtf8("导入模板"), QStringLiteral("importTemplateButton"), layerPanel);
    auto* exportTemplateButton = createButton(QString::fromUtf8("导出模板"), QStringLiteral("exportTemplateButton"), layerPanel);

    auto* saveToLibraryButton = createButton(QString::fromUtf8("保存到模板库"), QStringLiteral("saveTemplateToLibraryButton"), layerPanel);
    auto* loadFromLibraryButton = createButton(QString::fromUtf8("从模板库加载"), QStringLiteral("loadTemplateFromLibraryButton"), layerPanel);

    m_layerList = new QListWidget(layerPanel);
    m_layerList->setObjectName(QStringLiteral("templateLayerList"));

    auto* layerButtonGrid = new QGridLayout();
    layerButtonGrid->setContentsMargins(0, 0, 0, 0);
    layerButtonGrid->addWidget(addLayerButton, 0, 0);
    layerButtonGrid->addWidget(deleteLayerButton, 0, 1);
    layerButtonGrid->addWidget(lockLayerButton, 1, 0);
    layerButtonGrid->addWidget(hideLayerButton, 1, 1);
    layerButtonGrid->addWidget(moveLayerUpButton, 2, 0);
    layerButtonGrid->addWidget(moveLayerDownButton, 2, 1);

    auto* documentButtonGrid = new QGridLayout();
    documentButtonGrid->setContentsMargins(0, 0, 0, 0);
    documentButtonGrid->addWidget(saveVersionButton, 0, 0);
    documentButtonGrid->addWidget(restoreVersionButton, 0, 1);
    documentButtonGrid->addWidget(importTemplateButton, 1, 0);
    documentButtonGrid->addWidget(exportTemplateButton, 1, 1);
    documentButtonGrid->addWidget(saveToLibraryButton, 2, 0);
    documentButtonGrid->addWidget(loadFromLibraryButton, 2, 1);

    layerLayout->addWidget(titleLabel);
    layerLayout->addWidget(m_layerList, 1);
    layerLayout->addLayout(layerButtonGrid);
    layerLayout->addLayout(documentButtonGrid);

    auto* workspacePanel = new QWidget(this);
    auto* workspaceLayout = new QVBoxLayout(workspacePanel);
    workspaceLayout->setContentsMargins(0, 0, 0, 0);
    m_previewLabel = new TemplatePreviewLabel(workspacePanel);
    m_previewLabel->setObjectName(QStringLiteral("designerPreviewLabel"));
    m_previewLabel->setMinimumSize(800, 300);
    m_previewLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_previewLabel->setAlignment(Qt::AlignCenter);
    m_previewLabel->setScaledContents(true);
    m_previewLabel->setDraggingEnabled(true);

    m_statusLabel = new QLabel(QString::fromUtf8("模板设计器已就绪"), workspacePanel);
    workspaceLayout->addWidget(m_previewLabel, 1);
    workspaceLayout->addWidget(m_statusLabel);

    auto* elementPanel = new QWidget(this);
    auto* elementLayout = new QVBoxLayout(elementPanel);
    elementLayout->setContentsMargins(8, 0, 0, 0);
    auto* elementTitleLabel = new QLabel(QString::fromUtf8("元素"), elementPanel);
    m_elementList = new QListWidget(elementPanel);
    m_elementList->setObjectName(QStringLiteral("templateElementList"));

    auto* addFixedTextButton = createButton(QString::fromUtf8("固定文本"), QStringLiteral("designerAddFixedTextButton"), elementPanel);
    auto* addBoundFieldButton = createButton(QString::fromUtf8("绑定字段"), QStringLiteral("designerAddBoundFieldButton"), elementPanel);
    auto* addQrCodeButton = createButton(QString::fromUtf8("二维码"), QStringLiteral("designerAddQrCodeButton"), elementPanel);
    auto* addRectangleButton = createButton(QString::fromUtf8("矩形"), QStringLiteral("designerAddRectangleButton"), elementPanel);
    auto* deleteElementButton = createButton(QString::fromUtf8("删除元素"), QStringLiteral("deleteElementButton"), elementPanel);
    auto* lockElementButton = createButton(QString::fromUtf8("锁定元素"), QStringLiteral("lockElementButton"), elementPanel);
    auto* hideElementButton = createButton(QString::fromUtf8("隐藏元素"), QStringLiteral("hideElementButton"), elementPanel);
    auto* moveElementUpButton = createButton(QString::fromUtf8("上移元素"), QStringLiteral("moveElementUpButton"), elementPanel);
    auto* moveElementDownButton = createButton(QString::fromUtf8("下移元素"), QStringLiteral("moveElementDownButton"), elementPanel);

    auto* deviceProfileTitleLabel = new QLabel(QString::fromUtf8("设备校准"), elementPanel);
    m_deviceProfilePrinterEdit = new QLineEdit(elementPanel);
    m_deviceProfilePrinterEdit->setObjectName(QStringLiteral("deviceProfilePrinterEdit"));
    m_deviceProfilePrinterEdit->setPlaceholderText(QString::fromUtf8("打印机名称"));
    m_deviceProfileDpiSpin = createProfileSpinBox(QStringLiteral("deviceProfileDpiSpin"), 72.0, 1200.0, 300.0, elementPanel);
    m_deviceProfileScaleXSpin = createProfileSpinBox(QStringLiteral("deviceProfileScaleXSpin"), 0.5, 2.0, 1.0, elementPanel);
    m_deviceProfileScaleYSpin = createProfileSpinBox(QStringLiteral("deviceProfileScaleYSpin"), 0.5, 2.0, 1.0, elementPanel);
    m_deviceProfileOffsetXSpin = createProfileSpinBox(QStringLiteral("deviceProfileOffsetXSpin"), -50.0, 50.0, 0.0, elementPanel);
    m_deviceProfileOffsetYSpin = createProfileSpinBox(QStringLiteral("deviceProfileOffsetYSpin"), -50.0, 50.0, 0.0, elementPanel);
    auto* saveDeviceProfileButton = createButton(QString::fromUtf8("保存校准"), QStringLiteral("saveDeviceProfileButton"), elementPanel);

    auto* addElementGrid = new QGridLayout();
    addElementGrid->setContentsMargins(0, 0, 0, 0);
    addElementGrid->addWidget(addFixedTextButton, 0, 0);
    addElementGrid->addWidget(addBoundFieldButton, 0, 1);
    addElementGrid->addWidget(addQrCodeButton, 1, 0);
    addElementGrid->addWidget(addRectangleButton, 1, 1);

    auto* editElementGrid = new QGridLayout();
    editElementGrid->setContentsMargins(0, 0, 0, 0);
    editElementGrid->addWidget(deleteElementButton, 0, 0);
    editElementGrid->addWidget(lockElementButton, 0, 1);
    editElementGrid->addWidget(hideElementButton, 1, 0);
    editElementGrid->addWidget(moveElementUpButton, 1, 1);
    editElementGrid->addWidget(moveElementDownButton, 2, 0, 1, 2);

    auto* deviceProfileGrid = new QGridLayout();
    deviceProfileGrid->setContentsMargins(0, 0, 0, 0);
    deviceProfileGrid->addWidget(new QLabel(QString::fromUtf8("打印机"), elementPanel), 0, 0);
    deviceProfileGrid->addWidget(m_deviceProfilePrinterEdit, 0, 1);
    deviceProfileGrid->addWidget(new QLabel(QStringLiteral("DPI"), elementPanel), 1, 0);
    deviceProfileGrid->addWidget(m_deviceProfileDpiSpin, 1, 1);
    deviceProfileGrid->addWidget(new QLabel(QString::fromUtf8("横向缩放"), elementPanel), 2, 0);
    deviceProfileGrid->addWidget(m_deviceProfileScaleXSpin, 2, 1);
    deviceProfileGrid->addWidget(new QLabel(QString::fromUtf8("纵向缩放"), elementPanel), 3, 0);
    deviceProfileGrid->addWidget(m_deviceProfileScaleYSpin, 3, 1);
    deviceProfileGrid->addWidget(new QLabel(QString::fromUtf8("横向偏移"), elementPanel), 4, 0);
    deviceProfileGrid->addWidget(m_deviceProfileOffsetXSpin, 4, 1);
    deviceProfileGrid->addWidget(new QLabel(QString::fromUtf8("纵向偏移"), elementPanel), 5, 0);
    deviceProfileGrid->addWidget(m_deviceProfileOffsetYSpin, 5, 1);

    elementLayout->addWidget(elementTitleLabel);
    elementLayout->addWidget(m_elementList, 1);
    elementLayout->addLayout(addElementGrid);
    elementLayout->addLayout(editElementGrid);
    elementLayout->addWidget(deviceProfileTitleLabel);
    elementLayout->addLayout(deviceProfileGrid);
    elementLayout->addWidget(saveDeviceProfileButton);

    rootLayout->addWidget(layerPanel, 0);
    rootLayout->addWidget(workspacePanel, 1);
    rootLayout->addWidget(elementPanel, 0);

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
    connect(saveToLibraryButton, &QPushButton::clicked, this, [this] { saveCurrentTemplateToLibrary(); });
    connect(loadFromLibraryButton, &QPushButton::clicked, this, [this] { loadCurrentTemplateFromLibrary(); });
    connect(addFixedTextButton, &QPushButton::clicked, this, [this] { addFixedTextElement(); });
    connect(addBoundFieldButton, &QPushButton::clicked, this, [this] { addBoundFieldElement(); });
    connect(addQrCodeButton, &QPushButton::clicked, this, [this] { addQrCodeElement(); });
    connect(addRectangleButton, &QPushButton::clicked, this, [this] { addRectangleElement(); });
    connect(deleteElementButton, &QPushButton::clicked, this, [this] { deleteCurrentElement(); });
    connect(lockElementButton, &QPushButton::clicked, this, [this] { toggleCurrentElementLocked(); });
    connect(hideElementButton, &QPushButton::clicked, this, [this] { toggleCurrentElementVisible(); });
    connect(moveElementUpButton, &QPushButton::clicked, this, [this] { moveCurrentElementUp(); });
    connect(moveElementDownButton, &QPushButton::clicked, this, [this] { moveCurrentElementDown(); });
    connect(saveDeviceProfileButton, &QPushButton::clicked, this, [this] { saveDeviceProfile(); });
    connect(m_layerList, &QListWidget::currentRowChanged, this, [this] {
        refreshElementList();
        refreshPreview();
    });
    connect(m_previewLabel, &TemplatePreviewLabel::destroyed, this, [this] { m_previewLabel = nullptr; });
    m_previewLabel->setDragStartCallback([this](QPoint position) {
        const auto labelPlan = sleekpr::core::LabelRenderPlanner().createPlan(
            sleekpr::infrastructure::PreviewLabelFactory::createDemoLabel(sleekpr::core::LabelTemplateKey::Default80x30));
        const auto hit = TemplateElementHitTester().hitTest(
            m_previewCommands,
            QSizeF(labelPlan.paperSize.widthMm(), labelPlan.paperSize.heightMm()),
            m_previewLabel->size(),
            position);
        if (!hit.has_value() || !canEditElement(hit.value())) {
            return false;
        }
        selectElement(hit.value());
        return true;
    });
    m_previewLabel->setDragDeltaCallback([this](QPoint delta) { moveSelectedElementByPixels(delta); });
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
}

void TemplateDesignerWindow::addLayer()
{
    ensureCurrentTemplateDocument();
    auto& document = m_settings.templateDocuments[m_templateKey];

    const auto layerId = generatedLayerId();
    if (!sleekpr::core::TemplateDocumentEditModel::addLayer(document, layerId, nextLayerName(document.layers.size()))) {
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
    addElement(createDefaultElement(sleekpr::core::TemplateElementType::FixedText));
}

void TemplateDesignerWindow::addBoundFieldElement()
{
    addElement(createDefaultElement(sleekpr::core::TemplateElementType::BoundField));
}

void TemplateDesignerWindow::addQrCodeElement()
{
    addElement(createDefaultElement(sleekpr::core::TemplateElementType::QrCode));
}

void TemplateDesignerWindow::addRectangleElement()
{
    addElement(createDefaultElement(sleekpr::core::TemplateElementType::Rectangle));
}

void TemplateDesignerWindow::deleteCurrentElement()
{
    ensureCurrentTemplateDocument();
    auto& document = m_settings.templateDocuments[m_templateKey];
    const auto elementId = currentElementId();
    if (sleekpr::core::TemplateDocumentEditModel::deleteElement(document, elementId)) {
        refreshAll();
        notifySettingsChanged();
    }
}

void TemplateDesignerWindow::toggleCurrentElementLocked()
{
    ensureCurrentTemplateDocument();
    auto& document = m_settings.templateDocuments[m_templateKey];
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

    QString errorMessage;
    const auto& document = m_settings.templateDocuments[m_templateKey];
    // 设计器只负责发起保存，模板 id 安全性、目录创建和原子写入都交给模板库存储层。
    if (!sleekpr::core::TemplateLibraryStore(m_templateLibraryDirectoryPath).saveTemplate(document, &errorMessage)) {
        m_statusLabel->setText(errorMessage.isEmpty() ? QString::fromUtf8("保存到模板库失败") : errorMessage);
        return;
    }

    m_statusLabel->setText(QString::fromUtf8("已保存到模板库"));
}

void TemplateDesignerWindow::loadCurrentTemplateFromLibrary()
{
    if (m_templateLibraryDirectoryPath.trimmed().isEmpty()) {
        m_statusLabel->setText(QString::fromUtf8("未配置模板库目录"));
        return;
    }

    QString errorMessage;
    const auto loadedDocument = sleekpr::core::TemplateLibraryStore(m_templateLibraryDirectoryPath)
                                    .loadTemplate(QStringLiteral("template-") + m_templateKey, &errorMessage);
    if (!loadedDocument.has_value()) {
        m_statusLabel->setText(errorMessage.isEmpty() ? QString::fromUtf8("从模板库加载失败") : errorMessage);
        return;
    }

    // 加载后仍写回当前 settings 模板槽位，保持旧设置结构和预览刷新链路兼容。
    applyImportedTemplateDocument(loadedDocument.value());
    m_statusLabel->setText(QString::fromUtf8("已从模板库加载"));
    notifySettingsChanged();
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

void TemplateDesignerWindow::saveDeviceProfile()
{
    ensureCurrentTemplateDocument();
    auto& document = m_settings.templateDocuments[m_templateKey];
    const auto printerName = m_deviceProfilePrinterEdit->text().trimmed();

    sleekpr::core::DeviceProfile profile;
    profile.id = printerName.isEmpty()
        ? QStringLiteral("profile-default")
        : QStringLiteral("profile-%1").arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
    profile.printerName = printerName;
    profile.dpi = m_deviceProfileDpiSpin->value();
    profile.scaleX = m_deviceProfileScaleXSpin->value();
    profile.scaleY = m_deviceProfileScaleYSpin->value();
    profile.offsetX = m_deviceProfileOffsetXSpin->value();
    profile.offsetY = m_deviceProfileOffsetYSpin->value();
    profile.notes = QString::fromUtf8("模板设计器校准");

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
            ? nextLayerName(index)
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

    const auto selectedElementInCurrentLayer = std::any_of(layer->elements.begin(), layer->elements.end(), [&selectedElementId](const auto& element) {
        return element.id == selectedElementId;
    });
    if (!selectedElementInCurrentLayer) {
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
        item->setData(Qt::UserRole + 1, elementTypeKey(element.type));
    }

    // 刷新元素列表时只在当前图层内恢复选择，避免跨图层选择再次触发列表刷新。
    if (!selectElementInCurrentList(selectedElementId) && m_elementList->count() > 0) {
        m_elementList->setCurrentRow(0);
    }
    m_elementList->blockSignals(wasBlocked);
}

void TemplateDesignerWindow::refreshPreview()
{
    ensureCurrentTemplateDocument();
    if (m_previewLabel == nullptr) {
        return;
    }

    const auto labelItem = sleekpr::infrastructure::PreviewLabelFactory::createDemoLabel(
        sleekpr::core::LabelTemplateKey::Default80x30);
    const auto labelPlan = sleekpr::core::LabelRenderPlanner().createPlan(labelItem);
    const auto& document = m_settings.templateDocuments[m_templateKey];
    const auto printerName = m_deviceProfilePrinterEdit == nullptr ? QString() : m_deviceProfilePrinterEdit->text();
    const auto profile = sleekpr::core::DeviceProfileResolver::resolve(document.deviceProfiles, printerName);
    const auto drawingPlan = sleekpr::core::TemplateDocumentRenderer().render(document, labelPlan, m_settings.labelOffset, profile);
    m_previewCommands = drawingPlan.commands;

    const auto image = sleekpr::infrastructure::LabelPreviewImageRenderer().renderImage(drawingPlan, drawingPlan.renderDpi);
    m_previewLabel->setPixmap(QPixmap::fromImage(image));
}

void TemplateDesignerWindow::refreshAll()
{
    const auto layerId = currentLayerId();
    const auto elementId = currentElementId();
    refreshLayerList();
    selectLayer(layerId);
    refreshElementList();
    selectElement(elementId);
    refreshPreview();
}

void TemplateDesignerWindow::notifySettingsChanged()
{
    if (m_onSettingsChanged) {
        m_onSettingsChanged(m_settings);
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
    }
    return false;
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

void TemplateDesignerWindow::moveSelectedElementByPixels(QPoint delta)
{
    ensureCurrentTemplateDocument();
    auto* element = currentElement();
    if (element == nullptr || m_previewLabel == nullptr) {
        return;
    }

    const auto labelPlan = sleekpr::core::LabelRenderPlanner().createPlan(
        sleekpr::infrastructure::PreviewLabelFactory::createDemoLabel(sleekpr::core::LabelTemplateKey::Default80x30));
    const TemplateDragCoordinateMapper mapper(
        QSizeF(labelPlan.paperSize.widthMm(), labelPlan.paperSize.heightMm()),
        m_previewLabel->size());
    const auto elementId = element->id;
    const auto moved = mapper.movedTopLeftMm(
        QPointF(element->x, element->y),
        delta,
        QSizeF(element->width, element->height));

    auto& document = m_settings.templateDocuments[m_templateKey];
    if (sleekpr::core::TemplateDocumentEditModel::moveElement(document, elementId, moved.x(), moved.y())) {
        refreshAll();
        selectElement(elementId);
        notifySettingsChanged();
    }
}

void TemplateDesignerWindow::nudgeSelectedElement(QPoint direction, Qt::KeyboardModifiers modifiers)
{
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
        refreshAll();
        selectElement(elementId);
        notifySettingsChanged();
    }
}

} // 命名空间 sleekpr::app

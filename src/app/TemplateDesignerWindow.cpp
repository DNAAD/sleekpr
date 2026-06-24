#include "sleekpr/app/TemplateDesignerWindow.h"

#include "sleekpr/app/TemplateDragCoordinateMapper.h"
#include "sleekpr/app/TemplateElementHitTester.h"
#include "sleekpr/app/TemplatePreviewLabel.h"
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
#include <QAbstractItemView>
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
#include <QGridLayout>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QJsonArray>
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
#include <QSaveFile>
#include <QScrollArea>
#include <QSizePolicy>
#include <QSignalBlocker>
#include <QSplitter>
#include <QSpinBox>
#include <QStringList>
#include <QTabWidget>
#include <QTimer>
#include <QUuid>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>
#include <optional>
#include <utility>

namespace sleekpr::app {

namespace {

constexpr auto kDefaultPaperSpecId = "label-80x30";
constexpr int kTextAutoApplyDelayMs = 650;
constexpr int kControlAutoApplyDelayMs = 300;

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

QJsonObject defaultSampleDataObject()
{
    QJsonObject sampleData;
    sampleData.insert(QStringLiteral("sales_code"), QStringLiteral("606178PD35"));
    sampleData.insert(QStringLiteral("sale_attach_text"), QString::fromUtf8("附加:¥140.00"));
    sampleData.insert(QStringLiteral("identifier_code"), QStringLiteral("1210005822"));
    sampleData.insert(QStringLiteral("product_name"), QString::fromUtf8("足金串搭项链"));
    sampleData.insert(QStringLiteral("header_items"), QJsonArray{
        QJsonObject{{QStringLiteral("value"), QStringLiteral("2.99")}, {QStringLiteral("text"), QString::fromUtf8("素金KA")}},
        QJsonObject{{QStringLiteral("value"), QStringLiteral("4.13")}, {QStringLiteral("text"), QString::fromUtf8("素金PC")}},
        QJsonObject{{QStringLiteral("value"), QStringLiteral("3.91")}, {QStringLiteral("text"), QString::fromUtf8("素金PA")}},
        QJsonObject{{QStringLiteral("value"), QStringLiteral("11.49")}, {QStringLiteral("text"), QString::fromUtf8("素金PD")}},
    });
    return sampleData;
}

QString sampleDataJson(const QJsonObject& sampleData)
{
    return QString::fromUtf8(QJsonDocument(sampleData).toJson(QJsonDocument::Indented));
}

QString defaultSampleDataJson()
{
    return sampleDataJson(defaultSampleDataObject());
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

sleekpr::core::TemplateDocument createBlankTemplateDocument(
    const QString& id,
    const QString& name,
    const QString& templateKey,
    const QString& paperSpecId,
    const QList<sleekpr::core::DeviceProfile>& deviceProfiles)
{
    sleekpr::core::TemplateLayer baseLayer;
    baseLayer.id = QStringLiteral("base");
    baseLayer.name = QString::fromUtf8("基础图层");
    baseLayer.visible = true;

    sleekpr::core::TemplateDocument document;
    document.id = id;
    document.name = name;
    document.templateKey = templateKey;
    document.category = QStringLiteral("label");
    // 新建模板只继承纸张和设备校准上下文，版式内容必须从空白画布开始。
    document.paperSpecId = paperSpecId.trimmed().isEmpty()
        ? QString::fromLatin1(kDefaultPaperSpecId)
        : paperSpecId;
    document.layers = {baseLayer};
    document.sampleData = defaultSampleDataObject();
    document.deviceProfiles = deviceProfiles;
    return document;
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
    case sleekpr::core::TemplateElementType::ArrayGrid:
        return QStringLiteral("arrayGrid");
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
    case sleekpr::core::TemplateElementType::ArrayGrid:
        return QString::fromUtf8("数组网格");
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

QDoubleSpinBox* createTableSpinBox(
    const QString& objectName,
    double minimum,
    double maximum,
    double value,
    QWidget* parent)
{
    auto* spinBox = new QDoubleSpinBox(parent);
    spinBox->setObjectName(objectName);
    spinBox->setRange(minimum, maximum);
    spinBox->setDecimals(2);
    spinBox->setSingleStep(0.5);
    spinBox->setValue(value);
    return spinBox;
}

QSpinBox* createGridSpinBox(const QString& objectName, int minimum, int maximum, int value, QWidget* parent)
{
    auto* spinBox = new QSpinBox(parent);
    spinBox->setObjectName(objectName);
    spinBox->setRange(minimum, maximum);
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
    case sleekpr::core::TemplateElementType::ArrayGrid:
        element.id = generatedElementId(QStringLiteral("arrayGrid"));
        element.displayName = QString::fromUtf8("数组网格");
        element.width = 45.0;
        element.height = 12.0;
        element.fontSizePt = 4.0;
        element.dataPath = QStringLiteral("header_items");
        element.arrayGridRows = 2;
        element.arrayGridColumns = 3;
        element.arrayGridCellTemplate = QStringLiteral("${text}:${value}");
        element.arrayGridDrawBorders = true;
        break;
    }

    return element;
}

sleekpr::core::TableElement createDefaultTable()
{
    sleekpr::core::TableElement table;
    table.id = generatedElementId(QStringLiteral("table"));
    table.displayName = QString::fromUtf8("明细表");
    table.visible = true;
    table.locked = false;
    table.x = 5.0;
    table.y = 10.0;
    table.width = 70.0;
    table.height = 15.0;
    table.dataPath = QStringLiteral("items");
    table.headerRowHeightMm = 5.0;
    table.detailRowHeightMm = 5.0;
    table.drawBorders = true;
    table.repeatHeaderOnPage = true;

    sleekpr::core::TableColumn productNameColumn;
    productNameColumn.id = QStringLiteral("productName");
    productNameColumn.title = QString::fromUtf8("品名");
    productNameColumn.fieldKey = QStringLiteral("productName");
    productNameColumn.widthMm = 45.0;
    table.columns.append(productNameColumn);

    sleekpr::core::TableColumn weightColumn;
    weightColumn.id = QStringLiteral("weight");
    weightColumn.title = QString::fromUtf8("重量");
    weightColumn.fieldKey = QStringLiteral("weight");
    weightColumn.widthMm = 25.0;
    table.columns.append(weightColumn);

    return table;
}

QString formatTableColumns(const QList<sleekpr::core::TableColumn>& columns)
{
    // 第一批 UI 用单行文本维护列定义，后续可替换为专门的列编辑表格控件。
    QStringList parts;
    for (const auto& column : columns) {
        parts.append(QStringLiteral("%1=%2:%3").arg(
            column.title,
            column.fieldKey,
            QString::number(column.widthMm, 'f', 2)));
    }
    return parts.join(QStringLiteral(","));
}

QList<sleekpr::core::TableColumn> parseTableColumns(const QString& text)
{
    // 列配置语法：列标题=字段key:列宽，多列用英文逗号分隔，例如“品名=productName:45,重量=weight:25”。
    QList<sleekpr::core::TableColumn> columns;
    const auto parts = text.split(QStringLiteral(","), Qt::SkipEmptyParts);
    for (const auto& part : parts) {
        const auto trimmed = part.trimmed();
        const auto equalsIndex = trimmed.indexOf(QStringLiteral("="));
        const auto widthIndex = trimmed.lastIndexOf(QStringLiteral(":"));
        if (equalsIndex <= 0 || widthIndex <= equalsIndex + 1 || widthIndex >= trimmed.size() - 1) {
            return {};
        }

        bool widthOk = false;
        const auto width = trimmed.mid(widthIndex + 1).trimmed().toDouble(&widthOk);
        const auto title = trimmed.left(equalsIndex).trimmed();
        const auto fieldKey = trimmed.mid(equalsIndex + 1, widthIndex - equalsIndex - 1).trimmed();
        if (!widthOk || width <= 0.0 || title.isEmpty() || fieldKey.isEmpty()) {
            return {};
        }

        sleekpr::core::TableColumn column;
        column.id = fieldKey;
        column.title = title;
        column.fieldKey = fieldKey;
        column.widthMm = width;
        columns.append(column);
    }
    return columns;
}

bool sameDouble(double left, double right)
{
    return qFuzzyCompare(left + 1.0, right + 1.0);
}

bool sameTableColumn(const sleekpr::core::TableColumn& left, const sleekpr::core::TableColumn& right)
{
    return left.id == right.id
        && left.title == right.title
        && left.fieldKey == right.fieldKey
        && sameDouble(left.widthMm, right.widthMm)
        && left.widthMode == right.widthMode
        && sameDouble(left.flexWeight, right.flexWeight)
        && left.alignment == right.alignment
        && sameDouble(left.fontSizePt, right.fontSizePt)
        && left.bold == right.bold
        && left.ellipsis == right.ellipsis;
}

bool sameTableColumns(const QList<sleekpr::core::TableColumn>& left, const QList<sleekpr::core::TableColumn>& right)
{
    if (left.size() != right.size()) {
        return false;
    }
    for (qsizetype index = 0; index < left.size(); ++index) {
        if (!sameTableColumn(left[index], right[index])) {
            return false;
        }
    }
    return true;
}

bool sameTemplateElement(const sleekpr::core::TemplateElement& left, const sleekpr::core::TemplateElement& right)
{
    return left.id == right.id
        && left.layerId == right.layerId
        && left.zIndex == right.zIndex
        && left.visible == right.visible
        && left.locked == right.locked
        && left.type == right.type
        && left.displayName == right.displayName
        && sameDouble(left.x, right.x)
        && sameDouble(left.y, right.y)
        && sameDouble(left.width, right.width)
        && sameDouble(left.height, right.height)
        && left.text == right.text
        && left.fieldKey == right.fieldKey
        && left.payload == right.payload
        && sameDouble(left.fontSizePt, right.fontSizePt)
        && left.bold == right.bold
        && sameDouble(left.rotationDegrees, right.rotationDegrees)
        && left.verticalText == right.verticalText
        && left.dataPath == right.dataPath
        && left.arrayGridRows == right.arrayGridRows
        && left.arrayGridColumns == right.arrayGridColumns
        && sameDouble(left.arrayGridRowHeightMm, right.arrayGridRowHeightMm)
        && left.arrayGridCellTemplate == right.arrayGridCellTemplate
        && left.arrayGridDrawBorders == right.arrayGridDrawBorders
        && left.maxLines == right.maxLines
        && left.ellipsis == right.ellipsis
        && left.autoFitFont == right.autoFitFont
        && sameDouble(left.autoFitMinFontSizePt, right.autoFitMinFontSizePt)
        && sameDouble(left.autoFitMaxFontSizePt, right.autoFitMaxFontSizePt);
}

bool sameTableElement(const sleekpr::core::TableElement& left, const sleekpr::core::TableElement& right)
{
    return left.id == right.id
        && left.layerId == right.layerId
        && left.zIndex == right.zIndex
        && left.visible == right.visible
        && left.locked == right.locked
        && left.displayName == right.displayName
        && sameDouble(left.x, right.x)
        && sameDouble(left.y, right.y)
        && sameDouble(left.width, right.width)
        && sameDouble(left.height, right.height)
        && left.dataPath == right.dataPath
        && sameDouble(left.headerRowHeightMm, right.headerRowHeightMm)
        && sameDouble(left.detailRowHeightMm, right.detailRowHeightMm)
        && left.drawBorders == right.drawBorders
        && left.repeatHeaderOnPage == right.repeatHeaderOnPage
        && sameTableColumns(left.columns, right.columns);
}

bool ownsFocusWidget(const QWidget* owner, const QWidget* candidate)
{
    return owner != nullptr
        && candidate != nullptr
        && (owner == candidate || owner->isAncestorOf(candidate));
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

    auto* layerPanel = new QWidget(this);
    layerPanel->setObjectName(QStringLiteral("designerLayerPanel"));
    layerPanel->setMinimumWidth(250);
    auto* layerLayout = new QVBoxLayout(layerPanel);
    layerLayout->setContentsMargins(10, 10, 10, 10);
    layerLayout->setSpacing(8);

    auto* titleLabel = new QLabel(QString::fromUtf8("图层"), layerPanel);
    titleLabel->setObjectName(QStringLiteral("designerPanelTitle"));
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
    auto* prePrintTemplateButton = createButton(QString::fromUtf8("预打印当前模板"), QStringLiteral("prePrintCurrentTemplateButton"), layerPanel);
    auto* paperSpecTitleLabel = new QLabel(QString::fromUtf8("纸张规格"), layerPanel);
    m_paperSpecCombo = new QComboBox(layerPanel);
    m_paperSpecCombo->setObjectName(QStringLiteral("paperSpecCombo"));
    auto* openPaperSpecManagerButton = createButton(
        QString::fromUtf8("管理纸张规格"),
        QStringLiteral("openPaperSpecManagerFromDesignerButton"),
        layerPanel);
    auto* openFieldPresetManagerButton = createButton(
        QString::fromUtf8("管理字段预设"),
        QStringLiteral("openFieldPresetManagerFromDesignerButton"),
        layerPanel);

    auto* saveToLibraryButton = createButton(QString::fromUtf8("保存到模板库"), QStringLiteral("saveTemplateToLibraryButton"), layerPanel);
    auto* loadFromLibraryButton = createButton(QString::fromUtf8("从模板库加载"), QStringLiteral("loadTemplateFromLibraryButton"), layerPanel);
    auto* libraryTitleLabel = new QLabel(QString::fromUtf8("模板库"), layerPanel);
    libraryTitleLabel->setObjectName(QStringLiteral("designerPanelTitle"));
    saveToLibraryButton->setProperty("buttonRole", QStringLiteral("primary"));
    deleteLayerButton->setProperty("buttonRole", QStringLiteral("danger"));
    m_templateLibrarySearchEdit = new QLineEdit(layerPanel);
    m_templateLibrarySearchEdit->setObjectName(QStringLiteral("templateLibrarySearchEdit"));
    m_templateLibrarySearchEdit->setPlaceholderText(QString::fromUtf8("搜索名称、分类或模板键"));
    m_templateLibraryNameEdit = new QLineEdit(layerPanel);
    m_templateLibraryNameEdit->setObjectName(QStringLiteral("templateLibraryNameEdit"));
    m_templateLibraryNameEdit->setPlaceholderText(QString::fromUtf8("新建或重命名模板名称"));
    m_templateLibraryList = new QListWidget(layerPanel);
    m_templateLibraryList->setObjectName(QStringLiteral("templateLibraryList"));
    m_templateLibraryList->setMinimumHeight(84);
    m_templateLibraryList->setMaximumHeight(140);
    auto* refreshLibraryButton = createButton(QString::fromUtf8("刷新模板库"), QStringLiteral("refreshTemplateLibraryButton"), layerPanel);
    auto* loadSelectedLibraryButton = createButton(
        QString::fromUtf8("加载选中模板"),
        QStringLiteral("loadSelectedTemplateFromLibraryButton"),
        layerPanel);
    auto* createLibraryButton = createButton(QString::fromUtf8("新建模板"), QStringLiteral("createTemplateInLibraryButton"), layerPanel);
    auto* deleteLibraryButton = createButton(
        QString::fromUtf8("删除选中模板"),
        QStringLiteral("deleteSelectedTemplateFromLibraryButton"),
        layerPanel);
    deleteLibraryButton->setProperty("buttonRole", QStringLiteral("danger"));
    auto* duplicateLibraryButton = createButton(
        QString::fromUtf8("复制选中模板"),
        QStringLiteral("duplicateSelectedTemplateFromLibraryButton"),
        layerPanel);
    auto* renameLibraryButton = createButton(
        QString::fromUtf8("重命名模板"),
        QStringLiteral("renameSelectedTemplateFromLibraryButton"),
        layerPanel);

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

    auto* libraryButtonGrid = new QGridLayout();
    libraryButtonGrid->setContentsMargins(0, 0, 0, 0);
    libraryButtonGrid->addWidget(refreshLibraryButton, 0, 0);
    libraryButtonGrid->addWidget(loadSelectedLibraryButton, 0, 1);
    libraryButtonGrid->addWidget(createLibraryButton, 1, 0);
    libraryButtonGrid->addWidget(deleteLibraryButton, 1, 1);
    libraryButtonGrid->addWidget(duplicateLibraryButton, 2, 0);
    libraryButtonGrid->addWidget(renameLibraryButton, 2, 1);
    libraryButtonGrid->addWidget(loadFromLibraryButton, 3, 0, 1, 2);

    auto* leftResourceSplitter = new QSplitter(Qt::Vertical, layerPanel);
    leftResourceSplitter->setObjectName(QStringLiteral("designerLeftResourceSplitter"));
    leftResourceSplitter->setChildrenCollapsible(false);

    auto* librarySection = new QWidget(leftResourceSplitter);
    librarySection->setObjectName(QStringLiteral("designerLibrarySection"));
    auto* librarySectionLayout = new QVBoxLayout(librarySection);
    librarySectionLayout->setContentsMargins(8, 8, 8, 8);
    librarySectionLayout->setSpacing(8);
    librarySectionLayout->addWidget(libraryTitleLabel);
    librarySectionLayout->addWidget(m_templateLibrarySearchEdit);
    librarySectionLayout->addWidget(m_templateLibraryNameEdit);
    librarySectionLayout->addWidget(m_templateLibraryList, 1);
    librarySectionLayout->addLayout(libraryButtonGrid);

    auto* layerSection = new QWidget(leftResourceSplitter);
    layerSection->setObjectName(QStringLiteral("designerLayerSection"));
    auto* layerSectionLayout = new QVBoxLayout(layerSection);
    layerSectionLayout->setContentsMargins(8, 8, 8, 8);
    layerSectionLayout->setSpacing(8);
    layerSectionLayout->addWidget(titleLabel);
    layerSectionLayout->addWidget(m_layerList, 1);
    layerSectionLayout->addLayout(layerButtonGrid);
    layerSectionLayout->addLayout(documentButtonGrid);

    leftResourceSplitter->addWidget(librarySection);
    leftResourceSplitter->addWidget(layerSection);
    leftResourceSplitter->setSizes({260, 360});
    layerLayout->addWidget(leftResourceSplitter, 1);

    auto* workspacePanel = new QWidget(this);
    workspacePanel->setObjectName(QStringLiteral("designerWorkspacePanel"));
    auto* workspaceLayout = new QVBoxLayout(workspacePanel);
    workspaceLayout->setContentsMargins(10, 10, 10, 10);
    workspaceLayout->setSpacing(8);
    m_previewLabel = new TemplatePreviewLabel(workspacePanel);
    m_previewLabel->setObjectName(QStringLiteral("designerPreviewLabel"));
    m_previewLabel->setMinimumSize(800, 300);
    m_previewLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_previewLabel->setAlignment(Qt::AlignCenter);
    m_previewLabel->setScaledContents(false);
    m_previewLabel->setDraggingEnabled(true);
    m_previewLabel->setRulerVisible(true);
    m_previewLabel->setRulerPrecisionMm(1.0);
    m_previewLabel->setDesignAidVisible(true);
    auto* topToolbar = new QWidget(workspacePanel);
    topToolbar->setObjectName(QStringLiteral("designerTopToolbar"));
    auto* rulerOptionsLayout = new QHBoxLayout(topToolbar);
    rulerOptionsLayout->setContentsMargins(0, 0, 0, 0);
    rulerOptionsLayout->setSpacing(8);
    saveToLibraryButton->setText(QString::fromUtf8("保存"));
    importTemplateButton->setText(QString::fromUtf8("导入"));
    exportTemplateButton->setText(QString::fromUtf8("导出"));
    prePrintTemplateButton->setText(QString::fromUtf8("预打印"));
    m_undoButton = createButton(QString::fromUtf8("撤销"), QStringLiteral("designerUndoButton"), workspacePanel);
    m_redoButton = createButton(QString::fromUtf8("重做"), QStringLiteral("designerRedoButton"), workspacePanel);
    m_zoomCombo = new QComboBox(workspacePanel);
    m_zoomCombo->setObjectName(QStringLiteral("designerZoomCombo"));
    m_zoomCombo->addItem(QStringLiteral("75%"), 0.75);
    m_zoomCombo->addItem(QStringLiteral("100%"), 1.0);
    m_zoomCombo->addItem(QStringLiteral("125%"), 1.25);
    m_zoomCombo->addItem(QStringLiteral("150%"), 1.5);
    m_zoomCombo->addItem(QStringLiteral("200%"), 2.0);
    m_zoomCombo->setCurrentIndex(m_zoomCombo->findData(1.0));
    saveToLibraryButton->setProperty("buttonRole", QStringLiteral("primary"));
    prePrintTemplateButton->setProperty("buttonRole", QStringLiteral("primary"));
    rulerOptionsLayout->addWidget(saveToLibraryButton);
    rulerOptionsLayout->addWidget(prePrintTemplateButton);
    rulerOptionsLayout->addWidget(importTemplateButton);
    rulerOptionsLayout->addWidget(exportTemplateButton);
    rulerOptionsLayout->addWidget(m_undoButton);
    rulerOptionsLayout->addWidget(m_redoButton);
    rulerOptionsLayout->addWidget(new QLabel(QString::fromUtf8("缩放"), workspacePanel));
    rulerOptionsLayout->addWidget(m_zoomCombo);
    rulerOptionsLayout->addWidget(new QLabel(QString::fromUtf8("标尺精度"), workspacePanel));
    m_rulerPrecisionCombo = new QComboBox(workspacePanel);
    m_rulerPrecisionCombo->setObjectName(QStringLiteral("designerRulerPrecisionCombo"));
    m_rulerPrecisionCombo->addItem(QStringLiteral("0.5 mm"), 0.5);
    m_rulerPrecisionCombo->addItem(QStringLiteral("1 mm"), 1.0);
    m_rulerPrecisionCombo->addItem(QStringLiteral("5 mm"), 5.0);
    m_rulerPrecisionCombo->setCurrentIndex(m_rulerPrecisionCombo->findData(1.0));
    rulerOptionsLayout->addWidget(m_rulerPrecisionCombo);
    m_designAidCheck = new QCheckBox(QString::fromUtf8("辅助线/尺寸"), workspacePanel);
    m_designAidCheck->setObjectName(QStringLiteral("designerDesignAidCheck"));
    m_designAidCheck->setChecked(true);
    rulerOptionsLayout->addWidget(m_designAidCheck);
    rulerOptionsLayout->addStretch(1);
    rulerOptionsLayout->addWidget(paperSpecTitleLabel);
    rulerOptionsLayout->addWidget(m_paperSpecCombo);
    rulerOptionsLayout->addWidget(openPaperSpecManagerButton);
    rulerOptionsLayout->addWidget(openFieldPresetManagerButton);
    auto* previewScrollArea = new QScrollArea(workspacePanel);
    previewScrollArea->setWidgetResizable(false);
    previewScrollArea->setAlignment(Qt::AlignCenter);
    // 预览标签保持渲染图原始比例，滚动区只负责承载大纸张，不参与缩放。
    previewScrollArea->setWidget(m_previewLabel);

    auto* sampleDataTitleLabel = new QLabel(QString::fromUtf8("模拟数据"), workspacePanel);
    auto* sampleDataHeaderLayout = new QHBoxLayout();
    sampleDataHeaderLayout->setContentsMargins(0, 0, 0, 0);
    auto* saveSampleDataButton = createButton(QString::fromUtf8("保存模拟数据"), QStringLiteral("saveTemplateSampleDataButton"), workspacePanel);
    sampleDataHeaderLayout->addWidget(sampleDataTitleLabel);
    sampleDataHeaderLayout->addStretch(1);
    sampleDataHeaderLayout->addWidget(saveSampleDataButton);
    m_sampleDataEdit = new QPlainTextEdit(workspacePanel);
    m_sampleDataEdit->setObjectName(QStringLiteral("templateSampleDataEdit"));
    m_sampleDataEdit->setFixedHeight(124);
    m_sampleDataEdit->setPlaceholderText(QString::fromUtf8("输入 JSON，例如：{\"product_name\":\"足金串搭项链\"}"));
    m_sampleDataEdit->setPlainText(defaultSampleDataJson());
    connect(m_sampleDataEdit, &QPlainTextEdit::destroyed, this, [this] { m_sampleDataEdit = nullptr; });
    m_sampleDataErrorLabel = new QLabel(workspacePanel);
    m_sampleDataErrorLabel->setObjectName(QStringLiteral("templateSampleDataErrorLabel"));
    m_sampleDataErrorLabel->setWordWrap(true);
    m_sampleDataErrorLabel->setVisible(false);

    m_statusLabel = new QLabel(QString::fromUtf8("模板设计器已就绪"), workspacePanel);
    m_statusLabel->setObjectName(QStringLiteral("designerStatusLabel"));
    workspaceLayout->addWidget(topToolbar);
    workspaceLayout->addWidget(previewScrollArea, 1);
    workspaceLayout->addLayout(sampleDataHeaderLayout);
    workspaceLayout->addWidget(m_sampleDataEdit);
    workspaceLayout->addWidget(m_sampleDataErrorLabel);
    workspaceLayout->addWidget(m_statusLabel);

    auto* elementScrollArea = new QScrollArea(this);
    elementScrollArea->setWidgetResizable(true);
    elementScrollArea->setMinimumWidth(320);
    auto* elementPanel = new QWidget(elementScrollArea);
    elementPanel->setObjectName(QStringLiteral("designerInspectorPanel"));
    auto* elementLayout = new QVBoxLayout(elementPanel);
    elementLayout->setContentsMargins(10, 10, 10, 10);
    elementLayout->setSpacing(8);
    auto* elementTitleLabel = new QLabel(QString::fromUtf8("元素"), elementPanel);
    elementTitleLabel->setObjectName(QStringLiteral("designerPanelTitle"));
    m_elementList = new QListWidget(elementPanel);
    m_elementList->setObjectName(QStringLiteral("templateElementList"));
    m_elementList->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed);
    m_elementList->setDragDropMode(QAbstractItemView::InternalMove);
    m_elementList->setDefaultDropAction(Qt::MoveAction);
    m_elementList->setSelectionMode(QAbstractItemView::SingleSelection);
    m_elementList->setContextMenuPolicy(Qt::ActionsContextMenu);
    auto* renameElementAction = new QAction(QString::fromUtf8("重命名"), m_elementList);
    renameElementAction->setObjectName(QStringLiteral("renameElementAction"));
    auto* deleteElementAction = new QAction(QString::fromUtf8("删除"), m_elementList);
    deleteElementAction->setObjectName(QStringLiteral("deleteElementAction"));
    auto* lockElementAction = new QAction(QString::fromUtf8("锁定/解锁"), m_elementList);
    lockElementAction->setObjectName(QStringLiteral("lockElementAction"));
    auto* hideElementAction = new QAction(QString::fromUtf8("隐藏/显示"), m_elementList);
    hideElementAction->setObjectName(QStringLiteral("hideElementAction"));
    auto* moveElementUpAction = new QAction(QString::fromUtf8("上移"), m_elementList);
    moveElementUpAction->setObjectName(QStringLiteral("moveElementUpAction"));
    auto* moveElementDownAction = new QAction(QString::fromUtf8("下移"), m_elementList);
    moveElementDownAction->setObjectName(QStringLiteral("moveElementDownAction"));
    m_elementList->addAction(renameElementAction);
    m_elementList->addAction(deleteElementAction);
    m_elementList->addAction(lockElementAction);
    m_elementList->addAction(hideElementAction);
    m_elementList->addAction(moveElementUpAction);
    m_elementList->addAction(moveElementDownAction);

    auto* addFixedTextButton = createButton(QString::fromUtf8("固定文本"), QStringLiteral("designerAddFixedTextButton"), elementPanel);
    auto* addBoundFieldButton = createButton(QString::fromUtf8("绑定字段"), QStringLiteral("designerAddBoundFieldButton"), elementPanel);
    auto* addQrCodeButton = createButton(QString::fromUtf8("二维码"), QStringLiteral("designerAddQrCodeButton"), elementPanel);
    auto* addRectangleButton = createButton(QString::fromUtf8("矩形"), QStringLiteral("designerAddRectangleButton"), elementPanel);
    auto* addTableButton = createButton(QString::fromUtf8("表格"), QStringLiteral("designerAddTableButton"), elementPanel);
    auto* addArrayGridButton = createButton(QString::fromUtf8("数组网格"), QStringLiteral("designerAddArrayGridButton"), elementPanel);
    auto* deleteElementButton = createButton(QString::fromUtf8("删除元素"), QStringLiteral("deleteElementButton"), elementPanel);
    deleteElementButton->setProperty("buttonRole", QStringLiteral("danger"));
    auto* lockElementButton = createButton(QString::fromUtf8("锁定元素"), QStringLiteral("lockElementButton"), elementPanel);
    auto* hideElementButton = createButton(QString::fromUtf8("隐藏元素"), QStringLiteral("hideElementButton"), elementPanel);
    auto* moveElementUpButton = createButton(QString::fromUtf8("上移元素"), QStringLiteral("moveElementUpButton"), elementPanel);
    auto* moveElementDownButton = createButton(QString::fromUtf8("下移元素"), QStringLiteral("moveElementDownButton"), elementPanel);

    auto* tablePropertyTitleLabel = new QLabel(QString::fromUtf8("表格属性"), elementPanel);
    m_tableDisplayNameEdit = new QLineEdit(elementPanel);
    m_tableDisplayNameEdit->setObjectName(QStringLiteral("tableDisplayNameEdit"));
    m_tableDataPathEdit = new QLineEdit(elementPanel);
    m_tableDataPathEdit->setObjectName(QStringLiteral("tableDataPathEdit"));
    m_tableXSpin = createTableSpinBox(QStringLiteral("tableXSpin"), 0.0, 500.0, 5.0, elementPanel);
    m_tableYSpin = createTableSpinBox(QStringLiteral("tableYSpin"), 0.0, 500.0, 10.0, elementPanel);
    m_tableWidthSpin = createTableSpinBox(QStringLiteral("tableWidthSpin"), 1.0, 500.0, 70.0, elementPanel);
    m_tableHeightSpin = createTableSpinBox(QStringLiteral("tableHeightSpin"), 1.0, 500.0, 15.0, elementPanel);
    m_tableHeaderHeightSpin = createTableSpinBox(QStringLiteral("tableHeaderHeightSpin"), 1.0, 100.0, 5.0, elementPanel);
    m_tableDetailHeightSpin = createTableSpinBox(QStringLiteral("tableDetailHeightSpin"), 1.0, 100.0, 5.0, elementPanel);
    m_tableRepeatHeaderCheck = new QCheckBox(QString::fromUtf8("分页重复表头"), elementPanel);
    m_tableRepeatHeaderCheck->setObjectName(QStringLiteral("tableRepeatHeaderCheck"));
    m_tableDrawBordersCheck = new QCheckBox(QString::fromUtf8("绘制边框"), elementPanel);
    m_tableDrawBordersCheck->setObjectName(QStringLiteral("tableDrawBordersCheck"));
    m_tableColumnsEdit = new QLineEdit(elementPanel);
    m_tableColumnsEdit->setObjectName(QStringLiteral("tableColumnsEdit"));
    m_tableColumnsEdit->setPlaceholderText(QString::fromUtf8("品名=productName:45,重量=weight:25"));
    auto* applyTablePropertiesButton = createButton(
        QString::fromUtf8("应用表格属性"),
        QStringLiteral("applyTablePropertiesButton"),
        elementPanel);
    applyTablePropertiesButton->setProperty("buttonRole", QStringLiteral("primary"));

    auto* deviceProfileTitleLabel = new QLabel(QString::fromUtf8("设备校准"), elementPanel);
    m_deviceProfilePrinterEdit = new QLineEdit(elementPanel);
    m_deviceProfilePrinterEdit->setObjectName(QStringLiteral("deviceProfilePrinterEdit"));
    m_deviceProfilePrinterEdit->setPlaceholderText(QString::fromUtf8("打印机名称"));
    m_deviceProfileDpiSpin = createProfileSpinBox(QStringLiteral("deviceProfileDpiSpin"), 72.0, 1200.0, 300.0, elementPanel);
    m_deviceProfileScaleXSpin = createProfileSpinBox(QStringLiteral("deviceProfileScaleXSpin"), 0.5, 2.0, 1.0, elementPanel);
    m_deviceProfileScaleYSpin = createProfileSpinBox(QStringLiteral("deviceProfileScaleYSpin"), 0.5, 2.0, 1.0, elementPanel);
    m_deviceProfileOffsetXSpin = createProfileSpinBox(QStringLiteral("deviceProfileOffsetXSpin"), -50.0, 50.0, 0.0, elementPanel);
    m_deviceProfileOffsetYSpin = createProfileSpinBox(QStringLiteral("deviceProfileOffsetYSpin"), -50.0, 50.0, 0.0, elementPanel);
    m_deviceCalibrationExpectedWidthSpin = createProfileSpinBox(QStringLiteral("deviceCalibrationExpectedWidthSpin"), 1.0, 1000.0, 80.0, elementPanel);
    m_deviceCalibrationActualWidthSpin = createProfileSpinBox(QStringLiteral("deviceCalibrationActualWidthSpin"), 1.0, 1000.0, 80.0, elementPanel);
    m_deviceCalibrationExpectedHeightSpin = createProfileSpinBox(QStringLiteral("deviceCalibrationExpectedHeightSpin"), 1.0, 1000.0, 30.0, elementPanel);
    m_deviceCalibrationActualHeightSpin = createProfileSpinBox(QStringLiteral("deviceCalibrationActualHeightSpin"), 1.0, 1000.0, 30.0, elementPanel);
    auto* calculateDeviceCalibrationButton = createButton(
        QString::fromUtf8("按测量值计算缩放"),
        QStringLiteral("calculateDeviceCalibrationScaleButton"),
        elementPanel);
    auto* saveDeviceProfileButton = createButton(QString::fromUtf8("保存校准"), QStringLiteral("saveDeviceProfileButton"), elementPanel);
    saveDeviceProfileButton->setProperty("buttonRole", QStringLiteral("primary"));

    auto* addElementGrid = new QGridLayout();
    addElementGrid->setContentsMargins(0, 0, 0, 0);
    addElementGrid->addWidget(addFixedTextButton, 0, 0);
    addElementGrid->addWidget(addBoundFieldButton, 0, 1);
    addElementGrid->addWidget(addQrCodeButton, 1, 0);
    addElementGrid->addWidget(addRectangleButton, 1, 1);
    addElementGrid->addWidget(addTableButton, 2, 0);
    addElementGrid->addWidget(addArrayGridButton, 2, 1);

    auto* editElementGrid = new QGridLayout();
    editElementGrid->setContentsMargins(0, 0, 0, 0);
    editElementGrid->addWidget(deleteElementButton, 0, 0);
    editElementGrid->addWidget(lockElementButton, 0, 1);
    editElementGrid->addWidget(hideElementButton, 1, 0);
    editElementGrid->addWidget(moveElementUpButton, 1, 1);
    editElementGrid->addWidget(moveElementDownButton, 2, 0, 1, 2);

    auto* elementPropertyTitleLabel = new QLabel(QString::fromUtf8("元素属性"), elementPanel);
    auto* elementValueLabel = new QLabel(QString::fromUtf8("内容（可用 ${fieldKey} 占位）"), elementPanel);
    m_elementValueEdit = new QPlainTextEdit(elementPanel);
    m_elementValueEdit->setObjectName(QStringLiteral("elementValueEdit"));
    m_elementValueEdit->setFixedHeight(72);
    m_elementValueEdit->setPlaceholderText(QString::fromUtf8("例如：地址:${address}"));
    connect(m_elementValueEdit, &QPlainTextEdit::destroyed, this, [this] { m_elementValueEdit = nullptr; });
    m_elementXSpin = createTableSpinBox(QStringLiteral("elementXSpin"), 0.0, 1000.0, 5.0, elementPanel);
    m_elementYSpin = createTableSpinBox(QStringLiteral("elementYSpin"), 0.0, 1000.0, 5.0, elementPanel);
    m_elementWidthSpin = createTableSpinBox(QStringLiteral("elementWidthSpin"), 0.1, 1000.0, 10.0, elementPanel);
    m_elementHeightSpin = createTableSpinBox(QStringLiteral("elementHeightSpin"), 0.1, 1000.0, 3.0, elementPanel);
    m_elementFontSizeSpin = createTableSpinBox(QStringLiteral("elementFontSizeSpin"), 1.0, 96.0, 4.0, elementPanel);
    m_elementRotationSpin = createTableSpinBox(QStringLiteral("elementRotationSpin"), -360.0, 360.0, 0.0, elementPanel);
    m_elementBoldCheck = new QCheckBox(QString::fromUtf8("加粗"), elementPanel);
    m_elementBoldCheck->setObjectName(QStringLiteral("elementBoldCheck"));
    m_elementAutoFitFontCheck = new QCheckBox(QString::fromUtf8("自动适配字号"), elementPanel);
    m_elementAutoFitFontCheck->setObjectName(QStringLiteral("elementAutoFitFontCheck"));
    m_elementAutoFitMinFontSizeSpin = createTableSpinBox(QStringLiteral("elementAutoFitMinFontSizeSpin"), 1.0, 96.0, 3.0, elementPanel);
    m_elementAutoFitMaxFontSizeSpin = createTableSpinBox(QStringLiteral("elementAutoFitMaxFontSizeSpin"), 1.0, 96.0, 12.0, elementPanel);
    m_elementVerticalTextCheck = new QCheckBox(QString::fromUtf8("竖排文本"), elementPanel);
    m_elementVerticalTextCheck->setObjectName(QStringLiteral("elementVerticalTextCheck"));
    auto* arrayGridPropertyTitleLabel = new QLabel(QString::fromUtf8("数组网格属性"), elementPanel);
    m_arrayGridDataPathEdit = new QLineEdit(elementPanel);
    m_arrayGridDataPathEdit->setObjectName(QStringLiteral("arrayGridDataPathEdit"));
    m_arrayGridDataPathEdit->setPlaceholderText(QStringLiteral("header_items"));
    m_arrayGridRowsSpin = createGridSpinBox(QStringLiteral("arrayGridRowsSpin"), 1, 20, 2, elementPanel);
    m_arrayGridColumnsSpin = createGridSpinBox(QStringLiteral("arrayGridColumnsSpin"), 1, 20, 3, elementPanel);
    m_arrayGridRowHeightSpin = createTableSpinBox(QStringLiteral("arrayGridRowHeightSpin"), 0.0, 100.0, 0.0, elementPanel);
    m_arrayGridCellTemplateEdit = new QPlainTextEdit(elementPanel);
    m_arrayGridCellTemplateEdit->setObjectName(QStringLiteral("arrayGridCellTemplateEdit"));
    m_arrayGridCellTemplateEdit->setFixedHeight(56);
    m_arrayGridCellTemplateEdit->setPlaceholderText(QStringLiteral("${text}:${value}"));
    m_arrayGridDrawBordersCheck = new QCheckBox(QString::fromUtf8("绘制网格边框"), elementPanel);
    m_arrayGridDrawBordersCheck->setObjectName(QStringLiteral("arrayGridDrawBordersCheck"));
    auto* applyElementPropertiesButton = createButton(
        QString::fromUtf8("应用元素属性"),
        QStringLiteral("applyElementPropertiesButton"),
        elementPanel);
    applyElementPropertiesButton->setProperty("buttonRole", QStringLiteral("primary"));
    auto* applyArrayGridPropertiesButton = createButton(
        QString::fromUtf8("应用数组网格属性"),
        QStringLiteral("applyArrayGridPropertiesButton"),
        elementPanel);
    applyArrayGridPropertiesButton->setProperty("buttonRole", QStringLiteral("primary"));

    auto* tablePropertyGrid = new QGridLayout();
    tablePropertyGrid->setContentsMargins(0, 0, 0, 0);
    tablePropertyGrid->addWidget(new QLabel(QString::fromUtf8("名称"), elementPanel), 0, 0);
    tablePropertyGrid->addWidget(m_tableDisplayNameEdit, 0, 1);
    tablePropertyGrid->addWidget(new QLabel(QStringLiteral("dataPath"), elementPanel), 1, 0);
    tablePropertyGrid->addWidget(m_tableDataPathEdit, 1, 1);
    tablePropertyGrid->addWidget(new QLabel(QStringLiteral("X"), elementPanel), 2, 0);
    tablePropertyGrid->addWidget(m_tableXSpin, 2, 1);
    tablePropertyGrid->addWidget(new QLabel(QStringLiteral("Y"), elementPanel), 3, 0);
    tablePropertyGrid->addWidget(m_tableYSpin, 3, 1);
    tablePropertyGrid->addWidget(new QLabel(QString::fromUtf8("宽"), elementPanel), 4, 0);
    tablePropertyGrid->addWidget(m_tableWidthSpin, 4, 1);
    tablePropertyGrid->addWidget(new QLabel(QString::fromUtf8("高"), elementPanel), 5, 0);
    tablePropertyGrid->addWidget(m_tableHeightSpin, 5, 1);
    tablePropertyGrid->addWidget(new QLabel(QString::fromUtf8("表头高"), elementPanel), 6, 0);
    tablePropertyGrid->addWidget(m_tableHeaderHeightSpin, 6, 1);
    tablePropertyGrid->addWidget(new QLabel(QString::fromUtf8("明细高"), elementPanel), 7, 0);
    tablePropertyGrid->addWidget(m_tableDetailHeightSpin, 7, 1);
    tablePropertyGrid->addWidget(m_tableRepeatHeaderCheck, 8, 0, 1, 2);
    tablePropertyGrid->addWidget(m_tableDrawBordersCheck, 9, 0, 1, 2);
    tablePropertyGrid->addWidget(new QLabel(QString::fromUtf8("列"), elementPanel), 10, 0);
    tablePropertyGrid->addWidget(m_tableColumnsEdit, 10, 1);

    auto* elementGeometryGrid = new QGridLayout();
    elementGeometryGrid->setContentsMargins(0, 0, 0, 0);
    elementGeometryGrid->addWidget(new QLabel(QStringLiteral("X"), elementPanel), 0, 0);
    elementGeometryGrid->addWidget(m_elementXSpin, 0, 1);
    elementGeometryGrid->addWidget(new QLabel(QStringLiteral("Y"), elementPanel), 1, 0);
    elementGeometryGrid->addWidget(m_elementYSpin, 1, 1);
    elementGeometryGrid->addWidget(new QLabel(QString::fromUtf8("宽"), elementPanel), 2, 0);
    elementGeometryGrid->addWidget(m_elementWidthSpin, 2, 1);
    elementGeometryGrid->addWidget(new QLabel(QString::fromUtf8("高"), elementPanel), 3, 0);
    elementGeometryGrid->addWidget(m_elementHeightSpin, 3, 1);
    elementGeometryGrid->addWidget(new QLabel(QString::fromUtf8("字号"), elementPanel), 4, 0);
    elementGeometryGrid->addWidget(m_elementFontSizeSpin, 4, 1);
    elementGeometryGrid->addWidget(new QLabel(QString::fromUtf8("旋转"), elementPanel), 5, 0);
    elementGeometryGrid->addWidget(m_elementRotationSpin, 5, 1);
    elementGeometryGrid->addWidget(m_elementBoldCheck, 6, 0, 1, 2);
    elementGeometryGrid->addWidget(m_elementAutoFitFontCheck, 7, 0, 1, 2);
    elementGeometryGrid->addWidget(new QLabel(QString::fromUtf8("最小字号"), elementPanel), 8, 0);
    elementGeometryGrid->addWidget(m_elementAutoFitMinFontSizeSpin, 8, 1);
    elementGeometryGrid->addWidget(new QLabel(QString::fromUtf8("最大字号"), elementPanel), 9, 0);
    elementGeometryGrid->addWidget(m_elementAutoFitMaxFontSizeSpin, 9, 1);
    elementGeometryGrid->addWidget(m_elementVerticalTextCheck, 10, 0, 1, 2);

    auto* arrayGridPropertyGrid = new QGridLayout();
    arrayGridPropertyGrid->setContentsMargins(0, 0, 0, 0);
    arrayGridPropertyGrid->addWidget(new QLabel(QStringLiteral("dataPath"), elementPanel), 0, 0);
    arrayGridPropertyGrid->addWidget(m_arrayGridDataPathEdit, 0, 1);
    arrayGridPropertyGrid->addWidget(new QLabel(QString::fromUtf8("行数"), elementPanel), 1, 0);
    arrayGridPropertyGrid->addWidget(m_arrayGridRowsSpin, 1, 1);
    arrayGridPropertyGrid->addWidget(new QLabel(QString::fromUtf8("列数"), elementPanel), 2, 0);
    arrayGridPropertyGrid->addWidget(m_arrayGridColumnsSpin, 2, 1);
    arrayGridPropertyGrid->addWidget(new QLabel(QString::fromUtf8("行高(mm)"), elementPanel), 3, 0);
    arrayGridPropertyGrid->addWidget(m_arrayGridRowHeightSpin, 3, 1);
    arrayGridPropertyGrid->addWidget(new QLabel(QString::fromUtf8("单元格模板"), elementPanel), 4, 0, 1, 2);
    arrayGridPropertyGrid->addWidget(m_arrayGridCellTemplateEdit, 5, 0, 1, 2);
    arrayGridPropertyGrid->addWidget(m_arrayGridDrawBordersCheck, 6, 0, 1, 2);

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
    deviceProfileGrid->addWidget(new QLabel(QString::fromUtf8("期望宽度"), elementPanel), 6, 0);
    deviceProfileGrid->addWidget(m_deviceCalibrationExpectedWidthSpin, 6, 1);
    deviceProfileGrid->addWidget(new QLabel(QString::fromUtf8("实际宽度"), elementPanel), 7, 0);
    deviceProfileGrid->addWidget(m_deviceCalibrationActualWidthSpin, 7, 1);
    deviceProfileGrid->addWidget(new QLabel(QString::fromUtf8("期望高度"), elementPanel), 8, 0);
    deviceProfileGrid->addWidget(m_deviceCalibrationExpectedHeightSpin, 8, 1);
    deviceProfileGrid->addWidget(new QLabel(QString::fromUtf8("实际高度"), elementPanel), 9, 0);
    deviceProfileGrid->addWidget(m_deviceCalibrationActualHeightSpin, 9, 1);

    auto* inspectorTabs = new QTabWidget(elementPanel);
    inspectorTabs->setObjectName(QStringLiteral("designerInspectorTabs"));
    auto* elementTab = new QWidget(inspectorTabs);
    elementTab->setObjectName(QStringLiteral("designerElementInspectorTab"));
    auto* elementTabLayout = new QVBoxLayout(elementTab);
    elementTabLayout->setContentsMargins(8, 8, 8, 8);
    elementTabLayout->setSpacing(8);
    elementTabLayout->addWidget(elementTitleLabel);
    elementTabLayout->addWidget(m_elementList, 1);
    elementTabLayout->addLayout(addElementGrid);
    elementTabLayout->addLayout(editElementGrid);
    elementTabLayout->addWidget(elementPropertyTitleLabel);
    elementTabLayout->addWidget(elementValueLabel);
    elementTabLayout->addWidget(m_elementValueEdit);
    elementTabLayout->addLayout(elementGeometryGrid);
    elementTabLayout->addWidget(applyElementPropertiesButton);

    auto* tableTab = new QWidget(inspectorTabs);
    tableTab->setObjectName(QStringLiteral("designerTableInspectorTab"));
    auto* tableTabLayout = new QVBoxLayout(tableTab);
    tableTabLayout->setContentsMargins(8, 8, 8, 8);
    tableTabLayout->setSpacing(8);
    tableTabLayout->addWidget(tablePropertyTitleLabel);
    tableTabLayout->addLayout(tablePropertyGrid);
    tableTabLayout->addWidget(applyTablePropertiesButton);
    tableTabLayout->addStretch(1);

    auto* arrayGridTab = new QWidget(inspectorTabs);
    arrayGridTab->setObjectName(QStringLiteral("designerArrayGridInspectorTab"));
    auto* arrayGridTabLayout = new QVBoxLayout(arrayGridTab);
    arrayGridTabLayout->setContentsMargins(8, 8, 8, 8);
    arrayGridTabLayout->setSpacing(8);
    arrayGridTabLayout->addWidget(arrayGridPropertyTitleLabel);
    arrayGridTabLayout->addLayout(arrayGridPropertyGrid);
    arrayGridTabLayout->addWidget(applyArrayGridPropertiesButton);
    arrayGridTabLayout->addStretch(1);

    auto* deviceTab = new QWidget(inspectorTabs);
    deviceTab->setObjectName(QStringLiteral("designerDeviceInspectorTab"));
    auto* deviceTabLayout = new QVBoxLayout(deviceTab);
    deviceTabLayout->setContentsMargins(8, 8, 8, 8);
    deviceTabLayout->setSpacing(8);
    deviceTabLayout->addWidget(deviceProfileTitleLabel);
    deviceTabLayout->addLayout(deviceProfileGrid);
    deviceTabLayout->addWidget(calculateDeviceCalibrationButton);
    deviceTabLayout->addWidget(saveDeviceProfileButton);
    deviceTabLayout->addStretch(1);

    inspectorTabs->addTab(elementTab, QString::fromUtf8("元素"));
    inspectorTabs->addTab(tableTab, QString::fromUtf8("表格"));
    inspectorTabs->addTab(arrayGridTab, QString::fromUtf8("数组网格"));
    inspectorTabs->addTab(deviceTab, QString::fromUtf8("设备校准"));
    elementLayout->addWidget(inspectorTabs, 1);

    auto* mainSplitter = new QSplitter(Qt::Horizontal, shell);
    mainSplitter->setObjectName(QStringLiteral("designerMainSplitter"));
    mainSplitter->setChildrenCollapsible(false);
    mainSplitter->addWidget(layerPanel);
    mainSplitter->addWidget(workspacePanel);
    elementScrollArea->setWidget(elementPanel);
    mainSplitter->addWidget(elementScrollArea);
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
    connect(applyElementPropertiesButton, &QPushButton::clicked, this, [this] { applyCurrentElementProperties(); });
    connect(applyArrayGridPropertiesButton, &QPushButton::clicked, this, [this] { applyCurrentElementProperties(); });
    connect(applyTablePropertiesButton, &QPushButton::clicked, this, [this] { applyCurrentTableProperties(); });
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
    connect(m_elementValueEdit, &QPlainTextEdit::textChanged, this, [this] { scheduleElementAutoApply(kTextAutoApplyDelayMs); });
    connect(m_elementXSpin, &QDoubleSpinBox::valueChanged, this, [this] { scheduleElementAutoApply(kControlAutoApplyDelayMs); });
    connect(m_elementYSpin, &QDoubleSpinBox::valueChanged, this, [this] { scheduleElementAutoApply(kControlAutoApplyDelayMs); });
    connect(m_elementWidthSpin, &QDoubleSpinBox::valueChanged, this, [this] { scheduleElementAutoApply(kControlAutoApplyDelayMs); });
    connect(m_elementHeightSpin, &QDoubleSpinBox::valueChanged, this, [this] { scheduleElementAutoApply(kControlAutoApplyDelayMs); });
    connect(m_elementFontSizeSpin, &QDoubleSpinBox::valueChanged, this, [this] { scheduleElementAutoApply(kControlAutoApplyDelayMs); });
    connect(m_elementRotationSpin, &QDoubleSpinBox::valueChanged, this, [this] { scheduleElementAutoApply(kControlAutoApplyDelayMs); });
    connect(m_elementBoldCheck, &QCheckBox::toggled, this, [this] { scheduleElementAutoApply(kControlAutoApplyDelayMs); });
    connect(m_elementAutoFitFontCheck, &QCheckBox::toggled, this, [this](bool checked) {
        const auto* element = currentElement();
        const auto isTextElement = element != nullptr
            && (element->type == sleekpr::core::TemplateElementType::FixedText
                || element->type == sleekpr::core::TemplateElementType::BoundField);
        if (m_elementAutoFitMinFontSizeSpin != nullptr) {
            m_elementAutoFitMinFontSizeSpin->setEnabled(isTextElement && checked);
        }
        if (m_elementAutoFitMaxFontSizeSpin != nullptr) {
            m_elementAutoFitMaxFontSizeSpin->setEnabled(isTextElement && checked);
        }
        scheduleElementAutoApply(kControlAutoApplyDelayMs);
    });
    connect(m_elementAutoFitMinFontSizeSpin, &QDoubleSpinBox::valueChanged, this, [this] { scheduleElementAutoApply(kControlAutoApplyDelayMs); });
    connect(m_elementAutoFitMaxFontSizeSpin, &QDoubleSpinBox::valueChanged, this, [this] { scheduleElementAutoApply(kControlAutoApplyDelayMs); });
    connect(m_elementVerticalTextCheck, &QCheckBox::toggled, this, [this] { scheduleElementAutoApply(kControlAutoApplyDelayMs); });
    connect(m_arrayGridDataPathEdit, &QLineEdit::textChanged, this, [this] { scheduleElementAutoApply(kTextAutoApplyDelayMs); });
    connect(m_arrayGridRowsSpin, &QSpinBox::valueChanged, this, [this] { scheduleElementAutoApply(kControlAutoApplyDelayMs); });
    connect(m_arrayGridColumnsSpin, &QSpinBox::valueChanged, this, [this] { scheduleElementAutoApply(kControlAutoApplyDelayMs); });
    connect(m_arrayGridRowHeightSpin, &QDoubleSpinBox::valueChanged, this, [this] { scheduleElementAutoApply(kControlAutoApplyDelayMs); });
    connect(m_arrayGridCellTemplateEdit, &QPlainTextEdit::textChanged, this, [this] { scheduleElementAutoApply(kTextAutoApplyDelayMs); });
    connect(m_arrayGridDrawBordersCheck, &QCheckBox::toggled, this, [this] { scheduleElementAutoApply(kControlAutoApplyDelayMs); });
    connect(m_tableDisplayNameEdit, &QLineEdit::textChanged, this, [this] { scheduleTableAutoApply(kTextAutoApplyDelayMs); });
    connect(m_tableDataPathEdit, &QLineEdit::textChanged, this, [this] { scheduleTableAutoApply(kTextAutoApplyDelayMs); });
    connect(m_tableXSpin, &QDoubleSpinBox::valueChanged, this, [this] { scheduleTableAutoApply(kControlAutoApplyDelayMs); });
    connect(m_tableYSpin, &QDoubleSpinBox::valueChanged, this, [this] { scheduleTableAutoApply(kControlAutoApplyDelayMs); });
    connect(m_tableWidthSpin, &QDoubleSpinBox::valueChanged, this, [this] { scheduleTableAutoApply(kControlAutoApplyDelayMs); });
    connect(m_tableHeightSpin, &QDoubleSpinBox::valueChanged, this, [this] { scheduleTableAutoApply(kControlAutoApplyDelayMs); });
    connect(m_tableHeaderHeightSpin, &QDoubleSpinBox::valueChanged, this, [this] { scheduleTableAutoApply(kControlAutoApplyDelayMs); });
    connect(m_tableDetailHeightSpin, &QDoubleSpinBox::valueChanged, this, [this] { scheduleTableAutoApply(kControlAutoApplyDelayMs); });
    connect(m_tableRepeatHeaderCheck, &QCheckBox::toggled, this, [this] { scheduleTableAutoApply(kControlAutoApplyDelayMs); });
    connect(m_tableDrawBordersCheck, &QCheckBox::toggled, this, [this] { scheduleTableAutoApply(kControlAutoApplyDelayMs); });
    connect(m_tableColumnsEdit, &QLineEdit::textChanged, this, [this] { scheduleTableAutoApply(kTextAutoApplyDelayMs); });
    connect(m_arrayGridDataPathEdit, &QLineEdit::editingFinished, this, [this] { applyPendingElementAutoApply(); });
    connect(m_tableDisplayNameEdit, &QLineEdit::editingFinished, this, [this] { applyPendingTableAutoApply(); });
    connect(m_tableDataPathEdit, &QLineEdit::editingFinished, this, [this] { applyPendingTableAutoApply(); });
    connect(m_tableColumnsEdit, &QLineEdit::editingFinished, this, [this] { applyPendingTableAutoApply(); });
    const auto installFocusCommitFilter = [this](QWidget* editor) {
        if (editor == nullptr) {
            return;
        }
        editor->installEventFilter(this);
        for (auto* child : editor->findChildren<QWidget*>()) {
            child->installEventFilter(this);
        }
    };
    // 覆盖编辑控件和内部子控件，保证失焦时能提交最后一次尚在防抖中的修改。
    for (auto* editor : QList<QWidget*>{
             m_elementValueEdit,
             m_elementXSpin,
             m_elementYSpin,
             m_elementWidthSpin,
             m_elementHeightSpin,
             m_elementFontSizeSpin,
             m_elementRotationSpin,
             m_elementBoldCheck,
             m_elementAutoFitFontCheck,
             m_elementAutoFitMinFontSizeSpin,
             m_elementAutoFitMaxFontSizeSpin,
             m_elementVerticalTextCheck,
             m_arrayGridDataPathEdit,
             m_arrayGridRowsSpin,
             m_arrayGridColumnsSpin,
             m_arrayGridRowHeightSpin,
             m_arrayGridCellTemplateEdit,
             m_arrayGridDrawBordersCheck,
         }) {
        installFocusCommitFilter(editor);
    }
    for (auto* editor : QList<QWidget*>{
             m_tableDisplayNameEdit,
             m_tableDataPathEdit,
             m_tableXSpin,
             m_tableYSpin,
             m_tableWidthSpin,
             m_tableHeightSpin,
             m_tableHeaderHeightSpin,
             m_tableDetailHeightSpin,
             m_tableRepeatHeaderCheck,
             m_tableDrawBordersCheck,
             m_tableColumnsEdit,
         }) {
        installFocusCommitFilter(editor);
    }
    connect(qApp, &QApplication::focusChanged, this, [this](QWidget* previous, QWidget*) {
        const QList<QWidget*> elementEditors{
            m_elementValueEdit,
            m_elementXSpin,
            m_elementYSpin,
            m_elementWidthSpin,
            m_elementHeightSpin,
            m_elementFontSizeSpin,
            m_elementRotationSpin,
            m_elementAutoFitMinFontSizeSpin,
            m_elementAutoFitMaxFontSizeSpin,
            m_arrayGridDataPathEdit,
            m_arrayGridRowsSpin,
            m_arrayGridColumnsSpin,
            m_arrayGridRowHeightSpin,
            m_arrayGridCellTemplateEdit,
        };
        const auto wasElementEditor = std::any_of(elementEditors.cbegin(), elementEditors.cend(), [previous](const auto* editor) {
            return ownsFocusWidget(editor, previous);
        });
        if (wasElementEditor) {
            applyPendingElementAutoApply();
            return;
        }

        const QList<QWidget*> tableEditors{
            m_tableDisplayNameEdit,
            m_tableDataPathEdit,
            m_tableXSpin,
            m_tableYSpin,
            m_tableWidthSpin,
            m_tableHeightSpin,
            m_tableHeaderHeightSpin,
            m_tableDetailHeightSpin,
            m_tableColumnsEdit,
        };
        const auto wasTableEditor = std::any_of(tableEditors.cbegin(), tableEditors.cend(), [previous](const auto* editor) {
            return ownsFocusWidget(editor, previous);
        });
        if (wasTableEditor) {
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
    m_settings.templateDocuments[m_templateKey].paperSpecId = QString::fromLatin1(kDefaultPaperSpecId);
    m_settings.templateDocuments[m_templateKey].sampleData = defaultSampleDataObject();
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

void TemplateDesignerWindow::addTableElement()
{
    addTable(createDefaultTable());
}

void TemplateDesignerWindow::addArrayGridElement()
{
    addElement(createDefaultElement(sleekpr::core::TemplateElementType::ArrayGrid));
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
    const auto document = createBlankTemplateDocument(
        QStringLiteral("template-%1").arg(QUuid::createUuid().toString(QUuid::WithoutBraces)),
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
    document->id = QStringLiteral("template-%1").arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
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
    if (m_documentHistoryIndex <= 0 || m_documentHistoryIndex >= m_documentHistory.size()) {
        updateHistoryButtons();
        return;
    }

    --m_documentHistoryIndex;
    m_restoringDocumentHistory = true;
    m_settings.templateDocuments[m_templateKey] = m_documentHistory[m_documentHistoryIndex];
    refreshAll();
    notifySettingsChanged();
    m_restoringDocumentHistory = false;
    updateHistoryButtons();
    if (m_statusLabel != nullptr) {
        m_statusLabel->setText(QString::fromUtf8("已撤销"));
    }
}

void TemplateDesignerWindow::redoCurrentTemplateChange()
{
    if (m_documentHistoryIndex < 0 || m_documentHistoryIndex >= m_documentHistory.size() - 1) {
        updateHistoryButtons();
        return;
    }

    ++m_documentHistoryIndex;
    m_restoringDocumentHistory = true;
    m_settings.templateDocuments[m_templateKey] = m_documentHistory[m_documentHistoryIndex];
    refreshAll();
    notifySettingsChanged();
    m_restoringDocumentHistory = false;
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
        : QStringLiteral("profile-%1").arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
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
        item->setData(Qt::UserRole + 1, elementTypeKey(element.type));
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
    if (m_elementValueEdit == nullptr) {
        return;
    }

    ScopedBoolFlag updating(m_updatingPropertyEditors);
    const auto* element = currentElement();
    const QList<QWidget*> propertyControls{
        m_elementValueEdit,
        m_elementXSpin,
        m_elementYSpin,
        m_elementWidthSpin,
        m_elementHeightSpin,
        m_elementFontSizeSpin,
        m_elementRotationSpin,
        m_elementBoldCheck,
        m_elementAutoFitFontCheck,
        m_elementAutoFitMinFontSizeSpin,
        m_elementAutoFitMaxFontSizeSpin,
        m_elementVerticalTextCheck,
    };
    const QList<QWidget*> arrayGridControls{
        m_arrayGridDataPathEdit,
        m_arrayGridRowsSpin,
        m_arrayGridColumnsSpin,
        m_arrayGridRowHeightSpin,
        m_arrayGridCellTemplateEdit,
        m_arrayGridDrawBordersCheck,
    };
    if (element == nullptr || currentSelectionIsTable()) {
        m_elementValueEdit->clear();
        m_elementValueEdit->setPlaceholderText(QString::fromUtf8("请选择固定文本或绑定字段"));
        for (auto* control : propertyControls) {
            if (control != nullptr) {
                control->setEnabled(false);
            }
        }
        for (auto* control : arrayGridControls) {
            if (control != nullptr) {
                control->setEnabled(false);
            }
        }
        return;
    }

    const auto isTextElement = element->type == sleekpr::core::TemplateElementType::FixedText
        || element->type == sleekpr::core::TemplateElementType::BoundField;
    const auto isArrayGrid = element->type == sleekpr::core::TemplateElementType::ArrayGrid;
    const auto canEditValue = isTextElement || element->type == sleekpr::core::TemplateElementType::QrCode;
    for (auto* control : propertyControls) {
        if (control != nullptr) {
            control->setEnabled(true);
        }
    }
    m_elementValueEdit->setEnabled(canEditValue);
    for (auto* control : arrayGridControls) {
        if (control != nullptr) {
            control->setEnabled(isArrayGrid);
        }
    }
    if (m_elementFontSizeSpin != nullptr) {
        m_elementFontSizeSpin->setEnabled(isTextElement || isArrayGrid);
    }
    if (m_elementBoldCheck != nullptr) {
        m_elementBoldCheck->setEnabled(isTextElement || isArrayGrid);
    }
    if (m_elementAutoFitFontCheck != nullptr) {
        m_elementAutoFitFontCheck->setEnabled(isTextElement);
    }
    if (m_elementAutoFitMinFontSizeSpin != nullptr) {
        m_elementAutoFitMinFontSizeSpin->setEnabled(isTextElement && element->autoFitFont);
    }
    if (m_elementAutoFitMaxFontSizeSpin != nullptr) {
        m_elementAutoFitMaxFontSizeSpin->setEnabled(isTextElement && element->autoFitFont);
    }
    if (m_elementVerticalTextCheck != nullptr) {
        m_elementVerticalTextCheck->setEnabled(element->type == sleekpr::core::TemplateElementType::FixedText);
    }
    if (m_elementXSpin != nullptr) {
        m_elementXSpin->setValue(element->x);
    }
    if (m_elementYSpin != nullptr) {
        m_elementYSpin->setValue(element->y);
    }
    if (m_elementWidthSpin != nullptr) {
        m_elementWidthSpin->setValue(element->width);
    }
    if (m_elementHeightSpin != nullptr) {
        m_elementHeightSpin->setValue(element->height);
    }
    if (m_elementFontSizeSpin != nullptr) {
        m_elementFontSizeSpin->setValue(element->fontSizePt);
    }
    if (m_elementRotationSpin != nullptr) {
        m_elementRotationSpin->setValue(element->rotationDegrees);
    }
    if (m_elementBoldCheck != nullptr) {
        m_elementBoldCheck->setChecked(element->bold);
    }
    if (m_elementAutoFitFontCheck != nullptr) {
        m_elementAutoFitFontCheck->setChecked(element->autoFitFont);
    }
    if (m_elementAutoFitMinFontSizeSpin != nullptr) {
        m_elementAutoFitMinFontSizeSpin->setValue(element->autoFitMinFontSizePt);
    }
    if (m_elementAutoFitMaxFontSizeSpin != nullptr) {
        m_elementAutoFitMaxFontSizeSpin->setValue(element->autoFitMaxFontSizePt);
    }
    if (m_elementVerticalTextCheck != nullptr) {
        m_elementVerticalTextCheck->setChecked(element->verticalText);
    }
    if (m_arrayGridDataPathEdit != nullptr) {
        m_arrayGridDataPathEdit->setText(element->dataPath);
    }
    if (m_arrayGridRowsSpin != nullptr) {
        m_arrayGridRowsSpin->setValue(element->arrayGridRows);
    }
    if (m_arrayGridColumnsSpin != nullptr) {
        m_arrayGridColumnsSpin->setValue(element->arrayGridColumns);
    }
    if (m_arrayGridRowHeightSpin != nullptr) {
        m_arrayGridRowHeightSpin->setValue(element->arrayGridRowHeightMm);
    }
    if (m_arrayGridCellTemplateEdit != nullptr) {
        const QSignalBlocker cellTemplateBlocker(m_arrayGridCellTemplateEdit);
        m_arrayGridCellTemplateEdit->setPlainText(element->arrayGridCellTemplate);
    }
    if (m_arrayGridDrawBordersCheck != nullptr) {
        m_arrayGridDrawBordersCheck->setChecked(element->arrayGridDrawBorders);
    }

    const QSignalBlocker blocker(m_elementValueEdit);
    switch (element->type) {
    case sleekpr::core::TemplateElementType::FixedText:
        m_elementValueEdit->setPlainText(element->text);
        m_elementValueEdit->setPlaceholderText(QString::fromUtf8("例如：地址:${address}"));
        break;
    case sleekpr::core::TemplateElementType::BoundField:
        m_elementValueEdit->setPlainText(element->fieldKey);
        m_elementValueEdit->setPlaceholderText(QString::fromUtf8("字段 key，例如 productName"));
        break;
    case sleekpr::core::TemplateElementType::QrCode:
        m_elementValueEdit->setPlainText(element->payload.trimmed().isEmpty() && !element->fieldKey.trimmed().isEmpty()
                ? QStringLiteral("${%1}").arg(element->fieldKey.trimmed())
                : element->payload);
        m_elementValueEdit->setPlaceholderText(QString::fromUtf8("例如：${qrPayload} 或 地址:${address}"));
        break;
    case sleekpr::core::TemplateElementType::ArrayGrid:
        m_elementValueEdit->clear();
        m_elementValueEdit->setPlaceholderText(QString::fromUtf8("数组网格请编辑下方 dataPath 和单元格模板"));
        break;
    default:
        m_elementValueEdit->clear();
        m_elementValueEdit->setPlaceholderText(QString::fromUtf8("当前元素可编辑位置和尺寸"));
        break;
    }
}

void TemplateDesignerWindow::refreshTablePropertyEditor()
{
    // 表格属性面板只服务当前选中的表格；选中普通元素时禁用，避免误把普通元素写成表格配置。
    ScopedBoolFlag updating(m_updatingPropertyEditors);
    const auto* table = currentTable();
    const auto hasTable = table != nullptr;
    const QList<QWidget*> controls{
        m_tableDisplayNameEdit,
        m_tableDataPathEdit,
        m_tableXSpin,
        m_tableYSpin,
        m_tableWidthSpin,
        m_tableHeightSpin,
        m_tableHeaderHeightSpin,
        m_tableDetailHeightSpin,
        m_tableRepeatHeaderCheck,
        m_tableDrawBordersCheck,
        m_tableColumnsEdit,
    };
    for (auto* control : controls) {
        if (control != nullptr) {
            control->setEnabled(hasTable);
        }
    }

    if (!hasTable) {
        if (m_tableDisplayNameEdit != nullptr) {
            m_tableDisplayNameEdit->clear();
        }
        if (m_tableDataPathEdit != nullptr) {
            m_tableDataPathEdit->clear();
        }
        if (m_tableColumnsEdit != nullptr) {
            m_tableColumnsEdit->clear();
        }
        return;
    }

    m_tableDisplayNameEdit->setText(table->displayName);
    m_tableDataPathEdit->setText(table->dataPath);
    m_tableXSpin->setValue(table->x);
    m_tableYSpin->setValue(table->y);
    m_tableWidthSpin->setValue(table->width);
    m_tableHeightSpin->setValue(table->height);
    m_tableHeaderHeightSpin->setValue(table->headerRowHeightMm);
    m_tableDetailHeightSpin->setValue(table->detailRowHeightMm);
    m_tableRepeatHeaderCheck->setChecked(table->repeatHeaderOnPage);
    m_tableDrawBordersCheck->setChecked(table->drawBorders);
    m_tableColumnsEdit->setText(formatTableColumns(table->columns));
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

    const auto image = sleekpr::infrastructure::LabelPreviewImageRenderer().renderImage(drawingPlan, drawingPlan.renderDpi);
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
        ? defaultSampleDataObject()
        : document.sampleData;
    const QSignalBlocker blocker(m_sampleDataEdit);
    m_sampleDataEdit->setPlainText(sampleDataJson(sampleData));
    updateSampleDataErrorLabel(QString());
}

void TemplateDesignerWindow::notifySettingsChanged()
{
    if (m_onSettingsChanged) {
        m_onSettingsChanged(m_settings);
    }
    if (!m_restoringDocumentHistory) {
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
    if (element == nullptr || currentSelectionIsTable()) {
        m_statusLabel->setText(QString::fromUtf8("请先选择固定文本或绑定字段"));
        return false;
    }
    if (!canEditElement(element->id)) {
        m_statusLabel->setText(QString::fromUtf8("当前元素不可编辑"));
        return false;
    }

    const auto elementId = element->id;
    auto updated = *element;
    const auto value = m_elementValueEdit == nullptr ? QString() : m_elementValueEdit->toPlainText();
    switch (updated.type) {
    case sleekpr::core::TemplateElementType::FixedText:
        // 固定文本保存原始模板内容，${字段名} 会在预览和打印渲染阶段替换。
        updated.text = value;
        break;
    case sleekpr::core::TemplateElementType::BoundField:
        updated.fieldKey = value.trimmed();
        break;
    case sleekpr::core::TemplateElementType::QrCode:
        // 二维码保存原始模板内容，${字段名} 会在生成二维码矩阵前替换成接口传入值。
        updated.payload = value;
        break;
    case sleekpr::core::TemplateElementType::ArrayGrid:
        updated.dataPath = m_arrayGridDataPathEdit == nullptr ? updated.dataPath : m_arrayGridDataPathEdit->text().trimmed();
        updated.arrayGridRows = m_arrayGridRowsSpin == nullptr ? updated.arrayGridRows : m_arrayGridRowsSpin->value();
        updated.arrayGridColumns = m_arrayGridColumnsSpin == nullptr ? updated.arrayGridColumns : m_arrayGridColumnsSpin->value();
        updated.arrayGridRowHeightMm = m_arrayGridRowHeightSpin == nullptr
            ? updated.arrayGridRowHeightMm
            : m_arrayGridRowHeightSpin->value();
        updated.arrayGridCellTemplate = m_arrayGridCellTemplateEdit == nullptr
            ? updated.arrayGridCellTemplate
            : m_arrayGridCellTemplateEdit->toPlainText();
        updated.arrayGridDrawBorders = m_arrayGridDrawBordersCheck == nullptr
            ? updated.arrayGridDrawBorders
            : m_arrayGridDrawBordersCheck->isChecked();
        break;
    default:
        break;
    }
    // 普通元素的几何和文本样式统一在属性面板回写；表格仍使用独立的表格属性面板。
    updated.x = m_elementXSpin == nullptr ? updated.x : m_elementXSpin->value();
    updated.y = m_elementYSpin == nullptr ? updated.y : m_elementYSpin->value();
    updated.width = m_elementWidthSpin == nullptr ? updated.width : m_elementWidthSpin->value();
    updated.height = m_elementHeightSpin == nullptr ? updated.height : m_elementHeightSpin->value();
    updated.fontSizePt = m_elementFontSizeSpin == nullptr ? updated.fontSizePt : m_elementFontSizeSpin->value();
    updated.rotationDegrees = m_elementRotationSpin == nullptr ? updated.rotationDegrees : m_elementRotationSpin->value();
    updated.bold = m_elementBoldCheck == nullptr ? updated.bold : m_elementBoldCheck->isChecked();
    const auto supportsAutoFitFont = updated.type == sleekpr::core::TemplateElementType::FixedText
        || updated.type == sleekpr::core::TemplateElementType::BoundField;
    updated.autoFitFont = supportsAutoFitFont
        && m_elementAutoFitFontCheck != nullptr
        && m_elementAutoFitFontCheck->isChecked();
    updated.autoFitMinFontSizePt = m_elementAutoFitMinFontSizeSpin == nullptr
        ? updated.autoFitMinFontSizePt
        : m_elementAutoFitMinFontSizeSpin->value();
    updated.autoFitMaxFontSizePt = m_elementAutoFitMaxFontSizeSpin == nullptr
        ? updated.autoFitMaxFontSizePt
        : std::max(updated.autoFitMinFontSizePt, m_elementAutoFitMaxFontSizeSpin->value());
    updated.verticalText = updated.type == sleekpr::core::TemplateElementType::FixedText
        && m_elementVerticalTextCheck != nullptr
        && m_elementVerticalTextCheck->isChecked();

    if (sameTemplateElement(*element, updated)) {
        return false;
    }

    *element = updated;
    // 自动应用只刷新画布，避免重建列表和属性面板导致输入卡顿；手动应用保留完整刷新。
    if (lightweightRefresh) {
        refreshPreview();
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
    // 属性面板回写整张表格，但 id、所属图层和层内顺序仍由编辑模型保护。
    auto* table = currentTable();
    if (table == nullptr) {
        m_statusLabel->setText(QString::fromUtf8("请先选择表格"));
        return false;
    }

    auto updated = *table;
    updated.displayName = m_tableDisplayNameEdit == nullptr ? updated.displayName : m_tableDisplayNameEdit->text().trimmed();
    updated.dataPath = m_tableDataPathEdit == nullptr ? updated.dataPath : m_tableDataPathEdit->text().trimmed();
    updated.x = m_tableXSpin == nullptr ? updated.x : m_tableXSpin->value();
    updated.y = m_tableYSpin == nullptr ? updated.y : m_tableYSpin->value();
    updated.width = m_tableWidthSpin == nullptr ? updated.width : m_tableWidthSpin->value();
    updated.height = m_tableHeightSpin == nullptr ? updated.height : m_tableHeightSpin->value();
    updated.headerRowHeightMm = m_tableHeaderHeightSpin == nullptr ? updated.headerRowHeightMm : m_tableHeaderHeightSpin->value();
    updated.detailRowHeightMm = m_tableDetailHeightSpin == nullptr ? updated.detailRowHeightMm : m_tableDetailHeightSpin->value();
    updated.repeatHeaderOnPage = m_tableRepeatHeaderCheck == nullptr ? updated.repeatHeaderOnPage : m_tableRepeatHeaderCheck->isChecked();
    updated.drawBorders = m_tableDrawBordersCheck == nullptr ? updated.drawBorders : m_tableDrawBordersCheck->isChecked();

    const auto parsedColumns = parseTableColumns(m_tableColumnsEdit == nullptr ? QString() : m_tableColumnsEdit->text());
    if (parsedColumns.isEmpty()) {
        m_statusLabel->setText(QString::fromUtf8("表格列配置无效"));
        return false;
    }
    updated.columns = parsedColumns;

    if (sameTableElement(*table, updated)) {
        return false;
    }

    auto& document = m_settings.templateDocuments[m_templateKey];
    const auto tableId = updated.id;
    if (!sleekpr::core::TemplateDocumentEditModel::updateTable(document, tableId, updated)) {
        m_statusLabel->setText(QString::fromUtf8("表格属性保存失败"));
        return false;
    }

    // 自动应用表格属性时同样只刷新画布，减少列配置和尺寸微调时的 UI 抖动。
    if (lightweightRefresh) {
        refreshPreview();
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
    if (m_updatingPropertyEditors || currentSelectionIsTable() || currentElement() == nullptr || m_elementAutoApplyTimer == nullptr) {
        return;
    }
    m_elementAutoApplyTimer->start(std::max(0, delayMs));
}

void TemplateDesignerWindow::scheduleTableAutoApply(int delayMs)
{
    if (m_updatingPropertyEditors || !currentSelectionIsTable() || currentTable() == nullptr || m_tableAutoApplyTimer == nullptr) {
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
    const auto* watchedWidget = qobject_cast<const QWidget*>(watched);
    const QList<QWidget*> editors{
        m_elementValueEdit,
        m_elementXSpin,
        m_elementYSpin,
        m_elementWidthSpin,
        m_elementHeightSpin,
        m_elementFontSizeSpin,
        m_elementRotationSpin,
        m_elementBoldCheck,
        m_elementAutoFitFontCheck,
        m_elementAutoFitMinFontSizeSpin,
        m_elementAutoFitMaxFontSizeSpin,
        m_elementVerticalTextCheck,
        m_arrayGridDataPathEdit,
        m_arrayGridRowsSpin,
        m_arrayGridColumnsSpin,
        m_arrayGridRowHeightSpin,
        m_arrayGridCellTemplateEdit,
        m_arrayGridDrawBordersCheck,
    };
    return std::any_of(editors.cbegin(), editors.cend(), [watchedWidget](const auto* editor) {
        return ownsFocusWidget(editor, watchedWidget);
    });
}

bool TemplateDesignerWindow::isTableAutoApplyEditor(QObject* watched) const
{
    const auto* watchedWidget = qobject_cast<const QWidget*>(watched);
    const QList<QWidget*> editors{
        m_tableDisplayNameEdit,
        m_tableDataPathEdit,
        m_tableXSpin,
        m_tableYSpin,
        m_tableWidthSpin,
        m_tableHeightSpin,
        m_tableHeaderHeightSpin,
        m_tableDetailHeightSpin,
        m_tableRepeatHeaderCheck,
        m_tableDrawBordersCheck,
        m_tableColumnsEdit,
    };
    return std::any_of(editors.cbegin(), editors.cend(), [watchedWidget](const auto* editor) {
        return ownsFocusWidget(editor, watchedWidget);
    });
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
    m_documentHistory.clear();
    m_documentHistory.append(m_settings.templateDocuments[m_templateKey]);
    m_documentHistoryIndex = 0;
    updateHistoryButtons();
}

void TemplateDesignerWindow::rememberCurrentDocumentHistory()
{
    if (m_restoringDocumentHistory) {
        updateHistoryButtons();
        return;
    }

    ensureCurrentTemplateDocument();
    while (m_documentHistory.size() > m_documentHistoryIndex + 1) {
        m_documentHistory.removeLast();
    }
    m_documentHistory.append(m_settings.templateDocuments[m_templateKey]);
    m_documentHistoryIndex = m_documentHistory.size() - 1;
    while (m_documentHistory.size() > 80) {
        m_documentHistory.removeFirst();
        --m_documentHistoryIndex;
    }
    updateHistoryButtons();
}

void TemplateDesignerWindow::updateHistoryButtons()
{
    if (m_undoButton != nullptr) {
        m_undoButton->setEnabled(m_documentHistoryIndex > 0);
    }
    if (m_redoButton != nullptr) {
        m_redoButton->setEnabled(m_documentHistoryIndex >= 0 && m_documentHistoryIndex < m_documentHistory.size() - 1);
    }
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
            refreshAll();
            selectElement(tableId);
            notifySettingsChanged();
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
        refreshAll();
        selectElement(elementId);
        notifySettingsChanged();
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

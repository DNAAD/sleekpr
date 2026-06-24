#include "sleekpr/app/TemplateDesignerFactory.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QUuid>

namespace sleekpr::app {

namespace {

constexpr auto kDefaultPaperSpecId = "label-80x30";

} // 命名空间

QString TemplateDesignerFactory::createTemplateId()
{
    return QStringLiteral("template-%1").arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
}

QString TemplateDesignerFactory::createDeviceProfileId()
{
    return QStringLiteral("profile-%1").arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
}

QString TemplateDesignerFactory::createLayerId()
{
    return QStringLiteral("layer-%1").arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
}

QString TemplateDesignerFactory::createElementId(const QString& prefix)
{
    return QStringLiteral("%1-%2").arg(prefix, QUuid::createUuid().toString(QUuid::WithoutBraces));
}

QString TemplateDesignerFactory::nextLayerName(int layerCount)
{
    return QString::fromUtf8("图层 %1").arg(layerCount + 1);
}

QString TemplateDesignerFactory::elementTypeKey(sleekpr::core::TemplateElementType type)
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

QString TemplateDesignerFactory::elementTypeName(sleekpr::core::TemplateElementType type)
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

QJsonObject TemplateDesignerFactory::createDefaultSampleData()
{
    const auto productName = QString::fromUtf8("足金串搭项链");
    const auto weight = QStringLiteral("11.49g");
    const auto qrPayload = QStringLiteral("606178PD35|1210005822");

    QJsonObject sampleData;
    sampleData.insert(QStringLiteral("sales_code"), QStringLiteral("606178PD35"));
    sampleData.insert(QStringLiteral("sale_attach_text"), QString::fromUtf8("附加:¥140.00"));
    sampleData.insert(QStringLiteral("identifier_code"), QStringLiteral("1210005822"));
    // 同时保留旧模板字段和默认元素字段，保证新建元素立刻能在画布上看到模拟预览。
    sampleData.insert(QStringLiteral("product_name"), productName);
    sampleData.insert(QStringLiteral("productName"), productName);
    sampleData.insert(QStringLiteral("weight"), weight);
    sampleData.insert(QStringLiteral("qrPayload"), qrPayload);
    sampleData.insert(QStringLiteral("header_items"), QJsonArray{
        QJsonObject{{QStringLiteral("value"), QStringLiteral("2.99")}, {QStringLiteral("text"), QString::fromUtf8("素金KA")}},
        QJsonObject{{QStringLiteral("value"), QStringLiteral("4.13")}, {QStringLiteral("text"), QString::fromUtf8("素金PC")}},
        QJsonObject{{QStringLiteral("value"), QStringLiteral("3.91")}, {QStringLiteral("text"), QString::fromUtf8("素金PA")}},
        QJsonObject{{QStringLiteral("value"), QStringLiteral("11.49")}, {QStringLiteral("text"), QString::fromUtf8("素金PD")}},
    });
    sampleData.insert(QStringLiteral("items"), QJsonArray{
        QJsonObject{
            {QStringLiteral("productName"), productName},
            {QStringLiteral("weight"), weight},
        },
        QJsonObject{
            {QStringLiteral("productName"), QString::fromUtf8("足金戒指")},
            {QStringLiteral("weight"), QStringLiteral("3.21g")},
        },
    });
    return sampleData;
}

sleekpr::core::TemplateDocument TemplateDesignerFactory::createBlankDocument(
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
    // 新模板只继承纸张和设备校准上下文，版式内容从空白画布开始。
    document.paperSpecId = paperSpecId.trimmed().isEmpty()
        ? QString::fromLatin1(kDefaultPaperSpecId)
        : paperSpecId;
    document.layers = {baseLayer};
    document.sampleData = createDefaultSampleData();
    document.deviceProfiles = deviceProfiles;
    return document;
}

sleekpr::core::TemplateElement TemplateDesignerFactory::createElement(sleekpr::core::TemplateElementType type)
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
        element.id = createElementId(QStringLiteral("fixedText"));
        element.displayName = QString::fromUtf8("固定文本");
        element.text = QString::fromUtf8("固定文本");
        break;
    case sleekpr::core::TemplateElementType::BoundField:
        element.id = createElementId(QStringLiteral("boundField"));
        element.displayName = QString::fromUtf8("绑定字段");
        element.fieldKey = QStringLiteral("productName");
        break;
    case sleekpr::core::TemplateElementType::QrCode:
        element.id = createElementId(QStringLiteral("qrCode"));
        element.displayName = QString::fromUtf8("二维码");
        element.fieldKey = QStringLiteral("qrPayload");
        element.width = 8.0;
        element.height = 8.0;
        break;
    case sleekpr::core::TemplateElementType::Rectangle:
        element.id = createElementId(QStringLiteral("rectangle"));
        element.displayName = QString::fromUtf8("矩形");
        element.width = 16.0;
        element.height = 6.0;
        break;
    case sleekpr::core::TemplateElementType::ArrayGrid:
        element.id = createElementId(QStringLiteral("arrayGrid"));
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

sleekpr::core::TableElement TemplateDesignerFactory::createTable()
{
    sleekpr::core::TableElement table;
    table.id = createElementId(QStringLiteral("table"));
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

} // 命名空间 sleekpr::app

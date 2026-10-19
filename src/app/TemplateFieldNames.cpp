#include "sleekpr/app/TemplateFieldNames.h"
#include "sleekpr/core/templates/TemplateDocument.h"

namespace sleekpr::app {

const QList<QPair<QString, QString>>& TemplateFieldNames::builtIns()
{
    static const QList<QPair<QString, QString>> names{
        {QStringLiteral("productName"), QString::fromUtf8("商品名称")},
        {QStringLiteral("identifierText"), QString::fromUtf8("编号")},
        {QStringLiteral("salesCode"), QString::fromUtf8("销售码")},
        {QStringLiteral("finishedWeightLabel"), QString::fromUtf8("成品重标题")},
        {QStringLiteral("finishedWeightValue"), QString::fromUtf8("成品重数值")},
        {QStringLiteral("finishedWeightText"), QString::fromUtf8("成品重")},
        {QStringLiteral("roughWeightLabel"), QString::fromUtf8("总件重标题")},
        {QStringLiteral("roughWeightValue"), QString::fromUtf8("总件重数值")},
        {QStringLiteral("roughWeightText"), QString::fromUtf8("总件重")},
        {QStringLiteral("standardText"), QString::fromUtf8("执行标准")},
        {QStringLiteral("addressText"), QString::fromUtf8("地址")},
        {QStringLiteral("qrPayload"), QString::fromUtf8("二维码内容")},
        {QStringLiteral("goldPurityText"), QString::fromUtf8("成色")},
        {QStringLiteral("priceText"), QString::fromUtf8("价格")},
        {QStringLiteral("additionalPriceText"), QString::fromUtf8("附加金额")},
        {QStringLiteral("tagWeightText"), QString::fromUtf8("标签重量")},
        {QStringLiteral("qrNote"), QString::fromUtf8("二维码说明")},
        {QStringLiteral("footerText"), QString::fromUtf8("底部备注")},
    };
    return names;
}

QString TemplateFieldNames::resolve(const core::TemplateDocument& document, const QString& rawKey)
{
    const auto key = rawKey.trimmed();
    const auto overrideName = document.fieldDisplayNames.value(key).trimmed();
    if (!overrideName.isEmpty()) return overrideName;
    for (const auto& field : document.fieldSchema) {
        if (field.key.trimmed() != key) continue;
        const auto name = field.displayName.trimmed();
        // 显式名称即使等于 key，也不能被内置中文兜底覆盖。
        if (!name.isEmpty()) return name;
        break;
    }
    for (const auto& name : builtIns()) {
        if (name.first == key) return name.second;
    }
    return key;
}

} // 命名空间 sleekpr::app

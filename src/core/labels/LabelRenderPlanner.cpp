#include "sleekpr/core/labels/LabelRenderPlanner.h"

#include <algorithm>

namespace sleekpr::core {

LabelRenderPlan LabelRenderPlanner::createPlan(const LabelItem& item) const
{
    // factoryNo=25003 是现有系统区分银标模板的业务约定。
    const auto templateKey = item.factoryNo == 25003
        ? LabelTemplateKey::Silver80x30
        : LabelTemplateKey::Default80x30;
    // 重量类别为空时沿用默认“成品重”语义，避免标签出现空标题。
    const auto weightCategory = item.weightCategory.trimmed().isEmpty()
        ? QString("Finished")
        : item.weightCategory.trimmed();
    const auto purity = item.goldPurity.trimmed();

    LabelRenderPlan plan;
    plan.paperSize = LabelPaperSize::create80x30();
    plan.templateKey = templateKey;
    plan.identifierText = item.identifierCode.trimmed();
    plan.productName = item.productName.trimmed();
    plan.finishedWeightText = QString("%1(g): %2").arg(weightCategory, formatWeight(item.finishedProductWeight));
    plan.roughWeightText = templateKey == LabelTemplateKey::Silver80x30
        ? QString("总重(g): %1").arg(formatWeight(item.roughWeight))
        : QString("总件重(g): %1").arg(formatWeight(item.roughWeight));
    plan.salesCode = item.salesCode.trimmed();
    plan.standardText = createStandardText(templateKey, purity);
    plan.addressText = item.address.trimmed().isEmpty()
        ? QString("地址:")
        : QString("地址:%1").arg(item.address.trimmed());
    plan.qrPayload = item.identifierCode.trimmed();
    plan.goldPurityText = purity;
    plan.priceText = item.price > 0.0 ? QString("CNY %1").arg(formatWeight(item.price)) : QString();
    plan.additionalPriceText = createAdditionalPriceText(item.additionalPrice);
    plan.tagWeightText = item.tagWeight > 0.0 ? formatWeight(item.tagWeight) : QString("0.20");
    plan.parts = createParts(item);
    plan.footerText = createFooterText(item);
    return plan;
}

QString LabelRenderPlanner::formatWeight(double value)
{
    // 标签重量和价格相关数值统一保留两位小数，保持与现有客户端输出一致。
    return QString::number(value, 'f', 2);
}

QString LabelRenderPlanner::createStandardText(LabelTemplateKey templateKey, const QString& purity)
{
    const auto base = QString("执行标准 QB/T2062 GB11887");
    // 银标模板不在标准行追加成色，默认模板才追加 purity 文本。
    if (templateKey == LabelTemplateKey::Silver80x30 || purity.trimmed().isEmpty()) {
        return base;
    }
    return QString("%1  %2").arg(base, purity.trimmed());
}

QString LabelRenderPlanner::createAdditionalPriceText(double additionalPrice)
{
    return additionalPrice > 0.0
        ? QString("附加:￥ %1").arg(formatWeight(additionalPrice))
        : QString();
}

QList<LabelPartRenderPlan> LabelRenderPlanner::createParts(const LabelItem& item)
{
    QList<LabelPartRenderPlan> parts;
    if (!item.finishedProductParts.isEmpty()) {
        // 现有 .NET 客户端最多渲染 9 条明细，迁移阶段保持同样上限。
        const auto count = std::min(item.finishedProductParts.size(), qsizetype{9});
        for (int index = 0; index < count; ++index) {
            parts.append(LabelPartRenderPlan{
                item.finishedProductParts[index].categoryName.trimmed(),
                formatWeight(item.finishedProductParts[index].partWeight),
            });
        }
        return parts;
    }

    if (!item.categoryName.trimmed().isEmpty()) {
        // 明细数组为空时退回到品类名和成品重，避免标签明细区域完全缺失。
        parts.append(LabelPartRenderPlan{
            item.categoryName.trimmed(),
            formatWeight(item.finishedProductWeight),
        });
    }
    return parts;
}

QString LabelRenderPlanner::createFooterText(const LabelItem& item)
{
    QStringList fragments;
    // 底部备注由多个可选业务字段拼接，顺序必须稳定，便于测试和真实标签比对。
    if (!item.additionalRemark.trimmed().isEmpty()) {
        fragments.append(QString("附加:%1").arg(item.additionalRemark.trimmed()));
    }
    if (item.inlayWeight > 0.0) {
        fragments.append(QString("附加重:%1g").arg(formatWeight(item.inlayWeight)));
    }
    if (item.ropeWeight > 0.0) {
        fragments.append(QString("绳重:%1g").arg(formatWeight(item.ropeWeight)));
    }
    if (!item.finishedProductNote.trimmed().isEmpty()) {
        fragments.append(item.finishedProductNote.trimmed());
    }
    return fragments.join(' ');
}

} // 命名空间 sleekpr::core

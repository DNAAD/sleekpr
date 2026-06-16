#pragma once

#include "sleekpr/core/labels/LabelPartRenderPlan.h"
#include "sleekpr/core/labels/LabelTemplateKey.h"
#include "sleekpr/core/printing/LabelPaperSize.h"

#include <QList>
#include <QString>

namespace sleekpr::core {

struct LabelRenderPlan
{
    // 标签纸张规格，领域层统一以毫米描述。
    LabelPaperSize paperSize = LabelPaperSize::create80x30();

    // 选中的模板类型，决定后续绘制布局。
    LabelTemplateKey templateKey = LabelTemplateKey::Default80x30;

    // 以下字段均为可直接展示或绘制的业务文本。
    QString identifierText;
    QString productName;
    QString finishedWeightText;
    QString roughWeightText;
    QString salesCode;
    QString standardText;
    QString addressText;
    QString qrPayload;
    QString goldPurityText;
    QString priceText;
    QString additionalPriceText;
    QString tagWeightText;
    QList<LabelPartRenderPlan> parts;
    QString footerText;
};

} // 命名空间 sleekpr::core

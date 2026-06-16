#pragma once

#include "sleekpr/core/labels/LabelPartItem.h"

#include <QList>
#include <QString>

namespace sleekpr::core {

struct LabelItem
{
    // 工厂编号决定使用默认模板还是银标模板。
    int factoryNo = 0;

    // 标签唯一识别码，同时作为二维码内容。
    QString identifierCode;

    // 商品名称，后续绘制阶段会限制行数并省略超长文本。
    QString productName;
    QString weightCategory;
    double finishedProductWeight = 0.0;
    double roughWeight = 0.0;
    QString salesCode;
    QString goldPurity;
    QString address;
    double price = 0.0;
    double additionalPrice = 0.0;
    double tagWeight = 0.0;
    QString categoryName;

    // 接口传入的成品明细，最多渲染前 9 条。
    QList<LabelPartItem> finishedProductParts;
    QString additionalRemark;
    double inlayWeight = 0.0;
    double ropeWeight = 0.0;
    QString finishedProductNote;
};

} // 命名空间 sleekpr::core

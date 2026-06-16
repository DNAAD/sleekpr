#include "sleekpr/infrastructure/preview/PreviewLabelFactory.h"

namespace sleekpr::infrastructure {

sleekpr::core::LabelItem PreviewLabelFactory::createDemoLabel(sleekpr::core::LabelTemplateKey templateKey)
{
    sleekpr::core::LabelItem item;
    item.factoryNo = templateKey == sleekpr::core::LabelTemplateKey::Silver80x30 ? 25003 : 0;
    item.identifierCode = "1000035933";
    item.productName = QString::fromUtf8("足银镶金串珠 四方拉丝隔珠 11.5mm");
    item.weightCategory = QString::fromUtf8("成品重");
    item.finishedProductWeight = 123.0;
    item.roughWeight = 123.0;
    item.salesCode = "60318000ZB60";
    item.goldPurity = QString::fromUtf8("足金999‰");
    item.address = QString::fromUtf8("水贝金座一层 111 民族工匠");
    item.price = 128.5;
    item.additionalPrice = 430.0;
    item.tagWeight = 0.2;
    item.categoryName = QString::fromUtf8("镶嵌ZB");
    item.finishedProductParts = {
        sleekpr::core::LabelPartItem{QString::fromUtf8("镶嵌ZB"), 123.0},
        sleekpr::core::LabelPartItem{QString::fromUtf8("素金PD"), 330.45},
        sleekpr::core::LabelPartItem{QString::fromUtf8("镶嵌ZB"), 123.0},
    };
    item.additionalRemark = QString::fromUtf8("翡翠2");
    item.inlayWeight = 0.12;
    item.ropeWeight = 0.30;
    item.finishedProductNote = "1111111";
    return item;
}

} // 命名空间 sleekpr::infrastructure

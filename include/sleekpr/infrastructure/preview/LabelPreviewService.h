#pragma once

#include "sleekpr/core/labels/LabelItem.h"
#include "sleekpr/core/labels/LabelTemplateKey.h"
#include "sleekpr/core/settings/PrintClientSettings.h"

#include <QImage>

namespace sleekpr::infrastructure {

class LabelPreviewService
{
public:
    // 设置页和主预览共用这一入口，确保模板覆盖、整体偏移和二维码渲染链路一致。
    QImage renderDemoPreview(
        const sleekpr::core::PrintClientSettings& settings,
        sleekpr::core::LabelTemplateKey templateKey = sleekpr::core::LabelTemplateKey::Default80x30) const;

    QImage renderPreview(
        const sleekpr::core::LabelItem& item,
        const sleekpr::core::PrintClientSettings& settings) const;
};

} // 命名空间 sleekpr::infrastructure

#pragma once

#include "sleekpr/core/settings/LabelOffset.h"
#include "sleekpr/core/settings/LocalHttpLimitSettings.h"
#include "sleekpr/core/settings/TemplateElement.h"
#include "sleekpr/core/settings/TemplateElementOverride.h"
#include "sleekpr/core/templates/TemplateDocument.h"

#include <QHash>
#include <QList>
#include <QString>
#include <QStringList>

namespace sleekpr::core {

struct PrintClientSettings
{
    // 本机操作员选择的默认打印机；请求中的打印机名不会覆盖它。
    QString defaultPrinter;

    // 全局标签校准偏移，影响预览和真实打印。
    LabelOffset labelOffset;

    // 允许访问本地打印服务的网页 Origin。
    QStringList allowedOrigins;

    // 本地 HTTP 服务安全上限覆盖值，缺省时使用 HTTP 层集中默认值。
    LocalHttpLimitSettings localHttpLimits;

    // 模板默认元素覆盖：第一层是模板名，第二层是默认元素 key。
    QHash<QString, QHash<QString, TemplateElementOverride>> templateOverrides;

    // 自定义模板元素列表：第一层是模板名，列表顺序就是绘制顺序。
    QHash<QString, QList<TemplateElement>> templateElements;

    // 完整模板设计器文档：第一层是模板名，例如 default 或 silver。
    QHash<QString, TemplateDocument> templateDocuments;
};

} // 命名空间 sleekpr::core

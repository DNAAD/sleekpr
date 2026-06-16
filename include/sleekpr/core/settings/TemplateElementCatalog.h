#pragma once

#include "sleekpr/core/native/NativeLabelDrawingPlan.h"
#include "sleekpr/core/settings/TemplateElementOverride.h"

#include <QList>
#include <QString>

#include <optional>

namespace sleekpr::core {

struct TemplateElementDefinition
{
    // 设置窗口使用稳定 key 写入 templateOverrides，避免显示名称变更影响配置兼容性。
    QString key;

    // 面向操作员展示的中文名称，只用于界面显示。
    QString displayName;

    // 当前模板默认绘制值，设置窗口无覆盖值时回填这些坐标和字体参数。
    TemplateElementOverride defaultValue;
};

class TemplateElementCatalog
{
public:
    static QList<TemplateElementDefinition> fromDrawingPlan(const NativeLabelDrawingPlan& drawingPlan);
    static std::optional<TemplateElementDefinition> find(const QList<TemplateElementDefinition>& elements, const QString& key);
};

} // 命名空间 sleekpr::core

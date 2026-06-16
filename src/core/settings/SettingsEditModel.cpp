#include "sleekpr/core/settings/SettingsEditModel.h"

namespace sleekpr::core {

void SettingsEditModel::resetElement(PrintClientSettings& settings, const QString& templateKey, const QString& elementKey)
{
    if (!settings.templateOverrides.contains(templateKey)) {
        return;
    }

    // 只移除当前元素，避免误清其它已经微调好的标签元素。
    settings.templateOverrides[templateKey].remove(elementKey);
}

void SettingsEditModel::resetAllElements(PrintClientSettings& settings, const QString& templateKey)
{
    // 只清空指定模板的元素覆盖，不触碰打印机和整体偏移这些独立设置。
    settings.templateOverrides[templateKey].clear();
}

} // 命名空间 sleekpr::core

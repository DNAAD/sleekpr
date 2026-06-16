#pragma once

#include "sleekpr/core/labels/LabelTemplateKey.h"

#include <QString>

namespace sleekpr::core {

// 本地设置文件沿用 dotnet-print 的字符串 key，领域层继续使用强类型模板枚举。
QString templateOverrideKey(LabelTemplateKey templateKey);
LabelTemplateKey labelTemplateKeyFromOverrideKey(const QString& key);

} // 命名空间 sleekpr::core

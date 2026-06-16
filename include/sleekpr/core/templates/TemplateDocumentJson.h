#pragma once

#include "sleekpr/core/templates/TemplateDocument.h"

#include <QJsonObject>
#include <QString>

namespace sleekpr::core {

class TemplateDocumentJson
{
public:
    // 模板 JSON 是导入导出和 settings.json 的共同格式，集中维护可减少字段漂移。
    static QJsonObject toJson(const TemplateDocument& document);
    static TemplateDocument fromJson(const QJsonObject& json);
    static bool validateForImport(const QJsonObject& json, QString* errorMessage);
};

} // 命名空间 sleekpr::core

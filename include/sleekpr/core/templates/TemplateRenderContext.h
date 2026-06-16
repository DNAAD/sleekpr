#pragma once

#include <QJsonObject>
#include <QStringList>

namespace sleekpr::core {

struct TemplateRenderContext
{
    QJsonObject values;
    QStringList missingRequiredFieldNames;

    bool hasMissingRequiredFields() const;
};

} // 命名空间 sleekpr::core

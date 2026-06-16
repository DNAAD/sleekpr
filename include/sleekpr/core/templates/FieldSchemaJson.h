#pragma once

#include "sleekpr/core/templates/FieldSchema.h"

#include <QJsonObject>
#include <QList>
#include <QString>

namespace sleekpr::core {

class FieldSchemaJson
{
public:
    static QJsonObject toJson(const QList<FieldDefinition>& fields);
    static QList<FieldDefinition> fromJson(const QJsonObject& json);
    static bool validate(const QJsonObject& json, QString* errorMessage = nullptr);
};

} // 命名空间 sleekpr::core

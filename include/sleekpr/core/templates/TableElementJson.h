#pragma once

#include "sleekpr/core/templates/TableElement.h"

#include <QJsonObject>
#include <QString>

namespace sleekpr::core {

class TableElementJson
{
public:
    static QJsonObject toJson(const TableElement& table);
    static TableElement fromJson(const QJsonObject& json, const QString& parentLayerId = QString());
    static bool validate(const QJsonObject& json, const QString& parentLayerId, QString* errorMessage = nullptr);
};

} // 命名空间 sleekpr::core

#pragma once

#include "sleekpr/core/templates/PaperSpec.h"

#include <QJsonObject>
#include <QString>

namespace sleekpr::core {

class PaperSpecJson
{
public:
    static QJsonObject toJson(const PaperSpec& spec);
    static PaperSpec fromJson(const QJsonObject& json);
    static bool validate(const QJsonObject& json, QString* errorMessage = nullptr);
};

} // 命名空间 sleekpr::core

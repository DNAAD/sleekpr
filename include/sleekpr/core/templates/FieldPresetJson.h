#pragma once

#include "sleekpr/core/templates/FieldPreset.h"

#include <QJsonObject>
#include <QString>

namespace sleekpr::core {

class FieldPresetJson
{
public:
    static QJsonObject toJson(const FieldPreset& preset);
    static FieldPreset fromJson(const QJsonObject& json);
    static bool validate(const QJsonObject& json, QString* errorMessage = nullptr);
};

} // 命名空间 sleekpr::core

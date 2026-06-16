#pragma once

#include <QString>

namespace sleekpr::core {

enum class FieldValueType
{
    Text,
    Number,
    Money,
    Weight,
    Date,
    DateTime,
    Boolean,
    List,
    Object,
};

enum class FieldSource
{
    BuiltIn,
    Custom,
};

struct FieldDefinition
{
    // key 是模板绑定字段的稳定标识，必须避免首尾空白导致同名字段难以排查。
    QString key;
    QString displayName;
    FieldValueType type = FieldValueType::Text;
    bool required = false;
    QString defaultValue;
    QString sampleValue;
    QString format;
    FieldSource source = FieldSource::Custom;
};

} // 命名空间 sleekpr::core

#pragma once

#include <QJsonObject>
#include <QString>

namespace sleekpr::core {

struct FieldPreset
{
    // schemaVersion 用于后续拒绝未知字段方案结构，避免旧客户端误读。
    int schemaVersion = 1;
    QString id;
    QString name;
    QString templateId;
    QJsonObject values;
    QString updatedAt;
};

} // 命名空间 sleekpr::core

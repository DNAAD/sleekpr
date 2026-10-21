#pragma once

#include <QJsonObject>
#include <QString>
#include <optional>

namespace sleekpr::app {

struct BuildInfo
{
    QString version = QStringLiteral("unknown");
    QString revision = QStringLiteral("unknown");
    QString configuration = QStringLiteral("unknown");
    std::optional<bool> sourceDirty;
    std::optional<bool> workspaceDirty;
    QString sourceDigest = QStringLiteral("unknown");
    static BuildInfo current();
    QJsonObject toJson() const;
};

} // 命名空间 sleekpr::app

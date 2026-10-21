#include "sleekpr/app/BuildInfo.h"
#include "sleekpr/BuildVersion.h"

namespace sleekpr::app {

BuildInfo BuildInfo::current()
{
    const auto dirty = [](int value) -> std::optional<bool> { return value < 0 ? std::nullopt : std::optional<bool>(value != 0); };
    BuildInfo info;
    info.version = QString::fromLatin1(SLEEKPR_BUILD_VERSION);
    info.revision = QString::fromLatin1(SLEEKPR_BUILD_REVISION);
    info.configuration = QString::fromLatin1(SLEEKPR_BUILD_CONFIGURATION);
    info.sourceDirty = dirty(SLEEKPR_BUILD_SOURCE_DIRTY);
    info.workspaceDirty = dirty(SLEEKPR_BUILD_WORKSPACE_DIRTY);
    info.sourceDigest = QString::fromLatin1(SLEEKPR_BUILD_SOURCE_DIGEST);
    return info;
}

QJsonObject BuildInfo::toJson() const
{
    return {{QStringLiteral("schemaVersion"), 1}, {QStringLiteral("version"), version},
        {QStringLiteral("revision"), revision}, {QStringLiteral("configuration"), configuration},
        {QStringLiteral("sourceDirty"), sourceDirty ? QJsonValue(*sourceDirty) : QJsonValue(QJsonValue::Null)},
        {QStringLiteral("workspaceDirty"), workspaceDirty ? QJsonValue(*workspaceDirty) : QJsonValue(QJsonValue::Null)},
        {QStringLiteral("sourceDigest"), sourceDigest}};
}

} // 命名空间 sleekpr::app

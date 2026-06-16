#include "sleekpr/core/settings/FileSettingsStore.h"

#include "sleekpr/core/settings/PrintClientSettingsJson.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QSaveFile>

#include <utility>

namespace sleekpr::core {

FileSettingsStore::FileSettingsStore(QString settingsPath)
    : m_settingsPath(std::move(settingsPath))
{
}

PrintClientSettings FileSettingsStore::load() const
{
    QFile file(m_settingsPath);
    if (!file.exists() || !file.open(QIODevice::ReadOnly)) {
        // 设置文件不存在时不能阻断客户端启动，使用默认配置即可。
        return PrintClientSettings{};
    }

    QJsonParseError parseError;
    const auto document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        // 配置损坏时回退默认值，让客户端能启动；后续保存会用原子写重新生成合法 JSON。
        return PrintClientSettings{};
    }

    return PrintClientSettingsJson::fromJson(document.object());
}

bool FileSettingsStore::save(const PrintClientSettings& settings) const
{
    const QFileInfo info(m_settingsPath);
    if (!info.dir().exists() && !QDir().mkpath(info.dir().absolutePath())) {
        return false;
    }

    QSaveFile file(m_settingsPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }

    const auto payload = QJsonDocument(PrintClientSettingsJson::toJson(settings)).toJson(QJsonDocument::Indented);
    if (file.write(payload) != payload.size()) {
        return false;
    }
    return file.commit();
}

} // 命名空间 sleekpr::core

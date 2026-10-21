#include "sleekpr/core/settings/FileSettingsStore.h"

#include "sleekpr/infrastructure/settings/SettingsRepository.h"

#include <utility>

namespace sleekpr::core {

FileSettingsStore::FileSettingsStore(QString settingsPath)
    : m_settingsPath(std::move(settingsPath))
{
}

PrintClientSettings FileSettingsStore::load() const
{
    // 旧读取入口保持启动默认值兼容；新 UI 通过仓库获取明确的损坏或 I/O 状态。
    return sleekpr::infrastructure::SettingsRepository(m_settingsPath).read().settings;
}

bool FileSettingsStore::save(const PrintClientSettings& settings) const
{
    // 兼容整体替换，但不能再绕过跨进程锁、备份和原子写入。
    return sleekpr::infrastructure::SettingsRepository(m_settingsPath).replace(settings).ok();
}

} // 命名空间 sleekpr::core

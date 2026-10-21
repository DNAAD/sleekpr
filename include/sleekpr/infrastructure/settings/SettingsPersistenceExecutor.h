#pragma once
#include "sleekpr/infrastructure/settings/SettingsSaveSession.h"
#include <QObject>
#include <QThreadPool>
#include <functional>

namespace sleekpr::infrastructure {
class SettingsPersistenceExecutor final : public QObject
{
public:
    explicit SettingsPersistenceExecutor(QObject* parent = nullptr);
    ~SettingsPersistenceExecutor() override;
    void submit(std::function<SettingsStorageOutcome()> work, std::function<void(SettingsStorageOutcome)> completion);
    void shutdown();
private:
    QThreadPool m_pool;
    bool m_stopping = false;
};
} // 命名空间 sleekpr::infrastructure

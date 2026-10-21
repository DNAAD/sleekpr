#include "sleekpr/infrastructure/settings/SettingsPersistenceExecutor.h"
#include <QMetaObject>
#include <QRunnable>
#include <QThread>

namespace sleekpr::infrastructure {
SettingsPersistenceExecutor::SettingsPersistenceExecutor(QObject* parent) : QObject(parent) { m_pool.setMaxThreadCount(1); }
SettingsPersistenceExecutor::~SettingsPersistenceExecutor() { shutdown(); }
void SettingsPersistenceExecutor::shutdown()
{
    Q_ASSERT(QThread::currentThread() == thread());
    m_stopping = true;
    m_pool.waitForDone();
}
void SettingsPersistenceExecutor::submit(std::function<SettingsStorageOutcome()> work,
    std::function<void(SettingsStorageOutcome)> completion)
{
    Q_ASSERT(QThread::currentThread() == thread());
    Q_ASSERT(!m_stopping);
    m_pool.start(QRunnable::create([this, work = std::move(work), completion = std::move(completion)]() mutable {
        SettingsStorageOutcome result;
        try { result = work(); }
        catch (...) {
            result.executionFailed = true;
            result.observed.status = core::SettingsReadStatus::IoError;
            result.observed.message = QString::fromUtf8("后台保存任务异常，草稿已保留");
            result.writeResult = core::SettingsWriteResult{core::SettingsWriteStatus::WriteFailed,
                result.observed, {}, result.observed.message};
        }
        // 接收者是应用持有的执行器；析构先等待工作结束，再由 QObject 清理未派发通知。
        QMetaObject::invokeMethod(this, [completion = std::move(completion), result = std::move(result)]() mutable {
            completion(std::move(result));
        }, Qt::QueuedConnection);
    }));
}
} // 命名空间 sleekpr::infrastructure

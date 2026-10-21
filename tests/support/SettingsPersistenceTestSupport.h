#pragma once
#include "sleekpr/infrastructure/settings/SettingsRepository.h"
#include <QSaveFile>
#include <QSemaphore>
#include <QThread>
#include <atomic>

namespace sleekpr::test {
inline bool writeSettingsBytes(const QString& path, const QByteArray& bytes, QString* error = nullptr)
{
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit()) {
        if (error) *error = file.errorString();
        return false;
    }
    return true;
}
inline core::PrintClientSettings sampleSettings()
{
    core::PrintClientSettings settings;
    settings.defaultPrinter = QStringLiteral("old");
    core::TemplateDocument doc; doc.id = QStringLiteral("tag"); doc.name = QStringLiteral("base");
    settings.templateDocuments.insert(QStringLiteral("default"), doc);
    return settings;
}
struct SettingsWriteGate
{
    QString path;
    QThread* guiThread = QThread::currentThread();
    QSemaphore entered{0}, release{0};
    std::atomic<bool> armed{false}, offGui{false}, failMain{false}, failBackup{false};
    std::atomic<int> writes{0};
    infrastructure::SettingsRepository::AtomicWriter writer()
    {
        return [this](const QString& target, const QByteArray& bytes, QString* error) {
            offGui = QThread::currentThread() != guiThread;
            if (target == path) {
                ++writes;
                if (armed.exchange(false)) { entered.release(); release.acquire(); }
                if (failMain) return false;
            } else if (failBackup) return false;
            return writeSettingsBytes(target, bytes, error);
        };
    }
};
// 放在服务之后声明，确保断言提前返回也先解锁再析构执行器。
struct ReleaseSettingsGate
{
    SettingsWriteGate& gate;
    ~ReleaseSettingsGate() { gate.release.release(32); }
};
} // 命名空间 sleekpr::test

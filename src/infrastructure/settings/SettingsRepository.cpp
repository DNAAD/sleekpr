#include "sleekpr/infrastructure/settings/SettingsRepository.h"
#include "sleekpr/infrastructure/diagnostics/ClientPerformanceMetrics.h"
#include "sleekpr/core/settings/PrintClientSettingsJson.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QLockFile>
#include <QSaveFile>
#include <utility>

namespace sleekpr::infrastructure {
namespace {

using namespace core;

SettingsReadResult parseBytes(const QByteArray& bytes)
{
    SettingsReadResult result;
    result.sourceBytes = bytes;
    result.sourceDigest = QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex();
    QJsonParseError error;
    const auto json = QJsonDocument::fromJson(bytes, &error);
    if (error.error != QJsonParseError::NoError || !json.isObject()) {
        result.status = SettingsReadStatus::InvalidJson;
        result.message = QString::fromUtf8("设置文件 JSON 无效，未自动重建：%1").arg(
            error.error == QJsonParseError::NoError ? QString::fromUtf8("根节点必须为对象") : error.errorString());
        return result;
    }
    result.status = SettingsReadStatus::Ready;
    result.sourceJson = json.object();
    result.settings = PrintClientSettingsJson::fromJson(result.sourceJson);
    return result;
}

SettingsReadResult readPath(const QString& path)
{
    SettingsReadResult result;
    if (path.isEmpty()) {
        result.status = SettingsReadStatus::IoError;
        result.message = QString::fromUtf8("设置文件路径为空");
        return result;
    }
    const QFileInfo info(path);
    if (!info.exists()) return result;
    QFile file(path);
    if (!info.isFile() || !file.open(QIODevice::ReadOnly)) {
        result.status = SettingsReadStatus::IoError;
        result.message = QString::fromUtf8("无法读取设置文件：%1").arg(file.errorString());
        return result;
    }
    const auto bytes = file.readAll();
    if (file.error() != QFileDevice::NoError) {
        result.status = SettingsReadStatus::IoError;
        result.message = QString::fromUtf8("读取设置文件失败：%1").arg(file.errorString());
        return result;
    }
    return parseBytes(bytes);
}

bool atomicWrite(const QString& path, const QByteArray& bytes, QString* error)
{
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate) || file.write(bytes) != bytes.size() || !file.commit()) {
        if (error != nullptr) *error = file.errorString();
        return false;
    }
    return true;
}

SettingsWriteResult failure(SettingsWriteStatus status, const SettingsReadResult& snapshot, const QString& message)
{
    return {status, snapshot, {}, message};
}

} // 匿名命名空间

SettingsRepository::SettingsRepository(QString path, AtomicWriter writer)
    : m_path(path.trimmed().isEmpty() ? QString() : QDir::cleanPath(QFileInfo(path).absoluteFilePath())), m_writer(std::move(writer))
{
    if (!m_writer) m_writer = atomicWrite;
}

core::SettingsReadResult SettingsRepository::read() const { return readPath(m_path); }

core::SettingsWriteResult SettingsRepository::lockedWrite(const WriteOperation& operation) const
{
    PerformanceScope timing(PerformanceStage::Saving);
    if (m_path.isEmpty() || !QDir().mkpath(QFileInfo(m_path).absolutePath())) {
        return failure(SettingsWriteStatus::WriteFailed, {}, QString::fromUtf8("无法创建设置目录或设置路径为空"));
    }
    // 所有写入入口使用同一把跨进程锁；忙时立即返回，不阻塞界面等待其他进程。
    QLockFile lock(m_path + QStringLiteral(".lock"));
    if (!lock.tryLock(0)) {
        return failure(lock.error() == QLockFile::LockFailedError ? SettingsWriteStatus::Busy : SettingsWriteStatus::WriteFailed,
            read(), QString::fromUtf8("无法取得设置写入锁，请检查权限或稍后重试"));
    }
    const auto current = read();
    if (current.status == SettingsReadStatus::IoError) return failure(SettingsWriteStatus::ReadFailed, current, current.message);
    return operation(current);
}

core::SettingsWriteResult SettingsRepository::apply(const core::SettingsChangeSet& changes) const
{
    return lockedWrite([&](const SettingsReadResult& current) {
        if (current.status == SettingsReadStatus::InvalidJson) return failure(SettingsWriteStatus::ReadFailed, current, current.message);
        const auto merged = changes.merge(current.sourceJson);
        if (!merged.conflicts.isEmpty()) {
            return SettingsWriteResult{SettingsWriteStatus::Conflict, current, merged.conflicts,
                QString::fromUtf8("设置已被其他入口修改，请重新读取或导出当前草稿")};
        }
        if (!merged.changed) return SettingsWriteResult{SettingsWriteStatus::Unchanged, current, {}, QString::fromUtf8("设置未变化")};
        return writeDocument(current, merged.document);
    });
}

core::SettingsWriteResult SettingsRepository::replace(const core::PrintClientSettings& settings) const
{
    return lockedWrite([&](const SettingsReadResult& current) {
        const auto document = core::PrintClientSettingsJson::toJson(settings);
        if (current.status == SettingsReadStatus::Ready && current.sourceJson == document) {
            return SettingsWriteResult{SettingsWriteStatus::Unchanged, current, {}, QString::fromUtf8("设置未变化")};
        }
        // 整体替换保留旧 HTTP 语义；损坏原文必须先备份，不能冒充普通自动保存。
        return writeDocument(current, document);
    });
}

core::SettingsWriteResult SettingsRepository::restoreBackup(const QByteArray& expectedCurrentDigest) const
{
    return lockedWrite([&](const SettingsReadResult& current) {
        if (current.sourceDigest != expectedCurrentDigest) {
            return failure(SettingsWriteStatus::Conflict, current, QString::fromUtf8("恢复前设置已变化，请重新确认"));
        }
        const auto backup = readPath(m_path + QStringLiteral(".bak"));
        if (backup.status != SettingsReadStatus::Ready) {
            return failure(SettingsWriteStatus::ReadFailed, current, QString::fromUtf8("有效设置备份不存在或无法读取"));
        }
        if (current.status == SettingsReadStatus::Ready && backup.sourceJson == current.sourceJson) {
            return SettingsWriteResult{SettingsWriteStatus::Unchanged, current, {}, QString::fromUtf8("当前设置与备份相同")};
        }
        return writeDocument(current, backup.sourceJson, true);
    });
}

core::SettingsWriteResult SettingsRepository::writeDocument(const core::SettingsReadResult& previous, const QJsonObject& document,
    bool restoringBackup) const
{
    QString error;
    // 备份成功后才允许替换主文件；坏文件不覆盖最近的有效备份。
    if (previous.status == SettingsReadStatus::Ready || previous.status == SettingsReadStatus::InvalidJson) {
        // 恢复不能轮换正在读取的 .bak，主文件提交失败后必须还能重试相同恢复来源。
        const auto suffix = previous.status == SettingsReadStatus::Ready
            ? (restoringBackup ? QStringLiteral(".restore.bak") : QStringLiteral(".bak")) : QStringLiteral(".corrupt.bak");
        if (!m_writer(m_path + suffix, previous.sourceBytes, &error)) {
            return failure(SettingsWriteStatus::BackupFailed, previous, QString::fromUtf8("设置备份失败，原文件未覆盖：%1").arg(error));
        }
    }
    const auto bytes = QJsonDocument(document).toJson(QJsonDocument::Indented);
    if (!m_writer(m_path, bytes, &error)) {
        return failure(SettingsWriteStatus::WriteFailed, previous, QString::fromUtf8("设置写入失败，原文件未覆盖：%1").arg(error));
    }
    return {SettingsWriteStatus::Saved, parseBytes(bytes), {}, QString::fromUtf8("已保存设置")};
}

} // 命名空间 sleekpr::infrastructure

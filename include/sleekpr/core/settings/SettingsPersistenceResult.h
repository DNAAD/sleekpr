#pragma once

#include "sleekpr/core/settings/PrintClientSettings.h"

#include <QByteArray>
#include <QJsonObject>
#include <QStringList>

namespace sleekpr::core {

enum class SettingsReadStatus { Missing, Ready, InvalidJson, IoError };

struct SettingsReadResult
{
    SettingsReadStatus status = SettingsReadStatus::Missing;
    PrintClientSettings settings;
    QJsonObject sourceJson;
    QByteArray sourceBytes;
    QByteArray sourceDigest;
    QString message;
};

enum class SettingsWriteStatus { Saved, Unchanged, Conflict, Busy, ReadFailed, BackupFailed, WriteFailed };

struct SettingsWriteResult
{
    SettingsWriteStatus status = SettingsWriteStatus::WriteFailed;
    SettingsReadResult snapshot;
    QStringList conflicts;
    QString message;

    bool ok() const { return status == SettingsWriteStatus::Saved || status == SettingsWriteStatus::Unchanged; }
};

} // 命名空间 sleekpr::core

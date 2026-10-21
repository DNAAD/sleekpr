#pragma once
#include "sleekpr/infrastructure/settings/SettingsSaveSession.h"
#include <optional>

namespace sleekpr::app {
using SettingsSessionId = quint64;
struct SettingsSaveTicket
{
    SettingsSessionId sessionId = 0;
    quint64 epoch = 0;
    quint64 revision = 0;
    bool operator==(const SettingsSaveTicket&) const = default;
};
enum class SettingsSavePhase { Loading, Saved, Queued, Saving, Failed, Conflict };
struct SettingsPersistenceOutcome
{
    infrastructure::SettingsStorageOutcome storage;
    std::optional<SettingsSaveTicket> ticket;
};
struct SettingsSessionView
{
    QString title;
    quint64 epoch = 1, acceptedRevision = 0, savedRevision = 0, outcomeSerial = 0;
    bool running = false, pending = false, detached = false;
    SettingsSavePhase phase = SettingsSavePhase::Loading;
    std::optional<SettingsPersistenceOutcome> lastOutcome;
    std::optional<SettingsSaveTicket> runningTicket;
};
} // 命名空间 sleekpr::app

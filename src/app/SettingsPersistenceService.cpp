#include "sleekpr/app/SettingsPersistenceService.h"
#include "sleekpr/infrastructure/diagnostics/ClientPerformanceMetrics.h"
#include <QPointer>
#include <QThread>
#include <algorithm>
#include <utility>

namespace sleekpr::app {
using namespace infrastructure;
struct SettingsPersistenceService::Entry
{
    struct Save { SettingsSaveTicket ticket; core::PrintClientSettings settings; core::SettingsEditScope scope; };
    struct Control { SettingsStorageOperation kind; QByteArray digest; };
    std::shared_ptr<SettingsSaveSession> backend;
    SettingsSessionView view;
    std::optional<Save> pending, executing;
    std::optional<Control> control;
    bool paused = false;
};

SettingsPersistenceService::SettingsPersistenceService(QString path, AtomicWriter writer, QObject* parent)
    : QObject(parent), m_path(std::move(path)), m_writer(std::move(writer)) {}
SettingsPersistenceService::~SettingsPersistenceService()
{
    m_accepting = false;
    // 正常退出应已排空；异常的所有者销毁也不能丢掉已经接受的有效请求。
    for (const auto& entry : m_sessions) {
        const auto backend = entry->backend;
        if (entry->control) {
            const auto op = *entry->control;
            m_executor.submit([backend, op] {
                if (op.kind == SettingsStorageOperation::Open) return backend->open();
                if (op.kind == SettingsStorageOperation::Restore) return backend->restoreBackup(op.digest);
                return backend->reload();
            }, [](auto) {});
        } else if (entry->pending && !entry->paused && !m_suspended) {
            const auto request = *entry->pending;
            m_executor.submit([backend, request] { return backend->saveEdited(request.settings, request.scope); }, [](auto) {});
        }
    }
    m_executor.shutdown();
}

std::optional<SettingsSessionId> SettingsPersistenceService::openSession(QString title)
{
    if (!m_accepting || m_stopped) return {};
    const auto id = ++m_nextId;
    auto entry = std::make_shared<Entry>(); entry->view.title = std::move(title);
    entry->backend = std::make_shared<SettingsSaveSession>(m_path, m_writer);
    entry->control = Entry::Control{SettingsStorageOperation::Open, {}};
    m_sessions.insert(id, entry); pump();
    return id;
}
SettingsSessionView SettingsPersistenceService::view(SettingsSessionId id) const
{
    const auto entry = m_sessions.value(id);
    return entry ? entry->view : SettingsSessionView{};
}
QList<SettingsSessionId> SettingsPersistenceService::sessions() const { return m_sessions.keys(); }
void SettingsPersistenceService::publish(SettingsSessionId id)
{
    QPointer<SettingsPersistenceService> guard(this);
    emit sessionChanged(id);
    if (guard) emit activityChanged();
    if (guard) pump();
}
std::optional<SettingsSaveTicket> SettingsPersistenceService::requestSave(SettingsSessionId id,
    core::PrintClientSettings settings, core::SettingsEditScope scope)
{
    infrastructure::PerformanceScope timing(infrastructure::PerformanceStage::SaveEnqueue);
    const auto entry = m_sessions.value(id);
    if (!m_accepting || m_stopped || !entry || entry->control || entry->view.phase == SettingsSavePhase::Loading) return {};
    const SettingsSaveTicket ticket{id, entry->view.epoch, ++entry->view.acceptedRevision};
    // 这里保留累计目标，不提前和旧基线比较；在途 A 完成后，“撤销回原值”仍需真实写回。
    entry->pending = Entry::Save{ticket, std::move(settings), scope}; entry->view.pending = true;
    if (!entry->paused) entry->view.phase = entry->view.running ? SettingsSavePhase::Saving : SettingsSavePhase::Queued;
    publish(id); return ticket;
}
bool SettingsPersistenceService::retry(SettingsSessionId id)
{
    const auto entry = m_sessions.value(id);
    if (!entry || !entry->paused || !entry->pending || !entry->view.lastOutcome || m_stopped) return false;
    const auto& result = entry->view.lastOutcome->storage.writeResult;
    if (!result || result->status == core::SettingsWriteStatus::Conflict || result->status == core::SettingsWriteStatus::ReadFailed) return false;
    entry->paused = false; entry->view.phase = SettingsSavePhase::Queued; publish(id); return true;
}
bool SettingsPersistenceService::control(SettingsSessionId id, SettingsStorageOperation kind, QByteArray digest)
{
    const auto entry = m_sessions.value(id);
    if (!entry || entry->control || (entry->view.running && !entry->executing) || m_stopped) return false;
    entry->pending.reset(); entry->view.pending = false;
    entry->control = Entry::Control{kind, std::move(digest)};
    entry->view.phase = SettingsSavePhase::Loading;
    publish(id); return true;
}
bool SettingsPersistenceService::requestReload(SettingsSessionId id) { return control(id, SettingsStorageOperation::Reload); }
bool SettingsPersistenceService::requestRestore(SettingsSessionId id, QByteArray digest) { return control(id, SettingsStorageOperation::Restore, std::move(digest)); }
void SettingsPersistenceService::detachSession(SettingsSessionId id)
{
    const auto entry = m_sessions.value(id); if (!entry) return;
    entry->view.detached = true;
    if (!entry->view.running && !entry->pending && !entry->control && !entry->paused) m_sessions.remove(id);
}
void SettingsPersistenceService::setAcceptingSaves(bool value) { if (!m_stopped) m_accepting = value; }
void SettingsPersistenceService::setPendingSuspended(bool value) { m_suspended = value; pump(); }
void SettingsPersistenceService::discardPending(SettingsSessionId id)
{
    const auto entry = m_sessions.value(id); if (!entry) return;
    entry->pending.reset(); entry->view.pending = false; publish(id);
}
bool SettingsPersistenceService::hasRunningWork() const { return m_running; }
bool SettingsPersistenceService::hasControlWork() const
{
    if (m_controlRunning) return true;
    for (const auto& entry : m_sessions) if (entry->control) return true;
    return false;
}
bool SettingsPersistenceService::isIdle() const
{
    if (m_running) return false;
    for (const auto& entry : m_sessions) if (entry->pending || entry->control) return false;
    return true;
}
bool SettingsPersistenceService::shutdownIfIdle()
{
    if (!isIdle()) return false;
    m_accepting = false; m_stopped = true; m_executor.shutdown(); return true;
}

void SettingsPersistenceService::pump()
{
    Q_ASSERT(QThread::currentThread() == thread());
    if (m_running || m_stopped) return;
    auto ids = m_sessions.keys();
    // 从上次服务的会话之后轮转，避免连续自动保存饿死另一个窗口的显式保存。
    std::rotate(ids.begin(), std::upper_bound(ids.begin(), ids.end(), m_cursor), ids.end());
    for (const auto id : ids) {
        const auto entry = m_sessions.value(id);
        const bool isControl = entry->control.has_value();
        if (!isControl && (!entry->pending || entry->paused || m_suspended)) continue;
        const auto backend = entry->backend;
        SettingsStorageOperation kind = SettingsStorageOperation::Save;
        std::optional<SettingsSaveTicket> ticket;
        std::function<SettingsStorageOutcome()> work;
        if (isControl) {
            const auto control = *std::exchange(entry->control, {}); kind = control.kind;
            work = [backend, control] {
                if (control.kind == SettingsStorageOperation::Open) return backend->open();
                if (control.kind == SettingsStorageOperation::Restore) return backend->restoreBackup(control.digest);
                return backend->reload();
            };
        } else {
            entry->executing = std::exchange(entry->pending, {});
            const auto save = *entry->executing; ticket = save.ticket;
            work = [backend, save] { return backend->saveEdited(save.settings, save.scope); };
        }
        m_running = true; m_controlRunning = isControl; m_cursor = id;
        entry->view.running = true; entry->view.pending = entry->pending.has_value();
        entry->view.runningTicket = ticket;
        entry->view.phase = isControl ? SettingsSavePhase::Loading : SettingsSavePhase::Saving;
        QPointer<SettingsPersistenceService> guard(this);
        m_executor.submit(std::move(work), [guard, id, kind, ticket](SettingsStorageOutcome result) {
            // 弱对象仅在 GUI 回调使用；工作线程完全不读取 GUI 对象或会话队列。
            if (guard) guard->complete(id, kind, ticket, std::move(result));
        });
        emit sessionChanged(id);
        return;
    }
}

void SettingsPersistenceService::complete(SettingsSessionId id, SettingsStorageOperation kind,
    std::optional<SettingsSaveTicket> ticket, SettingsStorageOutcome result)
{
    const auto entry = m_sessions.value(id);
    m_running = false; m_controlRunning = false;
    if (!entry) { pump(); return; }
    result.operation = kind;
    if (result.executionFailed && entry->view.lastOutcome) {
        // 执行器异常只有错误状态，没有新的存储基线；不能用默认设置清空此前确认值。
        const auto& previous = entry->view.lastOutcome->storage;
        const auto message = result.observed.message;
        result.expectedSettings = previous.expectedSettings;
        result.observed = previous.observed;
        if (kind == SettingsStorageOperation::Open || kind == SettingsStorageOperation::Reload) {
            result.observed.status = core::SettingsReadStatus::IoError;
            result.observed.message = message;
            result.writeResult.reset();
        } else if (result.writeResult) result.writeResult->snapshot = result.observed;
    }
    const bool success = result.writeResult ? result.writeResult->ok() : result.baselineReset;
    entry->view.running = false;
    entry->view.runningTicket.reset();
    if (kind == SettingsStorageOperation::Save) {
        if (success) entry->view.savedRevision = ticket->revision;
        else {
            entry->paused = true;
            if (!entry->pending && !entry->control) entry->pending = entry->executing;
        }
        entry->executing.reset();
    } else if (success) {
        entry->paused = false;
        if (kind != SettingsStorageOperation::Open) ++entry->view.epoch;
        entry->view.acceptedRevision = entry->view.savedRevision = 0;
    } else entry->paused = true;
    entry->view.pending = entry->pending.has_value();
    if (!success) {
        entry->view.phase = result.writeResult && result.writeResult->status == core::SettingsWriteStatus::Conflict
            ? SettingsSavePhase::Conflict : SettingsSavePhase::Failed;
    } else entry->view.phase = entry->pending ? SettingsSavePhase::Queued : SettingsSavePhase::Saved;
    if (entry->control) entry->view.phase = SettingsSavePhase::Loading;
    entry->view.lastOutcome = SettingsPersistenceOutcome{std::move(result), ticket}; ++entry->view.outcomeSerial;
    const bool remove = entry->view.detached && !entry->pending && !entry->control && !entry->paused;
    if (remove) m_sessions.remove(id);
    publish(id);
}
} // 命名空间 sleekpr::app

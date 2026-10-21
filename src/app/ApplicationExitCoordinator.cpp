#include "sleekpr/app/ApplicationExitCoordinator.h"
#include <QScopedValueRollback>
#include <QTimer>

namespace sleekpr::app {
ApplicationExitCoordinator::ApplicationExitCoordinator(SettingsPersistenceService& service,
    std::function<void(bool)> admission, std::function<void()> quit, QObject* parent)
    : QObject(parent), m_service(service), m_admission(std::move(admission)), m_quit(std::move(quit))
{
    connect(&service, &SettingsPersistenceService::activityChanged, this, [this] { refresh(); });
}
ApplicationExitCoordinator::~ApplicationExitCoordinator()
{
    if (m_state != ExitState::Running && m_state != ExitState::Quitting) cancelExit();
}
void ApplicationExitCoordinator::addParticipant(ExitParticipant participant) { m_participants.append(std::move(participant)); }
ExitState ApplicationExitCoordinator::state() const { return m_state; }
quint64 ApplicationExitCoordinator::generation() const { return m_generation; }
QStringList ApplicationExitCoordinator::issues() const { return m_issues; }
QStringList ApplicationExitCoordinator::manualDraftTitles() const { return m_manualTitles; }
void ApplicationExitCoordinator::setState(ExitState state) { m_state = state; emit stateChanged(); }
void ApplicationExitCoordinator::requestExit()
{
    if (m_state != ExitState::Running) { emit stateChanged(); return; }
    ++m_generation; m_ignoreManual = false; m_discard = false; m_state = ExitState::Collecting;
    m_preparing = true;
    if (m_admission) m_admission(true);
    for (const auto& participant : m_participants) if (participant.setPaused) participant.setPaused(true);
    for (const auto& participant : m_participants) if (participant.flushEdits) participant.flushEdits();
    m_preparing = false; refresh();
}
void ApplicationExitCoordinator::cancelExit()
{
    if (m_state == ExitState::Running || m_state == ExitState::Quitting) return;
    ++m_generation; m_state = ExitState::Running; m_ignoreManual = false; m_discard = false;
    m_service.setAcceptingSaves(true); m_service.setPendingSuspended(false);
    if (m_admission) m_admission(false);
    for (const auto& participant : m_participants) if (participant.setPaused) participant.setPaused(false);
    m_issues.clear(); m_manualTitles.clear(); emit stateChanged();
}
void ApplicationExitCoordinator::saveManualAndContinue()
{
    if (m_state == ExitState::Running || m_state == ExitState::Quitting) return;
    m_preparing = true; m_service.setAcceptingSaves(true);
    for (const auto& participant : m_participants) {
        if (participant.inspect && participant.inspect().manualDraft && participant.saveManualDraft) participant.saveManualDraft();
    }
    m_preparing = false; refresh();
}
void ApplicationExitCoordinator::continueWithoutSavingManual()
{
    if (m_state == ExitState::Running || m_state == ExitState::Quitting) return;
    m_ignoreManual = true; refresh();
}
bool ApplicationExitCoordinator::canRetry() const
{
    for (const auto id : m_service.sessions()) {
        const auto view = m_service.view(id);
        if (view.phase != SettingsSavePhase::Failed || !view.lastOutcome || !view.pending) continue;
        const auto& result = view.lastOutcome->storage.writeResult;
        if (result && result->status != core::SettingsWriteStatus::Conflict && result->status != core::SettingsWriteStatus::ReadFailed) return true;
    }
    return false;
}
void ApplicationExitCoordinator::retryAndContinue()
{
    if (m_state == ExitState::Running || m_state == ExitState::Quitting) return;
    for (const auto id : m_service.sessions()) m_service.retry(id);
    refresh();
}
void ApplicationExitCoordinator::discardAndContinue()
{
    if (m_state == ExitState::Running || m_state == ExitState::Quitting) return;
    m_discard = true; m_service.setPendingSuspended(true); refresh();
}
void ApplicationExitCoordinator::refresh()
{
    if (m_state == ExitState::Running || m_state == ExitState::Quitting || m_preparing || m_refreshing || m_finalizing) return;
    QScopedValueRollback refreshing(m_refreshing, true);
    m_issues.clear(); m_manualTitles.clear(); bool activeSubmission = false;
    for (const auto& participant : m_participants) {
        if (!participant.inspect) continue;
        const auto draft = participant.inspect(); activeSubmission = activeSubmission || draft.activeSubmission;
        if (draft.manualDraft) m_manualTitles.append(participant.title);
        if (draft.invalidDraft) m_issues.append(participant.title + QString::fromUtf8("：存在未修正的 JSON 草稿"));
        for (const auto& issue : draft.issues) m_issues.append(participant.title + QString::fromUtf8("：") + issue);
    }
    for (const auto id : m_service.sessions()) {
        const auto view = m_service.view(id);
        if (view.phase != SettingsSavePhase::Failed && view.phase != SettingsSavePhase::Conflict) continue;
        QString reason = view.phase == SettingsSavePhase::Conflict ? QString::fromUtf8("配置冲突，需要返回编辑器处理") : QString::fromUtf8("保存失败，草稿已保留");
        if (view.lastOutcome) {
            const auto& outcome = view.lastOutcome->storage;
            const auto detail = outcome.writeResult ? outcome.writeResult->message : outcome.observed.message;
            if (!detail.isEmpty()) reason += QStringLiteral(" ") + detail;
        }
        m_issues.append(view.title + QString::fromUtf8("：") + reason);
    }
    if (!m_discard && !m_ignoreManual && !m_manualTitles.isEmpty()) { setState(ExitState::Collecting); return; }
    m_service.setAcceptingSaves(false);
    if (!m_discard && !m_issues.isEmpty()) { setState(ExitState::Blocked); return; }
    const bool waiting = activeSubmission || m_service.hasRunningWork() || m_service.hasControlWork()
        || (!m_discard && !m_service.isIdle());
    if (waiting) { setState(ExitState::WaitingForSaves); return; }
    // 放弃只在不可再取消的最终授权点清除意图；Ready 的零延迟回调前仍可返回编辑。
    const auto generation = m_generation;
    setState(ExitState::Ready);
    QTimer::singleShot(0, this, [this, generation] { finalize(generation); });
}
void ApplicationExitCoordinator::finalize(quint64 generation)
{
    if (generation != m_generation || m_state != ExitState::Ready || m_finalizing) return;
    QScopedValueRollback finalizing(m_finalizing, true);
    for (const auto& participant : m_participants) if (participant.inspect && participant.inspect().activeSubmission) return;
    if (m_service.hasRunningWork() || m_service.hasControlWork()) return;
    if (m_discard) for (const auto id : m_service.sessions()) m_service.discardPending(id);
    if (!m_service.shutdownIfIdle()) return;
    const auto quit = m_quit;
    setState(ExitState::Quitting);
    if (quit) quit();
}
} // 命名空间 sleekpr::app

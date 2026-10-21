#pragma once
#include "sleekpr/app/ApplicationExitParticipant.h"
#include "sleekpr/app/SettingsPersistenceService.h"
#include <QList>

namespace sleekpr::app {
enum class ExitState { Running, Collecting, WaitingForSaves, Blocked, Ready, Quitting };
class ApplicationExitCoordinator final : public QObject
{
    Q_OBJECT
public:
    ApplicationExitCoordinator(SettingsPersistenceService&, std::function<void(bool)> setAdmission,
        std::function<void()> quit, QObject* parent = nullptr);
    ~ApplicationExitCoordinator() override;
    void addParticipant(ExitParticipant);
    void requestExit();
    void cancelExit();
    void saveManualAndContinue();
    void continueWithoutSavingManual();
    void retryAndContinue();
    void discardAndContinue();
    void refresh();
    ExitState state() const;
    quint64 generation() const;
    QStringList issues() const;
    QStringList manualDraftTitles() const;
    bool canRetry() const;
signals:
    void stateChanged();
private:
    void setState(ExitState);
    void finalize(quint64 generation);
    SettingsPersistenceService& m_service;
    std::function<void(bool)> m_admission;
    std::function<void()> m_quit;
    QList<ExitParticipant> m_participants;
    QStringList m_issues, m_manualTitles;
    ExitState m_state = ExitState::Running;
    quint64 m_generation = 0;
    bool m_ignoreManual = false, m_discard = false, m_preparing = false, m_refreshing = false, m_finalizing = false;
};
} // 命名空间 sleekpr::app

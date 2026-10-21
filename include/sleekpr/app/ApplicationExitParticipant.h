#pragma once
#include <QStringList>
#include <functional>

namespace sleekpr::app {
struct ExitDraftState
{
    bool manualDraft = false, invalidDraft = false, activeSubmission = false;
    QStringList issues;
};
struct ExitParticipant
{
    QString id, title;
    std::function<void()> flushEdits;
    std::function<ExitDraftState()> inspect;
    std::function<void()> saveManualDraft;
    std::function<void(bool)> setPaused;
    std::function<void()> focus;
};
} // 命名空间 sleekpr::app

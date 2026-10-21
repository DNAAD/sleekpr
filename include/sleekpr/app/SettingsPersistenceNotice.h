#pragma once

#include "sleekpr/core/settings/SettingsPersistenceResult.h"
#include <QWidget>
#include <functional>

class QLabel;
class QPushButton;

namespace sleekpr::app {

class SettingsPersistenceNotice final : public QWidget
{
public:
    explicit SettingsPersistenceNotice(QWidget* parent = nullptr);
    void showFailure(const core::SettingsWriteResult& result);
    void clear();
    QByteArray observedFailureDigest() const;
    void setActions(std::function<void()> reload, std::function<void()> retry,
        std::function<void()> exportDraft = {}, std::function<void()> restore = {});
private:
    QLabel* m_message;
    QPushButton* m_reload;
    QPushButton* m_retry;
    QPushButton* m_export;
    QPushButton* m_restore;
    QByteArray m_failureDigest;
};

} // 命名空间 sleekpr::app

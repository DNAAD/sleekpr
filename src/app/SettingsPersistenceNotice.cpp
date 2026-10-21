#include "sleekpr/app/SettingsPersistenceNotice.h"
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>

namespace sleekpr::app {

SettingsPersistenceNotice::SettingsPersistenceNotice(QWidget* parent) : QWidget(parent)
{
    setObjectName(QStringLiteral("settingsPersistenceNotice"));
    auto* layout = new QHBoxLayout(this);
    m_message = new QLabel(this);
    m_message->setWordWrap(true);
    layout->addWidget(m_message, 1);
    const auto button = [this, layout](const char* text, const char* name) {
        auto* value = new QPushButton(QString::fromUtf8(text), this);
        value->setObjectName(QString::fromLatin1(name));
        layout->addWidget(value);
        return value;
    };
    m_reload = button("重新读取", "persistenceReloadButton");
    m_retry = button("重试保存", "persistenceRetryButton");
    m_export = button("导出草稿", "persistenceExportButton");
    m_restore = button("恢复备份", "persistenceRestoreButton");
    setActions({}, {});
    hide();
}

void SettingsPersistenceNotice::setActions(std::function<void()> reload, std::function<void()> retry,
    std::function<void()> exportDraft, std::function<void()> restore)
{
    const auto bind = [this](QPushButton* button, std::function<void()> action) {
        QObject::disconnect(button, nullptr, this, nullptr);
        button->setVisible(bool(action));
        if (action) connect(button, &QPushButton::clicked, this, [action] { action(); });
    };
    bind(m_reload, std::move(reload));
    bind(m_retry, std::move(retry));
    bind(m_export, std::move(exportDraft));
    bind(m_restore, std::move(restore));
}

void SettingsPersistenceNotice::showFailure(const core::SettingsWriteResult& result)
{
    m_failureDigest = result.snapshot.sourceDigest;
    QString reason;
    switch (result.status) {
    case core::SettingsWriteStatus::Conflict: reason = QString::fromUtf8("配置已被其他入口修改，自动保存已暂停。"); break;
    case core::SettingsWriteStatus::Busy: reason = QString::fromUtf8("配置正被其他进程使用，请稍后重试。"); break;
    case core::SettingsWriteStatus::ReadFailed: reason = QString::fromUtf8("配置无法读取，已停止自动保存。"); break;
    case core::SettingsWriteStatus::BackupFailed: reason = QString::fromUtf8("备份失败，原文件未修改。"); break;
    default: reason = QString::fromUtf8("写入失败，草稿仍保留在当前窗口。"); break;
    }
    m_message->setText(QString::fromUtf8("未保存：") + reason + QStringLiteral(" ") + result.message);
    m_retry->setEnabled(result.status != core::SettingsWriteStatus::Conflict
        && result.status != core::SettingsWriteStatus::ReadFailed);
    m_restore->setEnabled(result.snapshot.status == core::SettingsReadStatus::InvalidJson
        || result.snapshot.status == core::SettingsReadStatus::Missing);
    show();
}

void SettingsPersistenceNotice::clear() { hide(); }
QByteArray SettingsPersistenceNotice::observedFailureDigest() const { return m_failureDigest; }

} // 命名空间 sleekpr::app

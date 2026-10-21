#include "sleekpr/app/ApplicationExitDialog.h"
#include <QDialogButtonBox>
#include <QLabel>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QVBoxLayout>

namespace sleekpr::app {
ApplicationExitDialog::ApplicationExitDialog(ApplicationExitCoordinator& coordinator, QWidget* parent)
    : QDialog(parent), m_coordinator(coordinator)
{
    setWindowTitle(QString::fromUtf8("退出前检查")); resize(650, 330); setMinimumWidth(460);
    auto* layout = new QVBoxLayout(this); layout->setContentsMargins(20, 20, 20, 20); layout->setSpacing(12);
    m_title = new QLabel(this); m_title->setWordWrap(true); layout->addWidget(m_title);
    m_details = new QPlainTextEdit(this); m_details->setReadOnly(true); layout->addWidget(m_details, 1);
    auto* buttons = new QDialogButtonBox(this); layout->addWidget(buttons);
    const auto button = [buttons](const char* title, const char* name) {
        auto* value = buttons->addButton(QString::fromUtf8(title), QDialogButtonBox::ActionRole);
        value->setObjectName(QString::fromLatin1(name)); value->setAutoDefault(false); return value;
    };
    m_save = button("保存并退出", "exitSaveButton"); m_skip = button("不保存并退出", "exitSkipManualButton");
    m_retry = button("重试保存", "exitRetryButton"); m_discard = button("放弃未保存内容并退出", "exitDiscardButton");
    m_back = button("返回编辑", "exitReturnButton"); m_back->setDefault(true);
    connect(m_back, &QPushButton::clicked, this, [&coordinator] { coordinator.cancelExit(); });
    connect(m_save, &QPushButton::clicked, this, [&coordinator] { coordinator.saveManualAndContinue(); });
    connect(m_skip, &QPushButton::clicked, this, [&coordinator] { coordinator.continueWithoutSavingManual(); });
    connect(m_retry, &QPushButton::clicked, this, [&coordinator] { coordinator.retryAndContinue(); });
    connect(m_discard, &QPushButton::clicked, this, [this] {
        if (m_confirmation) { m_confirmation->raise(); return; }
        const auto generation = m_coordinator.generation();
        auto* box = new QMessageBox(QMessageBox::Warning, QString::fromUtf8("放弃未保存内容"),
            QString::fromUtf8("确认放弃尚未保存的内容并退出？已完成的保存不会回滚，正在执行的写入仍需安全完成。"),
            QMessageBox::Yes | QMessageBox::No, this);
        box->setDefaultButton(QMessageBox::No); box->setAttribute(Qt::WA_DeleteOnClose); m_confirmation = box;
        connect(box, &QMessageBox::finished, this, [this, generation](int result) {
            if (result == QMessageBox::Yes && generation == m_coordinator.generation()
                && m_coordinator.state() != ExitState::Running) m_coordinator.discardAndContinue();
        });
        box->open();
    });
    connect(&coordinator, &ApplicationExitCoordinator::stateChanged, this, [this] { refresh(); });
    refresh();
}
void ApplicationExitDialog::reject() { m_coordinator.cancelExit(); }
void ApplicationExitDialog::refresh()
{
    const auto state = m_coordinator.state();
    if (state == ExitState::Running || state == ExitState::Quitting) {
        if (m_confirmation) m_confirmation->close(); hide(); return;
    }
    const auto manual = m_coordinator.manualDraftTitles();
    const bool collecting = state == ExitState::Collecting && !manual.isEmpty();
    m_title->setText(collecting ? QString::fromUtf8("以下窗口有尚未请求保存的修改")
        : state == ExitState::Blocked ? QString::fromUtf8("存在未保存问题，尚未退出") : QString::fromUtf8("正在完成保存，请稍候"));
    auto details = m_coordinator.issues();
    if (collecting) details.prepend(manual.join(QStringLiteral("、")));
    if (details.isEmpty()) details.append(QString::fromUtf8("等待已接受的保存或打印提交完成。"));
    m_details->setPlainText(details.join(QStringLiteral("\n\n")));
    m_save->setVisible(collecting); m_skip->setVisible(collecting);
    m_retry->setVisible(state == ExitState::Blocked); m_retry->setEnabled(m_coordinator.canRetry());
    if (!isVisible() && state != ExitState::Ready) { show(); raise(); activateWindow(); m_back->setFocus(); }
}
} // 命名空间 sleekpr::app

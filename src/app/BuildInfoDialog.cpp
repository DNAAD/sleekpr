#include "sleekpr/app/BuildInfoDialog.h"
#include "sleekpr/app/BuildInfo.h"
#include <QApplication>
#include <QClipboard>
#include <QCoreApplication>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QJsonDocument>
#include <QLabel>
#include <QPushButton>
#include <QStyle>
#include <QVBoxLayout>
#include <utility>

namespace sleekpr::app {

BuildInfoDialog::BuildInfoDialog(QString path, QWidget* parent) : QDialog(parent), m_settingsPath(std::move(path))
{
    const auto info = BuildInfo::current();
    setWindowTitle(QString::fromUtf8("sleekpr 版本与诊断"));
    setMinimumWidth(700);
    auto* layout = new QVBoxLayout(this);
    auto* title = new QLabel(QStringLiteral("sleekpr %1").arg(info.version), this);
    auto font = title->font();
    font.setPointSize(16);
    font.setBold(true);
    title->setFont(font);
    layout->addWidget(title);
    auto* form = new QFormLayout;
    const auto row = [this, form](const char* label, const QString& value) {
        auto* text = new QLabel(value, this);
        text->setTextInteractionFlags(Qt::TextSelectableByMouse);
        text->setWordWrap(true);
        form->addRow(QString::fromUtf8(label), text);
    };
    const auto state = [](std::optional<bool> value) {
        return !value ? QString::fromUtf8("未知") : *value ? QString::fromUtf8("有未提交改动") : QString::fromUtf8("干净");
    };
    row("构建模式", info.configuration);
    row("Git 提交", info.revision);
    row("源码状态", state(info.sourceDirty));
    row("工作区状态", state(info.workspaceDirty));
    row("源码指纹", info.sourceDigest);
    row("Qt 运行时", QString::fromLatin1(qVersion()));
    row("程序路径", QCoreApplication::applicationFilePath());
    row("配置路径", m_settingsPath);
    layout->addLayout(form);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    auto* copy = buttons->addButton(QString::fromUtf8("复制诊断"), QDialogButtonBox::ActionRole);
    copy->setObjectName(QStringLiteral("buildInfoCopyButton"));
    copy->setIcon(style()->standardIcon(QStyle::SP_FileDialogDetailedView));
    connect(copy, &QPushButton::clicked, this, [this] {
        QApplication::clipboard()->setText(QString::fromUtf8(QJsonDocument(diagnosticJson()).toJson(QJsonDocument::Indented)));
    });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);
}

QJsonObject BuildInfoDialog::diagnosticJson() const
{
    // 只复制构建标识与运行时路径，绝不打开配置文件或附带模板、字段值、设备凭据。
    auto result = BuildInfo::current().toJson();
    result.insert(QStringLiteral("programPath"), QCoreApplication::applicationFilePath());
    result.insert(QStringLiteral("settingsPath"), m_settingsPath);
    result.insert(QStringLiteral("runtimeQtVersion"), QString::fromLatin1(qVersion()));
    return result;
}

} // 命名空间 sleekpr::app

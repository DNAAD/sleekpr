#include "sleekpr/app/FieldBindingSelector.h"
#include "sleekpr/app/AppTheme.h"

#include <QAbstractItemView>
#include <QComboBox>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLineEdit>
#include <QLabel>
#include <QPushButton>
#include <QSet>
#include <QSignalBlocker>
#include <QStyle>
#include <QVBoxLayout>

#include <algorithm>

namespace sleekpr::app {

FieldBindingSelector::FieldBindingSelector(QWidget* parent) : QWidget(parent)
{
    setObjectName(QStringLiteral("elementFieldBindingSelector"));
    setMinimumWidth(0);
    setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(6);
    auto* keyRow = new QHBoxLayout;
    keyRow->setContentsMargins(0, 0, 0, 0);
    keyRow->setSpacing(4);
    m_combo = new QComboBox(this);
    m_combo->setObjectName(QStringLiteral("elementFieldBindingCombo"));
    m_combo->setAccessibleName(QString::fromUtf8("绑定 key"));
    // 当前行直接填写接口 key；下拉选择读取 itemData，不反向解析中文说明。
    m_combo->setEditable(true);
    m_combo->setCompleter(nullptr);
    m_combo->setInsertPolicy(QComboBox::NoInsert);
    m_combo->setMinimumWidth(0);
    m_combo->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    m_combo->setMinimumContentsLength(8);
    m_combo->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    m_combo->view()->setTextElideMode(Qt::ElideRight);
    auto* custom = new QPushButton(this);
    custom->setObjectName(QStringLiteral("elementCustomFieldButton"));
    configureIconButton(custom, QStyle::SP_FileDialogDetailedView, QString::fromUtf8("输入自定义字段 key"));
    keyRow->addWidget(m_combo, 1);
    keyRow->addWidget(custom);
    layout->addLayout(keyRow);
    auto* nameLabel = new QLabel(QString::fromUtf8("显示名称（displayName）"), this);
    m_displayNameEdit = new QLineEdit(this);
    m_displayNameEdit->setObjectName(QStringLiteral("elementFieldDisplayNameEdit"));
    m_displayNameEdit->setAccessibleName(QString::fromUtf8("显示名称"));
    m_displayNameEdit->setMinimumWidth(0);
    m_displayNameEdit->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    m_displayNameEdit->setClearButtonEnabled(true);
    nameLabel->setBuddy(m_displayNameEdit);
    layout->addWidget(nameLabel);
    layout->addWidget(m_displayNameEdit);
    setFocusProxy(m_combo);
    connect(m_combo, &QComboBox::currentIndexChanged, this, [this](int index) {
        if (index < 0) return;
        // 显示标签可以重名或含括号，真实 key 只从 itemData 读取，绝不反向解析文案。
        const auto key = m_combo->itemData(index).toString();
        if (key == m_key) return;
        m_key = key;
        {
            const QSignalBlocker blocker(m_combo);
            m_combo->setEditText(m_key);
        }
        resetDisplayName();
        m_combo->setToolTip(m_combo->itemText(index));
        emit fieldKeyEdited();
    });
    connect(m_combo, &QComboBox::activated, this, [this](int) {
        // 重新选择同一项也会把可编辑组合框的文字换成选项标签，当前行仍必须只显示 key。
        const QSignalBlocker blocker(m_combo);
        m_combo->setEditText(m_key);
    });
    connect(m_combo->lineEdit(), &QLineEdit::textEdited, this, [this](const QString& text) {
        if (text == m_key) return;
        m_key = text;
        const auto cursorPosition = m_combo->lineEdit()->cursorPosition();
        {
            const QSignalBlocker blocker(m_combo);
            m_combo->setCurrentIndex(-1);
            m_combo->setEditText(text);
            m_combo->lineEdit()->setCursorPosition(cursorPosition);
        }
        resetDisplayName();
        emit fieldKeyTextEdited();
    });
    connect(m_displayNameEdit, &QLineEdit::textChanged, this, [this] { emit displayNameEdited(); });
    connect(m_displayNameEdit, &QLineEdit::editingFinished, this, &FieldBindingSelector::displayNameEditingFinished);
    connect(custom, &QPushButton::clicked, this, [this] {
        QInputDialog dialog(this);
        dialog.setWindowTitle(QString::fromUtf8("自定义绑定字段"));
        dialog.setLabelText(QString::fromUtf8("字段 key"));
        dialog.setInputMode(QInputDialog::TextInput);
        dialog.setTextValue(m_key);
        // 先完成主题布局再显示，避免 Windows 用过小的默认窗口裁切中文字段输入。
        dialog.setMinimumWidth(360);
        dialog.ensurePolished();
        dialog.adjustSize();
        if (dialog.exec() != QDialog::Accepted) return;
        const auto key = dialog.textValue().trimmed();
        if (key == m_key) return;
        setOptionsAndValue(m_options, key);
        emit fieldKeyEdited();
    });
}

void FieldBindingSelector::setOptionsAndValue(const QList<DesignerFieldOption>& options, const QString& key)
{
    // 属性位置频繁刷新时，不重建相同选项，也不制造自动应用或撤销记录。
    if (m_options == options && m_key == key && m_combo->count() > 0) return;
    m_options = options;
    m_key = key;
    const QSignalBlocker blocker(m_combo);
    m_combo->clear();
    QSet<QString> keys;
    for (const auto& option : m_options) {
        if (option.key.isEmpty() || keys.contains(option.key)) continue;
        keys.insert(option.key);
        const auto name = option.displayName.trimmed();
        const auto label = name.isEmpty() || name == option.key ? option.key
            : QString::fromUtf8("%1（%2）").arg(name, option.key);
        m_combo->addItem(label, option.key);
        m_combo->setItemData(m_combo->count() - 1, label, Qt::ToolTipRole);
    }
    auto index = m_combo->findData(key);
    if (index < 0) {
        // 无定义的旧字段也保留原值；即使 key 恰好长得像中文标签，也不偷偷改名。
        m_combo->insertItem(0, key.isEmpty() ? QString::fromUtf8("未绑定字段") : key, key);
        index = 0;
    }
    m_combo->setCurrentIndex(index);
    m_combo->setEditText(m_key);
    m_combo->setToolTip(m_combo->itemText(index));
    resetDisplayName();
}

QString FieldBindingSelector::fieldKey() const { return m_key; }
QString FieldBindingSelector::displayName() const { return m_displayNameEdit->text(); }
bool FieldBindingSelector::isDisplayNameEdited() const { return displayName().trimmed() != m_baseDisplayName; }

void FieldBindingSelector::resetDisplayName()
{
    const auto option = std::find_if(m_options.cbegin(), m_options.cend(), [this](const auto& item) { return item.key == m_key; });
    m_baseDisplayName = option == m_options.cend() ? QString() : option->displayName.trimmed();
    const QSignalBlocker blocker(m_displayNameEdit);
    if (m_displayNameEdit->text() != m_baseDisplayName) m_displayNameEdit->setText(m_baseDisplayName);
    m_displayNameEdit->setPlaceholderText(m_key);
}

} // 命名空间 sleekpr::app

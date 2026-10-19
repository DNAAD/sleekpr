#include "sleekpr/app/ElementBindingEditor.h"

#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QScopedValueRollback>
#include <QSignalBlocker>
#include <QTableWidget>
#include <QVBoxLayout>
#include <algorithm>

namespace sleekpr::app {

ElementBindingEditor::ElementBindingEditor(QWidget* parent) : QWidget(parent), m_table(new QTableWidget(this))
{
    setObjectName(QStringLiteral("elementBindingEditor"));
    setMinimumWidth(0);
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(6);
    layout->addWidget(new QLabel(QString::fromUtf8("数据绑定"), this));
    m_table->setObjectName(QStringLiteral("elementBindingsTable"));
    m_table->setColumnCount(2);
    m_table->setHorizontalHeaderLabels({QStringLiteral("key"), QStringLiteral("displayName")});
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_table->horizontalHeader()->setMinimumSectionSize(60);
    m_table->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    m_table->verticalHeader()->setDefaultSectionSize(36);
    m_table->verticalHeader()->setFixedWidth(44);
    m_table->setSelectionMode(QAbstractItemView::NoSelection);
    m_table->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_table->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    layout->addWidget(m_table);
    setBindings({}, {});
}

ElementBindingEditor::~ElementBindingEditor()
{
    // 子控件在成员之后析构，不能让失焦信号再访问已经销毁的绑定模型。
    for (auto* edit : findChildren<QLineEdit*>()) QObject::disconnect(edit, nullptr, this, nullptr);
}

void ElementBindingEditor::setBindings(const QList<DesignerFieldBinding>& bindings, const QList<DesignerFieldOption>& options)
{
    QScopedValueRollback<bool> loading(m_loading, true);
    m_options = options;
    m_bindings = bindings;
    m_baseNames.clear();
    m_table->verticalHeader()->setVisible(std::any_of(bindings.cbegin(), bindings.cend(), [](const auto& binding) {
        return binding.source != DesignerBindingSource::Value;
    }));
    // 复用已有输入控件，自动应用后只更新确实变化的文本，保留焦点和光标。
    m_table->setRowCount(bindings.size());
    for (int row = 0; row < bindings.size(); ++row) {
        const auto& binding = bindings[row];
        m_baseNames.append(binding.displayName.trimmed());
        const auto caption = binding.source == DesignerBindingSource::ArrayDataPath ? QString::fromUtf8("数据源")
            : binding.source == DesignerBindingSource::ArrayCellTemplate ? QString::fromUtf8("行内") : QString::number(row + 1);
        m_table->setVerticalHeaderItem(row, new QTableWidgetItem(caption));
        for (int column = 0; column < 2; ++column) {
            auto* edit = qobject_cast<QLineEdit*>(m_table->cellWidget(row, column));
            if (edit == nullptr) {
                edit = new QLineEdit(m_table);
                edit->setMinimumWidth(0);
                edit->setClearButtonEnabled(column == 1);
                edit->setObjectName(QStringLiteral("binding%1_%2").arg(column == 0 ? QStringLiteral("Key") : QStringLiteral("DisplayName")).arg(row));
                m_table->setCellWidget(row, column, edit);
                connect(edit, &QLineEdit::textChanged, this, [this, row, column](const QString& text) {
                    if (m_loading || row >= m_bindings.size()) return;
                    auto& current = m_bindings[row];
                    if (column == 0) {
                        current.key = text;
                        const auto option = std::find_if(m_options.cbegin(), m_options.cend(), [&](const auto& item) { return item.key == text.trimmed(); });
                        current.displayName = option == m_options.cend() ? QString() : option->displayName;
                        current.displayNameEdited = false;
                        m_baseNames[row] = current.displayName.trimmed();
                        auto* name = qobject_cast<QLineEdit*>(m_table->cellWidget(row, 1));
                        const QSignalBlocker blocker(name);
                        name->setText(current.displayName);
                        name->setPlaceholderText(text);
                    } else {
                        current.displayName = text;
                        current.displayNameEdited = text.trimmed() != m_baseNames[row];
                    }
                    emit bindingsEdited();
                });
                connect(edit, &QLineEdit::editingFinished, this, [this] {
                    if (m_loading) return;
                    // 自动应用暂缓的重名合并，在回车或失焦后显式确认，不能在输入到一半时删掉编辑行。
                    QScopedValueRollback<bool> finishing(m_finishing, true);
                    emit bindingsEdited();
                    emit editingFinished();
                });
            }
            edit->setAccessibleName(QStringLiteral("%1 %2").arg(caption, column == 0 ? QStringLiteral("key") : QStringLiteral("displayName")));
            const QSignalBlocker blocker(edit);
            const auto text = column == 0 ? binding.key : binding.displayName;
            if (edit->text() != text) edit->setText(text);
            edit->setToolTip(text);
            if (column == 1) edit->setPlaceholderText(binding.key);
        }
    }
    m_table->setFixedHeight(m_table->horizontalHeader()->height() + 2 + std::min(6, int(bindings.size())) * 36);
    m_table->setVisible(!bindings.isEmpty());
    setVisible(!bindings.isEmpty());
}

QList<DesignerFieldBinding> ElementBindingEditor::bindings() const { return m_bindings; }

bool ElementBindingEditor::hasActiveKeyEditor() const
{
    if (m_finishing) return false;
    for (int row = 0; row < m_table->rowCount(); ++row) {
        const auto* editor = m_table->cellWidget(row, 0);
        if (editor != nullptr && editor->hasFocus()) return true;
    }
    return false;
}

} // 命名空间 sleekpr::app

#include "sleekpr/app/TableColumnEditorPanel.h"

#include <QAbstractItemView>
#include <QCheckBox>
#include <QComboBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QMessageBox>
#include <QPushButton>
#include <QSignalBlocker>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

#include <algorithm>

namespace sleekpr::app {

namespace {

enum ColumnIndex
{
    ColumnTitle = 0,
    ColumnFieldKey,
    ColumnWidthMode,
    ColumnWidthMm,
    ColumnFlexWeight,
    ColumnAlignment,
    ColumnFontSize,
    ColumnBold,
    ColumnEllipsis,
    ColumnCount,
};

QTableWidgetItem* editableItem(const QString& text)
{
    auto* item = new QTableWidgetItem(text);
    item->setFlags(item->flags() | Qt::ItemIsEditable);
    return item;
}

QTableWidgetItem* fieldKeyItem(const DesignerTableColumnModel& column)
{
    // 字段名允许编辑，但 columnId 要保持稳定，后续列宽和渲染命令会依赖它定位同一列。
    auto* item = editableItem(column.fieldKey);
    item->setData(Qt::UserRole, column.columnId);
    return item;
}

double itemDouble(const QTableWidgetItem* item, double fallback)
{
    if (item == nullptr) {
        return fallback;
    }
    bool ok = false;
    const auto value = item->text().trimmed().toDouble(&ok);
    return ok ? value : fallback;
}

QComboBox* widthModeCombo(sleekpr::core::TableColumnWidthMode mode)
{
    auto* combo = new QComboBox;
    combo->addItem(QString::fromUtf8("固定"), static_cast<int>(sleekpr::core::TableColumnWidthMode::Fixed));
    combo->addItem(QString::fromUtf8("弹性"), static_cast<int>(sleekpr::core::TableColumnWidthMode::Flex));
    combo->setCurrentIndex(mode == sleekpr::core::TableColumnWidthMode::Flex ? 1 : 0);
    return combo;
}

QComboBox* alignmentCombo(sleekpr::core::TableCellAlignment alignment)
{
    auto* combo = new QComboBox;
    combo->addItem(QString::fromUtf8("左"), static_cast<int>(sleekpr::core::TableCellAlignment::Left));
    combo->addItem(QString::fromUtf8("中"), static_cast<int>(sleekpr::core::TableCellAlignment::Center));
    combo->addItem(QString::fromUtf8("右"), static_cast<int>(sleekpr::core::TableCellAlignment::Right));
    combo->setCurrentIndex(static_cast<int>(alignment));
    return combo;
}

QCheckBox* boolCheck(bool checked)
{
    auto* check = new QCheckBox;
    check->setChecked(checked);
    return check;
}

} // 匿名命名空间

TableColumnEditorPanel::TableColumnEditorPanel(QWidget* parent)
    : QWidget(parent)
{
    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(6);

    auto* buttonLayout = new QHBoxLayout;
    buttonLayout->setContentsMargins(0, 0, 0, 0);
    buttonLayout->setSpacing(6);

    m_addButton = new QPushButton(QString::fromUtf8("新增"), this);
    m_addButton->setObjectName(QStringLiteral("tableColumnAddButton"));
    m_deleteButton = new QPushButton(QString::fromUtf8("删除"), this);
    m_deleteButton->setObjectName(QStringLiteral("tableColumnDeleteButton"));
    m_duplicateButton = new QPushButton(QString::fromUtf8("复制"), this);
    m_duplicateButton->setObjectName(QStringLiteral("tableColumnDuplicateButton"));
    m_moveUpButton = new QPushButton(QString::fromUtf8("上移"), this);
    m_moveUpButton->setObjectName(QStringLiteral("tableColumnMoveUpButton"));
    m_moveDownButton = new QPushButton(QString::fromUtf8("下移"), this);
    m_moveDownButton->setObjectName(QStringLiteral("tableColumnMoveDownButton"));
    m_resetButton = new QPushButton(QString::fromUtf8("重置"), this);
    m_resetButton->setObjectName(QStringLiteral("tableColumnResetButton"));

    buttonLayout->addWidget(m_addButton);
    buttonLayout->addWidget(m_deleteButton);
    buttonLayout->addWidget(m_duplicateButton);
    buttonLayout->addWidget(m_moveUpButton);
    buttonLayout->addWidget(m_moveDownButton);
    buttonLayout->addWidget(m_resetButton);
    rootLayout->addLayout(buttonLayout);

    m_table = new QTableWidget(this);
    m_table->setObjectName(QStringLiteral("tableColumnEditorGrid"));
    m_table->setColumnCount(ColumnCount);
    m_table->setHorizontalHeaderLabels({
        QString::fromUtf8("标题"),
        QString::fromUtf8("字段"),
        QString::fromUtf8("宽度模式"),
        QString::fromUtf8("宽度(mm)"),
        QString::fromUtf8("弹性"),
        QString::fromUtf8("对齐"),
        QString::fromUtf8("字号"),
        QString::fromUtf8("加粗"),
        QString::fromUtf8("省略"),
    });
    m_table->horizontalHeader()->setStretchLastSection(false);
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(ColumnTitle, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(ColumnFieldKey, QHeaderView::Stretch);
    m_table->verticalHeader()->setVisible(false);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::AllEditTriggers);
    rootLayout->addWidget(m_table);

    connect(m_table, &QTableWidget::itemChanged, this, [this] {
        emitEdited();
    });
    connect(m_table, &QTableWidget::itemSelectionChanged, this, [this] {
        updateButtonState();
    });
    connect(m_addButton, &QPushButton::clicked, this, [this] {
        emit columnAddRequested();
        m_columns.append(createDefaultColumn());
        rebuildTable();
        selectColumn(m_columns.size() - 1);
        emitEdited();
    });
    connect(m_deleteButton, &QPushButton::clicked, this, [this] {
        const auto row = currentRow();
        if (row < 0 || row >= m_columns.size() || m_columns.size() <= 1) {
            return;
        }
        emit columnDeleteRequested(row);
        m_columns.removeAt(row);
        rebuildTable();
        selectColumn(std::min(row, static_cast<int>(m_columns.size()) - 1));
        emitEdited();
    });
    connect(m_duplicateButton, &QPushButton::clicked, this, [this] {
        const auto row = currentRow();
        if (row < 0 || row >= m_columns.size()) {
            return;
        }
        auto copy = m_columns[row];
        copy.columnId = uniqueColumnId(copy.columnId + QStringLiteral("_copy"));
        copy.title = copy.title + QString::fromUtf8(" 副本");
        emit columnDuplicateRequested(row);
        m_columns.insert(row + 1, copy);
        rebuildTable();
        selectColumn(row + 1);
        emitEdited();
    });
    connect(m_moveUpButton, &QPushButton::clicked, this, [this] {
        const auto row = currentRow();
        if (row <= 0 || row >= m_columns.size()) {
            return;
        }
        emit columnMoveRequested(row, row - 1);
        m_columns.swapItemsAt(row, row - 1);
        rebuildTable();
        selectColumn(row - 1);
        emitEdited();
    });
    connect(m_moveDownButton, &QPushButton::clicked, this, [this] {
        const auto row = currentRow();
        if (row < 0 || row >= m_columns.size() - 1) {
            return;
        }
        emit columnMoveRequested(row, row + 1);
        m_columns.swapItemsAt(row, row + 1);
        rebuildTable();
        selectColumn(row + 1);
        emitEdited();
    });
    connect(m_resetButton, &QPushButton::clicked, this, [this] {
        if (!confirmResetColumns()) {
            return;
        }
        emit columnsResetRequested();
        m_columns = {createDefaultColumn()};
        rebuildTable();
        selectColumn(0);
        emitEdited();
    });

    rebuildTable();
}

void TableColumnEditorPanel::setColumns(const QList<DesignerTableColumnModel>& columns)
{
    m_columns = columns;
    rebuildTable();
}

QList<DesignerTableColumnModel> TableColumnEditorPanel::columns() const
{
    return m_columns;
}

void TableColumnEditorPanel::setEditable(bool editable)
{
    m_editable = editable;
    m_table->setEditTriggers(editable ? QAbstractItemView::AllEditTriggers : QAbstractItemView::NoEditTriggers);
    rebuildTable();
    updateButtonState();
}

void TableColumnEditorPanel::selectColumn(int row)
{
    if (row < 0 || row >= m_table->rowCount()) {
        m_table->clearSelection();
        updateButtonState();
        return;
    }
    m_table->setCurrentCell(row, 0);
    m_table->selectRow(row);
    updateButtonState();
}

void TableColumnEditorPanel::rebuildTable()
{
    m_updating = true;
    const QSignalBlocker blocker(m_table);
    m_table->setRowCount(0);
    m_table->setRowCount(m_columns.size());
    for (int row = 0; row < m_columns.size(); ++row) {
        writeColumnToRow(row, m_columns[row]);
    }
    m_updating = false;
    updateButtonState();
}

void TableColumnEditorPanel::updateButtonState()
{
    const auto row = currentRow();
    const auto hasSelection = row >= 0 && row < m_table->rowCount();
    m_addButton->setEnabled(m_editable);
    m_deleteButton->setEnabled(m_editable && hasSelection && m_table->rowCount() > 1);
    m_duplicateButton->setEnabled(m_editable && hasSelection);
    m_moveUpButton->setEnabled(m_editable && hasSelection && row > 0);
    m_moveDownButton->setEnabled(m_editable && hasSelection && row < m_table->rowCount() - 1);
    m_resetButton->setEnabled(m_editable);
}

void TableColumnEditorPanel::emitEdited()
{
    if (m_updating || !m_editable) {
        return;
    }

    // 表格内有文本框、下拉框、复选框三类编辑器，统一从界面重建模型可避免状态分叉。
    m_columns.clear();
    for (int row = 0; row < m_table->rowCount(); ++row) {
        m_columns.append(columnFromRow(row));
    }
    updateButtonState();
    emit columnsEdited();
}

int TableColumnEditorPanel::currentRow() const
{
    return m_table->currentRow();
}

DesignerTableColumnModel TableColumnEditorPanel::columnFromRow(int row) const
{
    DesignerTableColumnModel column;
    column.title = m_table->item(row, ColumnTitle) != nullptr ? m_table->item(row, ColumnTitle)->text().trimmed() : QString();
    column.fieldKey = m_table->item(row, ColumnFieldKey) != nullptr ? m_table->item(row, ColumnFieldKey)->text().trimmed() : QString();
    column.columnId = m_table->item(row, ColumnFieldKey) != nullptr
        ? m_table->item(row, ColumnFieldKey)->data(Qt::UserRole).toString().trimmed()
        : QString();
    if (column.columnId.isEmpty()) {
        column.columnId = column.fieldKey;
    }
    column.widthMm = std::max(0.1, itemDouble(m_table->item(row, ColumnWidthMm), 20.0));
    column.flexWeight = std::max(0.1, itemDouble(m_table->item(row, ColumnFlexWeight), 1.0));
    column.fontSizePt = std::max(1.0, itemDouble(m_table->item(row, ColumnFontSize), 8.0));

    if (auto* combo = qobject_cast<QComboBox*>(m_table->cellWidget(row, ColumnWidthMode))) {
        column.widthMode = static_cast<sleekpr::core::TableColumnWidthMode>(combo->currentData().toInt());
    }
    if (auto* combo = qobject_cast<QComboBox*>(m_table->cellWidget(row, ColumnAlignment))) {
        column.alignment = static_cast<sleekpr::core::TableCellAlignment>(combo->currentData().toInt());
    }
    if (auto* check = qobject_cast<QCheckBox*>(m_table->cellWidget(row, ColumnBold))) {
        column.bold = check->isChecked();
    }
    if (auto* check = qobject_cast<QCheckBox*>(m_table->cellWidget(row, ColumnEllipsis))) {
        column.ellipsis = check->isChecked();
    }
    return column;
}

void TableColumnEditorPanel::writeColumnToRow(int row, const DesignerTableColumnModel& column)
{
    m_table->setItem(row, ColumnTitle, editableItem(column.title));
    m_table->setItem(row, ColumnFieldKey, fieldKeyItem(column));
    m_table->setItem(row, ColumnWidthMm, editableItem(QString::number(column.widthMm, 'f', 2)));
    m_table->setItem(row, ColumnFlexWeight, editableItem(QString::number(column.flexWeight, 'f', 2)));
    m_table->setItem(row, ColumnFontSize, editableItem(QString::number(column.fontSizePt, 'f', 2)));

    auto* widthCombo = widthModeCombo(column.widthMode);
    widthCombo->setEnabled(m_editable);
    connect(widthCombo, &QComboBox::currentIndexChanged, this, [this] {
        emitEdited();
    });
    m_table->setCellWidget(row, ColumnWidthMode, widthCombo);

    auto* alignCombo = alignmentCombo(column.alignment);
    alignCombo->setEnabled(m_editable);
    connect(alignCombo, &QComboBox::currentIndexChanged, this, [this] {
        emitEdited();
    });
    m_table->setCellWidget(row, ColumnAlignment, alignCombo);

    auto* boldCheck = boolCheck(column.bold);
    boldCheck->setEnabled(m_editable);
    connect(boldCheck, &QCheckBox::toggled, this, [this] {
        emitEdited();
    });
    m_table->setCellWidget(row, ColumnBold, boldCheck);

    auto* ellipsisCheck = boolCheck(column.ellipsis);
    ellipsisCheck->setEnabled(m_editable);
    connect(ellipsisCheck, &QCheckBox::toggled, this, [this] {
        emitEdited();
    });
    m_table->setCellWidget(row, ColumnEllipsis, ellipsisCheck);
}

DesignerTableColumnModel TableColumnEditorPanel::createDefaultColumn() const
{
    auto nextIndex = 1;
    while (hasColumnId(QStringLiteral("column%1").arg(nextIndex))) {
        ++nextIndex;
    }

    DesignerTableColumnModel column;
    column.columnId = QStringLiteral("column%1").arg(nextIndex);
    column.title = QString::fromUtf8("新列%1").arg(nextIndex);
    column.fieldKey = QStringLiteral("field%1").arg(nextIndex);
    return column;
}

QString TableColumnEditorPanel::uniqueColumnId(const QString& preferredId) const
{
    auto base = preferredId.trimmed();
    if (base.isEmpty()) {
        base = QStringLiteral("column");
    }
    auto candidate = base;
    auto suffix = 2;
    while (hasColumnId(candidate)) {
        candidate = QStringLiteral("%1_%2").arg(base).arg(suffix);
        ++suffix;
    }
    return candidate;
}

bool TableColumnEditorPanel::hasColumnId(const QString& columnId) const
{
    for (const auto& column : m_columns) {
        if (column.columnId == columnId) {
            return true;
        }
    }
    return false;
}

bool TableColumnEditorPanel::confirmResetColumns()
{
    // 重置会覆盖整组列定义，必须让用户显式确认，避免误点破坏复杂表格配置。
    QMessageBox messageBox(QMessageBox::Question,
        QString::fromUtf8("重置列配置"),
        QString::fromUtf8("确定要恢复默认列配置吗？当前列设置会被覆盖。"),
        QMessageBox::Yes | QMessageBox::No,
        this);
    messageBox.setDefaultButton(QMessageBox::No);
    messageBox.setButtonText(QMessageBox::Yes, QString::fromUtf8("重置"));
    messageBox.setButtonText(QMessageBox::No, QString::fromUtf8("取消"));
    return messageBox.exec() == QMessageBox::Yes;
}

} // namespace sleekpr::app

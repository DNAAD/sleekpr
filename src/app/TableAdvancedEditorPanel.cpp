#include "sleekpr/app/TableAdvancedEditorPanel.h"

#include <QAbstractItemView>
#include <QCheckBox>
#include <QComboBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QTabWidget>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

#include <algorithm>

namespace sleekpr::app {

namespace {

enum RowBandColumn
{
    RowBandId = 0,
    RowBandKind,
    RowBandTitle,
    RowBandDataPath,
    RowBandHeightMode,
    RowBandHeight,
    RowBandMinHeight,
    RowBandRepeat,
    RowBandPrintOn,
    RowBandColumnCount,
};

enum CellStyleColumn
{
    CellStyleId = 0,
    CellStyleFontSize,
    CellStyleBold,
    CellStyleAlignment,
    CellStyleVerticalAlignment,
    CellStylePaddingLeft,
    CellStylePaddingTop,
    CellStylePaddingRight,
    CellStylePaddingBottom,
    CellStyleDrawBorder,
    CellStyleBorderWidth,
    CellStyleBackground,
    CellStyleTextColor,
    CellStyleWrap,
    CellStyleEllipsis,
    CellStyleColumnCount,
};

enum CellTemplateColumn
{
    CellTemplateId = 0,
    CellTemplateRowBand,
    CellTemplateColumnId,
    CellTemplateText,
    CellTemplateFieldKey,
    CellTemplateStyle,
    CellTemplateOverflow,
    CellTemplateMaxLines,
    CellTemplateColSpan,
    CellTemplateRowSpan,
    CellTemplateVisible,
    CellTemplateColumnCount,
};

enum MergeRegionColumn
{
    MergeRegionId = 0,
    MergeRegionRowBand,
    MergeRegionStartOffset,
    MergeRegionStartColumn,
    MergeRegionRowSpan,
    MergeRegionColSpan,
    MergeRegionColumnCount,
};

QTableWidgetItem* editableItem(const QString& text)
{
    auto* item = new QTableWidgetItem(text);
    item->setFlags(item->flags() | Qt::ItemIsEditable);
    return item;
}

QString itemText(const QTableWidget* table, int row, int column)
{
    const auto* item = table != nullptr ? table->item(row, column) : nullptr;
    return item != nullptr ? item->text().trimmed() : QString();
}

double itemDouble(const QTableWidget* table, int row, int column, double fallback)
{
    bool ok = false;
    const auto value = itemText(table, row, column).toDouble(&ok);
    return ok ? value : fallback;
}

int itemInt(const QTableWidget* table, int row, int column, int fallback)
{
    bool ok = false;
    const auto value = itemText(table, row, column).toInt(&ok);
    return ok ? value : fallback;
}

QComboBox* enumCombo(const QList<QPair<QString, int>>& options, int currentValue)
{
    auto* combo = new QComboBox;
    for (const auto& option : options) {
        combo->addItem(option.first, option.second);
    }
    const auto index = combo->findData(currentValue);
    combo->setCurrentIndex(index >= 0 ? index : 0);
    return combo;
}

QCheckBox* boolCheck(bool checked)
{
    auto* check = new QCheckBox;
    check->setChecked(checked);
    return check;
}

int comboValue(const QTableWidget* table, int row, int column, int fallback)
{
    auto* combo = table != nullptr ? qobject_cast<QComboBox*>(table->cellWidget(row, column)) : nullptr;
    return combo != nullptr ? combo->currentData().toInt() : fallback;
}

bool checkValue(const QTableWidget* table, int row, int column, bool fallback)
{
    auto* check = table != nullptr ? qobject_cast<QCheckBox*>(table->cellWidget(row, column)) : nullptr;
    return check != nullptr ? check->isChecked() : fallback;
}

void configureGrid(QTableWidget* table)
{
    table->horizontalHeader()->setStretchLastSection(false);
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    table->verticalHeader()->setVisible(false);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    table->setEditTriggers(QAbstractItemView::AllEditTriggers);
    table->setMinimumHeight(180);
}

QComboBox* rowBandKindCombo(sleekpr::core::TableRowBandKind kind)
{
    return enumCombo({
                         {QString::fromUtf8("表头"), static_cast<int>(sleekpr::core::TableRowBandKind::Header)},
                         {QString::fromUtf8("明细"), static_cast<int>(sleekpr::core::TableRowBandKind::Detail)},
                         {QString::fromUtf8("小计"), static_cast<int>(sleekpr::core::TableRowBandKind::Subtotal)},
                         {QString::fromUtf8("汇总"), static_cast<int>(sleekpr::core::TableRowBandKind::Summary)},
                         {QString::fromUtf8("分组头"), static_cast<int>(sleekpr::core::TableRowBandKind::GroupHeader)},
                         {QString::fromUtf8("分组脚"), static_cast<int>(sleekpr::core::TableRowBandKind::GroupFooter)},
                         {QString::fromUtf8("页脚"), static_cast<int>(sleekpr::core::TableRowBandKind::Footer)},
                     },
        static_cast<int>(kind));
}

QComboBox* heightModeCombo(sleekpr::core::TableRowHeightMode mode)
{
    return enumCombo({
                         {QString::fromUtf8("固定"), static_cast<int>(sleekpr::core::TableRowHeightMode::Fixed)},
                         {QString::fromUtf8("自动"), static_cast<int>(sleekpr::core::TableRowHeightMode::Auto)},
                     },
        static_cast<int>(mode));
}

QComboBox* printScopeCombo(sleekpr::core::TableRowPrintScope scope)
{
    return enumCombo({
                         {QString::fromUtf8("首页"), static_cast<int>(sleekpr::core::TableRowPrintScope::FirstPage)},
                         {QString::fromUtf8("每页"), static_cast<int>(sleekpr::core::TableRowPrintScope::EveryPage)},
                         {QString::fromUtf8("末页"), static_cast<int>(sleekpr::core::TableRowPrintScope::LastPage)},
                     },
        static_cast<int>(scope));
}

QComboBox* alignmentCombo(sleekpr::core::TableCellAlignment alignment)
{
    return enumCombo({
                         {QString::fromUtf8("左"), static_cast<int>(sleekpr::core::TableCellAlignment::Left)},
                         {QString::fromUtf8("中"), static_cast<int>(sleekpr::core::TableCellAlignment::Center)},
                         {QString::fromUtf8("右"), static_cast<int>(sleekpr::core::TableCellAlignment::Right)},
                     },
        static_cast<int>(alignment));
}

QComboBox* verticalAlignmentCombo(sleekpr::core::TableVerticalAlignment alignment)
{
    return enumCombo({
                         {QString::fromUtf8("上"), static_cast<int>(sleekpr::core::TableVerticalAlignment::Top)},
                         {QString::fromUtf8("中"), static_cast<int>(sleekpr::core::TableVerticalAlignment::Middle)},
                         {QString::fromUtf8("下"), static_cast<int>(sleekpr::core::TableVerticalAlignment::Bottom)},
                     },
        static_cast<int>(alignment));
}

QComboBox* overflowCombo(sleekpr::core::TableCellOverflowPolicy policy)
{
    return enumCombo({
                         {QString::fromUtf8("裁剪"), static_cast<int>(sleekpr::core::TableCellOverflowPolicy::Clip)},
                         {QString::fromUtf8("省略"), static_cast<int>(sleekpr::core::TableCellOverflowPolicy::Ellipsis)},
                         {QString::fromUtf8("换行"), static_cast<int>(sleekpr::core::TableCellOverflowPolicy::Wrap)},
                         {QString::fromUtf8("缩小"), static_cast<int>(sleekpr::core::TableCellOverflowPolicy::Shrink)},
                     },
        static_cast<int>(policy));
}

} // 匿名命名空间

QComboBox* tableOverflowCombo(sleekpr::core::TableTableOverflowPolicy policy)
{
    return enumCombo({
                          {QString::fromUtf8("报错"), static_cast<int>(sleekpr::core::TableTableOverflowPolicy::Error)},
                          {QString::fromUtf8("裁剪"), static_cast<int>(sleekpr::core::TableTableOverflowPolicy::Clip)},
                          {QString::fromUtf8("继续分页"), static_cast<int>(sleekpr::core::TableTableOverflowPolicy::Continue)},
                      },
        static_cast<int>(policy));
}

TableAdvancedEditorPanel::TableAdvancedEditorPanel(QWidget* parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("tableAdvancedEditorPanel"));
    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(6);

    auto* title = new QLabel(QString::fromUtf8("复杂表格"), this);
    title->setObjectName(QStringLiteral("tableAdvancedEditorTitle"));
    rootLayout->addWidget(title);

    m_tabs = new QTabWidget(this);
    m_tabs->setObjectName(QStringLiteral("tableAdvancedEditorTabs"));
    rootLayout->addWidget(m_tabs);

    auto* rowBandPage = new QWidget(m_tabs);
    auto* rowBandLayout = new QVBoxLayout(rowBandPage);
    rowBandLayout->setContentsMargins(0, 0, 0, 0);
    auto* rowBandButtons = new QHBoxLayout;
    m_addRowBandButton = new QPushButton(QString::fromUtf8("新增"), rowBandPage);
    m_addRowBandButton->setObjectName(QStringLiteral("tableRowBandAddButton"));
    m_deleteRowBandButton = new QPushButton(QString::fromUtf8("删除"), rowBandPage);
    m_deleteRowBandButton->setObjectName(QStringLiteral("tableRowBandDeleteButton"));
    m_moveRowBandUpButton = new QPushButton(QString::fromUtf8("上移"), rowBandPage);
    m_moveRowBandUpButton->setObjectName(QStringLiteral("tableRowBandMoveUpButton"));
    m_moveRowBandDownButton = new QPushButton(QString::fromUtf8("下移"), rowBandPage);
    m_moveRowBandDownButton->setObjectName(QStringLiteral("tableRowBandMoveDownButton"));
    rowBandButtons->addWidget(m_addRowBandButton);
    rowBandButtons->addWidget(m_deleteRowBandButton);
    rowBandButtons->addWidget(m_moveRowBandUpButton);
    rowBandButtons->addWidget(m_moveRowBandDownButton);
    m_rowBandTable = new QTableWidget(rowBandPage);
    m_rowBandTable->setObjectName(QStringLiteral("tableRowBandEditorGrid"));
    m_rowBandTable->setColumnCount(RowBandColumnCount);
    m_rowBandTable->setHorizontalHeaderLabels({
        QStringLiteral("ID"),
        QString::fromUtf8("类型"),
        QString::fromUtf8("名称"),
        QStringLiteral("dataPath"),
        QString::fromUtf8("高度模式"),
        QString::fromUtf8("高度"),
        QString::fromUtf8("最小高"),
        QString::fromUtf8("重复"),
        QString::fromUtf8("打印"),
    });
    configureGrid(m_rowBandTable);
    rowBandLayout->addLayout(rowBandButtons);
    rowBandLayout->addWidget(m_rowBandTable);
    m_tabs->addTab(rowBandPage, QString::fromUtf8("行带"));

    auto* cellPage = new QWidget(m_tabs);
    auto* cellLayout = new QVBoxLayout(cellPage);
    cellLayout->setContentsMargins(0, 0, 0, 0);
    auto* cellButtons = new QHBoxLayout;
    m_addCellTemplateButton = new QPushButton(QString::fromUtf8("新增"), cellPage);
    m_addCellTemplateButton->setObjectName(QStringLiteral("tableCellTemplateAddButton"));
    m_deleteCellTemplateButton = new QPushButton(QString::fromUtf8("删除"), cellPage);
    m_deleteCellTemplateButton->setObjectName(QStringLiteral("tableCellTemplateDeleteButton"));
    cellButtons->addWidget(m_addCellTemplateButton);
    cellButtons->addWidget(m_deleteCellTemplateButton);
    m_cellTemplateTable = new QTableWidget(cellPage);
    m_cellTemplateTable->setObjectName(QStringLiteral("tableCellTemplateEditorGrid"));
    m_cellTemplateTable->setColumnCount(CellTemplateColumnCount);
    m_cellTemplateTable->setHorizontalHeaderLabels({
        QStringLiteral("ID"),
        QString::fromUtf8("行带"),
        QString::fromUtf8("列"),
        QString::fromUtf8("模板"),
        QString::fromUtf8("字段"),
        QString::fromUtf8("样式"),
        QString::fromUtf8("溢出"),
        QString::fromUtf8("行数"),
        QString::fromUtf8("跨列"),
        QString::fromUtf8("跨行"),
        QString::fromUtf8("显示"),
    });
    configureGrid(m_cellTemplateTable);
    m_cellTemplateTable->horizontalHeader()->setSectionResizeMode(CellTemplateText, QHeaderView::Stretch);
    cellLayout->addLayout(cellButtons);
    cellLayout->addWidget(m_cellTemplateTable);
    m_tabs->addTab(cellPage, QString::fromUtf8("单元格"));

    auto* stylePage = new QWidget(m_tabs);
    auto* styleLayout = new QVBoxLayout(stylePage);
    styleLayout->setContentsMargins(0, 0, 0, 0);
    auto* styleButtons = new QHBoxLayout;
    m_addStyleButton = new QPushButton(QString::fromUtf8("新增"), stylePage);
    m_addStyleButton->setObjectName(QStringLiteral("tableCellStyleAddButton"));
    m_deleteStyleButton = new QPushButton(QString::fromUtf8("删除"), stylePage);
    m_deleteStyleButton->setObjectName(QStringLiteral("tableCellStyleDeleteButton"));
    styleButtons->addWidget(m_addStyleButton);
    styleButtons->addWidget(m_deleteStyleButton);
    m_cellStyleTable = new QTableWidget(stylePage);
    m_cellStyleTable->setObjectName(QStringLiteral("tableCellStyleEditorGrid"));
    m_cellStyleTable->setColumnCount(CellStyleColumnCount);
    m_cellStyleTable->setHorizontalHeaderLabels({
        QStringLiteral("ID"),
        QString::fromUtf8("字号"),
        QString::fromUtf8("加粗"),
        QString::fromUtf8("水平"),
        QString::fromUtf8("垂直"),
        QString::fromUtf8("左距"),
        QString::fromUtf8("上距"),
        QString::fromUtf8("右距"),
        QString::fromUtf8("下距"),
        QString::fromUtf8("边框"),
        QString::fromUtf8("线宽"),
        QString::fromUtf8("背景"),
        QString::fromUtf8("文字"),
        QString::fromUtf8("换行"),
        QString::fromUtf8("省略"),
    });
    configureGrid(m_cellStyleTable);
    styleLayout->addLayout(styleButtons);
    styleLayout->addWidget(m_cellStyleTable);
    m_tabs->addTab(stylePage, QString::fromUtf8("样式"));

    auto* mergePage = new QWidget(m_tabs);
    auto* mergeLayout = new QVBoxLayout(mergePage);
    mergeLayout->setContentsMargins(0, 0, 0, 0);
    auto* mergeButtons = new QHBoxLayout;
    m_mergeCellButton = new QPushButton(QString::fromUtf8("合并单元格"), mergePage);
    m_mergeCellButton->setObjectName(QStringLiteral("tableMergeCellButton"));
    m_splitCellButton = new QPushButton(QString::fromUtf8("拆分单元格"), mergePage);
    m_splitCellButton->setObjectName(QStringLiteral("tableSplitCellButton"));
    mergeButtons->addWidget(m_mergeCellButton);
    mergeButtons->addWidget(m_splitCellButton);
    m_mergeRegionTable = new QTableWidget(mergePage);
    m_mergeRegionTable->setObjectName(QStringLiteral("tableMergeRegionEditorGrid"));
    m_mergeRegionTable->setColumnCount(MergeRegionColumnCount);
    m_mergeRegionTable->setHorizontalHeaderLabels({
        QStringLiteral("ID"),
        QString::fromUtf8("行带"),
        QString::fromUtf8("起始行"),
        QString::fromUtf8("起始列"),
        QString::fromUtf8("跨行"),
        QString::fromUtf8("跨列"),
    });
    configureGrid(m_mergeRegionTable);
    mergeLayout->addLayout(mergeButtons);
    mergeLayout->addWidget(m_mergeRegionTable);

    auto* paginationPage = new QWidget(m_tabs);
    auto* paginationLayout = new QVBoxLayout(paginationPage);
    paginationLayout->setContentsMargins(0, 0, 0, 0);
    paginationLayout->setSpacing(6);
    auto* paginationGrid = new QGridLayout;
    paginationGrid->setContentsMargins(0, 0, 0, 0);
    paginationGrid->setHorizontalSpacing(8);
    paginationGrid->setVerticalSpacing(6);

    m_paginationRepeatHeaderCheck = new QCheckBox(QString::fromUtf8("重复表头"), paginationPage);
    m_paginationRepeatHeaderCheck->setObjectName(QStringLiteral("tablePaginationRepeatHeaderCheck"));
    m_paginationKeepGroupTogetherCheck = new QCheckBox(QString::fromUtf8("分组不拆分"), paginationPage);
    m_paginationKeepGroupTogetherCheck->setObjectName(QStringLiteral("tablePaginationKeepGroupTogetherCheck"));
    m_paginationAllowRowSplitCheck = new QCheckBox(QString::fromUtf8("允许拆行"), paginationPage);
    m_paginationAllowRowSplitCheck->setObjectName(QStringLiteral("tablePaginationAllowRowSplitCheck"));
    m_paginationMaxPagesSpin = new QSpinBox(paginationPage);
    m_paginationMaxPagesSpin->setObjectName(QStringLiteral("tablePaginationMaxPagesSpin"));
    m_paginationMaxPagesSpin->setRange(1, 10000);
    m_paginationOrphanRowsSpin = new QSpinBox(paginationPage);
    m_paginationOrphanRowsSpin->setObjectName(QStringLiteral("tablePaginationOrphanRowsSpin"));
    m_paginationOrphanRowsSpin->setRange(0, 1000);
    m_paginationGroupKeyEdit = new QLineEdit(paginationPage);
    m_paginationGroupKeyEdit->setObjectName(QStringLiteral("tablePaginationGroupKeyEdit"));
    m_paginationGroupKeyEdit->setPlaceholderText(QStringLiteral("group"));
    m_paginationOverflowCombo = tableOverflowCombo(sleekpr::core::TableTableOverflowPolicy::Error);
    m_paginationOverflowCombo->setObjectName(QStringLiteral("tablePaginationOverflowCombo"));

    paginationGrid->addWidget(m_paginationRepeatHeaderCheck, 0, 0);
    paginationGrid->addWidget(m_paginationKeepGroupTogetherCheck, 0, 1);
    paginationGrid->addWidget(m_paginationAllowRowSplitCheck, 0, 2);
    paginationGrid->addWidget(new QLabel(QString::fromUtf8("最大页数"), paginationPage), 1, 0);
    paginationGrid->addWidget(m_paginationMaxPagesSpin, 1, 1);
    paginationGrid->addWidget(new QLabel(QString::fromUtf8("孤行保护"), paginationPage), 2, 0);
    paginationGrid->addWidget(m_paginationOrphanRowsSpin, 2, 1);
    paginationGrid->addWidget(new QLabel(QString::fromUtf8("分组字段"), paginationPage), 3, 0);
    paginationGrid->addWidget(m_paginationGroupKeyEdit, 3, 1, 1, 2);
    paginationGrid->addWidget(new QLabel(QString::fromUtf8("溢出策略"), paginationPage), 4, 0);
    paginationGrid->addWidget(m_paginationOverflowCombo, 4, 1);

    m_paginationPreviewTable = new QTableWidget(paginationPage);
    m_paginationPreviewTable->setObjectName(QStringLiteral("tablePaginationPreviewGrid"));
    m_paginationPreviewTable->setColumnCount(4);
    m_paginationPreviewTable->setHorizontalHeaderLabels({
        QString::fromUtf8("页码"),
        QString::fromUtf8("起始行"),
        QString::fromUtf8("行数"),
        QString::fromUtf8("说明"),
    });
    configureGrid(m_paginationPreviewTable);
    m_paginationPreviewTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_paginationPreviewTable->setSelectionMode(QAbstractItemView::NoSelection);
    m_paginationPreviewTable->setMinimumHeight(120);
    m_paginationPreviewTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);

    paginationLayout->addLayout(paginationGrid);
    paginationLayout->addWidget(m_paginationPreviewTable);
    m_tabs->addTab(mergePage, QString::fromUtf8("合并"));

    m_tabs->addTab(paginationPage, QString::fromUtf8("分页"));

    connect(m_rowBandTable, &QTableWidget::itemChanged, this, [this] { emitEdited(); });
    connect(m_cellStyleTable, &QTableWidget::itemChanged, this, [this] { emitEdited(); });
    connect(m_cellTemplateTable, &QTableWidget::itemChanged, this, [this] { emitEdited(); });
    connect(m_mergeRegionTable, &QTableWidget::itemChanged, this, [this] { emitEdited(); });
    connect(m_rowBandTable, &QTableWidget::itemSelectionChanged, this, [this] { updateButtonState(); });
    connect(m_cellStyleTable, &QTableWidget::itemSelectionChanged, this, [this] { updateButtonState(); });
    connect(m_cellTemplateTable, &QTableWidget::itemSelectionChanged, this, [this] { updateButtonState(); });
    connect(m_mergeRegionTable, &QTableWidget::itemSelectionChanged, this, [this] { updateButtonState(); });
    connect(m_paginationRepeatHeaderCheck, &QCheckBox::toggled, this, [this] { emitEdited(); });
    connect(m_paginationKeepGroupTogetherCheck, &QCheckBox::toggled, this, [this] { emitEdited(); });
    connect(m_paginationAllowRowSplitCheck, &QCheckBox::toggled, this, [this] { emitEdited(); });
    connect(m_paginationMaxPagesSpin, &QSpinBox::valueChanged, this, [this] { emitEdited(); });
    connect(m_paginationOrphanRowsSpin, &QSpinBox::valueChanged, this, [this] { emitEdited(); });
    connect(m_paginationGroupKeyEdit, &QLineEdit::textChanged, this, [this] { emitEdited(); });
    connect(m_paginationOverflowCombo, &QComboBox::currentIndexChanged, this, [this] { emitEdited(); });

    connect(m_addRowBandButton, &QPushButton::clicked, this, [this] {
        m_model = tableProperties();
        m_model.rowBands.append(createDefaultRowBand());
        rebuildTables();
        m_rowBandTable->selectRow(m_rowBandTable->rowCount() - 1);
        emitEdited();
    });
    connect(m_deleteRowBandButton, &QPushButton::clicked, this, [this] {
        const auto row = m_rowBandTable->currentRow();
        m_model = tableProperties();
        if (row < 0 || row >= m_model.rowBands.size()) {
            return;
        }
        m_model.rowBands.removeAt(row);
        rebuildTables();
        emitEdited();
    });
    connect(m_moveRowBandUpButton, &QPushButton::clicked, this, [this] {
        const auto row = m_rowBandTable->currentRow();
        m_model = tableProperties();
        if (row <= 0 || row >= m_model.rowBands.size()) {
            return;
        }
        m_model.rowBands.swapItemsAt(row, row - 1);
        rebuildTables();
        m_rowBandTable->selectRow(row - 1);
        emitEdited();
    });
    connect(m_moveRowBandDownButton, &QPushButton::clicked, this, [this] {
        const auto row = m_rowBandTable->currentRow();
        m_model = tableProperties();
        if (row < 0 || row >= m_model.rowBands.size() - 1) {
            return;
        }
        m_model.rowBands.swapItemsAt(row, row + 1);
        rebuildTables();
        m_rowBandTable->selectRow(row + 1);
        emitEdited();
    });
    connect(m_addStyleButton, &QPushButton::clicked, this, [this] {
        m_model = tableProperties();
        m_model.cellStyles.append(createDefaultCellStyle());
        rebuildTables();
        m_cellStyleTable->selectRow(m_cellStyleTable->rowCount() - 1);
        emitEdited();
    });
    connect(m_deleteStyleButton, &QPushButton::clicked, this, [this] {
        const auto row = m_cellStyleTable->currentRow();
        m_model = tableProperties();
        if (row < 0 || row >= m_model.cellStyles.size()) {
            return;
        }
        m_model.cellStyles.removeAt(row);
        rebuildTables();
        emitEdited();
    });
    connect(m_addCellTemplateButton, &QPushButton::clicked, this, [this] {
        m_model = tableProperties();
        m_model.cellTemplates.append(createDefaultCellTemplate());
        rebuildTables();
        m_cellTemplateTable->selectRow(m_cellTemplateTable->rowCount() - 1);
        emitEdited();
    });
    connect(m_deleteCellTemplateButton, &QPushButton::clicked, this, [this] {
        const auto row = m_cellTemplateTable->currentRow();
        m_model = tableProperties();
        if (row < 0 || row >= m_model.cellTemplates.size()) {
            return;
        }
        m_model.cellTemplates.removeAt(row);
        rebuildTables();
        emitEdited();
    });
    connect(m_mergeCellButton, &QPushButton::clicked, this, [this] {
        m_model = tableProperties();
        m_model.mergeRegions.append(createDefaultMergeRegion());
        rebuildTables();
        m_mergeRegionTable->selectRow(m_mergeRegionTable->rowCount() - 1);
        emitEdited();
    });
    connect(m_splitCellButton, &QPushButton::clicked, this, [this] {
        const auto row = m_mergeRegionTable->currentRow();
        m_model = tableProperties();
        if (row < 0 || row >= m_model.mergeRegions.size()) {
            return;
        }
        m_model.mergeRegions.removeAt(row);
        rebuildTables();
        emitEdited();
    });

    rebuildTables();
}

void TableAdvancedEditorPanel::setProperties(const DesignerTablePropertyModel& model)
{
    m_model = model;
    rebuildTables();
}

DesignerTablePropertyModel TableAdvancedEditorPanel::tableProperties() const
{
    auto model = m_model;
    model.rowBands = rowBandsFromTable();
    model.cellStyles = cellStylesFromTable();
    model.cellTemplates = cellTemplatesFromTable();
    model.mergeRegions = mergeRegionsFromTable();
    applyPaginationControls(&model);
    return model;
}

void TableAdvancedEditorPanel::setEditable(bool editable)
{
    m_editable = editable;
    for (auto* table : {m_rowBandTable, m_cellStyleTable, m_cellTemplateTable, m_mergeRegionTable}) {
        if (table != nullptr) {
            table->setEditTriggers(editable ? QAbstractItemView::AllEditTriggers : QAbstractItemView::NoEditTriggers);
        }
    }
    for (auto* control : {m_paginationRepeatHeaderCheck, m_paginationKeepGroupTogetherCheck, m_paginationAllowRowSplitCheck}) {
        if (control != nullptr) {
            control->setEnabled(editable);
        }
    }
    if (m_paginationMaxPagesSpin != nullptr) {
        m_paginationMaxPagesSpin->setEnabled(editable);
    }
    if (m_paginationOrphanRowsSpin != nullptr) {
        m_paginationOrphanRowsSpin->setEnabled(editable);
    }
    if (m_paginationGroupKeyEdit != nullptr) {
        m_paginationGroupKeyEdit->setEnabled(editable);
    }
    if (m_paginationOverflowCombo != nullptr) {
        m_paginationOverflowCombo->setEnabled(editable);
    }
    rebuildTables();
}

void TableAdvancedEditorPanel::rebuildTables()
{
    m_updating = true;
    rebuildRowBandTable();
    rebuildCellStyleTable();
    rebuildCellTemplateTable();
    rebuildMergeRegionTable();
    rebuildPaginationControls();
    m_updating = false;
    updateButtonState();
}

void TableAdvancedEditorPanel::rebuildRowBandTable()
{
    const QSignalBlocker blocker(m_rowBandTable);
    m_rowBandTable->setRowCount(0);
    m_rowBandTable->setRowCount(m_model.rowBands.size());
    for (int row = 0; row < m_model.rowBands.size(); ++row) {
        const auto& band = m_model.rowBands[row];
        m_rowBandTable->setItem(row, RowBandId, editableItem(band.rowBandId));
        auto* kind = rowBandKindCombo(band.kind);
        kind->setEnabled(m_editable);
        connect(kind, &QComboBox::currentIndexChanged, this, [this] { emitEdited(); });
        m_rowBandTable->setCellWidget(row, RowBandKind, kind);
        m_rowBandTable->setItem(row, RowBandTitle, editableItem(band.title));
        m_rowBandTable->setItem(row, RowBandDataPath, editableItem(band.dataPath));
        auto* heightMode = heightModeCombo(band.heightMode);
        heightMode->setEnabled(m_editable);
        connect(heightMode, &QComboBox::currentIndexChanged, this, [this] { emitEdited(); });
        m_rowBandTable->setCellWidget(row, RowBandHeightMode, heightMode);
        m_rowBandTable->setItem(row, RowBandHeight, editableItem(QString::number(band.heightMm, 'f', 2)));
        m_rowBandTable->setItem(row, RowBandMinHeight, editableItem(QString::number(band.minHeightMm, 'f', 2)));
        auto* repeat = boolCheck(band.repeatOnPage);
        repeat->setEnabled(m_editable);
        connect(repeat, &QCheckBox::toggled, this, [this] { emitEdited(); });
        m_rowBandTable->setCellWidget(row, RowBandRepeat, repeat);
        auto* printOn = printScopeCombo(band.printOn);
        printOn->setEnabled(m_editable);
        connect(printOn, &QComboBox::currentIndexChanged, this, [this] { emitEdited(); });
        m_rowBandTable->setCellWidget(row, RowBandPrintOn, printOn);
    }
}

void TableAdvancedEditorPanel::rebuildCellStyleTable()
{
    const QSignalBlocker blocker(m_cellStyleTable);
    m_cellStyleTable->setRowCount(0);
    m_cellStyleTable->setRowCount(m_model.cellStyles.size());
    for (int row = 0; row < m_model.cellStyles.size(); ++row) {
        const auto& style = m_model.cellStyles[row];
        m_cellStyleTable->setItem(row, CellStyleId, editableItem(style.styleId));
        m_cellStyleTable->setItem(row, CellStyleFontSize, editableItem(QString::number(style.fontSizePt, 'f', 2)));
        auto* bold = boolCheck(style.bold);
        bold->setEnabled(m_editable);
        connect(bold, &QCheckBox::toggled, this, [this] { emitEdited(); });
        m_cellStyleTable->setCellWidget(row, CellStyleBold, bold);
        auto* alignment = alignmentCombo(style.alignment);
        alignment->setEnabled(m_editable);
        connect(alignment, &QComboBox::currentIndexChanged, this, [this] { emitEdited(); });
        m_cellStyleTable->setCellWidget(row, CellStyleAlignment, alignment);
        auto* verticalAlignment = verticalAlignmentCombo(style.verticalAlignment);
        verticalAlignment->setEnabled(m_editable);
        connect(verticalAlignment, &QComboBox::currentIndexChanged, this, [this] { emitEdited(); });
        m_cellStyleTable->setCellWidget(row, CellStyleVerticalAlignment, verticalAlignment);
        m_cellStyleTable->setItem(row, CellStylePaddingLeft, editableItem(QString::number(style.paddingLeftMm, 'f', 2)));
        m_cellStyleTable->setItem(row, CellStylePaddingTop, editableItem(QString::number(style.paddingTopMm, 'f', 2)));
        m_cellStyleTable->setItem(row, CellStylePaddingRight, editableItem(QString::number(style.paddingRightMm, 'f', 2)));
        m_cellStyleTable->setItem(row, CellStylePaddingBottom, editableItem(QString::number(style.paddingBottomMm, 'f', 2)));
        auto* drawBorder = boolCheck(style.drawBorder);
        drawBorder->setEnabled(m_editable);
        connect(drawBorder, &QCheckBox::toggled, this, [this] { emitEdited(); });
        m_cellStyleTable->setCellWidget(row, CellStyleDrawBorder, drawBorder);
        m_cellStyleTable->setItem(row, CellStyleBorderWidth, editableItem(QString::number(style.borderWidthMm, 'f', 2)));
        m_cellStyleTable->setItem(row, CellStyleBackground, editableItem(style.backgroundColor));
        m_cellStyleTable->setItem(row, CellStyleTextColor, editableItem(style.textColor));
        auto* wrap = boolCheck(style.wrapText);
        wrap->setEnabled(m_editable);
        connect(wrap, &QCheckBox::toggled, this, [this] { emitEdited(); });
        m_cellStyleTable->setCellWidget(row, CellStyleWrap, wrap);
        auto* ellipsis = boolCheck(style.ellipsis);
        ellipsis->setEnabled(m_editable);
        connect(ellipsis, &QCheckBox::toggled, this, [this] { emitEdited(); });
        m_cellStyleTable->setCellWidget(row, CellStyleEllipsis, ellipsis);
    }
}

void TableAdvancedEditorPanel::rebuildCellTemplateTable()
{
    const QSignalBlocker blocker(m_cellTemplateTable);
    m_cellTemplateTable->setRowCount(0);
    m_cellTemplateTable->setRowCount(m_model.cellTemplates.size());
    for (int row = 0; row < m_model.cellTemplates.size(); ++row) {
        const auto& cell = m_model.cellTemplates[row];
        m_cellTemplateTable->setItem(row, CellTemplateId, editableItem(cell.templateId));
        m_cellTemplateTable->setItem(row, CellTemplateRowBand, editableItem(cell.rowBandId));
        m_cellTemplateTable->setItem(row, CellTemplateColumnId, editableItem(cell.columnId));
        m_cellTemplateTable->setItem(row, CellTemplateText, editableItem(cell.textTemplate));
        m_cellTemplateTable->setItem(row, CellTemplateFieldKey, editableItem(cell.fieldKey));
        m_cellTemplateTable->setItem(row, CellTemplateStyle, editableItem(cell.styleId));
        auto* overflow = overflowCombo(cell.overflowPolicy);
        overflow->setEnabled(m_editable);
        connect(overflow, &QComboBox::currentIndexChanged, this, [this] { emitEdited(); });
        m_cellTemplateTable->setCellWidget(row, CellTemplateOverflow, overflow);
        m_cellTemplateTable->setItem(row, CellTemplateMaxLines, editableItem(QString::number(cell.maxLines)));
        m_cellTemplateTable->setItem(row, CellTemplateColSpan, editableItem(QString::number(cell.colSpan)));
        m_cellTemplateTable->setItem(row, CellTemplateRowSpan, editableItem(QString::number(cell.rowSpan)));
        auto* visible = boolCheck(cell.visible);
        visible->setEnabled(m_editable);
        connect(visible, &QCheckBox::toggled, this, [this] { emitEdited(); });
        m_cellTemplateTable->setCellWidget(row, CellTemplateVisible, visible);
    }
}

void TableAdvancedEditorPanel::rebuildMergeRegionTable()
{
    const QSignalBlocker blocker(m_mergeRegionTable);
    m_mergeRegionTable->setRowCount(0);
    m_mergeRegionTable->setRowCount(m_model.mergeRegions.size());
    for (int row = 0; row < m_model.mergeRegions.size(); ++row) {
        const auto& merge = m_model.mergeRegions[row];
        m_mergeRegionTable->setItem(row, MergeRegionId, editableItem(merge.mergeId));
        m_mergeRegionTable->setItem(row, MergeRegionRowBand, editableItem(merge.rowBandId));
        m_mergeRegionTable->setItem(row, MergeRegionStartOffset, editableItem(QString::number(merge.startRowOffset)));
        m_mergeRegionTable->setItem(row, MergeRegionStartColumn, editableItem(merge.startColumnId));
        m_mergeRegionTable->setItem(row, MergeRegionRowSpan, editableItem(QString::number(merge.rowSpan)));
        m_mergeRegionTable->setItem(row, MergeRegionColSpan, editableItem(QString::number(merge.colSpan)));
    }
}

void TableAdvancedEditorPanel::rebuildPaginationControls()
{
    const QSignalBlocker repeatBlocker(m_paginationRepeatHeaderCheck);
    const QSignalBlocker keepGroupBlocker(m_paginationKeepGroupTogetherCheck);
    const QSignalBlocker allowRowSplitBlocker(m_paginationAllowRowSplitCheck);
    const QSignalBlocker maxPagesBlocker(m_paginationMaxPagesSpin);
    const QSignalBlocker orphanRowsBlocker(m_paginationOrphanRowsSpin);
    const QSignalBlocker groupKeyBlocker(m_paginationGroupKeyEdit);
    const QSignalBlocker overflowBlocker(m_paginationOverflowCombo);
    const QSignalBlocker previewBlocker(m_paginationPreviewTable);

    m_paginationRepeatHeaderCheck->setChecked(m_model.repeatHeaderOnPage);
    m_paginationKeepGroupTogetherCheck->setChecked(m_model.keepGroupTogether);
    m_paginationAllowRowSplitCheck->setChecked(m_model.allowRowSplit);
    m_paginationMaxPagesSpin->setValue(std::max(1, m_model.maxPages));
    m_paginationOrphanRowsSpin->setValue(std::max(0, m_model.orphanDetailRows));
    m_paginationGroupKeyEdit->setText(m_model.groupKeyField);
    const auto overflowIndex = m_paginationOverflowCombo->findData(static_cast<int>(m_model.tableOverflowPolicy));
    m_paginationOverflowCombo->setCurrentIndex(overflowIndex >= 0 ? overflowIndex : 0);

    m_paginationPreviewTable->setRowCount(0);
    m_paginationPreviewTable->setRowCount(m_model.pagePreviews.size());
    for (int row = 0; row < m_model.pagePreviews.size(); ++row) {
        const auto& preview = m_model.pagePreviews[row];
        const auto setReadOnlyItem = [this, row](int column, const QString& text) {
            auto* item = new QTableWidgetItem(text);
            item->setFlags(item->flags() & ~Qt::ItemIsEditable);
            m_paginationPreviewTable->setItem(row, column, item);
        };
        setReadOnlyItem(0, QString::number(preview.pageNumber));
        setReadOnlyItem(1, QString::number(preview.firstRowIndex));
        setReadOnlyItem(2, QString::number(preview.rowCount));
        setReadOnlyItem(3, preview.note);
    }
}

void TableAdvancedEditorPanel::updateButtonState()
{
    const auto rowBandRow = m_rowBandTable->currentRow();
    const auto styleRow = m_cellStyleTable->currentRow();
    const auto cellRow = m_cellTemplateTable->currentRow();
    const auto mergeRow = m_mergeRegionTable->currentRow();
    m_addRowBandButton->setEnabled(m_editable);
    m_deleteRowBandButton->setEnabled(m_editable && rowBandRow >= 0 && rowBandRow < m_rowBandTable->rowCount());
    m_moveRowBandUpButton->setEnabled(m_editable && rowBandRow > 0);
    m_moveRowBandDownButton->setEnabled(m_editable && rowBandRow >= 0 && rowBandRow < m_rowBandTable->rowCount() - 1);
    m_addStyleButton->setEnabled(m_editable);
    m_deleteStyleButton->setEnabled(m_editable && styleRow >= 0 && styleRow < m_cellStyleTable->rowCount());
    m_addCellTemplateButton->setEnabled(m_editable);
    m_deleteCellTemplateButton->setEnabled(m_editable && cellRow >= 0 && cellRow < m_cellTemplateTable->rowCount());
    m_mergeCellButton->setEnabled(m_editable);
    m_splitCellButton->setEnabled(m_editable && mergeRow >= 0 && mergeRow < m_mergeRegionTable->rowCount());
}

void TableAdvancedEditorPanel::emitEdited()
{
    if (m_updating || !m_editable) {
        return;
    }

    // 所有高级表格编辑都先折回同一个模型，避免多个网格之间出现旧数据分叉。
    m_model = tableProperties();
    updateButtonState();
    emit advancedPropertiesEdited();
}

QList<DesignerTableRowBandModel> TableAdvancedEditorPanel::rowBandsFromTable() const
{
    QList<DesignerTableRowBandModel> result;
    result.reserve(m_rowBandTable->rowCount());
    for (int row = 0; row < m_rowBandTable->rowCount(); ++row) {
        DesignerTableRowBandModel band;
        band.rowBandId = itemText(m_rowBandTable, row, RowBandId);
        band.kind = static_cast<sleekpr::core::TableRowBandKind>(
            comboValue(m_rowBandTable, row, RowBandKind, static_cast<int>(sleekpr::core::TableRowBandKind::Detail)));
        band.title = itemText(m_rowBandTable, row, RowBandTitle);
        band.dataPath = itemText(m_rowBandTable, row, RowBandDataPath);
        band.heightMode = static_cast<sleekpr::core::TableRowHeightMode>(
            comboValue(m_rowBandTable, row, RowBandHeightMode, static_cast<int>(sleekpr::core::TableRowHeightMode::Fixed)));
        band.heightMm = std::max(0.1, itemDouble(m_rowBandTable, row, RowBandHeight, 5.0));
        band.minHeightMm = std::max(0.1, itemDouble(m_rowBandTable, row, RowBandMinHeight, 4.0));
        band.repeatOnPage = checkValue(m_rowBandTable, row, RowBandRepeat, false);
        band.printOn = static_cast<sleekpr::core::TableRowPrintScope>(
            comboValue(m_rowBandTable, row, RowBandPrintOn, static_cast<int>(sleekpr::core::TableRowPrintScope::EveryPage)));
        result.append(band);
    }
    return result;
}

QList<DesignerTableCellStyleModel> TableAdvancedEditorPanel::cellStylesFromTable() const
{
    QList<DesignerTableCellStyleModel> result;
    result.reserve(m_cellStyleTable->rowCount());
    for (int row = 0; row < m_cellStyleTable->rowCount(); ++row) {
        DesignerTableCellStyleModel style;
        style.styleId = itemText(m_cellStyleTable, row, CellStyleId);
        style.fontSizePt = std::max(1.0, itemDouble(m_cellStyleTable, row, CellStyleFontSize, 8.0));
        style.bold = checkValue(m_cellStyleTable, row, CellStyleBold, false);
        style.alignment = static_cast<sleekpr::core::TableCellAlignment>(
            comboValue(m_cellStyleTable, row, CellStyleAlignment, static_cast<int>(sleekpr::core::TableCellAlignment::Left)));
        style.verticalAlignment = static_cast<sleekpr::core::TableVerticalAlignment>(
            comboValue(m_cellStyleTable, row, CellStyleVerticalAlignment, static_cast<int>(sleekpr::core::TableVerticalAlignment::Top)));
        style.paddingLeftMm = std::max(0.0, itemDouble(m_cellStyleTable, row, CellStylePaddingLeft, 0.6));
        style.paddingTopMm = std::max(0.0, itemDouble(m_cellStyleTable, row, CellStylePaddingTop, 0.4));
        style.paddingRightMm = std::max(0.0, itemDouble(m_cellStyleTable, row, CellStylePaddingRight, 0.6));
        style.paddingBottomMm = std::max(0.0, itemDouble(m_cellStyleTable, row, CellStylePaddingBottom, 0.4));
        style.drawBorder = checkValue(m_cellStyleTable, row, CellStyleDrawBorder, true);
        style.borderWidthMm = std::max(0.0, itemDouble(m_cellStyleTable, row, CellStyleBorderWidth, 0.1));
        style.backgroundColor = itemText(m_cellStyleTable, row, CellStyleBackground);
        style.textColor = itemText(m_cellStyleTable, row, CellStyleTextColor);
        style.wrapText = checkValue(m_cellStyleTable, row, CellStyleWrap, false);
        style.ellipsis = checkValue(m_cellStyleTable, row, CellStyleEllipsis, false);
        result.append(style);
    }
    return result;
}

QList<DesignerTableCellTemplateModel> TableAdvancedEditorPanel::cellTemplatesFromTable() const
{
    QList<DesignerTableCellTemplateModel> result;
    result.reserve(m_cellTemplateTable->rowCount());
    for (int row = 0; row < m_cellTemplateTable->rowCount(); ++row) {
        DesignerTableCellTemplateModel cell;
        cell.templateId = itemText(m_cellTemplateTable, row, CellTemplateId);
        cell.rowBandId = itemText(m_cellTemplateTable, row, CellTemplateRowBand);
        cell.columnId = itemText(m_cellTemplateTable, row, CellTemplateColumnId);
        cell.textTemplate = m_cellTemplateTable->item(row, CellTemplateText) != nullptr
            ? m_cellTemplateTable->item(row, CellTemplateText)->text()
            : QString();
        cell.fieldKey = itemText(m_cellTemplateTable, row, CellTemplateFieldKey);
        cell.styleId = itemText(m_cellTemplateTable, row, CellTemplateStyle);
        cell.overflowPolicy = static_cast<sleekpr::core::TableCellOverflowPolicy>(
            comboValue(m_cellTemplateTable, row, CellTemplateOverflow, static_cast<int>(sleekpr::core::TableCellOverflowPolicy::Clip)));
        cell.maxLines = std::max(1, itemInt(m_cellTemplateTable, row, CellTemplateMaxLines, 1));
        cell.colSpan = std::max(1, itemInt(m_cellTemplateTable, row, CellTemplateColSpan, 1));
        cell.rowSpan = std::max(1, itemInt(m_cellTemplateTable, row, CellTemplateRowSpan, 1));
        cell.visible = checkValue(m_cellTemplateTable, row, CellTemplateVisible, true);
        result.append(cell);
    }
    return result;
}

QList<DesignerTableMergeRegionModel> TableAdvancedEditorPanel::mergeRegionsFromTable() const
{
    QList<DesignerTableMergeRegionModel> result;
    result.reserve(m_mergeRegionTable->rowCount());
    for (int row = 0; row < m_mergeRegionTable->rowCount(); ++row) {
        DesignerTableMergeRegionModel merge;
        merge.mergeId = itemText(m_mergeRegionTable, row, MergeRegionId);
        merge.rowBandId = itemText(m_mergeRegionTable, row, MergeRegionRowBand);
        merge.startRowOffset = std::max(0, itemInt(m_mergeRegionTable, row, MergeRegionStartOffset, 0));
        merge.startColumnId = itemText(m_mergeRegionTable, row, MergeRegionStartColumn);
        merge.rowSpan = std::max(1, itemInt(m_mergeRegionTable, row, MergeRegionRowSpan, 1));
        merge.colSpan = std::max(1, itemInt(m_mergeRegionTable, row, MergeRegionColSpan, 1));
        result.append(merge);
    }
    return result;
}

void TableAdvancedEditorPanel::applyPaginationControls(DesignerTablePropertyModel* model) const
{
    if (model == nullptr) {
        return;
    }
    model->repeatHeaderOnPage = m_paginationRepeatHeaderCheck != nullptr
        ? m_paginationRepeatHeaderCheck->isChecked()
        : model->repeatHeaderOnPage;
    model->keepGroupTogether = m_paginationKeepGroupTogetherCheck != nullptr
        ? m_paginationKeepGroupTogetherCheck->isChecked()
        : model->keepGroupTogether;
    model->allowRowSplit = m_paginationAllowRowSplitCheck != nullptr
        ? m_paginationAllowRowSplitCheck->isChecked()
        : model->allowRowSplit;
    model->maxPages = m_paginationMaxPagesSpin != nullptr
        ? std::max(1, m_paginationMaxPagesSpin->value())
        : std::max(1, model->maxPages);
    model->orphanDetailRows = m_paginationOrphanRowsSpin != nullptr
        ? std::max(0, m_paginationOrphanRowsSpin->value())
        : std::max(0, model->orphanDetailRows);
    model->groupKeyField = m_paginationGroupKeyEdit != nullptr
        ? m_paginationGroupKeyEdit->text().trimmed()
        : model->groupKeyField.trimmed();
    if (m_paginationOverflowCombo != nullptr) {
        model->tableOverflowPolicy = static_cast<sleekpr::core::TableTableOverflowPolicy>(m_paginationOverflowCombo->currentData().toInt());
    }
}

DesignerTableRowBandModel TableAdvancedEditorPanel::createDefaultRowBand() const
{
    QSet<QString> ids;
    for (const auto& band : m_model.rowBands) {
        ids.insert(band.rowBandId);
    }
    DesignerTableRowBandModel band;
    band.rowBandId = uniqueId(QStringLiteral("band"), ids);
    band.title = QString::fromUtf8("明细行");
    band.dataPath = m_model.dataPath.trimmed().isEmpty() ? QStringLiteral("items") : m_model.dataPath.trimmed();
    return band;
}

DesignerTableCellStyleModel TableAdvancedEditorPanel::createDefaultCellStyle() const
{
    QSet<QString> ids;
    for (const auto& style : m_model.cellStyles) {
        ids.insert(style.styleId);
    }
    DesignerTableCellStyleModel style;
    style.styleId = uniqueId(QStringLiteral("style"), ids);
    return style;
}

DesignerTableCellTemplateModel TableAdvancedEditorPanel::createDefaultCellTemplate() const
{
    QSet<QString> ids;
    for (const auto& cell : m_model.cellTemplates) {
        ids.insert(cell.templateId);
    }
    DesignerTableCellTemplateModel cell;
    const auto rowBandId = m_model.rowBands.isEmpty() ? QStringLiteral("detail") : m_model.rowBands.first().rowBandId;
    const auto column = m_model.columns.isEmpty() ? DesignerTableColumnModel{} : m_model.columns.first();
    const auto columnId = column.columnId.trimmed().isEmpty() ? column.fieldKey.trimmed() : column.columnId.trimmed();
    cell.templateId = uniqueId(QStringLiteral("cell"), ids);
    cell.rowBandId = rowBandId;
    cell.columnId = columnId.isEmpty() ? QStringLiteral("column1") : columnId;
    cell.fieldKey = column.fieldKey.trimmed();
    cell.textTemplate = cell.fieldKey.isEmpty() ? QString() : QStringLiteral("${%1}").arg(cell.fieldKey);
    cell.styleId = m_model.cellStyles.isEmpty() ? QString() : m_model.cellStyles.first().styleId;
    return cell;
}

DesignerTableMergeRegionModel TableAdvancedEditorPanel::createDefaultMergeRegion() const
{
    QSet<QString> ids;
    for (const auto& merge : m_model.mergeRegions) {
        ids.insert(merge.mergeId);
    }
    const auto column = m_model.columns.isEmpty() ? DesignerTableColumnModel{} : m_model.columns.first();
    const auto columnId = column.columnId.trimmed().isEmpty() ? column.fieldKey.trimmed() : column.columnId.trimmed();
    DesignerTableMergeRegionModel merge;
    merge.mergeId = uniqueId(QStringLiteral("merge"), ids);
    merge.rowBandId = m_model.rowBands.isEmpty() ? QStringLiteral("detail") : m_model.rowBands.first().rowBandId;
    merge.startColumnId = columnId.isEmpty() ? QStringLiteral("column1") : columnId;
    merge.rowSpan = m_model.columns.size() > 1 ? 1 : 2;
    merge.colSpan = m_model.columns.size() > 1 ? 2 : 1;
    return merge;
}

QString TableAdvancedEditorPanel::uniqueId(const QString& prefix, const QSet<QString>& existingIds) const
{
    auto index = 1;
    auto candidate = QStringLiteral("%1%2").arg(prefix).arg(index);
    while (existingIds.contains(candidate)) {
        ++index;
        candidate = QStringLiteral("%1%2").arg(prefix).arg(index);
    }
    return candidate;
}

} // 命名空间 sleekpr::app

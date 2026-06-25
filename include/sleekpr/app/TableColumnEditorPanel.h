#pragma once

#include "sleekpr/app/TemplateDesignerPropertyModel.h"

#include <QWidget>

class QPushButton;
class QTableWidget;

namespace sleekpr::app {

class TableColumnEditorPanel final : public QWidget
{
    Q_OBJECT

public:
    explicit TableColumnEditorPanel(QWidget* parent = nullptr);

    void setColumns(const QList<DesignerTableColumnModel>& columns);
    QList<DesignerTableColumnModel> columns() const;
    void setEditable(bool editable);
    void selectColumn(int row);

signals:
    void columnsEdited();
    void columnAddRequested();
    void columnDeleteRequested(int row);
    void columnDuplicateRequested(int row);
    void columnMoveRequested(int fromRow, int toRow);
    void columnsResetRequested();

private:
    void rebuildTable();
    void updateButtonState();
    void emitEdited();
    int currentRow() const;
    DesignerTableColumnModel columnFromRow(int row) const;
    void writeColumnToRow(int row, const DesignerTableColumnModel& column);
    DesignerTableColumnModel createDefaultColumn() const;

    QTableWidget* m_table = nullptr;
    QPushButton* m_addButton = nullptr;
    QPushButton* m_deleteButton = nullptr;
    QPushButton* m_duplicateButton = nullptr;
    QPushButton* m_moveUpButton = nullptr;
    QPushButton* m_moveDownButton = nullptr;
    QPushButton* m_resetButton = nullptr;
    QList<DesignerTableColumnModel> m_columns;
    bool m_editable = true;
    bool m_updating = false;
};

} // namespace sleekpr::app

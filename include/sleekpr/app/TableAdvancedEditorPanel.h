#pragma once

#include "sleekpr/app/TemplateDesignerPropertyModel.h"

#include <QSet>
#include <QWidget>

class QPushButton;
class QTabWidget;
class QTableWidget;

namespace sleekpr::app {

class TableAdvancedEditorPanel final : public QWidget
{
    Q_OBJECT

public:
    explicit TableAdvancedEditorPanel(QWidget* parent = nullptr);

    void setProperties(const DesignerTablePropertyModel& model);
    DesignerTablePropertyModel tableProperties() const;
    void setEditable(bool editable);

signals:
    void advancedPropertiesEdited();

private:
    void rebuildTables();
    void rebuildRowBandTable();
    void rebuildCellStyleTable();
    void rebuildCellTemplateTable();
    void rebuildMergeRegionTable();
    void updateButtonState();
    void emitEdited();

    QList<DesignerTableRowBandModel> rowBandsFromTable() const;
    QList<DesignerTableCellStyleModel> cellStylesFromTable() const;
    QList<DesignerTableCellTemplateModel> cellTemplatesFromTable() const;
    QList<DesignerTableMergeRegionModel> mergeRegionsFromTable() const;

    DesignerTableRowBandModel createDefaultRowBand() const;
    DesignerTableCellStyleModel createDefaultCellStyle() const;
    DesignerTableCellTemplateModel createDefaultCellTemplate() const;
    DesignerTableMergeRegionModel createDefaultMergeRegion() const;
    QString uniqueId(const QString& prefix, const QSet<QString>& existingIds) const;

    QTabWidget* m_tabs = nullptr;
    QTableWidget* m_rowBandTable = nullptr;
    QTableWidget* m_cellStyleTable = nullptr;
    QTableWidget* m_cellTemplateTable = nullptr;
    QTableWidget* m_mergeRegionTable = nullptr;
    QPushButton* m_addRowBandButton = nullptr;
    QPushButton* m_deleteRowBandButton = nullptr;
    QPushButton* m_moveRowBandUpButton = nullptr;
    QPushButton* m_moveRowBandDownButton = nullptr;
    QPushButton* m_addStyleButton = nullptr;
    QPushButton* m_deleteStyleButton = nullptr;
    QPushButton* m_addCellTemplateButton = nullptr;
    QPushButton* m_deleteCellTemplateButton = nullptr;
    QPushButton* m_mergeCellButton = nullptr;
    QPushButton* m_splitCellButton = nullptr;
    DesignerTablePropertyModel m_model;
    bool m_editable = true;
    bool m_updating = false;
};

} // 命名空间 sleekpr::app

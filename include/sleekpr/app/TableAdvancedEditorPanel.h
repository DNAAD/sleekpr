#pragma once

#include "sleekpr/app/TemplateDesignerPropertyModel.h"

#include <QSet>
#include <QWidget>

class QCheckBox;
class QComboBox;
class QLineEdit;
class QPushButton;
class QSpinBox;
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
    void rebuildPaginationControls();
    void updateButtonState();
    void emitEdited();

    QList<DesignerTableRowBandModel> rowBandsFromTable() const;
    QList<DesignerTableCellStyleModel> cellStylesFromTable() const;
    QList<DesignerTableCellTemplateModel> cellTemplatesFromTable() const;
    QList<DesignerTableMergeRegionModel> mergeRegionsFromTable() const;
    void applyPaginationControls(DesignerTablePropertyModel* model) const;

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
    QTableWidget* m_paginationPreviewTable = nullptr;
    QCheckBox* m_paginationRepeatHeaderCheck = nullptr;
    QCheckBox* m_paginationKeepGroupTogetherCheck = nullptr;
    QCheckBox* m_paginationAllowRowSplitCheck = nullptr;
    QSpinBox* m_paginationMaxPagesSpin = nullptr;
    QSpinBox* m_paginationOrphanRowsSpin = nullptr;
    QLineEdit* m_paginationGroupKeyEdit = nullptr;
    QComboBox* m_paginationOverflowCombo = nullptr;
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

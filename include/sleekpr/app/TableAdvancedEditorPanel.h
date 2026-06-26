#pragma once

#include "sleekpr/app/TemplateDesignerPropertyModel.h"

#include <QSet>
#include <QWidget>

class QCheckBox;
class QComboBox;
class QLabel;
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
    void paginationPreviewSelected(int pageNumber, int firstRowIndex);

private:
    enum class PaginationPreset
    {
        ContinuePages,
        StrictCheck,
        CompactClip,
    };

    void rebuildTables();
    void rebuildRowBandTable();
    void rebuildCellStyleTable();
    void rebuildCellTemplateTable();
    void rebuildMergeRegionTable();
    void rebuildPaginationControls();
    void updateButtonState();
    void updatePaginationRiskState();
    void emitEdited();
    void emitPaginationPreviewSelection(int row);
    void applyPaginationPreset(PaginationPreset preset);
    void splitPaginationMergeRegions();
    void expandPaginationMaxPages();
    QString paginationRiskTextForRow(int row) const;
    QString paginationNoteForRow(int row) const;

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
    QPushButton* m_paginationPresetContinueButton = nullptr;
    QPushButton* m_paginationPresetStrictButton = nullptr;
    QPushButton* m_paginationPresetCompactButton = nullptr;
    QLabel* m_paginationRiskLabel = nullptr;
    QPushButton* m_paginationQuickSplitMergeButton = nullptr;
    QPushButton* m_paginationQuickContinueButton = nullptr;
    QPushButton* m_paginationQuickExpandPagesButton = nullptr;
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

#pragma once

#include "sleekpr/app/TemplateDesignerPropertyModel.h"

#include <QScrollArea>

class QAction;
class QCheckBox;
class QDoubleSpinBox;
class QLineEdit;
class QListWidget;
class QObject;
class QPlainTextEdit;
class QPushButton;
class QSpinBox;
class QWidget;

namespace sleekpr::app {

class TableColumnEditorPanel;
class TableAdvancedEditorPanel;

class TemplateInspectorPanel final : public QScrollArea
{
    Q_OBJECT

public:
    static constexpr int TextAutoApplyDelayMs = 650;
    static constexpr int ControlAutoApplyDelayMs = 80;

    // 只负责检查器控件创建、属性模型映射和语义信号，模板写回由 Presenter/Command 协调。
    explicit TemplateInspectorPanel(QWidget* parent = nullptr);

    void setElementProperties(const DesignerElementPropertyModel& model);
    DesignerElementPropertyModel elementProperties() const;
    void clearElementProperties();
    void setTableProperties(const DesignerTablePropertyModel& model);
    DesignerTablePropertyModel tableProperties() const;
    void clearTableProperties();
    bool isElementPropertyEditor(QObject* watched) const;
    bool isTablePropertyEditor(QObject* watched) const;
    void installPropertyEditorEventFilter(QObject* filterTarget);

    QListWidget* elementList() const;
    QAction* renameElementAction() const;
    QAction* deleteElementAction() const;
    QAction* lockElementAction() const;
    QAction* hideElementAction() const;
    QAction* moveElementUpAction() const;
    QAction* moveElementDownAction() const;

    QPushButton* addFixedTextButton() const;
    QPushButton* addBoundFieldButton() const;
    QPushButton* addQrCodeButton() const;
    QPushButton* addRectangleButton() const;
    QPushButton* addTableButton() const;
    QPushButton* addArrayGridButton() const;
    QPushButton* deleteElementButton() const;
    QPushButton* lockElementButton() const;
    QPushButton* hideElementButton() const;
    QPushButton* moveElementUpButton() const;
    QPushButton* moveElementDownButton() const;

    QPlainTextEdit* elementValueEdit() const;
    QDoubleSpinBox* elementXSpin() const;
    QDoubleSpinBox* elementYSpin() const;
    QDoubleSpinBox* elementWidthSpin() const;
    QDoubleSpinBox* elementHeightSpin() const;
    QDoubleSpinBox* elementFontSizeSpin() const;
    QDoubleSpinBox* elementRotationSpin() const;
    QCheckBox* elementBoldCheck() const;
    QCheckBox* elementAutoFitFontCheck() const;
    QDoubleSpinBox* elementAutoFitMinFontSizeSpin() const;
    QDoubleSpinBox* elementAutoFitMaxFontSizeSpin() const;
    QCheckBox* elementVerticalTextCheck() const;
    QPushButton* applyElementPropertiesButton() const;

    QLineEdit* arrayGridDataPathEdit() const;
    QSpinBox* arrayGridRowsSpin() const;
    QSpinBox* arrayGridColumnsSpin() const;
    QDoubleSpinBox* arrayGridRowHeightSpin() const;
    QPlainTextEdit* arrayGridCellTemplateEdit() const;
    QCheckBox* arrayGridDrawBordersCheck() const;
    QPushButton* applyArrayGridPropertiesButton() const;

    QLineEdit* tableDisplayNameEdit() const;
    QLineEdit* tableDataPathEdit() const;
    QDoubleSpinBox* tableXSpin() const;
    QDoubleSpinBox* tableYSpin() const;
    QDoubleSpinBox* tableWidthSpin() const;
    QDoubleSpinBox* tableHeightSpin() const;
    QDoubleSpinBox* tableHeaderHeightSpin() const;
    QDoubleSpinBox* tableDetailHeightSpin() const;
    QCheckBox* tableRepeatHeaderCheck() const;
    QCheckBox* tableDrawBordersCheck() const;
    QLineEdit* tableColumnsEdit() const;
    TableColumnEditorPanel* tableColumnEditor() const;
    TableAdvancedEditorPanel* tableAdvancedEditor() const;
    QPushButton* applyTablePropertiesButton() const;

    QLineEdit* deviceProfilePrinterEdit() const;
    QDoubleSpinBox* deviceProfileDpiSpin() const;
    QDoubleSpinBox* deviceProfileScaleXSpin() const;
    QDoubleSpinBox* deviceProfileScaleYSpin() const;
    QDoubleSpinBox* deviceProfileOffsetXSpin() const;
    QDoubleSpinBox* deviceProfileOffsetYSpin() const;
    QDoubleSpinBox* deviceCalibrationExpectedWidthSpin() const;
    QDoubleSpinBox* deviceCalibrationActualWidthSpin() const;
    QDoubleSpinBox* deviceCalibrationExpectedHeightSpin() const;
    QDoubleSpinBox* deviceCalibrationActualHeightSpin() const;
    QPushButton* calculateDeviceCalibrationButton() const;
    QPushButton* saveDeviceProfileButton() const;

signals:
    void elementPropertiesEdited(int delayMs);
    void tablePropertiesEdited(int delayMs);
    void elementPropertiesApplyRequested();
    void tablePropertiesApplyRequested();
    void elementPropertiesEditingFinished();
    void tablePropertiesEditingFinished();

private:
    void connectPropertySignals();
    void updateAutoFitRangeEnabled();

    QListWidget* m_elementList = nullptr;
    QAction* m_renameElementAction = nullptr;
    QAction* m_deleteElementAction = nullptr;
    QAction* m_lockElementAction = nullptr;
    QAction* m_hideElementAction = nullptr;
    QAction* m_moveElementUpAction = nullptr;
    QAction* m_moveElementDownAction = nullptr;

    QPushButton* m_addFixedTextButton = nullptr;
    QPushButton* m_addBoundFieldButton = nullptr;
    QPushButton* m_addQrCodeButton = nullptr;
    QPushButton* m_addRectangleButton = nullptr;
    QPushButton* m_addTableButton = nullptr;
    QPushButton* m_addArrayGridButton = nullptr;
    QPushButton* m_deleteElementButton = nullptr;
    QPushButton* m_lockElementButton = nullptr;
    QPushButton* m_hideElementButton = nullptr;
    QPushButton* m_moveElementUpButton = nullptr;
    QPushButton* m_moveElementDownButton = nullptr;

    QPlainTextEdit* m_elementValueEdit = nullptr;
    QDoubleSpinBox* m_elementXSpin = nullptr;
    QDoubleSpinBox* m_elementYSpin = nullptr;
    QDoubleSpinBox* m_elementWidthSpin = nullptr;
    QDoubleSpinBox* m_elementHeightSpin = nullptr;
    QDoubleSpinBox* m_elementFontSizeSpin = nullptr;
    QDoubleSpinBox* m_elementRotationSpin = nullptr;
    QCheckBox* m_elementBoldCheck = nullptr;
    QCheckBox* m_elementAutoFitFontCheck = nullptr;
    QDoubleSpinBox* m_elementAutoFitMinFontSizeSpin = nullptr;
    QDoubleSpinBox* m_elementAutoFitMaxFontSizeSpin = nullptr;
    QCheckBox* m_elementVerticalTextCheck = nullptr;
    QPushButton* m_applyElementPropertiesButton = nullptr;

    QLineEdit* m_arrayGridDataPathEdit = nullptr;
    QSpinBox* m_arrayGridRowsSpin = nullptr;
    QSpinBox* m_arrayGridColumnsSpin = nullptr;
    QDoubleSpinBox* m_arrayGridRowHeightSpin = nullptr;
    QPlainTextEdit* m_arrayGridCellTemplateEdit = nullptr;
    QCheckBox* m_arrayGridDrawBordersCheck = nullptr;
    QPushButton* m_applyArrayGridPropertiesButton = nullptr;

    QLineEdit* m_tableDisplayNameEdit = nullptr;
    QLineEdit* m_tableDataPathEdit = nullptr;
    QDoubleSpinBox* m_tableXSpin = nullptr;
    QDoubleSpinBox* m_tableYSpin = nullptr;
    QDoubleSpinBox* m_tableWidthSpin = nullptr;
    QDoubleSpinBox* m_tableHeightSpin = nullptr;
    QDoubleSpinBox* m_tableHeaderHeightSpin = nullptr;
    QDoubleSpinBox* m_tableDetailHeightSpin = nullptr;
    QCheckBox* m_tableRepeatHeaderCheck = nullptr;
    QCheckBox* m_tableDrawBordersCheck = nullptr;
    QLineEdit* m_tableColumnsEdit = nullptr;
    TableColumnEditorPanel* m_tableColumnEditor = nullptr;
    TableAdvancedEditorPanel* m_tableAdvancedEditor = nullptr;
    QPushButton* m_applyTablePropertiesButton = nullptr;

    QLineEdit* m_deviceProfilePrinterEdit = nullptr;
    QDoubleSpinBox* m_deviceProfileDpiSpin = nullptr;
    QDoubleSpinBox* m_deviceProfileScaleXSpin = nullptr;
    QDoubleSpinBox* m_deviceProfileScaleYSpin = nullptr;
    QDoubleSpinBox* m_deviceProfileOffsetXSpin = nullptr;
    QDoubleSpinBox* m_deviceProfileOffsetYSpin = nullptr;
    QDoubleSpinBox* m_deviceCalibrationExpectedWidthSpin = nullptr;
    QDoubleSpinBox* m_deviceCalibrationActualWidthSpin = nullptr;
    QDoubleSpinBox* m_deviceCalibrationExpectedHeightSpin = nullptr;
    QDoubleSpinBox* m_deviceCalibrationActualHeightSpin = nullptr;
    QPushButton* m_calculateDeviceCalibrationButton = nullptr;
    QPushButton* m_saveDeviceProfileButton = nullptr;
    DesignerElementPropertyModel m_elementModel;
    DesignerTablePropertyModel m_tableModel;
    bool m_settingElementProperties = false;
    bool m_settingTableProperties = false;
    bool m_tableColumnsTextEdited = false;
};

} // 命名空间 sleekpr::app

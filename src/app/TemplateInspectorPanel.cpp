#include "sleekpr/app/TemplateInspectorPanel.h"

#include "sleekpr/app/TableAdvancedEditorPanel.h"
#include "sleekpr/app/TableColumnEditorPanel.h"
#include "sleekpr/app/TableColumnTextCodec.h"

#include <QAbstractItemView>
#include <QAction>
#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScopedValueRollback>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QTabWidget>
#include <QVBoxLayout>

#include <algorithm>

namespace sleekpr::app {

namespace {

QPushButton* createInspectorButton(const QString& text, const QString& objectName, QWidget* parent)
{
    auto* button = new QPushButton(text, parent);
    button->setObjectName(objectName);
    return button;
}

QDoubleSpinBox* createProfileSpinBox(
    const QString& objectName,
    double minimum,
    double maximum,
    double value,
    QWidget* parent)
{
    auto* spinBox = new QDoubleSpinBox(parent);
    spinBox->setObjectName(objectName);
    spinBox->setRange(minimum, maximum);
    spinBox->setDecimals(3);
    spinBox->setSingleStep(0.01);
    spinBox->setValue(value);
    return spinBox;
}

QDoubleSpinBox* createInspectorSpinBox(
    const QString& objectName,
    double minimum,
    double maximum,
    double value,
    QWidget* parent)
{
    auto* spinBox = new QDoubleSpinBox(parent);
    spinBox->setObjectName(objectName);
    spinBox->setRange(minimum, maximum);
    spinBox->setDecimals(2);
    spinBox->setSingleStep(0.5);
    spinBox->setValue(value);
    return spinBox;
}

QSpinBox* createGridSpinBox(const QString& objectName, int minimum, int maximum, int value, QWidget* parent)
{
    auto* spinBox = new QSpinBox(parent);
    spinBox->setObjectName(objectName);
    spinBox->setRange(minimum, maximum);
    spinBox->setValue(value);
    return spinBox;
}

bool ownsEditor(const QWidget* owner, const QWidget* candidate)
{
    return owner != nullptr
        && candidate != nullptr
        && (owner == candidate || owner->isAncestorOf(candidate));
}

void installEditorFilter(QObject* filterTarget, QWidget* editor)
{
    if (filterTarget == nullptr || editor == nullptr) {
        return;
    }

    editor->installEventFilter(filterTarget);
    for (auto* child : editor->findChildren<QWidget*>()) {
        child->installEventFilter(filterTarget);
    }
}

QList<sleekpr::core::TableColumn> tableColumnsFromDesignerModels(const QList<DesignerTableColumnModel>& models)
{
    QList<sleekpr::core::TableColumn> columns;
    columns.reserve(models.size());
    // 高级文本框仍用于兼容旧格式，这里把结构化列临时转回核心模型供 codec 格式化。
    for (const auto& model : models) {
        sleekpr::core::TableColumn column;
        column.id = model.columnId.trimmed().isEmpty() ? model.fieldKey.trimmed() : model.columnId.trimmed();
        column.title = model.title;
        column.fieldKey = model.fieldKey.trimmed();
        column.widthMode = model.widthMode;
        column.widthMm = std::max(0.1, model.widthMm);
        column.flexWeight = std::max(0.1, model.flexWeight);
        column.alignment = model.alignment;
        column.fontSizePt = std::max(1.0, model.fontSizePt);
        column.bold = model.bold;
        column.ellipsis = model.ellipsis;
        columns.append(column);
    }
    return columns;
}

} // 命名空间

TemplateInspectorPanel::TemplateInspectorPanel(QWidget* parent)
    : QScrollArea(parent)
{
    setObjectName(QStringLiteral("designerInspectorScrollArea"));
    setWidgetResizable(true);
    setMinimumWidth(320);

    auto* elementPanel = new QWidget(this);
    elementPanel->setObjectName(QStringLiteral("designerInspectorPanel"));
    auto* elementLayout = new QVBoxLayout(elementPanel);
    elementLayout->setContentsMargins(10, 10, 10, 10);
    elementLayout->setSpacing(8);

    auto* elementTitleLabel = new QLabel(QString::fromUtf8("元素"), elementPanel);
    elementTitleLabel->setObjectName(QStringLiteral("designerPanelTitle"));
    m_elementList = new QListWidget(elementPanel);
    m_elementList->setObjectName(QStringLiteral("templateElementList"));
    m_elementList->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed);
    m_elementList->setDragDropMode(QAbstractItemView::InternalMove);
    m_elementList->setDefaultDropAction(Qt::MoveAction);
    m_elementList->setSelectionMode(QAbstractItemView::SingleSelection);
    m_elementList->setContextMenuPolicy(Qt::ActionsContextMenu);

    m_renameElementAction = new QAction(QString::fromUtf8("重命名"), m_elementList);
    m_renameElementAction->setObjectName(QStringLiteral("renameElementAction"));
    m_deleteElementAction = new QAction(QString::fromUtf8("删除"), m_elementList);
    m_deleteElementAction->setObjectName(QStringLiteral("deleteElementAction"));
    m_lockElementAction = new QAction(QString::fromUtf8("锁定/解锁"), m_elementList);
    m_lockElementAction->setObjectName(QStringLiteral("lockElementAction"));
    m_hideElementAction = new QAction(QString::fromUtf8("隐藏/显示"), m_elementList);
    m_hideElementAction->setObjectName(QStringLiteral("hideElementAction"));
    m_moveElementUpAction = new QAction(QString::fromUtf8("上移"), m_elementList);
    m_moveElementUpAction->setObjectName(QStringLiteral("moveElementUpAction"));
    m_moveElementDownAction = new QAction(QString::fromUtf8("下移"), m_elementList);
    m_moveElementDownAction->setObjectName(QStringLiteral("moveElementDownAction"));
    m_elementList->addAction(m_renameElementAction);
    m_elementList->addAction(m_deleteElementAction);
    m_elementList->addAction(m_lockElementAction);
    m_elementList->addAction(m_hideElementAction);
    m_elementList->addAction(m_moveElementUpAction);
    m_elementList->addAction(m_moveElementDownAction);

    m_addFixedTextButton = createInspectorButton(QString::fromUtf8("固定文本"), QStringLiteral("designerAddFixedTextButton"), elementPanel);
    m_addBoundFieldButton = createInspectorButton(QString::fromUtf8("绑定字段"), QStringLiteral("designerAddBoundFieldButton"), elementPanel);
    m_addQrCodeButton = createInspectorButton(QString::fromUtf8("二维码"), QStringLiteral("designerAddQrCodeButton"), elementPanel);
    m_addRectangleButton = createInspectorButton(QString::fromUtf8("矩形"), QStringLiteral("designerAddRectangleButton"), elementPanel);
    m_addTableButton = createInspectorButton(QString::fromUtf8("表格"), QStringLiteral("designerAddTableButton"), elementPanel);
    m_addArrayGridButton = createInspectorButton(QString::fromUtf8("数组网格"), QStringLiteral("designerAddArrayGridButton"), elementPanel);
    m_deleteElementButton = createInspectorButton(QString::fromUtf8("删除元素"), QStringLiteral("deleteElementButton"), elementPanel);
    m_deleteElementButton->setProperty("buttonRole", QStringLiteral("danger"));
    m_lockElementButton = createInspectorButton(QString::fromUtf8("锁定元素"), QStringLiteral("lockElementButton"), elementPanel);
    m_hideElementButton = createInspectorButton(QString::fromUtf8("隐藏元素"), QStringLiteral("hideElementButton"), elementPanel);
    m_moveElementUpButton = createInspectorButton(QString::fromUtf8("上移元素"), QStringLiteral("moveElementUpButton"), elementPanel);
    m_moveElementDownButton = createInspectorButton(QString::fromUtf8("下移元素"), QStringLiteral("moveElementDownButton"), elementPanel);

    auto* tablePropertyTitleLabel = new QLabel(QString::fromUtf8("表格属性"), elementPanel);
    m_tableDisplayNameEdit = new QLineEdit(elementPanel);
    m_tableDisplayNameEdit->setObjectName(QStringLiteral("tableDisplayNameEdit"));
    m_tableDataPathEdit = new QLineEdit(elementPanel);
    m_tableDataPathEdit->setObjectName(QStringLiteral("tableDataPathEdit"));
    m_tableXSpin = createInspectorSpinBox(QStringLiteral("tableXSpin"), 0.0, 500.0, 5.0, elementPanel);
    m_tableYSpin = createInspectorSpinBox(QStringLiteral("tableYSpin"), 0.0, 500.0, 10.0, elementPanel);
    m_tableWidthSpin = createInspectorSpinBox(QStringLiteral("tableWidthSpin"), 1.0, 500.0, 70.0, elementPanel);
    m_tableHeightSpin = createInspectorSpinBox(QStringLiteral("tableHeightSpin"), 1.0, 500.0, 15.0, elementPanel);
    m_tableHeaderHeightSpin = createInspectorSpinBox(QStringLiteral("tableHeaderHeightSpin"), 1.0, 100.0, 5.0, elementPanel);
    m_tableDetailHeightSpin = createInspectorSpinBox(QStringLiteral("tableDetailHeightSpin"), 1.0, 100.0, 5.0, elementPanel);
    m_tableRepeatHeaderCheck = new QCheckBox(QString::fromUtf8("分页重复表头"), elementPanel);
    m_tableRepeatHeaderCheck->setObjectName(QStringLiteral("tableRepeatHeaderCheck"));
    m_tableDrawBordersCheck = new QCheckBox(QString::fromUtf8("绘制边框"), elementPanel);
    m_tableDrawBordersCheck->setObjectName(QStringLiteral("tableDrawBordersCheck"));
    m_tableColumnsEdit = new QLineEdit(elementPanel);
    m_tableColumnsEdit->setObjectName(QStringLiteral("tableColumnsEdit"));
    m_tableColumnsEdit->setPlaceholderText(QString::fromUtf8("品名=productName:45,重量=weight:25"));
    m_tableColumnEditor = new TableColumnEditorPanel(elementPanel);
    m_tableColumnEditor->setObjectName(QStringLiteral("tableColumnEditorPanel"));
    m_tableAdvancedEditor = new TableAdvancedEditorPanel(elementPanel);
    m_applyTablePropertiesButton = createInspectorButton(
        QString::fromUtf8("应用表格属性"),
        QStringLiteral("applyTablePropertiesButton"),
        elementPanel);
    m_applyTablePropertiesButton->setProperty("buttonRole", QStringLiteral("primary"));

    auto* deviceProfileTitleLabel = new QLabel(QString::fromUtf8("设备校准"), elementPanel);
    m_deviceProfilePrinterEdit = new QLineEdit(elementPanel);
    m_deviceProfilePrinterEdit->setObjectName(QStringLiteral("deviceProfilePrinterEdit"));
    m_deviceProfilePrinterEdit->setPlaceholderText(QString::fromUtf8("打印机名称"));
    m_deviceProfileDpiSpin = createProfileSpinBox(QStringLiteral("deviceProfileDpiSpin"), 72.0, 1200.0, 300.0, elementPanel);
    m_deviceProfileScaleXSpin = createProfileSpinBox(QStringLiteral("deviceProfileScaleXSpin"), 0.5, 2.0, 1.0, elementPanel);
    m_deviceProfileScaleYSpin = createProfileSpinBox(QStringLiteral("deviceProfileScaleYSpin"), 0.5, 2.0, 1.0, elementPanel);
    m_deviceProfileOffsetXSpin = createProfileSpinBox(QStringLiteral("deviceProfileOffsetXSpin"), -50.0, 50.0, 0.0, elementPanel);
    m_deviceProfileOffsetYSpin = createProfileSpinBox(QStringLiteral("deviceProfileOffsetYSpin"), -50.0, 50.0, 0.0, elementPanel);
    m_deviceCalibrationExpectedWidthSpin = createProfileSpinBox(QStringLiteral("deviceCalibrationExpectedWidthSpin"), 1.0, 1000.0, 80.0, elementPanel);
    m_deviceCalibrationActualWidthSpin = createProfileSpinBox(QStringLiteral("deviceCalibrationActualWidthSpin"), 1.0, 1000.0, 80.0, elementPanel);
    m_deviceCalibrationExpectedHeightSpin = createProfileSpinBox(QStringLiteral("deviceCalibrationExpectedHeightSpin"), 1.0, 1000.0, 30.0, elementPanel);
    m_deviceCalibrationActualHeightSpin = createProfileSpinBox(QStringLiteral("deviceCalibrationActualHeightSpin"), 1.0, 1000.0, 30.0, elementPanel);
    m_calculateDeviceCalibrationButton = createInspectorButton(
        QString::fromUtf8("按测量值计算缩放"),
        QStringLiteral("calculateDeviceCalibrationScaleButton"),
        elementPanel);
    m_saveDeviceProfileButton = createInspectorButton(QString::fromUtf8("保存校准"), QStringLiteral("saveDeviceProfileButton"), elementPanel);
    m_saveDeviceProfileButton->setProperty("buttonRole", QStringLiteral("primary"));

    auto* addElementGrid = new QGridLayout();
    addElementGrid->setContentsMargins(0, 0, 0, 0);
    addElementGrid->addWidget(m_addFixedTextButton, 0, 0);
    addElementGrid->addWidget(m_addBoundFieldButton, 0, 1);
    addElementGrid->addWidget(m_addQrCodeButton, 1, 0);
    addElementGrid->addWidget(m_addRectangleButton, 1, 1);
    addElementGrid->addWidget(m_addTableButton, 2, 0);
    addElementGrid->addWidget(m_addArrayGridButton, 2, 1);

    auto* editElementGrid = new QGridLayout();
    editElementGrid->setContentsMargins(0, 0, 0, 0);
    editElementGrid->addWidget(m_deleteElementButton, 0, 0);
    editElementGrid->addWidget(m_lockElementButton, 0, 1);
    editElementGrid->addWidget(m_hideElementButton, 1, 0);
    editElementGrid->addWidget(m_moveElementUpButton, 1, 1);
    editElementGrid->addWidget(m_moveElementDownButton, 2, 0, 1, 2);

    auto* elementPropertyTitleLabel = new QLabel(QString::fromUtf8("元素属性"), elementPanel);
    auto* elementValueLabel = new QLabel(QString::fromUtf8("内容（可用 ${fieldKey} 占位）"), elementPanel);
    m_elementValueEdit = new QPlainTextEdit(elementPanel);
    m_elementValueEdit->setObjectName(QStringLiteral("elementValueEdit"));
    m_elementValueEdit->setFixedHeight(72);
    m_elementValueEdit->setPlaceholderText(QString::fromUtf8("例如：地址:${address}"));
    m_elementXSpin = createInspectorSpinBox(QStringLiteral("elementXSpin"), 0.0, 1000.0, 5.0, elementPanel);
    m_elementYSpin = createInspectorSpinBox(QStringLiteral("elementYSpin"), 0.0, 1000.0, 5.0, elementPanel);
    m_elementWidthSpin = createInspectorSpinBox(QStringLiteral("elementWidthSpin"), 0.1, 1000.0, 10.0, elementPanel);
    m_elementHeightSpin = createInspectorSpinBox(QStringLiteral("elementHeightSpin"), 0.1, 1000.0, 3.0, elementPanel);
    m_elementFontSizeSpin = createInspectorSpinBox(QStringLiteral("elementFontSizeSpin"), 1.0, 96.0, 4.0, elementPanel);
    m_elementRotationSpin = createInspectorSpinBox(QStringLiteral("elementRotationSpin"), -360.0, 360.0, 0.0, elementPanel);
    m_elementBoldCheck = new QCheckBox(QString::fromUtf8("加粗"), elementPanel);
    m_elementBoldCheck->setObjectName(QStringLiteral("elementBoldCheck"));
    m_elementAutoFitFontCheck = new QCheckBox(QString::fromUtf8("自动适配字号"), elementPanel);
    m_elementAutoFitFontCheck->setObjectName(QStringLiteral("elementAutoFitFontCheck"));
    m_elementAutoFitMinFontSizeSpin = createInspectorSpinBox(QStringLiteral("elementAutoFitMinFontSizeSpin"), 1.0, 96.0, 3.0, elementPanel);
    m_elementAutoFitMaxFontSizeSpin = createInspectorSpinBox(QStringLiteral("elementAutoFitMaxFontSizeSpin"), 1.0, 96.0, 12.0, elementPanel);
    m_elementVerticalTextCheck = new QCheckBox(QString::fromUtf8("竖排文本"), elementPanel);
    m_elementVerticalTextCheck->setObjectName(QStringLiteral("elementVerticalTextCheck"));

    auto* arrayGridPropertyTitleLabel = new QLabel(QString::fromUtf8("数组网格属性"), elementPanel);
    m_arrayGridDataPathEdit = new QLineEdit(elementPanel);
    m_arrayGridDataPathEdit->setObjectName(QStringLiteral("arrayGridDataPathEdit"));
    m_arrayGridDataPathEdit->setPlaceholderText(QStringLiteral("header_items"));
    m_arrayGridRowsSpin = createGridSpinBox(QStringLiteral("arrayGridRowsSpin"), 1, 20, 2, elementPanel);
    m_arrayGridColumnsSpin = createGridSpinBox(QStringLiteral("arrayGridColumnsSpin"), 1, 20, 3, elementPanel);
    m_arrayGridRowHeightSpin = createInspectorSpinBox(QStringLiteral("arrayGridRowHeightSpin"), 0.0, 100.0, 0.0, elementPanel);
    m_arrayGridCellTemplateEdit = new QPlainTextEdit(elementPanel);
    m_arrayGridCellTemplateEdit->setObjectName(QStringLiteral("arrayGridCellTemplateEdit"));
    m_arrayGridCellTemplateEdit->setFixedHeight(56);
    m_arrayGridCellTemplateEdit->setPlaceholderText(QStringLiteral("${text}:${value}"));
    m_arrayGridDrawBordersCheck = new QCheckBox(QString::fromUtf8("绘制网格边框"), elementPanel);
    m_arrayGridDrawBordersCheck->setObjectName(QStringLiteral("arrayGridDrawBordersCheck"));
    m_applyElementPropertiesButton = createInspectorButton(
        QString::fromUtf8("应用元素属性"),
        QStringLiteral("applyElementPropertiesButton"),
        elementPanel);
    m_applyElementPropertiesButton->setProperty("buttonRole", QStringLiteral("primary"));
    m_applyArrayGridPropertiesButton = createInspectorButton(
        QString::fromUtf8("应用数组网格属性"),
        QStringLiteral("applyArrayGridPropertiesButton"),
        elementPanel);
    m_applyArrayGridPropertiesButton->setProperty("buttonRole", QStringLiteral("primary"));

    auto* tablePropertyGrid = new QGridLayout();
    tablePropertyGrid->setContentsMargins(0, 0, 0, 0);
    tablePropertyGrid->addWidget(new QLabel(QString::fromUtf8("名称"), elementPanel), 0, 0);
    tablePropertyGrid->addWidget(m_tableDisplayNameEdit, 0, 1);
    tablePropertyGrid->addWidget(new QLabel(QStringLiteral("dataPath"), elementPanel), 1, 0);
    tablePropertyGrid->addWidget(m_tableDataPathEdit, 1, 1);
    tablePropertyGrid->addWidget(new QLabel(QStringLiteral("X"), elementPanel), 2, 0);
    tablePropertyGrid->addWidget(m_tableXSpin, 2, 1);
    tablePropertyGrid->addWidget(new QLabel(QStringLiteral("Y"), elementPanel), 3, 0);
    tablePropertyGrid->addWidget(m_tableYSpin, 3, 1);
    tablePropertyGrid->addWidget(new QLabel(QString::fromUtf8("宽"), elementPanel), 4, 0);
    tablePropertyGrid->addWidget(m_tableWidthSpin, 4, 1);
    tablePropertyGrid->addWidget(new QLabel(QString::fromUtf8("高"), elementPanel), 5, 0);
    tablePropertyGrid->addWidget(m_tableHeightSpin, 5, 1);
    tablePropertyGrid->addWidget(new QLabel(QString::fromUtf8("表头高"), elementPanel), 6, 0);
    tablePropertyGrid->addWidget(m_tableHeaderHeightSpin, 6, 1);
    tablePropertyGrid->addWidget(new QLabel(QString::fromUtf8("明细高"), elementPanel), 7, 0);
    tablePropertyGrid->addWidget(m_tableDetailHeightSpin, 7, 1);
    tablePropertyGrid->addWidget(m_tableRepeatHeaderCheck, 8, 0, 1, 2);
    tablePropertyGrid->addWidget(m_tableDrawBordersCheck, 9, 0, 1, 2);
    tablePropertyGrid->addWidget(new QLabel(QString::fromUtf8("列"), elementPanel), 10, 0);
    tablePropertyGrid->addWidget(m_tableColumnsEdit, 10, 1);

    auto* elementGeometryGrid = new QGridLayout();
    elementGeometryGrid->setContentsMargins(0, 0, 0, 0);
    elementGeometryGrid->addWidget(new QLabel(QStringLiteral("X"), elementPanel), 0, 0);
    elementGeometryGrid->addWidget(m_elementXSpin, 0, 1);
    elementGeometryGrid->addWidget(new QLabel(QStringLiteral("Y"), elementPanel), 1, 0);
    elementGeometryGrid->addWidget(m_elementYSpin, 1, 1);
    elementGeometryGrid->addWidget(new QLabel(QString::fromUtf8("宽"), elementPanel), 2, 0);
    elementGeometryGrid->addWidget(m_elementWidthSpin, 2, 1);
    elementGeometryGrid->addWidget(new QLabel(QString::fromUtf8("高"), elementPanel), 3, 0);
    elementGeometryGrid->addWidget(m_elementHeightSpin, 3, 1);
    elementGeometryGrid->addWidget(new QLabel(QString::fromUtf8("字号"), elementPanel), 4, 0);
    elementGeometryGrid->addWidget(m_elementFontSizeSpin, 4, 1);
    elementGeometryGrid->addWidget(new QLabel(QString::fromUtf8("旋转"), elementPanel), 5, 0);
    elementGeometryGrid->addWidget(m_elementRotationSpin, 5, 1);
    elementGeometryGrid->addWidget(m_elementBoldCheck, 6, 0, 1, 2);
    elementGeometryGrid->addWidget(m_elementAutoFitFontCheck, 7, 0, 1, 2);
    elementGeometryGrid->addWidget(new QLabel(QString::fromUtf8("最小字号"), elementPanel), 8, 0);
    elementGeometryGrid->addWidget(m_elementAutoFitMinFontSizeSpin, 8, 1);
    elementGeometryGrid->addWidget(new QLabel(QString::fromUtf8("最大字号"), elementPanel), 9, 0);
    elementGeometryGrid->addWidget(m_elementAutoFitMaxFontSizeSpin, 9, 1);
    elementGeometryGrid->addWidget(m_elementVerticalTextCheck, 10, 0, 1, 2);

    auto* arrayGridPropertyGrid = new QGridLayout();
    arrayGridPropertyGrid->setContentsMargins(0, 0, 0, 0);
    arrayGridPropertyGrid->addWidget(new QLabel(QStringLiteral("dataPath"), elementPanel), 0, 0);
    arrayGridPropertyGrid->addWidget(m_arrayGridDataPathEdit, 0, 1);
    arrayGridPropertyGrid->addWidget(new QLabel(QString::fromUtf8("行数"), elementPanel), 1, 0);
    arrayGridPropertyGrid->addWidget(m_arrayGridRowsSpin, 1, 1);
    arrayGridPropertyGrid->addWidget(new QLabel(QString::fromUtf8("列数"), elementPanel), 2, 0);
    arrayGridPropertyGrid->addWidget(m_arrayGridColumnsSpin, 2, 1);
    arrayGridPropertyGrid->addWidget(new QLabel(QString::fromUtf8("行高(mm)"), elementPanel), 3, 0);
    arrayGridPropertyGrid->addWidget(m_arrayGridRowHeightSpin, 3, 1);
    arrayGridPropertyGrid->addWidget(new QLabel(QString::fromUtf8("单元格模板"), elementPanel), 4, 0, 1, 2);
    arrayGridPropertyGrid->addWidget(m_arrayGridCellTemplateEdit, 5, 0, 1, 2);
    arrayGridPropertyGrid->addWidget(m_arrayGridDrawBordersCheck, 6, 0, 1, 2);

    auto* deviceProfileGrid = new QGridLayout();
    deviceProfileGrid->setContentsMargins(0, 0, 0, 0);
    deviceProfileGrid->addWidget(new QLabel(QString::fromUtf8("打印机"), elementPanel), 0, 0);
    deviceProfileGrid->addWidget(m_deviceProfilePrinterEdit, 0, 1);
    deviceProfileGrid->addWidget(new QLabel(QStringLiteral("DPI"), elementPanel), 1, 0);
    deviceProfileGrid->addWidget(m_deviceProfileDpiSpin, 1, 1);
    deviceProfileGrid->addWidget(new QLabel(QString::fromUtf8("横向缩放"), elementPanel), 2, 0);
    deviceProfileGrid->addWidget(m_deviceProfileScaleXSpin, 2, 1);
    deviceProfileGrid->addWidget(new QLabel(QString::fromUtf8("纵向缩放"), elementPanel), 3, 0);
    deviceProfileGrid->addWidget(m_deviceProfileScaleYSpin, 3, 1);
    deviceProfileGrid->addWidget(new QLabel(QString::fromUtf8("横向偏移"), elementPanel), 4, 0);
    deviceProfileGrid->addWidget(m_deviceProfileOffsetXSpin, 4, 1);
    deviceProfileGrid->addWidget(new QLabel(QString::fromUtf8("纵向偏移"), elementPanel), 5, 0);
    deviceProfileGrid->addWidget(m_deviceProfileOffsetYSpin, 5, 1);
    deviceProfileGrid->addWidget(new QLabel(QString::fromUtf8("期望宽度"), elementPanel), 6, 0);
    deviceProfileGrid->addWidget(m_deviceCalibrationExpectedWidthSpin, 6, 1);
    deviceProfileGrid->addWidget(new QLabel(QString::fromUtf8("实际宽度"), elementPanel), 7, 0);
    deviceProfileGrid->addWidget(m_deviceCalibrationActualWidthSpin, 7, 1);
    deviceProfileGrid->addWidget(new QLabel(QString::fromUtf8("期望高度"), elementPanel), 8, 0);
    deviceProfileGrid->addWidget(m_deviceCalibrationExpectedHeightSpin, 8, 1);
    deviceProfileGrid->addWidget(new QLabel(QString::fromUtf8("实际高度"), elementPanel), 9, 0);
    deviceProfileGrid->addWidget(m_deviceCalibrationActualHeightSpin, 9, 1);

    auto* inspectorTabs = new QTabWidget(elementPanel);
    inspectorTabs->setObjectName(QStringLiteral("designerInspectorTabs"));
    auto* elementTab = new QWidget(inspectorTabs);
    elementTab->setObjectName(QStringLiteral("designerElementInspectorTab"));
    auto* elementTabLayout = new QVBoxLayout(elementTab);
    elementTabLayout->setContentsMargins(8, 8, 8, 8);
    elementTabLayout->setSpacing(8);
    elementTabLayout->addWidget(elementTitleLabel);
    elementTabLayout->addWidget(m_elementList, 1);
    elementTabLayout->addLayout(addElementGrid);
    elementTabLayout->addLayout(editElementGrid);
    elementTabLayout->addWidget(elementPropertyTitleLabel);
    elementTabLayout->addWidget(elementValueLabel);
    elementTabLayout->addWidget(m_elementValueEdit);
    elementTabLayout->addLayout(elementGeometryGrid);
    elementTabLayout->addWidget(m_applyElementPropertiesButton);

    auto* tableTab = new QWidget(inspectorTabs);
    tableTab->setObjectName(QStringLiteral("designerTableInspectorTab"));
    auto* tableTabLayout = new QVBoxLayout(tableTab);
    tableTabLayout->setContentsMargins(8, 8, 8, 8);
    tableTabLayout->setSpacing(8);
    tableTabLayout->addWidget(tablePropertyTitleLabel);
    tableTabLayout->addLayout(tablePropertyGrid);
    tableTabLayout->addWidget(m_tableColumnEditor);
    tableTabLayout->addWidget(m_tableAdvancedEditor);
    tableTabLayout->addWidget(m_applyTablePropertiesButton);
    tableTabLayout->addStretch(1);

    auto* arrayGridTab = new QWidget(inspectorTabs);
    arrayGridTab->setObjectName(QStringLiteral("designerArrayGridInspectorTab"));
    auto* arrayGridTabLayout = new QVBoxLayout(arrayGridTab);
    arrayGridTabLayout->setContentsMargins(8, 8, 8, 8);
    arrayGridTabLayout->setSpacing(8);
    arrayGridTabLayout->addWidget(arrayGridPropertyTitleLabel);
    arrayGridTabLayout->addLayout(arrayGridPropertyGrid);
    arrayGridTabLayout->addWidget(m_applyArrayGridPropertiesButton);
    arrayGridTabLayout->addStretch(1);

    auto* deviceTab = new QWidget(inspectorTabs);
    deviceTab->setObjectName(QStringLiteral("designerDeviceInspectorTab"));
    auto* deviceTabLayout = new QVBoxLayout(deviceTab);
    deviceTabLayout->setContentsMargins(8, 8, 8, 8);
    deviceTabLayout->setSpacing(8);
    deviceTabLayout->addWidget(deviceProfileTitleLabel);
    deviceTabLayout->addLayout(deviceProfileGrid);
    deviceTabLayout->addWidget(m_calculateDeviceCalibrationButton);
    deviceTabLayout->addWidget(m_saveDeviceProfileButton);
    deviceTabLayout->addStretch(1);

    inspectorTabs->addTab(elementTab, QString::fromUtf8("元素"));
    inspectorTabs->addTab(tableTab, QString::fromUtf8("表格"));
    inspectorTabs->addTab(arrayGridTab, QString::fromUtf8("数组网格"));
    inspectorTabs->addTab(deviceTab, QString::fromUtf8("设备校准"));
    elementLayout->addWidget(inspectorTabs, 1);
    setWidget(elementPanel);
    connectPropertySignals();
    clearElementProperties();
    clearTableProperties();
}

void TemplateInspectorPanel::setElementProperties(const DesignerElementPropertyModel& model)
{
    QScopedValueRollback<bool> setting(m_settingElementProperties, true);
    m_elementModel = model;

    const auto hasElement = model.visible;
    const auto canEdit = hasElement && model.canEdit;
    const auto valuePlaceholder = hasElement
        ? model.valuePlaceholder
        : QString::fromUtf8("请选择固定文本或绑定字段");

    if (m_elementValueEdit != nullptr) {
        const QSignalBlocker blocker(m_elementValueEdit);
        m_elementValueEdit->setPlainText(hasElement ? model.value : QString());
        m_elementValueEdit->setPlaceholderText(valuePlaceholder);
        m_elementValueEdit->setEnabled(canEdit && model.canEditValue);
    }
    if (m_elementXSpin != nullptr) {
        m_elementXSpin->setValue(model.x);
        m_elementXSpin->setEnabled(canEdit);
    }
    if (m_elementYSpin != nullptr) {
        m_elementYSpin->setValue(model.y);
        m_elementYSpin->setEnabled(canEdit);
    }
    if (m_elementWidthSpin != nullptr) {
        m_elementWidthSpin->setValue(model.width);
        m_elementWidthSpin->setEnabled(canEdit);
    }
    if (m_elementHeightSpin != nullptr) {
        m_elementHeightSpin->setValue(model.height);
        m_elementHeightSpin->setEnabled(canEdit);
    }
    if (m_elementFontSizeSpin != nullptr) {
        m_elementFontSizeSpin->setValue(model.fontSizePt);
        m_elementFontSizeSpin->setEnabled(canEdit && model.supportsTextStyle);
    }
    if (m_elementRotationSpin != nullptr) {
        m_elementRotationSpin->setValue(model.rotationDegrees);
        m_elementRotationSpin->setEnabled(canEdit);
    }
    if (m_elementBoldCheck != nullptr) {
        m_elementBoldCheck->setChecked(model.bold);
        m_elementBoldCheck->setEnabled(canEdit && model.supportsTextStyle);
    }
    if (m_elementAutoFitFontCheck != nullptr) {
        m_elementAutoFitFontCheck->setChecked(model.autoFitFont);
        m_elementAutoFitFontCheck->setEnabled(canEdit && model.supportsAutoFitFont);
    }
    if (m_elementAutoFitMinFontSizeSpin != nullptr) {
        m_elementAutoFitMinFontSizeSpin->setValue(model.autoFitMinFontSizePt);
    }
    if (m_elementAutoFitMaxFontSizeSpin != nullptr) {
        m_elementAutoFitMaxFontSizeSpin->setValue(model.autoFitMaxFontSizePt);
    }
    if (m_elementVerticalTextCheck != nullptr) {
        m_elementVerticalTextCheck->setChecked(model.verticalText);
        m_elementVerticalTextCheck->setEnabled(canEdit && model.supportsVerticalText);
    }
    if (m_arrayGridDataPathEdit != nullptr) {
        m_arrayGridDataPathEdit->setText(model.dataPath);
        m_arrayGridDataPathEdit->setEnabled(canEdit && model.isArrayGrid);
    }
    if (m_arrayGridRowsSpin != nullptr) {
        m_arrayGridRowsSpin->setValue(model.arrayGridRows);
        m_arrayGridRowsSpin->setEnabled(canEdit && model.isArrayGrid);
    }
    if (m_arrayGridColumnsSpin != nullptr) {
        m_arrayGridColumnsSpin->setValue(model.arrayGridColumns);
        m_arrayGridColumnsSpin->setEnabled(canEdit && model.isArrayGrid);
    }
    if (m_arrayGridRowHeightSpin != nullptr) {
        m_arrayGridRowHeightSpin->setValue(model.arrayGridRowHeightMm);
        m_arrayGridRowHeightSpin->setEnabled(canEdit && model.isArrayGrid);
    }
    if (m_arrayGridCellTemplateEdit != nullptr) {
        const QSignalBlocker blocker(m_arrayGridCellTemplateEdit);
        m_arrayGridCellTemplateEdit->setPlainText(model.arrayGridCellTemplate);
        m_arrayGridCellTemplateEdit->setEnabled(canEdit && model.isArrayGrid);
    }
    if (m_arrayGridDrawBordersCheck != nullptr) {
        m_arrayGridDrawBordersCheck->setChecked(model.arrayGridDrawBorders);
        m_arrayGridDrawBordersCheck->setEnabled(canEdit && model.isArrayGrid);
    }
    if (m_applyElementPropertiesButton != nullptr) {
        m_applyElementPropertiesButton->setEnabled(canEdit);
    }
    if (m_applyArrayGridPropertiesButton != nullptr) {
        m_applyArrayGridPropertiesButton->setEnabled(canEdit && model.isArrayGrid);
    }
    updateAutoFitRangeEnabled();
}

DesignerElementPropertyModel TemplateInspectorPanel::elementProperties() const
{
    auto model = m_elementModel;
    if (m_elementValueEdit != nullptr) {
        model.value = m_elementValueEdit->toPlainText();
    }
    if (m_elementXSpin != nullptr) {
        model.x = m_elementXSpin->value();
    }
    if (m_elementYSpin != nullptr) {
        model.y = m_elementYSpin->value();
    }
    if (m_elementWidthSpin != nullptr) {
        model.width = m_elementWidthSpin->value();
    }
    if (m_elementHeightSpin != nullptr) {
        model.height = m_elementHeightSpin->value();
    }
    if (m_elementFontSizeSpin != nullptr) {
        model.fontSizePt = m_elementFontSizeSpin->value();
    }
    if (m_elementRotationSpin != nullptr) {
        model.rotationDegrees = m_elementRotationSpin->value();
    }
    if (m_elementBoldCheck != nullptr) {
        model.bold = m_elementBoldCheck->isChecked();
    }
    if (m_elementAutoFitFontCheck != nullptr) {
        model.autoFitFont = m_elementAutoFitFontCheck->isChecked();
    }
    if (m_elementAutoFitMinFontSizeSpin != nullptr) {
        model.autoFitMinFontSizePt = m_elementAutoFitMinFontSizeSpin->value();
    }
    if (m_elementAutoFitMaxFontSizeSpin != nullptr) {
        model.autoFitMaxFontSizePt = m_elementAutoFitMaxFontSizeSpin->value();
    }
    if (m_elementVerticalTextCheck != nullptr) {
        model.verticalText = m_elementVerticalTextCheck->isChecked();
    }
    if (m_arrayGridDataPathEdit != nullptr) {
        model.dataPath = m_arrayGridDataPathEdit->text().trimmed();
    }
    if (m_arrayGridRowsSpin != nullptr) {
        model.arrayGridRows = m_arrayGridRowsSpin->value();
    }
    if (m_arrayGridColumnsSpin != nullptr) {
        model.arrayGridColumns = m_arrayGridColumnsSpin->value();
    }
    if (m_arrayGridRowHeightSpin != nullptr) {
        model.arrayGridRowHeightMm = m_arrayGridRowHeightSpin->value();
    }
    if (m_arrayGridCellTemplateEdit != nullptr) {
        model.arrayGridCellTemplate = m_arrayGridCellTemplateEdit->toPlainText();
    }
    if (m_arrayGridDrawBordersCheck != nullptr) {
        model.arrayGridDrawBorders = m_arrayGridDrawBordersCheck->isChecked();
    }
    return model;
}

void TemplateInspectorPanel::clearElementProperties()
{
    DesignerElementPropertyModel model;
    model.valuePlaceholder = QString::fromUtf8("请选择固定文本或绑定字段");
    setElementProperties(model);
}

void TemplateInspectorPanel::setTableProperties(const DesignerTablePropertyModel& model)
{
    QScopedValueRollback<bool> setting(m_settingTableProperties, true);
    m_tableModel = model;
    m_tableColumnsTextEdited = false;
    const auto canEdit = model.visible && model.canEdit;

    if (m_tableDisplayNameEdit != nullptr) {
        m_tableDisplayNameEdit->setText(model.visible ? model.displayName : QString());
        m_tableDisplayNameEdit->setEnabled(canEdit);
    }
    if (m_tableDataPathEdit != nullptr) {
        m_tableDataPathEdit->setText(model.visible ? model.dataPath : QString());
        m_tableDataPathEdit->setEnabled(canEdit);
    }
    if (m_tableXSpin != nullptr) {
        m_tableXSpin->setValue(model.x);
        m_tableXSpin->setEnabled(canEdit);
    }
    if (m_tableYSpin != nullptr) {
        m_tableYSpin->setValue(model.y);
        m_tableYSpin->setEnabled(canEdit);
    }
    if (m_tableWidthSpin != nullptr) {
        m_tableWidthSpin->setValue(model.width);
        m_tableWidthSpin->setEnabled(canEdit);
    }
    if (m_tableHeightSpin != nullptr) {
        m_tableHeightSpin->setValue(model.height);
        m_tableHeightSpin->setEnabled(canEdit);
    }
    if (m_tableHeaderHeightSpin != nullptr) {
        m_tableHeaderHeightSpin->setValue(model.headerRowHeightMm);
        m_tableHeaderHeightSpin->setEnabled(canEdit);
    }
    if (m_tableDetailHeightSpin != nullptr) {
        m_tableDetailHeightSpin->setValue(model.detailRowHeightMm);
        m_tableDetailHeightSpin->setEnabled(canEdit);
    }
    if (m_tableRepeatHeaderCheck != nullptr) {
        m_tableRepeatHeaderCheck->setChecked(model.repeatHeaderOnPage);
        m_tableRepeatHeaderCheck->setEnabled(canEdit);
    }
    if (m_tableDrawBordersCheck != nullptr) {
        m_tableDrawBordersCheck->setChecked(model.drawBorders);
        m_tableDrawBordersCheck->setEnabled(canEdit);
    }
    if (m_tableColumnsEdit != nullptr) {
        m_tableColumnsEdit->setText(model.visible ? model.columnsText : QString());
        m_tableColumnsEdit->setEnabled(canEdit);
    }
    if (m_tableColumnEditor != nullptr) {
        m_tableColumnEditor->setEditable(canEdit);
        m_tableColumnEditor->setColumns(model.visible ? model.columns : QList<DesignerTableColumnModel>{});
    }
    if (m_tableAdvancedEditor != nullptr) {
        m_tableAdvancedEditor->setEditable(canEdit);
        m_tableAdvancedEditor->setProperties(model.visible ? model : DesignerTablePropertyModel{});
    }
    if (m_applyTablePropertiesButton != nullptr) {
        m_applyTablePropertiesButton->setEnabled(canEdit);
    }
}

DesignerTablePropertyModel TemplateInspectorPanel::tableProperties() const
{
    auto model = m_tableModel;
    if (m_tableDisplayNameEdit != nullptr) {
        model.displayName = m_tableDisplayNameEdit->text().trimmed();
    }
    if (m_tableDataPathEdit != nullptr) {
        model.dataPath = m_tableDataPathEdit->text().trimmed();
    }
    if (m_tableXSpin != nullptr) {
        model.x = m_tableXSpin->value();
    }
    if (m_tableYSpin != nullptr) {
        model.y = m_tableYSpin->value();
    }
    if (m_tableWidthSpin != nullptr) {
        model.width = m_tableWidthSpin->value();
    }
    if (m_tableHeightSpin != nullptr) {
        model.height = m_tableHeightSpin->value();
    }
    if (m_tableHeaderHeightSpin != nullptr) {
        model.headerRowHeightMm = m_tableHeaderHeightSpin->value();
    }
    if (m_tableDetailHeightSpin != nullptr) {
        model.detailRowHeightMm = m_tableDetailHeightSpin->value();
    }
    if (m_tableRepeatHeaderCheck != nullptr) {
        model.repeatHeaderOnPage = m_tableRepeatHeaderCheck->isChecked();
    }
    if (m_tableDrawBordersCheck != nullptr) {
        model.drawBorders = m_tableDrawBordersCheck->isChecked();
    }
    if (m_tableColumnsEdit != nullptr) {
        model.columnsText = m_tableColumnsEdit->text();
    }
    if (m_tableColumnEditor != nullptr) {
        model.columns = m_tableColumnEditor->columns();
        model.preferStructuredColumns = !m_tableColumnsTextEdited && !model.columns.isEmpty();
    }
    if (m_tableAdvancedEditor != nullptr) {
        const auto advancedModel = m_tableAdvancedEditor->tableProperties();
        model.rowBands = advancedModel.rowBands;
        model.cellStyles = advancedModel.cellStyles;
        model.cellTemplates = advancedModel.cellTemplates;
        model.mergeRegions = advancedModel.mergeRegions;
    }
    return model;
}

void TemplateInspectorPanel::clearTableProperties()
{
    setTableProperties(DesignerTablePropertyModel{});
}

bool TemplateInspectorPanel::isElementPropertyEditor(QObject* watched) const
{
    const auto* watchedWidget = qobject_cast<const QWidget*>(watched);
    const QList<QWidget*> editors{
        m_elementValueEdit,
        m_elementXSpin,
        m_elementYSpin,
        m_elementWidthSpin,
        m_elementHeightSpin,
        m_elementFontSizeSpin,
        m_elementRotationSpin,
        m_elementBoldCheck,
        m_elementAutoFitFontCheck,
        m_elementAutoFitMinFontSizeSpin,
        m_elementAutoFitMaxFontSizeSpin,
        m_elementVerticalTextCheck,
        m_arrayGridDataPathEdit,
        m_arrayGridRowsSpin,
        m_arrayGridColumnsSpin,
        m_arrayGridRowHeightSpin,
        m_arrayGridCellTemplateEdit,
        m_arrayGridDrawBordersCheck,
    };
    return std::any_of(editors.cbegin(), editors.cend(), [watchedWidget](const auto* editor) {
        return ownsEditor(editor, watchedWidget);
    });
}

bool TemplateInspectorPanel::isTablePropertyEditor(QObject* watched) const
{
    const auto* watchedWidget = qobject_cast<const QWidget*>(watched);
    const QList<QWidget*> editors{
        m_tableDisplayNameEdit,
        m_tableDataPathEdit,
        m_tableXSpin,
        m_tableYSpin,
        m_tableWidthSpin,
        m_tableHeightSpin,
        m_tableHeaderHeightSpin,
        m_tableDetailHeightSpin,
              m_tableRepeatHeaderCheck,
              m_tableDrawBordersCheck,
              m_tableColumnsEdit,
              m_tableColumnEditor,
              m_tableAdvancedEditor,
    };
    return std::any_of(editors.cbegin(), editors.cend(), [watchedWidget](const auto* editor) {
        return ownsEditor(editor, watchedWidget);
    });
}

void TemplateInspectorPanel::installPropertyEditorEventFilter(QObject* filterTarget)
{
    for (auto* editor : QList<QWidget*>{
             m_elementValueEdit,
             m_elementXSpin,
             m_elementYSpin,
             m_elementWidthSpin,
             m_elementHeightSpin,
             m_elementFontSizeSpin,
             m_elementRotationSpin,
             m_elementBoldCheck,
             m_elementAutoFitFontCheck,
             m_elementAutoFitMinFontSizeSpin,
             m_elementAutoFitMaxFontSizeSpin,
             m_elementVerticalTextCheck,
             m_arrayGridDataPathEdit,
             m_arrayGridRowsSpin,
             m_arrayGridColumnsSpin,
             m_arrayGridRowHeightSpin,
             m_arrayGridCellTemplateEdit,
             m_arrayGridDrawBordersCheck,
             m_tableDisplayNameEdit,
             m_tableDataPathEdit,
             m_tableXSpin,
             m_tableYSpin,
             m_tableWidthSpin,
             m_tableHeightSpin,
             m_tableHeaderHeightSpin,
             m_tableDetailHeightSpin,
             m_tableRepeatHeaderCheck,
              m_tableDrawBordersCheck,
              m_tableColumnsEdit,
              m_tableColumnEditor,
              m_tableAdvancedEditor,
          }) {
        installEditorFilter(filterTarget, editor);
    }
}

void TemplateInspectorPanel::connectPropertySignals()
{
    const auto emitElementTextEdited = [this] {
        if (!m_settingElementProperties) {
            emit elementPropertiesEdited(TextAutoApplyDelayMs);
        }
    };
    const auto emitElementControlEdited = [this] {
        if (!m_settingElementProperties) {
            updateAutoFitRangeEnabled();
            emit elementPropertiesEdited(ControlAutoApplyDelayMs);
        }
    };
    const auto emitTableTextEdited = [this] {
        if (!m_settingTableProperties) {
            emit tablePropertiesEdited(TextAutoApplyDelayMs);
        }
    };
    const auto emitTableControlEdited = [this] {
        if (!m_settingTableProperties) {
            emit tablePropertiesEdited(ControlAutoApplyDelayMs);
        }
    };

    connect(m_applyElementPropertiesButton, &QPushButton::clicked, this, [this] { emit elementPropertiesApplyRequested(); });
    connect(m_applyArrayGridPropertiesButton, &QPushButton::clicked, this, [this] { emit elementPropertiesApplyRequested(); });
    connect(m_applyTablePropertiesButton, &QPushButton::clicked, this, [this] { emit tablePropertiesApplyRequested(); });

    connect(m_elementValueEdit, &QPlainTextEdit::textChanged, this, emitElementTextEdited);
    connect(m_elementXSpin, &QDoubleSpinBox::valueChanged, this, emitElementControlEdited);
    connect(m_elementYSpin, &QDoubleSpinBox::valueChanged, this, emitElementControlEdited);
    connect(m_elementWidthSpin, &QDoubleSpinBox::valueChanged, this, emitElementControlEdited);
    connect(m_elementHeightSpin, &QDoubleSpinBox::valueChanged, this, emitElementControlEdited);
    connect(m_elementFontSizeSpin, &QDoubleSpinBox::valueChanged, this, emitElementControlEdited);
    connect(m_elementRotationSpin, &QDoubleSpinBox::valueChanged, this, emitElementControlEdited);
    connect(m_elementBoldCheck, &QCheckBox::toggled, this, emitElementControlEdited);
    connect(m_elementAutoFitFontCheck, &QCheckBox::toggled, this, emitElementControlEdited);
    connect(m_elementAutoFitMinFontSizeSpin, &QDoubleSpinBox::valueChanged, this, emitElementControlEdited);
    connect(m_elementAutoFitMaxFontSizeSpin, &QDoubleSpinBox::valueChanged, this, emitElementControlEdited);
    connect(m_elementVerticalTextCheck, &QCheckBox::toggled, this, emitElementControlEdited);
    connect(m_arrayGridDataPathEdit, &QLineEdit::textChanged, this, emitElementTextEdited);
    connect(m_arrayGridRowsSpin, &QSpinBox::valueChanged, this, emitElementControlEdited);
    connect(m_arrayGridColumnsSpin, &QSpinBox::valueChanged, this, emitElementControlEdited);
    connect(m_arrayGridRowHeightSpin, &QDoubleSpinBox::valueChanged, this, emitElementControlEdited);
    connect(m_arrayGridCellTemplateEdit, &QPlainTextEdit::textChanged, this, emitElementTextEdited);
    connect(m_arrayGridDrawBordersCheck, &QCheckBox::toggled, this, emitElementControlEdited);
    connect(m_arrayGridDataPathEdit, &QLineEdit::editingFinished, this, [this] { emit elementPropertiesEditingFinished(); });

    connect(m_tableDisplayNameEdit, &QLineEdit::textChanged, this, emitTableTextEdited);
    connect(m_tableDataPathEdit, &QLineEdit::textChanged, this, emitTableTextEdited);
    connect(m_tableXSpin, &QDoubleSpinBox::valueChanged, this, emitTableControlEdited);
    connect(m_tableYSpin, &QDoubleSpinBox::valueChanged, this, emitTableControlEdited);
    connect(m_tableWidthSpin, &QDoubleSpinBox::valueChanged, this, emitTableControlEdited);
    connect(m_tableHeightSpin, &QDoubleSpinBox::valueChanged, this, emitTableControlEdited);
    connect(m_tableHeaderHeightSpin, &QDoubleSpinBox::valueChanged, this, emitTableControlEdited);
    connect(m_tableDetailHeightSpin, &QDoubleSpinBox::valueChanged, this, emitTableControlEdited);
    connect(m_tableRepeatHeaderCheck, &QCheckBox::toggled, this, emitTableControlEdited);
    connect(m_tableDrawBordersCheck, &QCheckBox::toggled, this, emitTableControlEdited);
    connect(m_tableColumnsEdit, &QLineEdit::textChanged, this, [this] {
        if (!m_settingTableProperties) {
            m_tableColumnsTextEdited = true;
            emit tablePropertiesEdited(TextAutoApplyDelayMs);
        }
    });
    connect(m_tableColumnEditor, &TableColumnEditorPanel::columnsEdited, this, [this] {
        if (m_settingTableProperties) {
            return;
        }
        m_tableColumnsTextEdited = false;
        if (m_tableColumnsEdit != nullptr && m_tableColumnEditor != nullptr) {
            const QSignalBlocker blocker(m_tableColumnsEdit);
            m_tableColumnsEdit->setText(TableColumnTextCodec::format(tableColumnsFromDesignerModels(m_tableColumnEditor->columns())));
        }
        emit tablePropertiesEdited(ControlAutoApplyDelayMs);
    });
    connect(m_tableAdvancedEditor, &TableAdvancedEditorPanel::advancedPropertiesEdited, this, [this] {
        if (!m_settingTableProperties) {
            emit tablePropertiesEdited(ControlAutoApplyDelayMs);
        }
    });
    connect(m_tableDisplayNameEdit, &QLineEdit::editingFinished, this, [this] { emit tablePropertiesEditingFinished(); });
    connect(m_tableDataPathEdit, &QLineEdit::editingFinished, this, [this] { emit tablePropertiesEditingFinished(); });
    connect(m_tableColumnsEdit, &QLineEdit::editingFinished, this, [this] { emit tablePropertiesEditingFinished(); });
}

void TemplateInspectorPanel::updateAutoFitRangeEnabled()
{
    const auto canEdit = m_elementModel.visible && m_elementModel.canEdit && m_elementModel.supportsAutoFitFont;
    const auto enabled = canEdit && m_elementAutoFitFontCheck != nullptr && m_elementAutoFitFontCheck->isChecked();
    if (m_elementAutoFitMinFontSizeSpin != nullptr) {
        m_elementAutoFitMinFontSizeSpin->setEnabled(enabled);
    }
    if (m_elementAutoFitMaxFontSizeSpin != nullptr) {
        m_elementAutoFitMaxFontSizeSpin->setEnabled(enabled);
    }
}

QListWidget* TemplateInspectorPanel::elementList() const { return m_elementList; }
QAction* TemplateInspectorPanel::renameElementAction() const { return m_renameElementAction; }
QAction* TemplateInspectorPanel::deleteElementAction() const { return m_deleteElementAction; }
QAction* TemplateInspectorPanel::lockElementAction() const { return m_lockElementAction; }
QAction* TemplateInspectorPanel::hideElementAction() const { return m_hideElementAction; }
QAction* TemplateInspectorPanel::moveElementUpAction() const { return m_moveElementUpAction; }
QAction* TemplateInspectorPanel::moveElementDownAction() const { return m_moveElementDownAction; }

QPushButton* TemplateInspectorPanel::addFixedTextButton() const { return m_addFixedTextButton; }
QPushButton* TemplateInspectorPanel::addBoundFieldButton() const { return m_addBoundFieldButton; }
QPushButton* TemplateInspectorPanel::addQrCodeButton() const { return m_addQrCodeButton; }
QPushButton* TemplateInspectorPanel::addRectangleButton() const { return m_addRectangleButton; }
QPushButton* TemplateInspectorPanel::addTableButton() const { return m_addTableButton; }
QPushButton* TemplateInspectorPanel::addArrayGridButton() const { return m_addArrayGridButton; }
QPushButton* TemplateInspectorPanel::deleteElementButton() const { return m_deleteElementButton; }
QPushButton* TemplateInspectorPanel::lockElementButton() const { return m_lockElementButton; }
QPushButton* TemplateInspectorPanel::hideElementButton() const { return m_hideElementButton; }
QPushButton* TemplateInspectorPanel::moveElementUpButton() const { return m_moveElementUpButton; }
QPushButton* TemplateInspectorPanel::moveElementDownButton() const { return m_moveElementDownButton; }

QPlainTextEdit* TemplateInspectorPanel::elementValueEdit() const { return m_elementValueEdit; }
QDoubleSpinBox* TemplateInspectorPanel::elementXSpin() const { return m_elementXSpin; }
QDoubleSpinBox* TemplateInspectorPanel::elementYSpin() const { return m_elementYSpin; }
QDoubleSpinBox* TemplateInspectorPanel::elementWidthSpin() const { return m_elementWidthSpin; }
QDoubleSpinBox* TemplateInspectorPanel::elementHeightSpin() const { return m_elementHeightSpin; }
QDoubleSpinBox* TemplateInspectorPanel::elementFontSizeSpin() const { return m_elementFontSizeSpin; }
QDoubleSpinBox* TemplateInspectorPanel::elementRotationSpin() const { return m_elementRotationSpin; }
QCheckBox* TemplateInspectorPanel::elementBoldCheck() const { return m_elementBoldCheck; }
QCheckBox* TemplateInspectorPanel::elementAutoFitFontCheck() const { return m_elementAutoFitFontCheck; }
QDoubleSpinBox* TemplateInspectorPanel::elementAutoFitMinFontSizeSpin() const { return m_elementAutoFitMinFontSizeSpin; }
QDoubleSpinBox* TemplateInspectorPanel::elementAutoFitMaxFontSizeSpin() const { return m_elementAutoFitMaxFontSizeSpin; }
QCheckBox* TemplateInspectorPanel::elementVerticalTextCheck() const { return m_elementVerticalTextCheck; }
QPushButton* TemplateInspectorPanel::applyElementPropertiesButton() const { return m_applyElementPropertiesButton; }

QLineEdit* TemplateInspectorPanel::arrayGridDataPathEdit() const { return m_arrayGridDataPathEdit; }
QSpinBox* TemplateInspectorPanel::arrayGridRowsSpin() const { return m_arrayGridRowsSpin; }
QSpinBox* TemplateInspectorPanel::arrayGridColumnsSpin() const { return m_arrayGridColumnsSpin; }
QDoubleSpinBox* TemplateInspectorPanel::arrayGridRowHeightSpin() const { return m_arrayGridRowHeightSpin; }
QPlainTextEdit* TemplateInspectorPanel::arrayGridCellTemplateEdit() const { return m_arrayGridCellTemplateEdit; }
QCheckBox* TemplateInspectorPanel::arrayGridDrawBordersCheck() const { return m_arrayGridDrawBordersCheck; }
QPushButton* TemplateInspectorPanel::applyArrayGridPropertiesButton() const { return m_applyArrayGridPropertiesButton; }

QLineEdit* TemplateInspectorPanel::tableDisplayNameEdit() const { return m_tableDisplayNameEdit; }
QLineEdit* TemplateInspectorPanel::tableDataPathEdit() const { return m_tableDataPathEdit; }
QDoubleSpinBox* TemplateInspectorPanel::tableXSpin() const { return m_tableXSpin; }
QDoubleSpinBox* TemplateInspectorPanel::tableYSpin() const { return m_tableYSpin; }
QDoubleSpinBox* TemplateInspectorPanel::tableWidthSpin() const { return m_tableWidthSpin; }
QDoubleSpinBox* TemplateInspectorPanel::tableHeightSpin() const { return m_tableHeightSpin; }
QDoubleSpinBox* TemplateInspectorPanel::tableHeaderHeightSpin() const { return m_tableHeaderHeightSpin; }
QDoubleSpinBox* TemplateInspectorPanel::tableDetailHeightSpin() const { return m_tableDetailHeightSpin; }
QCheckBox* TemplateInspectorPanel::tableRepeatHeaderCheck() const { return m_tableRepeatHeaderCheck; }
QCheckBox* TemplateInspectorPanel::tableDrawBordersCheck() const { return m_tableDrawBordersCheck; }
QLineEdit* TemplateInspectorPanel::tableColumnsEdit() const { return m_tableColumnsEdit; }
TableColumnEditorPanel* TemplateInspectorPanel::tableColumnEditor() const { return m_tableColumnEditor; }
TableAdvancedEditorPanel* TemplateInspectorPanel::tableAdvancedEditor() const { return m_tableAdvancedEditor; }
QPushButton* TemplateInspectorPanel::applyTablePropertiesButton() const { return m_applyTablePropertiesButton; }

QLineEdit* TemplateInspectorPanel::deviceProfilePrinterEdit() const { return m_deviceProfilePrinterEdit; }
QDoubleSpinBox* TemplateInspectorPanel::deviceProfileDpiSpin() const { return m_deviceProfileDpiSpin; }
QDoubleSpinBox* TemplateInspectorPanel::deviceProfileScaleXSpin() const { return m_deviceProfileScaleXSpin; }
QDoubleSpinBox* TemplateInspectorPanel::deviceProfileScaleYSpin() const { return m_deviceProfileScaleYSpin; }
QDoubleSpinBox* TemplateInspectorPanel::deviceProfileOffsetXSpin() const { return m_deviceProfileOffsetXSpin; }
QDoubleSpinBox* TemplateInspectorPanel::deviceProfileOffsetYSpin() const { return m_deviceProfileOffsetYSpin; }
QDoubleSpinBox* TemplateInspectorPanel::deviceCalibrationExpectedWidthSpin() const { return m_deviceCalibrationExpectedWidthSpin; }
QDoubleSpinBox* TemplateInspectorPanel::deviceCalibrationActualWidthSpin() const { return m_deviceCalibrationActualWidthSpin; }
QDoubleSpinBox* TemplateInspectorPanel::deviceCalibrationExpectedHeightSpin() const { return m_deviceCalibrationExpectedHeightSpin; }
QDoubleSpinBox* TemplateInspectorPanel::deviceCalibrationActualHeightSpin() const { return m_deviceCalibrationActualHeightSpin; }
QPushButton* TemplateInspectorPanel::calculateDeviceCalibrationButton() const { return m_calculateDeviceCalibrationButton; }
QPushButton* TemplateInspectorPanel::saveDeviceProfileButton() const { return m_saveDeviceProfileButton; }

} // 命名空间 sleekpr::app

#include "sleekpr/app/PaperSpecManagerWindow.h"

#include "sleekpr/core/templates/PaperSpecStore.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

#include <algorithm>
#include <utility>

namespace sleekpr::app {

namespace {

QDoubleSpinBox* millimeterSpinBox(double minimum = 0.0, double maximum = 1000.0)
{
    auto* spinBox = new QDoubleSpinBox;
    spinBox->setRange(minimum, maximum);
    spinBox->setDecimals(2);
    spinBox->setSingleStep(0.10);
    spinBox->setSuffix(QString::fromUtf8(" mm"));
    return spinBox;
}

QDoubleSpinBox* dpiSpinBox()
{
    auto* spinBox = new QDoubleSpinBox;
    spinBox->setRange(1.0, 2400.0);
    spinBox->setDecimals(0);
    spinBox->setSingleStep(1.0);
    spinBox->setSuffix(QString::fromUtf8(" DPI"));
    return spinBox;
}

QSpinBox* countSpinBox()
{
    auto* spinBox = new QSpinBox;
    spinBox->setRange(1, 1000);
    spinBox->setSingleStep(1);
    return spinBox;
}

QString kindData(sleekpr::core::PaperSpecKind kind)
{
    switch (kind) {
    case sleekpr::core::PaperSpecKind::Label:
        return QStringLiteral("label");
    case sleekpr::core::PaperSpecKind::Roll:
        return QStringLiteral("roll");
    case sleekpr::core::PaperSpecKind::Sheet:
        return QStringLiteral("sheet");
    }
    return QStringLiteral("label");
}

sleekpr::core::PaperSpecKind kindFromData(const QString& value)
{
    if (value == QStringLiteral("roll")) {
        return sleekpr::core::PaperSpecKind::Roll;
    }
    if (value == QStringLiteral("sheet")) {
        return sleekpr::core::PaperSpecKind::Sheet;
    }
    return sleekpr::core::PaperSpecKind::Label;
}

QString orientationData(sleekpr::core::PaperOrientation orientation)
{
    switch (orientation) {
    case sleekpr::core::PaperOrientation::Portrait:
        return QStringLiteral("portrait");
    case sleekpr::core::PaperOrientation::Landscape:
        return QStringLiteral("landscape");
    }
    return QStringLiteral("portrait");
}

sleekpr::core::PaperOrientation orientationFromData(const QString& value)
{
    if (value == QStringLiteral("landscape")) {
        return sleekpr::core::PaperOrientation::Landscape;
    }
    return sleekpr::core::PaperOrientation::Portrait;
}

QString specListText(const sleekpr::core::PaperSpec& spec)
{
    const auto name = spec.name.trimmed().isEmpty() ? spec.id : spec.name.trimmed();
    return QString::fromUtf8("%1 (%2x%3 mm)").arg(name).arg(spec.widthMm, 0, 'f', 2).arg(spec.heightMm, 0, 'f', 2);
}

} // 匿名命名空间

PaperSpecManagerWindow::PaperSpecManagerWindow(
    QString paperSpecFilePath,
    PaperSpecsChangedCallback onPaperSpecsChanged,
    QWidget* parent)
    : QWidget(parent)
    , m_paperSpecFilePath(std::move(paperSpecFilePath))
    , m_onPaperSpecsChanged(std::move(onPaperSpecsChanged))
{
    setWindowTitle(QString::fromUtf8("纸张规格管理"));
    resize(760, 520);
    buildUi();
    reloadFromDisk();
}

void PaperSpecManagerWindow::reloadFromDisk()
{
    refreshList(currentSpecId());
}

void PaperSpecManagerWindow::buildUi()
{
    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(14, 14, 14, 14);
    rootLayout->setSpacing(10);

    auto* contentLayout = new QHBoxLayout;
    m_specList = new QListWidget(this);
    m_specList->setObjectName(QStringLiteral("paperSpecList"));
    m_specList->setMinimumWidth(220);
    contentLayout->addWidget(m_specList, 0);

    auto* formContainer = new QWidget(this);
    auto* formLayout = new QFormLayout(formContainer);
    formLayout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);

    m_idEdit = new QLineEdit(formContainer);
    m_idEdit->setObjectName(QStringLiteral("paperSpecIdEdit"));
    m_nameEdit = new QLineEdit(formContainer);
    m_nameEdit->setObjectName(QStringLiteral("paperSpecNameEdit"));
    m_kindCombo = new QComboBox(formContainer);
    m_kindCombo->setObjectName(QStringLiteral("paperSpecKindCombo"));
    m_kindCombo->addItem(QString::fromUtf8("标签纸"), QStringLiteral("label"));
    m_kindCombo->addItem(QString::fromUtf8("卷纸"), QStringLiteral("roll"));
    m_kindCombo->addItem(QString::fromUtf8("单页纸"), QStringLiteral("sheet"));
    m_orientationCombo = new QComboBox(formContainer);
    m_orientationCombo->setObjectName(QStringLiteral("paperSpecOrientationCombo"));
    m_orientationCombo->addItem(QString::fromUtf8("纵向"), QStringLiteral("portrait"));
    m_orientationCombo->addItem(QString::fromUtf8("横向"), QStringLiteral("landscape"));

    m_widthSpin = millimeterSpinBox(0.1, 2000.0);
    m_widthSpin->setObjectName(QStringLiteral("paperSpecWidthSpin"));
    m_heightSpin = millimeterSpinBox(0.1, 2000.0);
    m_heightSpin->setObjectName(QStringLiteral("paperSpecHeightSpin"));
    m_marginLeftSpin = millimeterSpinBox();
    m_marginLeftSpin->setObjectName(QStringLiteral("paperSpecMarginLeftSpin"));
    m_marginTopSpin = millimeterSpinBox();
    m_marginTopSpin->setObjectName(QStringLiteral("paperSpecMarginTopSpin"));
    m_marginRightSpin = millimeterSpinBox();
    m_marginRightSpin->setObjectName(QStringLiteral("paperSpecMarginRightSpin"));
    m_marginBottomSpin = millimeterSpinBox();
    m_marginBottomSpin->setObjectName(QStringLiteral("paperSpecMarginBottomSpin"));
    m_dpiSpin = dpiSpinBox();
    m_dpiSpin->setObjectName(QStringLiteral("paperSpecDpiSpin"));

    m_gridEnabledCheck = new QCheckBox(QString::fromUtf8("启用多列标签网格"), formContainer);
    m_gridEnabledCheck->setObjectName(QStringLiteral("paperSpecGridEnabledCheck"));
    m_gridRowsSpin = countSpinBox();
    m_gridRowsSpin->setObjectName(QStringLiteral("paperSpecGridRowsSpin"));
    m_gridColumnsSpin = countSpinBox();
    m_gridColumnsSpin->setObjectName(QStringLiteral("paperSpecGridColumnsSpin"));
    m_gridHorizontalGapSpin = millimeterSpinBox();
    m_gridHorizontalGapSpin->setObjectName(QStringLiteral("paperSpecGridHorizontalGapSpin"));
    m_gridVerticalGapSpin = millimeterSpinBox();
    m_gridVerticalGapSpin->setObjectName(QStringLiteral("paperSpecGridVerticalGapSpin"));

    formLayout->addRow(QString::fromUtf8("ID"), m_idEdit);
    formLayout->addRow(QString::fromUtf8("名称"), m_nameEdit);
    formLayout->addRow(QString::fromUtf8("类型"), m_kindCombo);
    formLayout->addRow(QString::fromUtf8("方向"), m_orientationCombo);
    formLayout->addRow(QString::fromUtf8("宽度"), m_widthSpin);
    formLayout->addRow(QString::fromUtf8("高度"), m_heightSpin);
    formLayout->addRow(QString::fromUtf8("左边距"), m_marginLeftSpin);
    formLayout->addRow(QString::fromUtf8("上边距"), m_marginTopSpin);
    formLayout->addRow(QString::fromUtf8("右边距"), m_marginRightSpin);
    formLayout->addRow(QString::fromUtf8("下边距"), m_marginBottomSpin);
    formLayout->addRow(QString::fromUtf8("默认 DPI"), m_dpiSpin);
    formLayout->addRow(QString(), m_gridEnabledCheck);
    formLayout->addRow(QString::fromUtf8("网格行数"), m_gridRowsSpin);
    formLayout->addRow(QString::fromUtf8("网格列数"), m_gridColumnsSpin);
    formLayout->addRow(QString::fromUtf8("横向间距"), m_gridHorizontalGapSpin);
    formLayout->addRow(QString::fromUtf8("纵向间距"), m_gridVerticalGapSpin);
    contentLayout->addWidget(formContainer, 1);
    rootLayout->addLayout(contentLayout, 1);

    m_statusLabel = new QLabel(this);
    m_statusLabel->setObjectName(QStringLiteral("paperSpecStatusLabel"));
    rootLayout->addWidget(m_statusLabel);

    auto* buttonLayout = new QHBoxLayout;
    auto* addButton = new QPushButton(QString::fromUtf8("新增"), this);
    addButton->setObjectName(QStringLiteral("addPaperSpecButton"));
    m_saveButton = new QPushButton(QString::fromUtf8("保存"), this);
    m_saveButton->setObjectName(QStringLiteral("savePaperSpecButton"));
    m_saveButton->setProperty("buttonRole", QStringLiteral("primary"));
    m_deleteButton = new QPushButton(QString::fromUtf8("删除"), this);
    m_deleteButton->setObjectName(QStringLiteral("deletePaperSpecButton"));
    m_deleteButton->setProperty("buttonRole", QStringLiteral("danger"));
    auto* reloadButton = new QPushButton(QString::fromUtf8("重新加载"), this);
    reloadButton->setObjectName(QStringLiteral("reloadPaperSpecsButton"));
    auto* closeButton = new QPushButton(QString::fromUtf8("关闭"), this);
    buttonLayout->addWidget(addButton);
    buttonLayout->addWidget(m_saveButton);
    buttonLayout->addWidget(m_deleteButton);
    buttonLayout->addWidget(reloadButton);
    buttonLayout->addStretch();
    buttonLayout->addWidget(closeButton);
    rootLayout->addLayout(buttonLayout);

    connect(addButton, &QPushButton::clicked, this, [this] { startNewSpec(); });
    connect(m_saveButton, &QPushButton::clicked, this, [this] { saveCurrentSpec(); });
    connect(m_deleteButton, &QPushButton::clicked, this, [this] { deleteCurrentSpec(); });
    connect(reloadButton, &QPushButton::clicked, this, [this] { reloadFromDisk(); });
    connect(closeButton, &QPushButton::clicked, this, [this] { close(); });
    connect(m_specList, &QListWidget::currentRowChanged, this, [this](int) {
        if (!m_loadingForm) {
            loadSelectedSpec();
        }
    });
}

void PaperSpecManagerWindow::refreshList(const QString& selectedId)
{
    QString errorMessage;
    const auto specs = sleekpr::core::PaperSpecStore(m_paperSpecFilePath).paperSpecs(&errorMessage);

    m_loadingForm = true;
    m_specList->clear();
    for (const auto& spec : specs) {
        auto* item = new QListWidgetItem(specListText(spec), m_specList);
        item->setData(Qt::UserRole, spec.id);
    }
    m_loadingForm = false;

    int selectedRow = -1;
    for (int row = 0; row < m_specList->count(); ++row) {
        if (m_specList->item(row)->data(Qt::UserRole).toString() == selectedId) {
            selectedRow = row;
            break;
        }
    }
    if (selectedRow < 0 && m_specList->count() > 0) {
        selectedRow = 0;
    }

    if (selectedRow >= 0) {
        m_specList->setCurrentRow(selectedRow);
        loadSelectedSpec();
    } else {
        startNewSpec();
    }

    showStatus(errorMessage.trimmed().isEmpty() ? QString::fromUtf8("已加载纸张规格") : errorMessage);
}

void PaperSpecManagerWindow::loadSelectedSpec()
{
    const auto id = currentSpecId();
    if (id.isEmpty()) {
        setFormEnabled(false);
        return;
    }

    QString errorMessage;
    const auto spec = sleekpr::core::PaperSpecStore(m_paperSpecFilePath).loadPaperSpec(id, &errorMessage);
    if (!spec.has_value()) {
        showStatus(errorMessage);
        setFormEnabled(false);
        return;
    }

    loadSpecToForm(spec.value());
    showStatus(QString::fromUtf8("正在编辑：%1").arg(spec->name.trimmed().isEmpty() ? spec->id : spec->name));
}

void PaperSpecManagerWindow::startNewSpec()
{
    sleekpr::core::PaperSpec spec;
    spec.id.clear();
    spec.name.clear();
    spec.widthMm = 80.0;
    spec.heightMm = 30.0;
    spec.defaultDpi = 300.0;
    m_specList->clearSelection();
    loadSpecToForm(spec);
    showStatus(QString::fromUtf8("正在新增纸张规格"));
}

void PaperSpecManagerWindow::saveCurrentSpec()
{
    const auto spec = specFromForm();
    QString errorMessage;
    // 管理界面只负责编排表单，真实持久化复用 core 的 PaperSpecStore，确保 HTTP 和 Qt UI 共用同一份 paper-specs.json。
    if (!sleekpr::core::PaperSpecStore(m_paperSpecFilePath).savePaperSpec(spec, &errorMessage)) {
        showStatus(errorMessage);
        return;
    }

    refreshList(spec.id);
    notifySpecsChanged();
    showStatus(QString::fromUtf8("已保存纸张规格：%1").arg(spec.id));
}

void PaperSpecManagerWindow::deleteCurrentSpec()
{
    const auto id = currentSpecId().isEmpty() ? m_idEdit->text().trimmed() : currentSpecId();
    if (id.isEmpty()) {
        return;
    }

    QString errorMessage;
    // 删除同样走 PaperSpecStore，让原子写入和 id 校验规则集中在 core 层维护。
    if (!sleekpr::core::PaperSpecStore(m_paperSpecFilePath).removePaperSpec(id, &errorMessage)) {
        showStatus(errorMessage);
        return;
    }

    refreshList();
    notifySpecsChanged();
    showStatus(QString::fromUtf8("已删除纸张规格：%1").arg(id));
}

void PaperSpecManagerWindow::loadSpecToForm(const sleekpr::core::PaperSpec& spec)
{
    m_loadingForm = true;
    m_idEdit->setText(spec.id);
    m_nameEdit->setText(spec.name);
    m_kindCombo->setCurrentIndex(std::max(0, m_kindCombo->findData(kindData(spec.kind))));
    m_orientationCombo->setCurrentIndex(std::max(0, m_orientationCombo->findData(orientationData(spec.orientation))));
    m_widthSpin->setValue(spec.widthMm);
    m_heightSpin->setValue(spec.heightMm);
    m_marginLeftSpin->setValue(spec.marginLeftMm);
    m_marginTopSpin->setValue(spec.marginTopMm);
    m_marginRightSpin->setValue(spec.marginRightMm);
    m_marginBottomSpin->setValue(spec.marginBottomMm);
    m_dpiSpin->setValue(spec.defaultDpi);
    m_gridEnabledCheck->setChecked(spec.labelGrid.enabled);
    m_gridRowsSpin->setValue(spec.labelGrid.rows);
    m_gridColumnsSpin->setValue(spec.labelGrid.columns);
    m_gridHorizontalGapSpin->setValue(spec.labelGrid.horizontalGapMm);
    m_gridVerticalGapSpin->setValue(spec.labelGrid.verticalGapMm);
    m_loadingForm = false;
    setFormEnabled(true);
}

sleekpr::core::PaperSpec PaperSpecManagerWindow::specFromForm() const
{
    sleekpr::core::PaperSpec spec;
    spec.id = m_idEdit->text().trimmed();
    spec.name = m_nameEdit->text().trimmed();
    spec.kind = kindFromData(m_kindCombo->currentData().toString());
    spec.orientation = orientationFromData(m_orientationCombo->currentData().toString());
    spec.widthMm = m_widthSpin->value();
    spec.heightMm = m_heightSpin->value();
    spec.marginLeftMm = m_marginLeftSpin->value();
    spec.marginTopMm = m_marginTopSpin->value();
    spec.marginRightMm = m_marginRightSpin->value();
    spec.marginBottomMm = m_marginBottomSpin->value();
    spec.defaultDpi = m_dpiSpin->value();
    spec.labelGrid.enabled = m_gridEnabledCheck->isChecked();
    spec.labelGrid.rows = m_gridRowsSpin->value();
    spec.labelGrid.columns = m_gridColumnsSpin->value();
    spec.labelGrid.horizontalGapMm = m_gridHorizontalGapSpin->value();
    spec.labelGrid.verticalGapMm = m_gridVerticalGapSpin->value();
    return spec;
}

void PaperSpecManagerWindow::setFormEnabled(bool enabled)
{
    m_idEdit->setEnabled(enabled);
    m_nameEdit->setEnabled(enabled);
    m_kindCombo->setEnabled(enabled);
    m_orientationCombo->setEnabled(enabled);
    m_widthSpin->setEnabled(enabled);
    m_heightSpin->setEnabled(enabled);
    m_marginLeftSpin->setEnabled(enabled);
    m_marginTopSpin->setEnabled(enabled);
    m_marginRightSpin->setEnabled(enabled);
    m_marginBottomSpin->setEnabled(enabled);
    m_dpiSpin->setEnabled(enabled);
    m_gridEnabledCheck->setEnabled(enabled);
    m_gridRowsSpin->setEnabled(enabled);
    m_gridColumnsSpin->setEnabled(enabled);
    m_gridHorizontalGapSpin->setEnabled(enabled);
    m_gridVerticalGapSpin->setEnabled(enabled);
    m_saveButton->setEnabled(enabled);
    m_deleteButton->setEnabled(enabled);
}

QString PaperSpecManagerWindow::currentSpecId() const
{
    const auto* item = m_specList == nullptr ? nullptr : m_specList->currentItem();
    return item == nullptr ? QString() : item->data(Qt::UserRole).toString();
}

void PaperSpecManagerWindow::showStatus(const QString& message)
{
    if (m_statusLabel != nullptr) {
        m_statusLabel->setText(message);
    }
}

void PaperSpecManagerWindow::notifySpecsChanged()
{
    if (m_onPaperSpecsChanged) {
        m_onPaperSpecsChanged();
    }
}

} // 命名空间 sleekpr::app

#include "sleekpr/app/FieldPresetManagerWindow.h"

#include "sleekpr/core/templates/FieldPresetJson.h"
#include "sleekpr/core/templates/FieldPresetStore.h"

#include <QDateTime>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QVBoxLayout>

#include <utility>

namespace sleekpr::app {

namespace {

QString presetListText(const sleekpr::core::FieldPreset& preset)
{
    const auto name = preset.name.trimmed().isEmpty() ? preset.id : preset.name.trimmed();
    return QString::fromUtf8("%1 (%2)").arg(name, preset.templateId);
}

QString valuesText(const QJsonObject& values)
{
    return QString::fromUtf8(QJsonDocument(values).toJson(QJsonDocument::Indented));
}

QJsonObject defaultValuesObject()
{
    return QJsonObject{};
}

} // 匿名命名空间

FieldPresetManagerWindow::FieldPresetManagerWindow(
    QString fieldPresetFilePath,
    FieldPresetsChangedCallback onFieldPresetsChanged,
    QWidget* parent)
    : QWidget(parent)
    , m_fieldPresetFilePath(std::move(fieldPresetFilePath))
    , m_onFieldPresetsChanged(std::move(onFieldPresetsChanged))
{
    setWindowTitle(QString::fromUtf8("字段预设管理"));
    resize(760, 520);
    buildUi();
    reloadFromDisk();
}

void FieldPresetManagerWindow::reloadFromDisk()
{
    refreshList(currentPresetId());
}

void FieldPresetManagerWindow::buildUi()
{
    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(14, 14, 14, 14);
    rootLayout->setSpacing(10);

    auto* contentLayout = new QHBoxLayout;
    m_presetList = new QListWidget(this);
    m_presetList->setObjectName(QStringLiteral("fieldPresetList"));
    m_presetList->setMinimumWidth(240);
    contentLayout->addWidget(m_presetList, 0);

    auto* formContainer = new QWidget(this);
    auto* formLayout = new QFormLayout(formContainer);
    formLayout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);

    m_idEdit = new QLineEdit(formContainer);
    m_idEdit->setObjectName(QStringLiteral("fieldPresetIdEdit"));
    m_nameEdit = new QLineEdit(formContainer);
    m_nameEdit->setObjectName(QStringLiteral("fieldPresetNameEdit"));
    m_templateIdEdit = new QLineEdit(formContainer);
    m_templateIdEdit->setObjectName(QStringLiteral("fieldPresetTemplateIdEdit"));
    m_valuesEdit = new QPlainTextEdit(formContainer);
    m_valuesEdit->setObjectName(QStringLiteral("fieldPresetValuesEdit"));
    m_valuesEdit->setMinimumHeight(240);

    formLayout->addRow(QString::fromUtf8("ID"), m_idEdit);
    formLayout->addRow(QString::fromUtf8("名称"), m_nameEdit);
    formLayout->addRow(QString::fromUtf8("模板 ID"), m_templateIdEdit);
    formLayout->addRow(QString::fromUtf8("字段值 JSON"), m_valuesEdit);
    contentLayout->addWidget(formContainer, 1);
    rootLayout->addLayout(contentLayout, 1);

    m_statusLabel = new QLabel(this);
    m_statusLabel->setObjectName(QStringLiteral("fieldPresetStatusLabel"));
    rootLayout->addWidget(m_statusLabel);

    auto* buttonLayout = new QHBoxLayout;
    auto* addButton = new QPushButton(QString::fromUtf8("新增"), this);
    addButton->setObjectName(QStringLiteral("addFieldPresetButton"));
    m_saveButton = new QPushButton(QString::fromUtf8("保存"), this);
    m_saveButton->setObjectName(QStringLiteral("saveFieldPresetButton"));
    m_deleteButton = new QPushButton(QString::fromUtf8("删除"), this);
    m_deleteButton->setObjectName(QStringLiteral("deleteFieldPresetButton"));
    auto* reloadButton = new QPushButton(QString::fromUtf8("重新加载"), this);
    reloadButton->setObjectName(QStringLiteral("reloadFieldPresetsButton"));
    auto* closeButton = new QPushButton(QString::fromUtf8("关闭"), this);
    buttonLayout->addWidget(addButton);
    buttonLayout->addWidget(m_saveButton);
    buttonLayout->addWidget(m_deleteButton);
    buttonLayout->addWidget(reloadButton);
    buttonLayout->addStretch();
    buttonLayout->addWidget(closeButton);
    rootLayout->addLayout(buttonLayout);

    connect(addButton, &QPushButton::clicked, this, [this] { startNewPreset(); });
    connect(m_saveButton, &QPushButton::clicked, this, [this] { saveCurrentPreset(); });
    connect(m_deleteButton, &QPushButton::clicked, this, [this] { deleteCurrentPreset(); });
    connect(reloadButton, &QPushButton::clicked, this, [this] { reloadFromDisk(); });
    connect(closeButton, &QPushButton::clicked, this, [this] { close(); });
    connect(m_presetList, &QListWidget::currentRowChanged, this, [this](int) {
        if (!m_loadingForm) {
            loadSelectedPreset();
        }
    });
}

void FieldPresetManagerWindow::refreshList(const QString& selectedId)
{
    QString errorMessage;
    const auto presets = sleekpr::core::FieldPresetStore(m_fieldPresetFilePath).presets(&errorMessage);

    m_loadingForm = true;
    m_presetList->clear();
    for (const auto& preset : presets) {
        auto* item = new QListWidgetItem(presetListText(preset), m_presetList);
        item->setData(Qt::UserRole, preset.id);
    }
    m_loadingForm = false;

    int selectedRow = -1;
    for (int row = 0; row < m_presetList->count(); ++row) {
        if (m_presetList->item(row)->data(Qt::UserRole).toString() == selectedId) {
            selectedRow = row;
            break;
        }
    }
    if (selectedRow < 0 && m_presetList->count() > 0) {
        selectedRow = 0;
    }

    if (selectedRow >= 0) {
        m_presetList->setCurrentRow(selectedRow);
        loadSelectedPreset();
    } else {
        startNewPreset();
    }

    if (!errorMessage.isEmpty()) {
        showStatus(errorMessage);
    }
}

void FieldPresetManagerWindow::loadSelectedPreset()
{
    const auto presetId = currentPresetId();
    if (presetId.isEmpty()) {
        startNewPreset();
        return;
    }

    QString errorMessage;
    const auto preset = sleekpr::core::FieldPresetStore(m_fieldPresetFilePath).loadPreset(presetId, &errorMessage);
    if (!preset.has_value()) {
        showStatus(errorMessage);
        return;
    }

    loadPresetToForm(preset.value());
    setFormEnabled(true);
    showStatus(QString::fromUtf8("已加载字段预设"));
}

void FieldPresetManagerWindow::startNewPreset()
{
    m_loadingForm = true;
    m_presetList->clearSelection();
    m_idEdit->clear();
    m_nameEdit->clear();
    m_templateIdEdit->setText(QStringLiteral("default"));
    m_valuesEdit->setPlainText(valuesText(defaultValuesObject()));
    m_loadingForm = false;
    setFormEnabled(true);
    showStatus(QString::fromUtf8("正在新建字段预设"));
}

void FieldPresetManagerWindow::saveCurrentPreset()
{
    QString errorMessage;
    const auto preset = presetFromForm(&errorMessage);
    if (!preset.has_value()) {
        showStatus(errorMessage);
        return;
    }

    if (!sleekpr::core::FieldPresetStore(m_fieldPresetFilePath).savePreset(preset.value(), &errorMessage)) {
        showStatus(errorMessage);
        return;
    }

    refreshList(preset->id);
    notifyPresetsChanged();
    showStatus(QString::fromUtf8("已保存字段预设"));
}

void FieldPresetManagerWindow::deleteCurrentPreset()
{
    const auto presetId = currentPresetId();
    if (presetId.isEmpty()) {
        return;
    }

    QString errorMessage;
    if (!sleekpr::core::FieldPresetStore(m_fieldPresetFilePath).removePreset(presetId, &errorMessage)) {
        showStatus(errorMessage);
        return;
    }

    refreshList();
    notifyPresetsChanged();
    showStatus(QString::fromUtf8("已删除字段预设"));
}

void FieldPresetManagerWindow::loadPresetToForm(const sleekpr::core::FieldPreset& preset)
{
    m_loadingForm = true;
    m_idEdit->setText(preset.id);
    m_nameEdit->setText(preset.name);
    m_templateIdEdit->setText(preset.templateId);
    m_valuesEdit->setPlainText(valuesText(preset.values));
    m_loadingForm = false;
}

std::optional<sleekpr::core::FieldPreset> FieldPresetManagerWindow::presetFromForm(QString* errorMessage) const
{
    QJsonParseError parseError;
    const auto valuesDocument = QJsonDocument::fromJson(m_valuesEdit->toPlainText().toUtf8(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !valuesDocument.isObject()) {
        if (errorMessage != nullptr) {
            *errorMessage = QString::fromUtf8("字段值 JSON 必须是对象：%1").arg(parseError.errorString());
        }
        return std::nullopt;
    }

    sleekpr::core::FieldPreset preset;
    preset.id = m_idEdit->text().trimmed();
    preset.name = m_nameEdit->text().trimmed();
    preset.templateId = m_templateIdEdit->text().trimmed();
    preset.values = valuesDocument.object();
    preset.updatedAt = QDateTime::currentDateTime().toString(Qt::ISODate);

    const auto json = sleekpr::core::FieldPresetJson::toJson(preset);
    QString validationError;
    if (!sleekpr::core::FieldPresetJson::validate(json, &validationError)) {
        if (errorMessage != nullptr) {
            *errorMessage = validationError;
        }
        return std::nullopt;
    }

    return preset;
}

void FieldPresetManagerWindow::setFormEnabled(bool enabled)
{
    m_idEdit->setEnabled(enabled);
    m_nameEdit->setEnabled(enabled);
    m_templateIdEdit->setEnabled(enabled);
    m_valuesEdit->setEnabled(enabled);
    m_saveButton->setEnabled(enabled);
    m_deleteButton->setEnabled(enabled && !currentPresetId().isEmpty());
}

QString FieldPresetManagerWindow::currentPresetId() const
{
    const auto* item = m_presetList->currentItem();
    return item == nullptr ? QString() : item->data(Qt::UserRole).toString();
}

void FieldPresetManagerWindow::showStatus(const QString& message)
{
    if (m_statusLabel != nullptr) {
        m_statusLabel->setText(message);
    }
}

void FieldPresetManagerWindow::notifyPresetsChanged()
{
    if (m_onFieldPresetsChanged) {
        m_onFieldPresetsChanged();
    }
}

} // 命名空间 sleekpr::app

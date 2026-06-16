#include "sleekpr/app/TemplateDesignerWindow.h"

#include "sleekpr/core/labels/LabelRenderPlanner.h"
#include "sleekpr/core/native/NativeLabelDrawingPlanner.h"
#include "sleekpr/core/settings/TemplateElement.h"
#include "sleekpr/core/templates/TemplateDocumentFactory.h"
#include "sleekpr/infrastructure/preview/PreviewLabelFactory.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QListWidgetItem>
#include <QPushButton>
#include <QUuid>
#include <QVBoxLayout>

#include <utility>

namespace sleekpr::app {

namespace {

QString generatedLayerId()
{
    return QStringLiteral("layer-%1").arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
}

QString nextLayerName(int layerCount)
{
    return QString::fromUtf8("图层 %1").arg(layerCount + 1);
}

} // 匿名命名空间

TemplateDesignerWindow::TemplateDesignerWindow(
    sleekpr::core::PrintClientSettings settings,
    SettingsChangedCallback onSettingsChanged,
    QWidget* parent)
    : QWidget(parent)
    , m_settings(std::move(settings))
    , m_onSettingsChanged(std::move(onSettingsChanged))
{
    setWindowTitle(QString::fromUtf8("sleekpr 模板设计器"));
    resize(1180, 680);
    buildUi();
    ensureCurrentTemplateDocument();
    refreshLayerList();
}

void TemplateDesignerWindow::buildUi()
{
    auto* rootLayout = new QHBoxLayout(this);
    rootLayout->setContentsMargins(14, 14, 14, 14);
    rootLayout->setSpacing(12);

    auto* layerPanel = new QWidget(this);
    auto* layerLayout = new QVBoxLayout(layerPanel);
    layerLayout->setContentsMargins(0, 0, 8, 0);

    auto* titleLabel = new QLabel(QString::fromUtf8("图层"), layerPanel);
    auto* addLayerButton = new QPushButton(QString::fromUtf8("新增图层"), layerPanel);
    addLayerButton->setObjectName(QStringLiteral("addLayerButton"));

    m_layerList = new QListWidget(layerPanel);
    m_layerList->setObjectName(QStringLiteral("templateLayerList"));

    layerLayout->addWidget(titleLabel);
    layerLayout->addWidget(m_layerList, 1);
    layerLayout->addWidget(addLayerButton);

    auto* workspacePanel = new QWidget(this);
    auto* workspaceLayout = new QVBoxLayout(workspacePanel);
    workspaceLayout->setContentsMargins(0, 0, 0, 0);
    m_statusLabel = new QLabel(QString::fromUtf8("模板设计器已就绪"), workspacePanel);
    workspaceLayout->addWidget(m_statusLabel);
    workspaceLayout->addStretch();

    rootLayout->addWidget(layerPanel, 0);
    rootLayout->addWidget(workspacePanel, 1);

    connect(addLayerButton, &QPushButton::clicked, this, [this] { addLayer(); });
}

void TemplateDesignerWindow::ensureCurrentTemplateDocument()
{
    if (m_settings.templateDocuments.contains(m_templateKey)) {
        return;
    }

    const auto labelItem = sleekpr::infrastructure::PreviewLabelFactory::createDemoLabel(
        sleekpr::core::LabelTemplateKey::Default80x30);
    const auto labelPlan = sleekpr::core::LabelRenderPlanner().createPlan(labelItem);
    const auto drawingPlan = sleekpr::core::NativeLabelDrawingPlanner().createPlan(
        labelPlan,
        m_settings.labelOffset,
        m_settings.templateOverrides.value(m_templateKey),
        QList<sleekpr::core::TemplateElement>{});

    // 首次打开设计器只在内存中启动完整模板文档；旧版自定义元素必须原样迁移，避免绑定字段和动态二维码被演示数据固化。
    m_settings.templateDocuments[m_templateKey] = sleekpr::core::TemplateDocumentFactory::fromDrawingPlan(
        m_templateKey,
        QString::fromUtf8("默认标签"),
        drawingPlan,
        m_settings.templateElements.value(m_templateKey));
}

void TemplateDesignerWindow::addLayer()
{
    ensureCurrentTemplateDocument();
    auto& document = m_settings.templateDocuments[m_templateKey];

    sleekpr::core::TemplateLayer layer;
    layer.id = generatedLayerId();
    layer.name = nextLayerName(document.layers.size());
    layer.visible = true;
    layer.locked = false;
    document.layers.append(layer);

    refreshLayerList();
    m_layerList->setCurrentRow(m_layerList->count() - 1);
    m_statusLabel->setText(QString::fromUtf8("已新增图层"));

    if (m_onSettingsChanged) {
        m_onSettingsChanged(m_settings);
    }
}

void TemplateDesignerWindow::refreshLayerList()
{
    ensureCurrentTemplateDocument();
    const auto& document = m_settings.templateDocuments[m_templateKey];

    m_layerList->clear();
    for (int index = 0; index < document.layers.size(); ++index) {
        const auto& layer = document.layers[index];
        const auto displayName = layer.name.trimmed().isEmpty()
            ? nextLayerName(index)
            : layer.name.trimmed();
        auto* item = new QListWidgetItem(displayName, m_layerList);
        item->setData(Qt::UserRole, layer.id);
    }

    if (m_layerList->count() > 0 && m_layerList->currentRow() < 0) {
        m_layerList->setCurrentRow(0);
    }
}

} // 命名空间 sleekpr::app

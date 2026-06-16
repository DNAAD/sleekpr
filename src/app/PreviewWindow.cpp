#include "sleekpr/app/PreviewWindow.h"

#include "sleekpr/core/settings/FileSettingsStore.h"
#include "sleekpr/infrastructure/preview/LabelPreviewService.h"

#include <QFont>
#include <QHBoxLayout>
#include <QLabel>
#include <QPixmap>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>

#include <utility>

namespace sleekpr::app {

PreviewWindow::PreviewWindow(QString settingsPath, QWidget* parent)
    : QWidget(parent)
    , m_settingsPath(std::move(settingsPath))
{
    setWindowTitle(QString::fromUtf8("sleekpr 标签预览"));
    resize(1080, 520);

    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(16, 16, 16, 16);
    rootLayout->setSpacing(12);

    auto* headerLayout = new QHBoxLayout;
    auto* titleLabel = new QLabel(QString::fromUtf8("标签预览"));
    QFont titleFont = titleLabel->font();
    titleFont.setPointSize(14);
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);

    auto* refreshButton = new QPushButton(QString::fromUtf8("刷新"));
    headerLayout->addWidget(titleLabel);
    headerLayout->addStretch();
    headerLayout->addWidget(refreshButton);
    rootLayout->addLayout(headerLayout);

    m_statusLabel = new QLabel(this);
    m_statusLabel->setText(QString::fromUtf8("预览尺寸：80mm x 30mm，渲染精度：300 DPI"));
    rootLayout->addWidget(m_statusLabel);

    m_previewLabel = new QLabel(this);
    m_previewLabel->setAlignment(Qt::AlignCenter);
    m_previewLabel->setMinimumSize(945, 354);
    m_previewLabel->setStyleSheet(QStringLiteral("QLabel { background: white; border: 1px solid #111827; }"));

    auto* scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    scrollArea->setAlignment(Qt::AlignCenter);
    scrollArea->setWidget(m_previewLabel);
    rootLayout->addWidget(scrollArea, 1);

    connect(refreshButton, &QPushButton::clicked, this, &PreviewWindow::refreshPreview);
    refreshPreview();
}

void PreviewWindow::refreshPreview()
{
    const auto settings = sleekpr::core::FileSettingsStore(m_settingsPath).load();
    refreshFromSettings(settings);
}

void PreviewWindow::refreshFromSettings(const sleekpr::core::PrintClientSettings& settings)
{
    updatePreviewImage(createPreviewImage(settings));
}

QImage PreviewWindow::createPreviewImage(const sleekpr::core::PrintClientSettings& settings) const
{
    // 原生预览窗口和后续真实打印共用同一份毫米坐标命令，避免显示效果和打印效果走两套布局逻辑。
    return sleekpr::infrastructure::LabelPreviewService().renderDemoPreview(settings);
}

void PreviewWindow::updatePreviewImage(const QImage& image)
{
    if (image.isNull()) {
        m_previewLabel->setText(QString::fromUtf8("预览图生成失败"));
        return;
    }

    // QLabel 直接承载 QPixmap，保持 Qt 原生显示链路，不再通过浏览器或 HTML 页面预览。
    m_previewLabel->setPixmap(QPixmap::fromImage(image));
    m_previewLabel->setFixedSize(image.size());
}

} // 命名空间 sleekpr::app

#pragma once

#include "sleekpr/core/native/NativePrintDrawingPlan.h"
#include "sleekpr/core/templates/TemplateDocument.h"

#include <QSizeF>
#include <QStringList>
#include <QWidget>
#include <optional>

class QComboBox;
class QLabel;
class QPushButton;
class QScrollArea;
class QResizeEvent;

namespace sleekpr::app {

class PrintPreviewImage;

class PrintPreviewPanel final : public QWidget
{
public:
    explicit PrintPreviewPanel(QWidget* parent = nullptr);
    void setPlan(const sleekpr::core::NativePrintDrawingPlan& plan);
    // 诊断只展示当前版式，不能以旧预览或样例数据冒充正式打印预览。
    void setDiagnosticDocument(const sleekpr::core::TemplateDocument& document, const QSizeF& paperSizeMm,
        double offsetXmm = 0, double offsetYmm = 0, double scaleX = 1, double scaleY = 1);
    // 优先定位已校准绘制命令；无命令时才按先偏移、后缩放的规则换算模板坐标。
    void setElementHighlight(const sleekpr::core::TemplateDocument& document, const QStringList& elementIds,
        double offsetXmm = 0, double offsetYmm = 0, double scaleX = 1, double scaleY = 1);
    void clearElementHighlight();
    void clear();

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void renderPage();
    void scheduleRender();
    void revealHighlight();
    void updateHighlightState();
    std::optional<sleekpr::core::NativePrintDrawingPlan> m_plan;
    QScrollArea* m_scroll;
    PrintPreviewImage* m_image;
    QLabel* m_title;
    QLabel* m_pageLabel;
    QComboBox* m_zoom;
    QPushButton* m_previous;
    QPushButton* m_next;
    int m_page = 0;
    QSizeF m_diagnosticPaperSizeMm;
    bool m_diagnostic = false;
    // 视口事件可能重复到达；相同页、倍率和 DPR 只重绘叠加层，不重复栅格化底图。
    int m_renderedPage = -1;
    double m_renderedPixelsPerMm = 0;
    double m_renderedDpr = 0;
    bool m_renderQueued = false;
};

} // 命名空间 sleekpr::app

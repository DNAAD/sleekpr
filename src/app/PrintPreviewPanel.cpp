#include "sleekpr/app/PrintPreviewPanel.h"
#include "sleekpr/app/LatestPreviewQueue.h"
#include "sleekpr/infrastructure/preview/TemplatePreviewJob.h"
#include "sleekpr/infrastructure/diagnostics/ClientPerformanceMetrics.h"
#include "sleekpr/app/AppTheme.h"
#include "sleekpr/infrastructure/preview/LabelPreviewImageRenderer.h"

#include <QComboBox>
#include <QColor>
#include <QEvent>
#include <QHBoxLayout>
#include <QImage>
#include <QLabel>
#include <QPainter>
#include <QPainterPath>
#include <QPainterPathStroker>
#include <QPaintEvent>
#include <QPen>
#include <QPushButton>
#include <QScrollArea>
#include <QSet>
#include <QTimer>
#include <QTransform>
#include <QVariant>
#include <QVBoxLayout>
#include <QtMath>

#include <algorithm>
#include <cmath>

namespace sleekpr::app {

namespace {

bool usablePaperSize(const QSizeF& size)
{
    return std::isfinite(size.width()) && std::isfinite(size.height())
        && size.width() > 0 && size.height() > 0;
}

QPainterPath drawingShape(const QRectF& rect, double rotation, bool text)
{
    if (!usablePaperSize(rect.size()) || !std::isfinite(rect.x()) || !std::isfinite(rect.y())
        || !std::isfinite(rotation)) return {};
    QPainterPath shape;
    QTransform transform;
    if (text && rotation != 0) {
        auto normalized = std::fmod(rotation, 360.0);
        if (normalized < 0) normalized += 360;
        // 与 LabelPreviewImageRenderer 一致：文本旋转使用交换后的排版框和专用锚点。
        if (std::abs(normalized - 90.0) < 0.001) {
            transform.translate(rect.right(), rect.top());
            transform.rotate(90);
        } else {
            transform.translate(rect.left(), rect.bottom());
            transform.rotate(-rotation);
        }
        shape.addRect(QRectF(0, 0, rect.height(), rect.width()));
    } else {
        // 矩形与二维码围绕自身中心旋转；保留真实轮廓而非未旋转的轴对齐矩形。
        transform.translate(rect.center().x(), rect.center().y());
        transform.rotate(-rotation);
        transform.translate(-rect.center().x(), -rect.center().y());
        shape.addRect(rect);
    }
    return transform.map(shape);
}

double usableScale(double scale)
{
    return std::isfinite(scale) && scale > 0 ? scale : 1.0;
}

QPainterPath elementShape(const sleekpr::core::TemplateElement& element,
    double offsetX, double offsetY, double scaleX, double scaleY)
{
    // Renderer 先加标签和 profile 总偏移，之后把坐标整体缩放，不能把偏移放在缩放后。
    const QRectF rect((element.x + offsetX) * scaleX, (element.y + offsetY) * scaleY,
        element.width * scaleX, element.height * scaleY);
    const bool text = element.type == sleekpr::core::TemplateElementType::FixedText
        || element.type == sleekpr::core::TemplateElementType::BoundField;
    return drawingShape(rect, element.rotationDegrees, text);
}

QList<QPainterPath> documentShapes(const sleekpr::core::TemplateDocument& document,
    const QSet<QString>* targets, double offsetX, double offsetY, double scaleX, double scaleY)
{
    QList<QPainterPath> shapes;
    for (const auto& layer : document.layers) {
        if (!layer.visible) continue;
        for (const auto& element : layer.elements) {
            if (!element.visible || (targets != nullptr && !targets->contains(element.id.trimmed()))) continue;
            const auto shape = elementShape(element, offsetX, offsetY, scaleX, scaleY);
            if (!shape.isEmpty()) shapes.append(shape);
        }
        for (const auto& table : layer.tables) {
            if (!table.visible || (targets != nullptr && !targets->contains(table.id.trimmed()))) continue;
            // 诊断表格只画整体外框，不布局明细、不渲染单元格，也不读取业务数据。
            const QRectF rect((table.x + offsetX) * scaleX, (table.y + offsetY) * scaleY,
                table.width * scaleX, table.height * scaleY);
            const auto shape = drawingShape(rect, 0, false);
            if (!shape.isEmpty()) shapes.append(shape);
        }
    }
    return shapes;
}

} // 匿名命名空间

class PrintPreviewImage final : public QLabel
{
public:
    explicit PrintPreviewImage(QWidget* parent) : QLabel(parent) {}

    void setPaperSizeMm(const QSizeF& paperSize)
    {
        m_paperSizeMm = paperSize;
    }

    void setDiagnosticShapes(const QList<QPainterPath>& shapes, bool enabled)
    {
        m_diagnosticShapes = shapes;
        m_diagnostic = enabled;
        update();
    }

    void setHighlightShapes(const QList<QPainterPath>& shapes)
    {
        m_highlightShapes = shapes;
        update();
    }

    void showHighlights(bool visible)
    {
        m_highlightsVisible = visible;
        update();
    }

    int highlightCount() const
    {
        return m_highlightsVisible ? static_cast<int>(m_highlightShapes.size()) : 0;
    }

    QRectF highlightBounds() const
    {
        QRectF bounds;
        if (!m_highlightsVisible) return bounds;
        const auto transform = paperTransform();
        for (const auto& shape : m_highlightShapes) {
            const auto mapped = transform.map(shape);
            const auto indicator = edgeIndicator(mapped);
            bounds = bounds.united(indicator.isEmpty()
                ? mapped.boundingRect().intersected(paperRect()) : indicator.boundingRect());
        }
        return bounds;
    }

protected:
    void paintEvent(QPaintEvent* event) override
    {
        QLabel::paintEvent(event);
        if (pixmap().isNull() || !usablePaperSize(m_paperSizeMm)) return;
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setClipRect(paperRect());
        painter.setBrush(Qt::NoBrush);
        if (m_diagnostic) {
            QPen pen(QColor(QStringLiteral("#87939b")));
            pen.setCosmetic(true);
            painter.setPen(pen);
            painter.drawRect(paperRect().adjusted(0.5, 0.5, -0.5, -0.5));
            paintShapes(painter, m_diagnosticShapes);
        }
        if (m_highlightsVisible) {
            QPen pen(QColor(QStringLiteral("#d74727")));
            pen.setWidthF(2);
            pen.setCosmetic(true);
            painter.setPen(pen);
            paintShapes(painter, m_highlightShapes);
        }
    }

private:
    QPainterPath edgeIndicator(const QPainterPath& shape) const
    {
        const auto paper = paperRect();
        QPainterPathStroker stroker;
        stroker.setWidth(2);
        if (stroker.createStroke(shape).intersects(paper)) return {};
        const auto bounds = shape.boundingRect();
        QPointF direction(1, 0);
        auto overflow = bounds.right() - paper.right();
        if (paper.left() - bounds.left() > overflow) {
            overflow = paper.left() - bounds.left();
            direction = QPointF(-1, 0);
        }
        if (bounds.bottom() - paper.bottom() > overflow) {
            overflow = bounds.bottom() - paper.bottom();
            direction = QPointF(0, 1);
        }
        if (paper.top() - bounds.top() > overflow) direction = QPointF(0, -1);
        const auto inset = std::min({8.0, paper.width() / 4, paper.height() / 4});
        const auto inner = paper.adjusted(inset, inset, -inset, -inset);
        QPointF tip(std::clamp(bounds.center().x(), inner.left(), inner.right()),
            std::clamp(bounds.center().y(), inner.top(), inner.bottom()));
        if (direction.x() != 0) tip.setX(direction.x() > 0 ? inner.right() : inner.left());
        else tip.setY(direction.y() > 0 ? inner.bottom() : inner.top());
        const auto size = std::min({6.0, paper.width() / 5, paper.height() / 5});
        const auto base = tip - direction * (size * 1.8);
        const QPointF side(-direction.y() * size / 1.3, direction.x() * size / 1.3);
        // 箭头只是纸边越界指示，保留原始轮廓和坐标，不把纸外元素伪装成纸内元素。
        QPainterPath indicator;
        indicator.moveTo(tip);
        indicator.lineTo(base + side);
        indicator.lineTo(base - side);
        indicator.closeSubpath();
        return indicator;
    }

    void paintShapes(QPainter& painter, const QList<QPainterPath>& shapes) const
    {
        const auto transform = paperTransform();
        for (const auto& shape : shapes) {
            const auto mapped = transform.map(shape);
            painter.drawPath(mapped);
            const auto indicator = edgeIndicator(mapped);
            if (!indicator.isEmpty()) painter.fillPath(indicator, painter.pen().color());
        }
    }

    QRectF paperRect() const
    {
        const auto size = pixmap().deviceIndependentSize();
        return QRectF(QPointF((width() - size.width()) / 2, (height() - size.height()) / 2), size);
    }

    QTransform paperTransform() const
    {
        QTransform transform;
        if (!usablePaperSize(m_paperSizeMm)) return transform;
        const auto rect = paperRect();
        // 用实际位图像素除以 DPR 得到逻辑尺寸，分别换算两轴，兼容取整、缩放和高 DPI。
        transform.translate(rect.x(), rect.y());
        transform.scale(rect.width() / m_paperSizeMm.width(), rect.height() / m_paperSizeMm.height());
        return transform;
    }

    QSizeF m_paperSizeMm;
    QList<QPainterPath> m_diagnosticShapes;
    QList<QPainterPath> m_highlightShapes;
    bool m_diagnostic = false;
    bool m_highlightsVisible = false;
};

PrintPreviewPanel::PrintPreviewPanel(QWidget* parent) : QWidget(parent)
{
    setObjectName(QStringLiteral("workbenchPreviewPanel"));
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    auto* header = new QWidget(this);
    header->setObjectName(QStringLiteral("workbenchPreviewHeader"));
    auto* headerLayout = new QVBoxLayout(header);
    headerLayout->setContentsMargins(12, 8, 12, 8);
    headerLayout->setSpacing(4);
    m_title = new QLabel(QString::fromUtf8("标签预览"), header);
    m_title->setWordWrap(true);
    // 标题独占一行，窄面板内不与翻页和缩放控件争宽，完整保留“非打印预览”提示。
    headerLayout->addWidget(m_title);
    auto* tools = new QHBoxLayout;
    tools->setContentsMargins(0, 0, 0, 0);
    headerLayout->addLayout(tools);
    tools->addStretch();
    m_previous = new QPushButton(header);
    m_previous->setObjectName(QStringLiteral("workbenchPagePreviousButton"));
    configureIconButton(m_previous, QStyle::SP_ArrowBack, QString::fromUtf8("上一页"));
    m_next = new QPushButton(header);
    m_next->setObjectName(QStringLiteral("workbenchPageNextButton"));
    configureIconButton(m_next, QStyle::SP_ArrowForward, QString::fromUtf8("下一页"));
    m_pageLabel = new QLabel(header);
    m_pageLabel->setAlignment(Qt::AlignCenter);
    m_pageLabel->setMinimumWidth(48);
    m_zoom = new QComboBox(header);
    m_zoom->setObjectName(QStringLiteral("workbenchPreviewZoom"));
    m_zoom->addItem(QString::fromUtf8("适合窗口"), 0.0);
    for (const auto zoom : {0.75, 1.0, 1.5, 2.0}) {
        m_zoom->addItem(QStringLiteral("%1%").arg(qRound(zoom * 100)), zoom);
    }
    m_zoom->setFixedWidth(108);
    m_zoom->setToolTip(QString::fromUtf8("预览缩放，不改变打印尺寸"));
    tools->addWidget(m_previous);
    tools->addWidget(m_pageLabel);
    tools->addWidget(m_next);
    tools->addSpacing(8);
    tools->addWidget(m_zoom);
    layout->addWidget(header);
    m_scroll = new QScrollArea(this);
    m_scroll->setObjectName(QStringLiteral("workbenchPreviewScrollArea"));
    m_scroll->setWidgetResizable(false);
    m_scroll->setAlignment(Qt::AlignCenter);
    m_scroll->setFrameShape(QFrame::NoFrame);
    m_scroll->viewport()->installEventFilter(this);
    installEventFilter(this);
    m_image = new PrintPreviewImage(m_scroll);
    m_image->setObjectName(QStringLiteral("workbenchPreviewImage"));
    m_image->setAlignment(Qt::AlignCenter);
    m_scroll->setWidget(m_image);
    layout->addWidget(m_scroll, 1);
    connect(m_previous, &QPushButton::clicked, this, [this] { --m_page; renderPage(); });
    connect(m_next, &QPushButton::clicked, this, [this] { ++m_page; renderPage(); });
    connect(m_zoom, &QComboBox::currentIndexChanged, this, [this] { renderPage(); });
    m_rasterQueue = new LatestPreviewQueue(infrastructure::PreviewExecutor::applicationInstance(), this);
    connect(m_rasterQueue, &LatestPreviewQueue::resultReady, this, [this](quint64, const auto& result) {
        infrastructure::PerformanceScope timing(infrastructure::PerformanceStage::Publication);
        m_rasterPending = false;
        if (!result.errorCode.isEmpty() || result.image.isNull()) {
            m_ready = false;
            m_renderedPage = -1;
            m_previewErrorCode = result.errorCode.isEmpty() ? QStringLiteral("PREVIEW_RENDER_FAILED") : result.errorCode;
            m_previewError = result.errorMessage.isEmpty() ? QString::fromUtf8("预览生成失败") : result.errorMessage;
            m_image->clear();
            m_image->setText(m_previewError);
        } else {
            const auto pixmap = QPixmap::fromImage(result.image);
            m_image->setPixmap(pixmap);
            m_image->setFixedSize(pixmap.deviceIndependentSize().toSize());
            m_renderedPage = m_requestedPage;
            m_renderedPixelsPerMm = m_requestedPixelsPerMm;
            m_renderedDpr = m_requestedDpr;
            m_renderedPlanVersion = m_requestedPlanVersion;
            m_previewError.clear();
            m_previewErrorCode.clear();
            m_ready = true;
            revealHighlight();
        }
        notifyReadyChanged();
    });
    clear();
}

PrintPreviewPanel::~PrintPreviewPanel()
{
    m_readyChanged = {};
    m_rasterQueue->invalidate();
    QObject::disconnect(m_rasterQueue, nullptr, this, nullptr);
}

bool PrintPreviewPanel::hasCurrentImage() const { return m_ready && !m_diagnostic && m_plan.has_value(); }
QString PrintPreviewPanel::previewError() const { return m_previewError; }
QString PrintPreviewPanel::previewErrorCode() const { return m_previewErrorCode; }
void PrintPreviewPanel::setReadyChangedHandler(std::function<void()> handler) { m_readyChanged = std::move(handler); }

void PrintPreviewPanel::notifyReadyChanged()
{
    setProperty("printPreviewCurrent", hasCurrentImage());
    if (m_readyChanged) m_readyChanged();
}

void PrintPreviewPanel::setPlan(const sleekpr::core::NativePrintDrawingPlan& plan)
{
    ++m_planVersion;
    m_rasterQueue->invalidate();
    m_rasterPending = false;
    m_ready = false;
    m_previewError.clear();
    m_previewErrorCode.clear();
    clearElementHighlight();
    m_diagnostic = false;
    m_diagnosticPaperSizeMm = {};
    m_image->setDiagnosticShapes({}, false);
    m_title->setText(QString::fromUtf8("标签预览"));
    setProperty("printPreflightDiagnostic", false);
    m_renderedPage = -1;
    m_plan = plan;
    if (plan.pages.isEmpty()) { clear(); return; }
    m_page = std::clamp(m_page, 0, std::max(0, int(plan.pages.size()) - 1));
    notifyReadyChanged();
    renderPage();
}

void PrintPreviewPanel::setDiagnosticDocument(const sleekpr::core::TemplateDocument& document,
    const QSizeF& paperSizeMm, double offsetXmm, double offsetYmm, double scaleX, double scaleY)
{
    clear();
    if (!usablePaperSize(paperSizeMm)) return;
    m_diagnostic = true;
    m_diagnosticPaperSizeMm = paperSizeMm;
    m_image->setDiagnosticShapes(documentShapes(document, nullptr,
        offsetXmm, offsetYmm, usableScale(scaleX), usableScale(scaleY)), true);
    m_title->setText(QString::fromUtf8("版式定位（非打印预览）"));
    setProperty("printPreflightDiagnostic", true);
    renderPage();
}

void PrintPreviewPanel::setElementHighlight(const sleekpr::core::TemplateDocument& document,
    const QStringList& elementIds, double offsetXmm, double offsetYmm, double scaleX, double scaleY)
{
    if (!m_plan.has_value() && !m_diagnostic) { clearElementHighlight(); return; }
    QSet<QString> remaining;
    for (const auto& id : elementIds) {
        if (!id.trimmed().isEmpty()) remaining.insert(id.trimmed());
    }
    QSet<QString> compoundTargets;
    for (const auto& layer : document.layers) {
        for (const auto& element : layer.elements) {
            if (element.type == sleekpr::core::TemplateElementType::ArrayGrid)
                compoundTargets.insert(element.id.trimmed());
        }
        for (const auto& table : layer.tables) compoundTargets.insert(table.id.trimmed());
    }
    QList<QPainterPath> shapes;
    if (m_plan.has_value() && !m_plan->pages.isEmpty()) {
        QSet<QString> matched;
        // 校准后的命令已包含偏移与缩放，绝不能再次套用 profile；数组/表格需收集全部子命令。
        for (const auto& command : m_plan->pages.first().commands) {
            auto target = remaining.contains(command.elementKey) ? command.elementKey : QString();
            if (target.isEmpty()) {
                for (const auto& id : remaining) {
                    if (compoundTargets.contains(id) && command.elementKey.startsWith(id + QLatin1Char('.'))
                        && id.size() > target.size()) target = id;
                }
            }
            if (target.isEmpty()) continue;
            const auto shape = drawingShape(QRectF(command.x, command.y, command.width, command.height),
                command.rotationDegrees, command.type == sleekpr::core::NativeDrawCommandType::Text);
            if (shape.isEmpty()) continue;
            shapes.append(shape);
            matched.insert(target);
        }
        remaining.subtract(matched);
    }
    shapes.append(documentShapes(document, &remaining, offsetXmm, offsetYmm, usableScale(scaleX), usableScale(scaleY)));
    m_image->setHighlightShapes(shapes);
    // 首阶段的模板定位只对应第一页，不能把首模板页的轮廓误画到后续打印页面。
    m_page = 0;
    renderPage();
}

void PrintPreviewPanel::clearElementHighlight()
{
    m_image->setHighlightShapes({});
    updateHighlightState();
}

void PrintPreviewPanel::clear()
{
    ++m_planVersion;
    if (m_rasterQueue) m_rasterQueue->invalidate();
    m_ready = false;
    m_rasterPending = false;
    m_previewError.clear();
    m_previewErrorCode.clear();
    m_plan.reset();
    m_diagnostic = false;
    m_diagnosticPaperSizeMm = {};
    m_renderedPage = -1;
    m_page = 0;
    m_image->setPaperSizeMm({});
    m_image->setDiagnosticShapes({}, false);
    clearElementHighlight();
    m_image->clear();
    m_image->setText(QString::fromUtf8("暂无可用预览"));
    m_image->setFixedSize(180, 48);
    m_previous->setEnabled(false);
    m_next->setEnabled(false);
    m_pageLabel->setText(QStringLiteral("0 / 0"));
    m_title->setText(QString::fromUtf8("标签预览"));
    setProperty("printPreflightDiagnostic", false);
    notifyReadyChanged();
}

bool PrintPreviewPanel::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == this && event->type() == QEvent::Hide && m_rasterQueue) {
        m_rasterQueue->invalidate();
        m_rasterPending = false;
        m_renderedPage = -1;
        m_ready = false;
        notifyReadyChanged();
    }
    if ((watched == m_scroll->viewport() && event->type() == QEvent::Resize)
        || (watched == this && (event->type() == QEvent::Show || event->type() == QEvent::DevicePixelRatioChange))) {
        scheduleRender();
    }
    return QWidget::eventFilter(watched, event);
}

void PrintPreviewPanel::scheduleRender()
{
    if (m_renderQueued) return;
    m_renderQueued = true;
    QTimer::singleShot(0, this, [this] { m_renderQueued = false; renderPage(); });
}

void PrintPreviewPanel::renderPage()
{
    if (!m_diagnostic && (!m_plan.has_value() || m_plan->pages.isEmpty())) return;
    if (!isVisible()) { m_ready = false; notifyReadyChanged(); return; }
    const auto paperSize = m_diagnostic ? m_diagnosticPaperSizeMm
        : QSizeF(m_plan->paperSize.widthMm(), m_plan->paperSize.heightMm());
    if (!usablePaperSize(paperSize)) {
        clear();
        return;
    }
    const auto widthMm = paperSize.width();
    const auto heightMm = paperSize.height();
    const int pageCount = m_diagnostic ? 1 : static_cast<int>(m_plan->pages.size());
    m_page = std::clamp(m_page, 0, pageCount - 1);
    const auto zoom = m_zoom->currentData().toDouble();
    const auto viewport = m_scroll->viewport()->size();
    const auto fit = std::max(0.01, std::min((viewport.width() - 40.0) / widthMm, (viewport.height() - 40.0) / heightMm));
    const auto ratio = std::max(1.0, devicePixelRatioF());
    // 仅可见窗口提交任务；适配模式使用当前视口，固定倍率沿用屏幕毫米换算。
    auto pixelsPerMm = zoom == 0.0 ? fit : 96.0 / 25.4 * zoom;
    const auto minimumScale = 1.0 / (std::min(widthMm, heightMm) * ratio);
    // 窗口布局切换时视口可能接近零，栅格化后两个方向也必须至少各有一个设备像素。
    // 用户倍率不因资源上限而悄悄降低；后台预算在分配前返回明确失败。
    pixelsPerMm = std::max(pixelsPerMm, minimumScale);
    const bool cached = m_renderedPlanVersion == m_planVersion && m_renderedPage == m_page
        && m_renderedPixelsPerMm == pixelsPerMm && m_renderedDpr == ratio;
    const bool pending = m_rasterPending && m_requestedPlanVersion == m_planVersion && m_requestedPage == m_page
        && m_requestedPixelsPerMm == pixelsPerMm && m_requestedDpr == ratio;
    if (cached && m_rasterPending) {
        // 回到已有缓存的倍率时也要废弃另一倍率的在途结果，不能让它稍后覆盖缓存图。
        m_rasterQueue->invalidate();
        m_rasterPending = false;
    }
    if (!cached && !pending) {
        infrastructure::TemplatePreviewSnapshot snapshot;
        if (m_diagnostic) {
            // 诊断只生成空白纸张，轮廓仍在 GUI 叠加，绝不把真实数据或高亮写进底图。
            core::NativePrintDrawingPlan blank;
            blank.paperSize = core::LabelPaperSize(widthMm, heightMm);
            blank.renderDpi = 96;
            blank.pages = {{1, {}}};
            snapshot.preparedPlan = blank;
        } else snapshot.preparedPlan = m_plan;
        snapshot.pageIndex = m_page;
        snapshot.zoom = pixelsPerMm * 25.4 / snapshot.preparedPlan->renderDpi;
        snapshot.devicePixelRatio = ratio;
        m_requestedPlanVersion = m_planVersion;
        m_requestedPage = m_page;
        m_requestedPixelsPerMm = pixelsPerMm;
        m_requestedDpr = ratio;
        m_rasterPending = true;
        m_ready = false;
        m_previewError.clear();
        m_previewErrorCode.clear();
        notifyReadyChanged();
        m_rasterQueue->submit([snapshot] { return infrastructure::TemplatePreviewJob::run(snapshot); });
    } else if (cached && !m_ready) {
        m_ready = true;
        notifyReadyChanged();
    }
    m_image->setPaperSizeMm(paperSize);
    m_pageLabel->setText(QStringLiteral("%1 / %2").arg(m_page + 1).arg(pageCount));
    m_previous->setEnabled(m_page > 0);
    m_next->setEnabled(m_page + 1 < pageCount);
    updateHighlightState();
    revealHighlight();
}

void PrintPreviewPanel::updateHighlightState()
{
    m_image->showHighlights(m_page == 0);
    const auto count = m_image->highlightCount();
    // 状态公开在面板本体，调用方和无障碍工具无需依赖内部图像控件的结构。
    setProperty("printPreflightHighlightCount", count);
    setAccessibleDescription(m_diagnostic ? QString::fromUtf8("版式定位（非打印预览），%1 个定位轮廓").arg(count)
        : QString::fromUtf8("打印预览，%1 个定位轮廓").arg(count));
}

void PrintPreviewPanel::revealHighlight()
{
    const auto bounds = m_image->highlightBounds();
    if (bounds.isEmpty()) return;
    const auto viewport = m_scroll->viewport()->size();
    const auto marginX = std::max(0, std::min(qCeil(bounds.width() / 2) + 12, viewport.width() / 2 - 8));
    const auto marginY = std::max(0, std::min(qCeil(bounds.height() / 2) + 12, viewport.height() / 2 - 8));
    m_scroll->ensureVisible(qRound(bounds.center().x()), qRound(bounds.center().y()), marginX, marginY);
}

} // 命名空间 sleekpr::app

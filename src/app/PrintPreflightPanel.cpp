#include "sleekpr/app/PrintPreflightPanel.h"

#include <QEvent>
#include <QKeyEvent>
#include <QListWidget>
#include <QPainter>
#include <QPalette>
#include <QSignalBlocker>
#include <QStyle>
#include <QStyledItemDelegate>
#include <QStyleOptionFocusRect>
#include <QTextLayout>
#include <QTextOption>
#include <QToolButton>
#include <QVBoxLayout>
#include <QtMath>

#include <algorithm>

namespace sleekpr::app {

namespace {

constexpr int issueHorizontalPadding = 8;
constexpr int issueVerticalPadding = 5;
constexpr int issueIconSize = 16;
constexpr int issueIconSpacing = 8;

class PreflightIssueList final : public QListWidget
{
public:
    explicit PreflightIssueList(QWidget* parent) : QListWidget(parent) {}

    QSize sizeHint() const override { return QSize(320, 104); }
    QSize minimumSizeHint() const override { return QSize(0, 24); }

protected:
    bool viewportEvent(QEvent* event) override
    {
        const auto handled = QListWidget::viewportEvent(event);
        // 滚动条显隐也会改变实际视口宽度，统一延迟重排，避免保留过期行高。
        if (event->type() == QEvent::Resize) scheduleDelayedItemsLayout();
        return handled;
    }
};

QString layoutText(const QModelIndex& index)
{
    auto text = index.data(Qt::DisplayRole).toString();
    // 单段排版用显式行分隔符保留消息中的换行，不更改模型中的原始文字。
    text.replace(QLatin1Char('\n'), QChar::LineSeparator);
    return text;
}

qreal wrapText(QTextLayout& layout, int width, Qt::LayoutDirection direction)
{
    QTextOption option;
    option.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
    option.setTextDirection(direction);
    layout.setTextOption(option);
    qreal height = 0;
    layout.beginLayout();
    while (true) {
        auto line = layout.createLine();
        if (!line.isValid()) break;
        line.setLineWidth(std::max(1, width));
        line.setPosition(QPointF(0, height));
        height += line.height();
    }
    layout.endLayout();
    return height;
}

class PreflightIssueDelegate final : public QStyledItemDelegate
{
public:
    explicit PreflightIssueDelegate(QListWidget* view) : QStyledItemDelegate(view), m_view(view) {}

    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override
    {
        auto styled = option;
        initStyleOption(&styled, index);
        const auto width = std::max(1, m_view->viewport()->width());
        QTextLayout text(layoutText(index), styled.font);
        const auto height = wrapText(text, textWidth(width), styled.direction);
        return QSize(width, std::max(issueIconSize, qCeil(height)) + 2 * issueVerticalPadding);
    }

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override
    {
        auto styled = option;
        initStyleOption(&styled, index);
        const bool selected = styled.state.testFlag(QStyle::State_Selected);
        const bool enabled = styled.state.testFlag(QStyle::State_Enabled);
        const auto group = !enabled ? QPalette::Disabled
            : styled.state.testFlag(QStyle::State_Active) ? QPalette::Active : QPalette::Inactive;
        const auto backgroundRole = selected ? QPalette::Highlight
            : styled.state.testFlag(QStyle::State_MouseOver) ? QPalette::AlternateBase : QPalette::Base;
        const auto background = styled.palette.brush(group, backgroundRole);
        painter->save();
        painter->setClipRect(styled.rect);
        // 沿用控件调色板绘制选中、悬停与禁用状态，不另建颜色主题。
        painter->fillRect(styled.rect, background);
        painter->setPen(styled.palette.color(group, selected ? QPalette::HighlightedText : QPalette::Text));
        const auto iconMode = !enabled ? QIcon::Disabled : selected ? QIcon::Selected : QIcon::Normal;
        styled.icon.paint(painter,
            QRect(styled.rect.left() + issueHorizontalPadding,
                styled.rect.top() + issueVerticalPadding,
                issueIconSize, issueIconSize), Qt::AlignCenter, iconMode);
        QTextLayout text(layoutText(index), styled.font);
        wrapText(text, textWidth(styled.rect.width()), styled.direction);
        text.draw(painter, QPointF(styled.rect.left() + issueHorizontalPadding + issueIconSize + issueIconSpacing,
            styled.rect.top() + issueVerticalPadding));
        if (styled.state.testFlag(QStyle::State_HasFocus)) {
            QStyleOptionFocusRect focus;
            focus.initFrom(m_view);
            focus.state = styled.state;
            focus.rect = styled.rect.adjusted(1, 1, -1, -1);
            focus.backgroundColor = background.color();
            m_view->style()->drawPrimitive(QStyle::PE_FrameFocusRect, &focus, painter, m_view);
        }
        painter->restore();
    }

private:
    static int textWidth(int width)
    {
        return std::max(1, width - 2 * issueHorizontalPadding - issueIconSize - issueIconSpacing);
    }

    QListWidget* m_view;
};

} // 匿名命名空间

PrintPreflightPanel::PrintPreflightPanel(QWidget* parent) : QWidget(parent)
{
    setObjectName(QStringLiteral("printPreflightPanel"));
    setAccessibleName(QStringLiteral("打印前检查"));
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Maximum);
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    m_summary = new QToolButton(this);
    m_summary->setObjectName(QStringLiteral("printPreflightSummary"));
    m_summary->setCheckable(true);
    m_summary->setAutoRaise(true);
    m_summary->setFocusPolicy(Qt::StrongFocus);
    m_summary->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    m_summary->setIconSize(QSize(issueIconSize, issueIconSize));
    m_summary->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    m_summary->installEventFilter(this);
    layout->addWidget(m_summary);
    m_issues = new PreflightIssueList(this);
    m_issues->setObjectName(QStringLiteral("printPreflightIssues"));
    m_issues->setAccessibleName(QStringLiteral("打印前检查问题"));
    m_issues->setAccessibleDescription(QStringLiteral("错误优先显示，警告不阻断打印。"));
    m_issues->setFocusPolicy(Qt::StrongFocus);
    m_issues->setSelectionMode(QAbstractItemView::SingleSelection);
    m_issues->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_issues->setWordWrap(true);
    m_issues->setTextElideMode(Qt::ElideNone);
    m_issues->setResizeMode(QListView::Adjust);
    m_issues->setUniformItemSizes(false);
    m_issues->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_issues->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_issues->setMouseTracking(true);
    m_issues->setFrameShape(QFrame::NoFrame);
    m_issues->setItemDelegate(new PreflightIssueDelegate(m_issues));
    m_issues->installEventFilter(this);
    layout->addWidget(m_issues);
    // 继承 AppTheme 的全局样式，仅去掉嵌套边框和圆角，不引入颜色常量。
    setStyleSheet(QStringLiteral(
        "QToolButton#printPreflightSummary { border: 0; border-radius: 0; }"
        "QListWidget#printPreflightIssues { border: 0; border-radius: 0; }"));
    setTabOrder(m_summary, m_issues);
    connect(m_summary, &QToolButton::toggled, this, [this] { updateExpansion(); });
    connect(m_issues, &QListWidget::itemClicked, this, &PrintPreflightPanel::activateIssue);
    setPending();
}

void PrintPreflightPanel::setReport(const core::PrintPreflightReport& report)
{
    m_pending = true;
    m_issues->setEnabled(false);
    const bool updatesEnabled = m_issues->updatesEnabled();
    m_issues->setUpdatesEnabled(false);
    const QSignalBlocker blocker(m_issues);
    m_issues->clear();
    m_displayIssues = report.issues;
    // 只对展示副本稳定排序，同等级保留报告顺序与完整的结构化定位字段。
    std::stable_sort(m_displayIssues.begin(), m_displayIssues.end(), [](const auto& left, const auto& right) {
        return left.severity == core::PrintPreflightSeverity::Error
            && right.severity != core::PrintPreflightSeverity::Error;
    });
    const auto errorIcon = style()->standardIcon(QStyle::SP_MessageBoxCritical);
    const auto warningIcon = style()->standardIcon(QStyle::SP_MessageBoxWarning);
    qsizetype errorCount = 0;
    for (const auto& issue : m_displayIssues) {
        const bool error = issue.severity == core::PrintPreflightSeverity::Error;
        if (error) ++errorCount;
        const auto text = QStringLiteral("%1：%2").arg(error ? QStringLiteral("错误") : QStringLiteral("警告"), issue.message);
        auto* item = new QListWidgetItem(error ? errorIcon : warningIcon, text, m_issues);
        item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        item->setToolTip(text);
        item->setData(Qt::AccessibleTextRole, text);
        item->setData(Qt::AccessibleDescriptionRole, QStringLiteral("定位到此问题"));
    }
    m_pending = false;
    m_issues->setEnabled(!m_displayIssues.isEmpty());
    m_issues->scrollToTop();
    m_issues->setUpdatesEnabled(updatesEnabled);
    m_summary->setText(m_displayIssues.isEmpty() ? QStringLiteral("打印前检查：检查通过")
        : QStringLiteral("打印前检查：%1 项错误，%2 项警告").arg(errorCount).arg(m_displayIssues.size() - errorCount));
    // 新报告默认展开，确保新增错误不会藏在上一份报告的折叠状态中。
    m_summary->setChecked(!m_displayIssues.isEmpty());
    updateExpansion();
}

void PrintPreflightPanel::setPending()
{
    // 重检开始时同步清空并禁用，不能让旧条目继续触发字段或元素定位。
    m_pending = true;
    m_issues->setEnabled(false);
    const QSignalBlocker blocker(m_issues);
    m_displayIssues.clear();
    m_issues->clear();
    m_summary->setText(QStringLiteral("打印前检查：待检查"));
    m_summary->setChecked(false);
    updateExpansion();
}

bool PrintPreflightPanel::eventFilter(QObject* watched, QEvent* event)
{
    if ((watched == m_issues || watched == m_summary)
        && (event->type() == QEvent::KeyPress || event->type() == QEvent::KeyRelease)) {
        const auto* key = static_cast<QKeyEvent*>(event);
        if (key->key() == Qt::Key_Return || key->key() == Qt::Key_Enter) {
            // 消费回车，避免平台的 itemActivated 与自定义定位叠加造成重复发射。
            if (event->type() == QEvent::KeyPress && !key->isAutoRepeat()) {
                if (watched == m_issues) activateIssue(m_issues->currentItem());
                else m_summary->click();
            }
            return true;
        }
    }
    return QWidget::eventFilter(watched, event);
}

void PrintPreflightPanel::updateExpansion()
{
    const bool expanded = m_summary->isChecked() && !m_pending && !m_displayIssues.isEmpty();
    if (!expanded && m_issues->hasFocus()) m_summary->setFocus(Qt::OtherFocusReason);
    m_issues->setVisible(expanded);
    m_summary->setIcon(style()->standardIcon(expanded ? QStyle::SP_ArrowDown : QStyle::SP_ArrowRight));
    m_summary->setAccessibleName(QStringLiteral("打印前检查，%1").arg(m_summary->text()));
    const auto action = expanded ? QStringLiteral("收起问题列表") : QStringLiteral("展开问题列表");
    m_summary->setToolTip(QStringLiteral("%1\n%2").arg(m_summary->text(), action));
    m_summary->setAccessibleDescription(action);
    setAccessibleDescription(m_summary->text());
    updateGeometry();
}

void PrintPreflightPanel::activateIssue(QListWidgetItem* item)
{
    if (m_pending || !m_issues->isEnabled() || !m_summary->isChecked() || item == nullptr) return;
    const auto row = m_issues->row(item);
    if (row < 0 || row >= m_displayIssues.size()) return;
    // 接收者可能在信号处理中立即重检，局部副本保证其余接收者不会读到失效引用。
    const auto issue = m_displayIssues[row];
    emit issueActivated(issue);
}

} // 命名空间 sleekpr::app

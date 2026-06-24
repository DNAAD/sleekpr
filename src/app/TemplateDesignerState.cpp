#include "sleekpr/app/TemplateDesignerState.h"

#include "sleekpr/core/templates/TemplateDocumentJson.h"

namespace sleekpr::app {

namespace {

constexpr int kMaxDocumentHistorySize = 80;

bool sameDocumentSnapshot(
    const sleekpr::core::TemplateDocument& left,
    const sleekpr::core::TemplateDocument& right)
{
    // 撤销历史以模板 JSON 作为值快照比较，避免新增模板字段时忘记同步手写比较逻辑。
    return sleekpr::core::TemplateDocumentJson::toJson(left)
        == sleekpr::core::TemplateDocumentJson::toJson(right);
}

} // 匿名命名空间

void TemplateDesignerState::resetDocumentHistory(const sleekpr::core::TemplateDocument& document)
{
    m_documentHistory.clear();
    m_documentHistory.append(document);
    m_documentHistoryIndex = 0;
    m_restoringDocumentHistory = false;
}

void TemplateDesignerState::rememberDocument(const sleekpr::core::TemplateDocument& document)
{
    if (m_restoringDocumentHistory) {
        return;
    }

    if (m_documentHistoryIndex < 0) {
        resetDocumentHistory(document);
        return;
    }

    if (m_documentHistoryIndex < m_documentHistory.size()
        && sameDocumentSnapshot(m_documentHistory[m_documentHistoryIndex], document)) {
        return;
    }

    while (m_documentHistory.size() > m_documentHistoryIndex + 1) {
        m_documentHistory.removeLast();
    }
    m_documentHistory.append(document);
    m_documentHistoryIndex = m_documentHistory.size() - 1;

    while (m_documentHistory.size() > kMaxDocumentHistorySize) {
        m_documentHistory.removeFirst();
        --m_documentHistoryIndex;
    }
}

bool TemplateDesignerState::canUndo() const
{
    return m_documentHistoryIndex > 0 && m_documentHistoryIndex < m_documentHistory.size();
}

bool TemplateDesignerState::canRedo() const
{
    return m_documentHistoryIndex >= 0 && m_documentHistoryIndex < m_documentHistory.size() - 1;
}

std::optional<sleekpr::core::TemplateDocument> TemplateDesignerState::undoDocument()
{
    if (!canUndo()) {
        return std::nullopt;
    }

    --m_documentHistoryIndex;
    return m_documentHistory[m_documentHistoryIndex];
}

std::optional<sleekpr::core::TemplateDocument> TemplateDesignerState::redoDocument()
{
    if (!canRedo()) {
        return std::nullopt;
    }

    ++m_documentHistoryIndex;
    return m_documentHistory[m_documentHistoryIndex];
}

bool TemplateDesignerState::isRestoringDocumentHistory() const
{
    return m_restoringDocumentHistory;
}

void TemplateDesignerState::beginDocumentHistoryRestore()
{
    m_restoringDocumentHistory = true;
}

void TemplateDesignerState::endDocumentHistoryRestore()
{
    m_restoringDocumentHistory = false;
}

} // 命名空间 sleekpr::app

#pragma once

#include "sleekpr/core/templates/TemplateDocument.h"

#include <QList>

#include <optional>

namespace sleekpr::app {

class TemplateDesignerState
{
public:
    void resetDocumentHistory(const sleekpr::core::TemplateDocument& document);
    void rememberDocument(const sleekpr::core::TemplateDocument& document);

    bool canUndo() const;
    bool canRedo() const;
    std::optional<sleekpr::core::TemplateDocument> undoDocument();
    std::optional<sleekpr::core::TemplateDocument> redoDocument();

    bool isRestoringDocumentHistory() const;
    void beginDocumentHistoryRestore();
    void endDocumentHistoryRestore();

private:
    QList<sleekpr::core::TemplateDocument> m_documentHistory;
    int m_documentHistoryIndex = -1;
    bool m_restoringDocumentHistory = false;
};

} // 命名空间 sleekpr::app

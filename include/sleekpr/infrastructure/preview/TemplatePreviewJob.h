#pragma once

#include "sleekpr/infrastructure/preview/PreviewExecutor.h"
#include "sleekpr/infrastructure/printing/TemplatePrintService.h"

namespace sleekpr::infrastructure {

struct TemplatePreviewSnapshot
{
    TemplatePrintSnapshot resolved;
    std::optional<core::NativePrintDrawingPlan> preparedPlan;
    std::optional<core::PrintClientSettings> legacySettings;
    std::optional<core::TableElement> selectedTable;
    int pageIndex = 0;
    double zoom = 1;
    double devicePixelRatio = 1;
    double maxPreviewDpi = 0;
    bool designMode = false;
    bool renderImage = true;
    core::PreviewResourceBudget imageBudget;
    core::PreviewPlanningBudget planningBudget;
};

class TemplatePreviewJob
{
public:
    static PreviewWorkResult run(const TemplatePreviewSnapshot& snapshot);
};

} // 命名空间 sleekpr::infrastructure

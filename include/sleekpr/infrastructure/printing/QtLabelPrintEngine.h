#pragma once

#include "sleekpr/infrastructure/printing/LabelPrintEngine.h"

namespace sleekpr::infrastructure {

class QtLabelPrintEngine final : public LabelPrintEngine
{
public:
    bool print(const sleekpr::core::NativeLabelDrawingPlan& plan, const QString& printerName) override;
};

} // 命名空间 sleekpr::infrastructure

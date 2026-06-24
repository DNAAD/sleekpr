#pragma once

#include "sleekpr/core/labels/LabelRenderPlan.h"
#include "sleekpr/core/native/NativePrintDrawingPlan.h"
#include "sleekpr/core/settings/PrintClientSettings.h"

#include <QJsonObject>
#include <QString>

#include <optional>

namespace sleekpr::http {

struct TemplateRequestPlan
{
    QString requestId;
    QString templateKey;
    sleekpr::core::NativePrintDrawingPlan printPlan;
};

std::optional<TemplateRequestPlan> buildTemplateRequestPlan(
    const sleekpr::core::PrintClientSettings& settings,
    const QString& settingsPath,
    const QJsonObject& root,
    const QString& selectedPrinter,
    QString* errorCode,
    QString* errorMessage);

sleekpr::core::NativePrintDrawingPlan createPrintPlan(
    const sleekpr::core::LabelRenderPlan& labelPlan,
    const sleekpr::core::PrintClientSettings& settings,
    const QString& settingsPath,
    const QString& printerName);

} // 命名空间 sleekpr::http

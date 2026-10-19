#pragma once

#include "sleekpr/core/labels/LabelRenderPlan.h"
#include "sleekpr/core/native/NativePrintDrawingPlan.h"
#include "sleekpr/core/printing/PrintPreflightReport.h"
#include "sleekpr/core/settings/PrintClientSettings.h"
#include "sleekpr/core/templates/FieldPreset.h"
#include "sleekpr/core/templates/TemplateDocument.h"
#include "sleekpr/core/templates/TemplateRenderContext.h"

#include <QJsonObject>
#include <QList>
#include <QString>

#include <optional>

namespace sleekpr::infrastructure {

struct TemplatePrintChoice
{
    // key 区分内嵌模板和模板库，不能用可能重名的显示名称代替。
    QString key;
    QString title;
    sleekpr::core::TemplateDocument document;
};

struct TemplatePrintInput
{
    QString templateId;
    QJsonObject values;
    QString fieldPresetId;
    QString printerName;
    // 仅桌面入口启用完整性阻断，默认关闭以保持 HTTP 调用兼容。
    bool requireCompleteValues = false;
};

struct PreparedTemplatePrint
{
    QString templateKey;
    sleekpr::core::TemplateDocument document;
    sleekpr::core::NativePrintDrawingPlan printPlan;
    sleekpr::core::TemplateRenderContext context;
};

class TemplatePrintService
{
public:
    explicit TemplatePrintService(QString settingsPath);

    QList<TemplatePrintChoice> templates(
        const core::PrintClientSettings& settings,
        QString* errorMessage = nullptr) const;
    std::optional<core::TemplateDocument> findTemplate(
        const core::PrintClientSettings& settings,
        const QString& templateId) const;
    std::optional<PreparedTemplatePrint> prepare(
        const core::PrintClientSettings& settings,
        const TemplatePrintInput& input,
        QString* errorCode = nullptr,
        QString* errorMessage = nullptr,
        core::PrintPreflightReport* preflight = nullptr) const;
    core::NativePrintDrawingPlan createLegacyPlan(
        const core::LabelRenderPlan& labelPlan,
        const core::PrintClientSettings& settings,
        const QString& printerName) const;
    QList<core::FieldPreset> fieldPresets(QString* errorMessage = nullptr) const;
    bool saveFieldPreset(const core::FieldPreset& preset, QString* errorMessage = nullptr) const;

private:
    QString m_settingsPath;
};

} // 命名空间 sleekpr::infrastructure

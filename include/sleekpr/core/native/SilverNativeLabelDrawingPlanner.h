#pragma once

#include "sleekpr/core/labels/LabelRenderPlan.h"
#include "sleekpr/core/native/NativeLabelDrawingPlan.h"
#include "sleekpr/core/settings/LabelOffset.h"
#include "sleekpr/core/settings/TemplateElement.h"
#include "sleekpr/core/settings/TemplateElementOverride.h"

#include <QHash>
#include <QList>

namespace sleekpr::core {

class SilverNativeLabelDrawingPlanner
{
public:
    // factoryNo=25003 的银标模板坐标，与默认模板分开维护，避免两个版式互相污染。
    NativeLabelDrawingPlan createPlan(
        const LabelRenderPlan& labelPlan,
        const LabelOffset& offset = {},
        const QHash<QString, TemplateElementOverride>& overrides = {},
        const QList<TemplateElement>& templateElements = QList<TemplateElement>{}) const;

private:
    static NativeDrawCommand text(
        double x,
        double y,
        double width,
        double height,
        const QString& value,
        double fontSizePt,
        bool bold,
        const QString& elementKey,
        double rotationDegrees = 0.0,
        int maxLines = 0,
        bool ellipsis = false);
    static NativeDrawCommand qrCode(double x, double y, double size, const QString& payload, const QString& elementKey);
    static void addWeightCommands(QList<NativeDrawCommand>& commands, const QString& value, double x, double y, const QString& keyPrefix);
    static void addPartCommands(QList<NativeDrawCommand>& commands, const LabelRenderPlan& labelPlan, double offsetX, double offsetY);
    static double calculateFooterY(const LabelRenderPlan& labelPlan);
};

} // 命名空间 sleekpr::core

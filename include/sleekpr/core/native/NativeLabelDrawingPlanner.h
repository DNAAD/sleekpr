#pragma once

#include "sleekpr/core/labels/LabelRenderPlan.h"
#include "sleekpr/core/native/NativeLabelDrawingPlan.h"
#include "sleekpr/core/settings/LabelOffset.h"
#include "sleekpr/core/settings/TemplateElement.h"
#include "sleekpr/core/settings/TemplateElementOverride.h"

#include <QHash>
#include <QList>

namespace sleekpr::core {

class NativeLabelDrawingPlanner
{
public:
    // 将展示文本转换为毫米坐标绘制命令；渲染和打印后端必须消费该计划，避免重复实现布局规则。
    NativeLabelDrawingPlan createPlan(
        const LabelRenderPlan& labelPlan,
        const LabelOffset& offset = {},
        const QHash<QString, TemplateElementOverride>& overrides = {},
        const QList<TemplateElement>& templateElements = QList<TemplateElement>{}) const;

    // 银标模板复用同一套覆盖语义，公开该方法可以避免重复 key 相对位移规则出现两份实现。
    static QList<NativeDrawCommand> applyOverrides(
        const QList<NativeDrawCommand>& commands,
        const QHash<QString, TemplateElementOverride>& overrides,
        double offsetX,
        double offsetY);

    // 自定义模板元素统一追加在默认模板命令之后，保证旧覆盖逻辑先完成再叠加新增内容。
    static QList<NativeDrawCommand> appendTemplateElements(
        const QList<NativeDrawCommand>& commands,
        const LabelRenderPlan& labelPlan,
        const QList<TemplateElement>& templateElements,
        double offsetX,
        double offsetY);

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
    static NativeDrawCommand rectangle(double x, double y, double width, double height, const QString& elementKey);
    static QString valueForField(const LabelRenderPlan& labelPlan, const QString& fieldKey);
    static void addWeightCommands(QList<NativeDrawCommand>& commands, const QString& value, double x, double y, const QString& keyPrefix);
    static void addPartCommands(QList<NativeDrawCommand>& commands, const LabelRenderPlan& labelPlan, double offsetX, double offsetY);
    static double calculateFooterY(const LabelRenderPlan& labelPlan);
};

} // 命名空间 sleekpr::core

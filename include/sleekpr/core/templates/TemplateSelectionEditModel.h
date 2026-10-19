#pragma once

#include "sleekpr/core/templates/TemplateDocument.h"

#include <QPointF>
#include <QRectF>
#include <QSizeF>
#include <QStringList>

namespace sleekpr::core {

enum class SelectionAlignment
{
    Left,
    HorizontalCenter,
    Right,
    Top,
    VerticalCenter,
    Bottom,
    DistributeHorizontally,
    DistributeVertically,
};

class TemplateSelectionEditModel
{
public:
    // 仅处理可见、未锁定图层内可见且未锁定的普通元素；所有几何参数均为毫米。
    // 选区使用元素未旋转的设计矩形，不修改旋转、字体或设备校准数据。
    static QRectF bounds(const TemplateDocument& document, const QStringList& ids);

    // 返回是否实际修改文档；未知标识会忽略，非法几何或无法整体容纳时不做部分修改。
    static bool move(TemplateDocument& document, const QStringList& ids, QPointF deltaMm, QSizeF paperSizeMm);
    // 按选区的横纵比例同步缩放位置和尺寸，最小边为 0.5 毫米，不缩放字体或网格行高。
    static bool resize(TemplateDocument& document, const QStringList& ids, QRectF targetBoundsMm, QSizeF paperSizeMm);
    // 单选参照纸张、多选参照选区；均匀分布至少需要三个可编辑元素，按边缘间距计算。
    static bool align(TemplateDocument& document, const QStringList& ids, SelectionAlignment alignment, QSizeF paperSizeMm);

    // 复制保持源图层顺序及层内绘制顺序；粘贴保持输入顺序，返回按此顺序生成的新标识。
    static QStringList duplicate(TemplateDocument& document, const QStringList& ids, const QString& targetLayerId,
        QPointF offsetMm, QSizeF paperSizeMm);
    // 粘贴不依赖来源图层是否仍存在，只校验元素自身状态及当前目标图层是否可写。
    static QStringList paste(TemplateDocument& document, const QList<TemplateElement>& elements, const QString& targetLayerId,
        QPointF offsetMm, QSizeF paperSizeMm);
};

} // 命名空间 sleekpr::core

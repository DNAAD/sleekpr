#pragma once

#include <QString>

namespace sleekpr::core {

enum class TemplateElementType
{
    FixedText,
    BoundField,
    QrCode,
    Rectangle,
    ArrayGrid,
};

struct TemplateElement
{
    // 自定义元素的稳定标识，设置窗口、命中检测和绘制计划都依赖它定位同一个元素。
    QString id;

    // 所属图层；为空时表示来自旧版 templateElements 列表。
    QString layerId;

    // 图层内排序值，数值越小越先绘制。
    int zIndex = 0;

    // 元素级显示和锁定状态，图层锁定时优先生效。
    bool visible = true;
    bool locked = false;

    // 元素类型决定如何把模板配置转换成原生绘制命令。
    TemplateElementType type = TemplateElementType::FixedText;

    // 仅用于设置窗口展示，缺省时可由类型和序号生成。
    QString displayName;

    // 以下坐标和尺寸均为标签纸内的毫米值，不包含整体偏移。
    double x = 0.0;
    double y = 0.0;
    double width = 10.0;
    double height = 3.0;

    // 固定文本元素直接绘制该内容。
    QString text;

    // 绑定字段和动态二维码使用该 key 从 LabelRenderPlan 中取值。
    QString fieldKey;

    // 二维码静态内容；为空时才回退到 fieldKey 对应的字段值。
    QString payload;

    // 文本类元素的字体设置，二维码和矩形会忽略这些值。
    double fontSizePt = 4.0;
    bool bold = false;

    // 文本旋转角度，现有竖排编号依赖该字段保持版式。
    double rotationDegrees = 0.0;

    // 固定文本竖排显示：字符保持正向，只在渲染阶段逐字换行。
    bool verticalText = false;

    // 数组网格用于把上下文中的数组按固定行列数渲染，例如 header_items 的 2 行 3 列。
    QString dataPath;
    int arrayGridRows = 2;
    int arrayGridColumns = 3;
    QString arrayGridCellTemplate = QStringLiteral("${text}:${value}");
    bool arrayGridDrawBorders = true;

    // 文本最大行数，0 表示不限制。
    int maxLines = 0;

    // 文本超出区域时是否使用省略号。
    bool ellipsis = false;
};

} // 命名空间 sleekpr::core

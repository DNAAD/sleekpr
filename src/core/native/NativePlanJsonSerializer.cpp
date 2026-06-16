#include "sleekpr/core/native/NativePlanJsonSerializer.h"

#include <QJsonArray>

namespace sleekpr::core {

QJsonObject NativePlanJsonSerializer::toJson(const NativeLabelDrawingPlan& plan) const
{
    QJsonArray commands;
    for (const auto& command : plan.commands) {
        // JSON 字段名保持稳定，方便浏览器调试和后续自动化比对。
        commands.append(QJsonObject{
            {"type", commandTypeName(command.type)},
            {"x", command.x},
            {"y", command.y},
            {"width", command.width},
            {"height", command.height},
            {"text", command.text},
            {"fontSizePt", command.fontSizePt},
            {"bold", command.bold},
            {"rotationDegrees", command.rotationDegrees},
            {"maxLines", command.maxLines},
            {"ellipsis", command.ellipsis},
            {"elementKey", command.elementKey},
        });
    }

    return QJsonObject{
        {"paperSize", QJsonObject{
            {"widthMm", plan.paperSize.widthMm()},
            {"heightMm", plan.paperSize.heightMm()},
            {"widthHundredthsInch", plan.paperSize.widthHundredthsInch()},
            {"heightHundredthsInch", plan.paperSize.heightHundredthsInch()},
        }},
        {"commands", commands},
    };
}

QString NativePlanJsonSerializer::commandTypeName(NativeDrawCommandType type)
{
    switch (type) {
    case NativeDrawCommandType::Text:
        return "text";
    case NativeDrawCommandType::QrCode:
        return "qrCode";
    case NativeDrawCommandType::Rectangle:
        return "rectangle";
    }
    return "unknown";
}

} // 命名空间 sleekpr::core

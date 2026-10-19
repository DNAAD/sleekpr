#pragma once

#include "sleekpr/core/templates/TemplateInputSchema.h"

#include <QList>
#include <QSizeF>
#include <QString>
#include <QStringList>

#include <optional>

namespace sleekpr::core {

enum class PrintPreflightSeverity { Warning, Error };
enum class PrintPreflightTarget { General, Field, Json, Element, Printer, Copies };

struct PrintPreflightIssue
{
    PrintPreflightSeverity severity = PrintPreflightSeverity::Error;
    QString code;
    QString message;
    PrintPreflightTarget target = PrintPreflightTarget::General;
    QString fieldKey;
    QStringList elementIds;
    // 行列从 1 开始；0 表示不能可靠定位，禁止解析本地化错误文字猜测位置。
    int line = 0;
    int column = 0;
    std::optional<TemplateInputIssue> inputIssue;
};

struct PrintPreflightReport
{
    QList<PrintPreflightIssue> issues;
    QSizeF paperSizeMm;
    // 定位只使用这次检查的模板快照，不能点击时重新读盘而误指向另一版元素。
    std::optional<TemplateDocument> document;
    // 诊断图使用毫米偏移，与正式渲染中合入纸张边距后的坐标一致。
    double offsetXmm = 0;
    double offsetYmm = 0;
    double scaleX = 1;
    double scaleY = 1;
};

} // 命名空间 sleekpr::core

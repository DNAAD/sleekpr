#include "sleekpr/app/TableColumnTextCodec.h"

#include <QStringList>

namespace sleekpr::app {

QString TableColumnTextCodec::format(const QList<sleekpr::core::TableColumn>& columns)
{
    // 兼容旧版高级列配置文本，格式为：标题=字段:宽度。
    QStringList parts;
    for (const auto& column : columns) {
        parts.append(QStringLiteral("%1=%2:%3").arg(
            column.title,
            column.fieldKey,
            QString::number(column.widthMm, 'f', 2)));
    }
    return parts.join(QStringLiteral(","));
}

QList<sleekpr::core::TableColumn> TableColumnTextCodec::parse(const QString& text, QString* errorMessage)
{
    QList<sleekpr::core::TableColumn> columns;
    const auto parts = text.split(QStringLiteral(","), Qt::SkipEmptyParts);
    for (const auto& part : parts) {
        // 只解析旧文本入口，新的表格列编辑器会直接传结构化模型。
        const auto trimmed = part.trimmed();
        const auto equalsIndex = trimmed.indexOf(QStringLiteral("="));
        const auto widthIndex = trimmed.lastIndexOf(QStringLiteral(":"));
        if (equalsIndex <= 0 || widthIndex <= equalsIndex + 1 || widthIndex >= trimmed.size() - 1) {
            if (errorMessage != nullptr) {
                *errorMessage = QString::fromUtf8("列配置格式应为：标题=字段:宽度");
            }
            return {};
        }

        bool widthOk = false;
        const auto width = trimmed.mid(widthIndex + 1).trimmed().toDouble(&widthOk);
        const auto title = trimmed.left(equalsIndex).trimmed();
        const auto fieldKey = trimmed.mid(equalsIndex + 1, widthIndex - equalsIndex - 1).trimmed();
        if (!widthOk || width <= 0.0 || title.isEmpty() || fieldKey.isEmpty()) {
            if (errorMessage != nullptr) {
                *errorMessage = QString::fromUtf8("列标题、字段和宽度必须有效");
            }
            return {};
        }

        sleekpr::core::TableColumn column;
        column.id = fieldKey;
        column.title = title;
        column.fieldKey = fieldKey;
        column.widthMm = width;
        columns.append(column);
    }
    return columns;
}

} // namespace sleekpr::app

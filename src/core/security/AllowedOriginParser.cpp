#include "sleekpr/core/security/AllowedOriginParser.h"

#include <QRegularExpression>
#include <QSet>
#include <QUrl>

namespace sleekpr::core {

QStringList AllowedOriginParser::parse(const QString& text)
{
    QStringList origins;
    QSet<QString> seen;

    for (const auto& rawLine : text.split(QRegularExpression("[\\r\\n]+"), Qt::SkipEmptyParts)) {
        const QUrl url(rawLine.trimmed());
        // 设置页只接受完整 Origin；路径、查询串和片段会导致 CORS 语义不明确。
        if (!url.isValid()
            || (!url.path().isEmpty() && url.path() != "/")
            || !url.query().isEmpty()
            || !url.fragment().isEmpty()) {
            continue;
        }
        if (url.scheme() != "http" && url.scheme() != "https") {
            continue;
        }

        QString origin = url.scheme() + "://" + url.host();
        if (url.port() > 0) {
            // 非默认端口必须保留，否则开发环境 localhost 端口会被错误合并。
            origin += ":" + QString::number(url.port());
        }

        if (!seen.contains(origin)) {
            // 保持用户输入顺序，同时去除重复项。
            origins.append(origin);
            seen.insert(origin);
        }
    }

    return origins;
}

} // 命名空间 sleekpr::core

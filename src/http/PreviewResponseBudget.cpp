#include "sleekpr/http/PreviewResponseBudget.h"
#include "sleekpr/http/LocalHttpRouteSupport.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <limits>

namespace sleekpr::http {

PreviewResponseBudget::PreviewResponseBudget(qsizetype maximum)
    : m_maximum(maximum < 0 ? std::numeric_limits<qsizetype>::max() : maximum) {}

bool PreviewResponseBudget::appendPage(const QJsonObject& page)
{
    const auto bytes = QJsonDocument(page).toJson(QJsonDocument::Compact).size();
    const qsizetype comma = m_pageCount > 0 ? 1 : 0;
    if (m_pageCount == std::numeric_limits<int>::max() || m_pageBytes > m_maximum
        || comma > m_maximum - m_pageBytes || bytes > m_maximum - m_pageBytes - comma) return false;
    m_pageBytes += bytes + comma;
    ++m_pageCount;
    return true;
}

qsizetype PreviewResponseBudget::bodyBytes(const QString& id, int items, int pages, const QJsonObject& paper) const
{
    if (!m_cached || m_requestId != id || m_totalItems != items || m_paper != paper) {
        // 固定包络只序列化一次，字段转义由 Qt 完成，不能按 QString 字符数猜 UTF-8 字节数。
        m_envelopeBytes = QJsonDocument(okEnvelope({{QStringLiteral("requestId"), id}, {QStringLiteral("totalItems"), items},
            {QStringLiteral("totalPages"), 0}, {QStringLiteral("paper"), paper}, {QStringLiteral("pages"), QJsonArray{}}}))
            .toJson(QJsonDocument::Compact).size();
        m_requestId = id;
        m_totalItems = items;
        m_paper = paper;
        m_cached = true;
    }
    // 用单值 JSON 数组计算页数位数变化，计入 9→10 等边界，不重写已有 Base64 页。
    const auto numberAdjustment = QJsonDocument(QJsonArray{pages}).toJson(QJsonDocument::Compact).size() - 3;
    const auto maximum = std::numeric_limits<qsizetype>::max();
    if (m_envelopeBytes > maximum - m_pageBytes || numberAdjustment > maximum - m_envelopeBytes - m_pageBytes) return -1;
    return m_envelopeBytes + m_pageBytes + numberAdjustment;
}

} // 命名空间 sleekpr::http

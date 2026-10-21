#pragma once

#include <QJsonObject>
#include <QString>

namespace sleekpr::http {

class PreviewResponseBudget
{
public:
    explicit PreviewResponseBudget(qsizetype maximum);
    bool appendPage(const QJsonObject& page);
    qsizetype bodyBytes(const QString& requestId, int totalItems, int totalPages, const QJsonObject& paper) const;
private:
    qsizetype m_maximum;
    qsizetype m_pageBytes = 0;
    int m_pageCount = 0;
    mutable bool m_cached = false;
    mutable QString m_requestId;
    mutable int m_totalItems = 0;
    mutable QJsonObject m_paper;
    mutable qsizetype m_envelopeBytes = 0;
};

} // 命名空间 sleekpr::http

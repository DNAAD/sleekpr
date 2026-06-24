#pragma once

#include "sleekpr/core/settings/PrintClientSettings.h"
#include "sleekpr/http/LocalHttpRouter.h"

#include <QByteArray>
#include <QHash>
#include <QString>

#include <optional>

namespace sleekpr::http {

class PrintRoutes
{
public:
    PrintRoutes(QString settingsPath, sleekpr::infrastructure::LabelPrintEngine* printEngine);

    std::optional<LocalHttpResponse> route(
        const LocalHttpRequest& request,
        const sleekpr::core::PrintClientSettings& settings,
        const QHash<QByteArray, QByteArray>& extraHeaders) const;

private:
    QString m_settingsPath;
    sleekpr::infrastructure::LabelPrintEngine* m_printEngine = nullptr;
};

} // 命名空间 sleekpr::http

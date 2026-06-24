#pragma once

#include "sleekpr/http/LocalHttpRouter.h"

#include <QByteArray>
#include <QHash>
#include <QString>

#include <optional>

namespace sleekpr::http {

class TemplateRoutes
{
public:
    explicit TemplateRoutes(QString settingsPath);

    std::optional<LocalHttpResponse> route(
        const LocalHttpRequest& request,
        const QHash<QByteArray, QByteArray>& extraHeaders) const;

private:
    QString m_settingsPath;
};

} // 命名空间 sleekpr::http

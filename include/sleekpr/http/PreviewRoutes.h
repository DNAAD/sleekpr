#pragma once

#include "sleekpr/core/settings/PrintClientSettings.h"
#include "sleekpr/http/LocalHttpLimits.h"
#include "sleekpr/http/LocalHttpRouter.h"
#include "sleekpr/infrastructure/preview/LabelPreviewImageRenderer.h"

#include <QByteArray>
#include <QHash>
#include <QString>

#include <optional>

namespace sleekpr::http {

class PreviewRoutes
{
public:
    explicit PreviewRoutes(QString settingsPath, LocalHttpLimits limits = {},
        infrastructure::LabelPreviewImageRenderer::PngEncoder encoder = {});

    std::optional<LocalHttpResponse> route(
        const LocalHttpRequest& request,
        const sleekpr::core::PrintClientSettings& settings,
        const QHash<QByteArray, QByteArray>& extraHeaders) const;

private:
    QString m_settingsPath;
    LocalHttpLimits m_limits;
    infrastructure::LabelPreviewImageRenderer::PngEncoder m_encoder;
};

} // 命名空间 sleekpr::http

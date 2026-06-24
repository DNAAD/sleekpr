#pragma once

#include "sleekpr/core/settings/PrintClientSettings.h"
#include "sleekpr/core/templates/FieldPresetStore.h"
#include "sleekpr/core/templates/PaperSpecStore.h"
#include "sleekpr/core/templates/TemplateLibraryStore.h"
#include "sleekpr/http/LocalHttpRouter.h"

#include <QByteArray>
#include <QHash>
#include <QJsonObject>
#include <QJsonValue>
#include <QString>

#include <initializer_list>

namespace sleekpr::http {

QJsonObject okEnvelope(const QJsonObject& data);
QJsonObject failEnvelope(const QString& code, const QString& message);

LocalHttpResponse jsonResponse(
    const QJsonObject& payload,
    int statusCode,
    const QHash<QByteArray, QByteArray>& extraHeaders = {});
LocalHttpResponse emptyResponse(int statusCode, const QHash<QByteArray, QByteArray>& extraHeaders);
LocalHttpResponse requestTooLargeResponse(
    const QString& message,
    const QHash<QByteArray, QByteArray>& extraHeaders = {});

QString headerValue(const LocalHttpRequest& request, const QString& name);
QHash<QByteArray, QByteArray> corsHeaders(
    const LocalHttpRequest& request,
    const sleekpr::core::PrintClientSettings& settings);
bool hasRejectedOrigin(const LocalHttpRequest& request, const sleekpr::core::PrintClientSettings& settings);

QJsonValue valueFor(const QJsonObject& object, std::initializer_list<const char*> names);
QString stringFor(const QJsonObject& object, std::initializer_list<const char*> names);
double doubleFor(const QJsonObject& object, std::initializer_list<const char*> names);
int intFor(const QJsonObject& object, std::initializer_list<const char*> names);

QString requestIdOrFallback(const QString& requestId);
QJsonObject printResultJson(const QString& requestId, int total, int printed, int failed, bool executePrint);

QString templateLibraryDirectoryForSettings(const QString& settingsPath);
sleekpr::core::TemplateLibraryStore templateLibraryStoreForSettings(const QString& settingsPath);
QString paperSpecFilePathForSettings(const QString& settingsPath);
sleekpr::core::PaperSpecStore paperSpecStoreForSettings(const QString& settingsPath);
QString fieldPresetFilePathForSettings(const QString& settingsPath);
sleekpr::core::FieldPresetStore fieldPresetStoreForSettings(const QString& settingsPath);

QString templateIdFromPath(const QString& path);
QString paperSpecIdFromPath(const QString& path);
QString fieldPresetIdFromPath(const QString& path);

} // 命名空间 sleekpr::http

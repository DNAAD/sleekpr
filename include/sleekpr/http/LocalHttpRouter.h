#pragma once

#include "sleekpr/infrastructure/printing/LabelPrintEngine.h"

#include <QByteArray>
#include <QHash>
#include <QString>

namespace sleekpr::http {

struct LocalHttpRequest
{
    QString method;
    QString path;
    QHash<QString, QString> headers;
    QByteArray body;
};

struct LocalHttpResponse
{
    int statusCode = 200;
    QHash<QByteArray, QByteArray> headers;
    QByteArray body;
};

class LocalHttpRouter
{
public:
    explicit LocalHttpRouter(QString settingsPath, sleekpr::infrastructure::LabelPrintEngine* printEngine = nullptr);

    // 路由层不依赖 socket，单元测试可以直接验证 HTTP 契约和打印调用语义。
    LocalHttpResponse route(const LocalHttpRequest& request) const;

private:
    QString m_settingsPath;
    sleekpr::infrastructure::LabelPrintEngine* m_printEngine = nullptr;
};

} // 命名空间 sleekpr::http

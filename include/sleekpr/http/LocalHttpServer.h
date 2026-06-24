#pragma once

#include "sleekpr/http/LocalHttpLimits.h"
#include "sleekpr/http/LocalHttpRouter.h"

#include <QByteArray>
#include <QHash>
#include <QHostAddress>
#include <QJsonObject>
#include <QTcpServer>
#include <QUrl>

class QTcpSocket;

namespace sleekpr::http {

class LocalHttpServer
{
public:
    explicit LocalHttpServer(QString settingsPath, sleekpr::infrastructure::LabelPrintEngine* printEngine = nullptr);
    LocalHttpServer(QString settingsPath, sleekpr::infrastructure::LabelPrintEngine* printEngine, LocalHttpLimits limits);

    // 启动管理端调用的兼容 HTTP 接口；标签预览已迁移到 Qt 原生窗口，不再通过浏览器路由展示。
    bool listen(const QHostAddress& address, quint16 port);
    void setLimits(LocalHttpLimits limits);
    QString errorString() const;
    quint16 serverPort() const;

private:
    void acceptConnection();
    void handleRequest(QTcpSocket* socket, const QByteArray& requestBytes);
    QByteArray httpResponseBytes(const LocalHttpResponse& response) const;

    QTcpServer m_server;
    LocalHttpRouter m_router;
    LocalHttpLimits m_limits;
};

} // 命名空间 sleekpr::http

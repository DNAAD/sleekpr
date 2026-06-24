#include "sleekpr/http/LocalHttpServer.h"
#include "sleekpr/http/LocalHttpRouteSupport.h"

#include <QJsonDocument>
#include <QTcpSocket>

#include <memory>
#include <utility>

namespace sleekpr::http {

namespace {

QHash<QString, QString> parseHeaders(const QList<QByteArray>& lines)
{
    QHash<QString, QString> headers;
    for (int index = 1; index < lines.size(); ++index) {
        const auto separator = lines[index].indexOf(':');
        if (separator <= 0) {
            continue;
        }
        headers.insert(
            // 请求头名称统一转小写，后续查找不再依赖浏览器传入的大小写。
            QString::fromLatin1(lines[index].left(separator)).trimmed().toLower(),
            QString::fromUtf8(lines[index].mid(separator + 1)).trimmed());
    }
    return headers;
}

QByteArray statusReason(int statusCode)
{
    switch (statusCode) {
    case 200:
        return "OK";
    case 204:
        return "No Content";
    case 400:
        return "Bad Request";
    case 403:
        return "Forbidden";
    case 404:
        return "Not Found";
    case 413:
        return "Payload Too Large";
    case 500:
        return "Internal Server Error";
    default:
        return "OK";
    }
}

int headerEndIndex(const QByteArray& requestBytes)
{
    const auto crlfIndex = requestBytes.indexOf("\r\n\r\n");
    if (crlfIndex >= 0) {
        return crlfIndex + 4;
    }
    const auto lfIndex = requestBytes.indexOf("\n\n");
    return lfIndex >= 0 ? lfIndex + 2 : -1;
}

qint64 contentLengthFromHeaders(const QByteArray& headerBytes)
{
    const auto lines = headerBytes.split('\n');
    for (int index = 1; index < lines.size(); ++index) {
        const auto separator = lines[index].indexOf(':');
        if (separator <= 0) {
            continue;
        }

        const auto name = QString::fromLatin1(lines[index].left(separator)).trimmed().toLower();
        if (name != "content-length") {
            continue;
        }

        bool ok = false;
        const auto length = QString::fromLatin1(lines[index].mid(separator + 1)).trimmed().toLongLong(&ok);
        return ok && length > 0 ? length : 0;
    }
    return 0;
}

bool hasCompleteRequestBody(const QByteArray& requestBytes)
{
    const auto bodyStart = headerEndIndex(requestBytes);
    if (bodyStart < 0) {
        return false;
    }

    // QTcpSocket 的 readyRead 可能只拿到部分 POST body，必须按 Content-Length 等完整再交给路由。
    const auto expectedBodyLength = contentLengthFromHeaders(requestBytes.left(bodyStart));
    return static_cast<qint64>(requestBytes.size()) >= static_cast<qint64>(bodyStart) + expectedBodyLength;
}

QString requestLimitErrorMessage(const QByteArray& requestBytes, const LocalHttpLimits& limits)
{
    const auto bodyStart = headerEndIndex(requestBytes);
    if (bodyStart < 0) {
        if (limits.maxHeaderBytes >= 0 && requestBytes.size() > limits.maxHeaderBytes) {
            return QString::fromUtf8("请求头超过本地接口限制。");
        }
        return {};
    }

    if (limits.maxHeaderBytes >= 0 && bodyStart > limits.maxHeaderBytes) {
        return QString::fromUtf8("请求头超过本地接口限制。");
    }

    const auto expectedBodyLength = contentLengthFromHeaders(requestBytes.left(bodyStart));
    if (limits.maxContentLengthBytes >= 0 && expectedBodyLength > limits.maxContentLengthBytes) {
        return QString::fromUtf8("请求体超过本地接口限制。");
    }

    const auto receivedBodyLength = static_cast<qint64>(requestBytes.size() - bodyStart);
    if (limits.maxContentLengthBytes >= 0 && receivedBodyLength > limits.maxContentLengthBytes) {
        return QString::fromUtf8("请求体超过本地接口限制。");
    }

    return {};
}

} // 匿名命名空间

LocalHttpServer::LocalHttpServer(QString settingsPath, sleekpr::infrastructure::LabelPrintEngine* printEngine)
    : LocalHttpServer(std::move(settingsPath), printEngine, LocalHttpLimits{})
{
}

LocalHttpServer::LocalHttpServer(
    QString settingsPath,
    sleekpr::infrastructure::LabelPrintEngine* printEngine,
    LocalHttpLimits limits)
    : m_router(std::move(settingsPath), printEngine, limits)
    , m_limits(limits)
{
    QObject::connect(&m_server, &QTcpServer::newConnection, &m_server, [this] {
        acceptConnection();
    });
}

bool LocalHttpServer::listen(const QHostAddress& address, quint16 port)
{
    // 端口绑定由应用启动阶段统一处理，失败时由 main.cpp 给出用户可见诊断。
    return m_server.listen(address, port);
}

void LocalHttpServer::setLimits(LocalHttpLimits limits)
{
    // socket 阶段和路由阶段必须使用同一份上限，否则超限请求可能在不同层表现不一致。
    m_limits = limits;
    m_router.setLimits(limits);
}

QString LocalHttpServer::errorString() const
{
    return m_server.errorString();
}

quint16 LocalHttpServer::serverPort() const
{
    return m_server.serverPort();
}

void LocalHttpServer::acceptConnection()
{
    while (auto* socket = m_server.nextPendingConnection()) {
        auto requestBuffer = std::make_shared<QByteArray>();
        // 本地接口仍采用短连接，但会缓存同一连接上的分段数据，避免 POST 半包被提前解析。
        QObject::connect(socket, &QTcpSocket::readyRead, socket, [this, socket, requestBuffer] {
            requestBuffer->append(socket->readAll());
            const auto limitErrorMessage = requestLimitErrorMessage(*requestBuffer, m_limits);
            if (!limitErrorMessage.isEmpty()) {
                socket->write(httpResponseBytes(requestTooLargeResponse(limitErrorMessage)));
                socket->disconnectFromHost();
                return;
            }
            if (!hasCompleteRequestBody(*requestBuffer)) {
                return;
            }
            handleRequest(socket, *requestBuffer);
        });
        QObject::connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
    }
}

void LocalHttpServer::handleRequest(QTcpSocket* socket, const QByteArray& requestBytes)
{
    const auto bodyStart = headerEndIndex(requestBytes);
    const auto headerBytes = bodyStart >= 0 ? requestBytes.left(bodyStart) : requestBytes;
    const auto body = bodyStart >= 0 ? requestBytes.mid(bodyStart) : QByteArray{};
    const auto lines = headerBytes.split('\n');
    if (lines.isEmpty()) {
        socket->disconnectFromHost();
        return;
    }

    const auto requestLine = QString::fromLatin1(lines.first()).trimmed().split(' ');
    if (requestLine.size() < 2) {
        // 请求行不完整时直接返回兼容错误包络，避免静默断开导致排查困难。
        socket->write(httpResponseBytes(LocalHttpResponse{
            400,
            {{"Content-Type", "application/json; charset=utf-8"}},
            R"json({"success":false,"code":"BAD_REQUEST","message":"Malformed HTTP request","data":{}})json",
        }));
        socket->disconnectFromHost();
        return;
    }

    const auto method = requestLine[0].toUpper();
    const auto path = QUrl(requestLine[1]).path();
    const auto headers = parseHeaders(lines);

    socket->write(httpResponseBytes(m_router.route(LocalHttpRequest{method, path, headers, body})));
    socket->disconnectFromHost();
}

QByteArray LocalHttpServer::httpResponseBytes(const LocalHttpResponse& routeResponse) const
{
    QByteArray response;
    // 手写 HTTP 响应只用于迁移 POC；所有响应都显式写入长度，避免浏览器等待连接超时。
    response += "HTTP/1.1 " + QByteArray::number(routeResponse.statusCode) + " " + statusReason(routeResponse.statusCode) + "\r\n";
    for (auto it = routeResponse.headers.begin(); it != routeResponse.headers.end(); ++it) {
        response += it.key() + ": " + it.value() + "\r\n";
    }
    response += "Content-Length: " + QByteArray::number(routeResponse.body.size()) + "\r\n";
    response += "Connection: close\r\n\r\n";
    response += routeResponse.body;
    return response;
}

} // 命名空间 sleekpr::http

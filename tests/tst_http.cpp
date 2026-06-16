#include <QtTest/QtTest>

#include <QJsonDocument>
#include <QJsonObject>
#include <QTcpSocket>
#include <QTemporaryDir>

#include "sleekpr/core/settings/FileSettingsStore.h"
#include "sleekpr/http/LocalHttpRouter.h"
#include "sleekpr/http/LocalHttpServer.h"
#include "sleekpr/infrastructure/printing/LabelPrintEngine.h"

using namespace sleekpr::core;
using namespace sleekpr::http;
using namespace sleekpr::infrastructure;

class RecordingLabelPrintEngine final : public LabelPrintEngine
{
public:
    bool print(const NativeLabelDrawingPlan& plan, const QString& printerName) override
    {
        ++printCount;
        lastPrinterName = printerName;
        lastPlanCommandCount = plan.commands.size();
        return true;
    }

    int printCount = 0;
    int lastPlanCommandCount = 0;
    QString lastPrinterName;
};

class HttpTests final : public QObject
{
    Q_OBJECT

private slots:
    void optionsUsesTrustedCorsPolicy();
    void postSettingsPersistsFullSettingsShape();
    void postPrintTagCreatesAcceptedJobAndUsesConfiguredPrinter();
    void localServerWaitsForFullPostBody();
};

void HttpTests::optionsUsesTrustedCorsPolicy()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    FileSettingsStore store(dir.filePath("settings.json"));
    PrintClientSettings settings;
    settings.allowedOrigins = {"https://manager.example.com"};
    QVERIFY(store.save(settings));

    LocalHttpRouter router(dir.filePath("settings.json"));
    LocalHttpRequest trustedRequest;
    trustedRequest.method = "OPTIONS";
    trustedRequest.path = "/print/tag";
    trustedRequest.headers.insert("origin", "https://manager.example.com");
    trustedRequest.headers.insert("access-control-request-method", "POST");
    trustedRequest.headers.insert("access-control-request-private-network", "true");

    const auto trustedResponse = router.route(trustedRequest);
    QCOMPARE(trustedResponse.statusCode, 204);
    QCOMPARE(trustedResponse.headers.value("Access-Control-Allow-Origin"), QByteArray("https://manager.example.com"));
    QCOMPARE(trustedResponse.headers.value("Access-Control-Allow-Private-Network"), QByteArray("true"));

    LocalHttpRequest untrustedRequest = trustedRequest;
    untrustedRequest.headers["origin"] = "https://evil.example.com";
    const auto untrustedResponse = router.route(untrustedRequest);
    QCOMPARE(untrustedResponse.statusCode, 403);
    QVERIFY(!untrustedResponse.headers.contains("Access-Control-Allow-Origin"));
}

void HttpTests::postSettingsPersistsFullSettingsShape()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    LocalHttpRouter router(dir.filePath("settings.json"));
    LocalHttpRequest request;
    request.method = "POST";
    request.path = "/settings";
    request.body = R"json({
        "defaultPrinter": "Printer A",
        "labelOffset": { "x": 1.2, "y": -0.4 },
        "allowedOrigins": ["https://manager.example.com"],
        "templateOverrides": {
            "silver": {
                "priceText": { "x": 17.5, "y": 12.6, "fontSizePt": 5.5, "bold": true }
            }
        }
    })json";

    const auto response = router.route(request);
    QCOMPARE(response.statusCode, 200);

    const auto actual = FileSettingsStore(dir.filePath("settings.json")).load();
    QCOMPARE(actual.defaultPrinter, QString("Printer A"));
    QCOMPARE(actual.labelOffset.x, 1.2);
    QCOMPARE(actual.labelOffset.y, -0.4);
    QCOMPARE(actual.allowedOrigins, QStringList{"https://manager.example.com"});
    const auto priceText = actual.templateOverrides.value("silver").value("priceText");
    QCOMPARE(priceText.x.value(), 17.5);
    QCOMPARE(priceText.fontSizePt.value(), 5.5);
    QCOMPARE(priceText.bold.value(), true);
}

void HttpTests::postPrintTagCreatesAcceptedJobAndUsesConfiguredPrinter()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    PrintClientSettings settings;
    settings.defaultPrinter = "Configured Printer";
    QVERIFY(FileSettingsStore(dir.filePath("settings.json")).save(settings));

    RecordingLabelPrintEngine printEngine;
    LocalHttpRouter router(dir.filePath("settings.json"), &printEngine);

    LocalHttpRequest request;
    request.method = "POST";
    request.path = "/print/tag";
    request.body = R"json({
        "requestId": "request-1",
        "printerName": "Ignored Printer",
        "executePrint": true,
        "items": [{
            "identifierCode": "10000000789",
            "productName": "Gold Ring",
            "weightCategory": "Finished",
            "finishedProductWeight": "3.20",
            "roughWeight": "3.56",
            "salesCode": "XS-001",
            "goldPurity": "Au999",
            "address": "Store 1",
            "finishedProductPartVO": [
                { "categoryName": "Ring", "partWeight": "3.20" }
            ]
        }]
    })json";

    const auto response = router.route(request);
    QCOMPARE(response.statusCode, 200);
    QCOMPARE(printEngine.printCount, 1);
    QCOMPARE(printEngine.lastPrinterName, QString("Configured Printer"));
    QVERIFY(printEngine.lastPlanCommandCount > 0);

    const auto payload = QJsonDocument::fromJson(response.body).object();
    QVERIFY(payload["success"].toBool());
    const auto data = payload["data"].toObject();
    QCOMPARE(data["requestId"].toString(), QString("request-1"));
    QCOMPARE(data["accepted"].toInt(), 1);
    QCOMPARE(data["total"].toInt(), 1);
    QCOMPARE(data["printed"].toInt(), 1);
    QCOMPARE(data["failed"].toInt(), 0);
}

void HttpTests::localServerWaitsForFullPostBody()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    LocalHttpServer server(dir.filePath("settings.json"));
    QVERIFY(server.listen(QHostAddress::LocalHost, 0));

    QTcpSocket socket;
    socket.connectToHost(QHostAddress::LocalHost, server.serverPort());
    QVERIFY(socket.waitForConnected());

    const QByteArray body = R"json({"defaultPrinter":"Chunked Printer"})json";
    const QByteArray headers =
        "POST /settings HTTP/1.1\r\n"
        "Host: 127.0.0.1\r\n"
        "Content-Type: application/json\r\n"
        "Content-Length: " + QByteArray::number(body.size()) + "\r\n\r\n";

    // 先只发送请求头和一小段 body，服务器必须等待 Content-Length 声明的完整 JSON。
    socket.write(headers + body.left(8));
    QVERIFY(socket.waitForBytesWritten());
    QTest::qWait(80);
    QCOMPARE(socket.bytesAvailable(), qint64{0});

    socket.write(body.mid(8));
    QVERIFY(socket.waitForBytesWritten());
    QTRY_VERIFY_WITH_TIMEOUT(socket.bytesAvailable() > 0, 1000);

    const auto response = socket.readAll();
    QVERIFY(response.startsWith("HTTP/1.1 200 OK"));
    QCOMPARE(FileSettingsStore(dir.filePath("settings.json")).load().defaultPrinter, QString("Chunked Printer"));
}

QTEST_MAIN(HttpTests)
#include "tst_http.moc"

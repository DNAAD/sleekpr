#include <QtTest/QtTest>

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTcpSocket>
#include <QTemporaryDir>

#include <algorithm>

#include "sleekpr/core/settings/FileSettingsStore.h"
#include "sleekpr/core/templates/TemplateDocumentJson.h"
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
        usedSinglePageEntry = true;
        lastPrinterName = printerName;
        lastPlanCommandCount = plan.commands.size();
        lastPlan = plan;
        return true;
    }

    bool print(const NativePrintDrawingPlan& plan, const QString& printerName) override
    {
        ++printCount;
        usedPrintPlanEntry = true;
        lastPrinterName = printerName;
        lastPrintPlanPageCount = plan.pages.size();
        lastPlan = plan.firstPageAsLabelPlan();
        lastPlanCommandCount = lastPlan.commands.size();
        return true;
    }

    int printCount = 0;
    int lastPlanCommandCount = 0;
    int lastPrintPlanPageCount = 0;
    bool usedSinglePageEntry = false;
    bool usedPrintPlanEntry = false;
    QString lastPrinterName;
    NativeLabelDrawingPlan lastPlan;
};

class PageCollectingPrintEngine final : public LabelPrintEngine
{
public:
    using LabelPrintEngine::print;

    bool print(const NativeLabelDrawingPlan& plan, const QString& printerName) override
    {
        printerNames.append(printerName);
        printedPages.append(plan);
        return true;
    }

    QStringList printerNames;
    QList<NativeLabelDrawingPlan> printedPages;
};

TemplateDocument createHttpInvoiceTemplateDocument()
{
    FieldDefinition customerName;
    customerName.key = "customerName";
    customerName.displayName = QString::fromUtf8("客户名称");
    customerName.required = true;

    TemplateElement customerText;
    customerText.id = "customerNameText";
    customerText.type = TemplateElementType::BoundField;
    customerText.fieldKey = "customerName";
    customerText.x = 1.0;
    customerText.y = 2.0;
    customerText.width = 30.0;
    customerText.height = 5.0;
    customerText.fontSizePt = 9.0;

    TemplateLayer layer;
    layer.id = "base";
    layer.name = QString::fromUtf8("基础图层");
    layer.elements = {customerText};

    TemplateDocument document;
    document.id = "template-invoice";
    document.name = QString::fromUtf8("客户标签");
    document.templateKey = "invoice";
    document.category = "label";
    document.fieldSchema = {customerName};
    document.layers = {layer};
    return document;
}

class HttpTests final : public QObject
{
    Q_OBJECT

private slots:
    void optionsUsesTrustedCorsPolicy();
    void getSettingsReturnsTemplateCompatibilityFields();
    void postSettingsPersistsFullSettingsShape();
    void labelPrintEngineDispatchesPrintPlanPagesInOrder();
    void postPrintTagCreatesAcceptedJobAndUsesConfiguredPrinter();
    void templateLibraryRoutesPersistReadAndRemoveTemplates();
    void postPrintTemplateUsesRequestValuesAndConfiguredPrinter();
    void postPrintTemplateCanUseTemplateLibraryDocument();
    void postPrintTemplateRejectsMissingRequiredFields();
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

void HttpTests::getSettingsReturnsTemplateCompatibilityFields()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    QFile file(dir.filePath(QStringLiteral("settings.json")));
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
    file.write(R"json({
        "templateOverrides": {},
        "templateElements": {
            "default": [{
                "id": "legacy-fixed",
                "type": "fixedText",
                "text": "LEGACY"
            }]
        }
    })json");
    file.close();

    LocalHttpRouter router(dir.filePath("settings.json"));
    LocalHttpRequest request;
    request.method = "GET";
    request.path = "/settings";

    const auto response = router.route(request);
    QCOMPARE(response.statusCode, 200);

    const auto root = QJsonDocument::fromJson(response.body).object();
    const auto data = root["data"].toObject();
    QVERIFY(data.contains("templateOverrides"));
    QVERIFY(data["templateOverrides"].isObject());
    QVERIFY(data.contains("templateElements"));
    QVERIFY(data["templateElements"].isObject());
    QVERIFY(data.contains("templateDocuments"));
    QVERIFY(data["templateDocuments"].isObject());
    QVERIFY(data["templateDocuments"].toObject().isEmpty());
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

void HttpTests::labelPrintEngineDispatchesPrintPlanPagesInOrder()
{
    NativePrintDrawingPlan printPlan;
    printPlan.paperSize = LabelPaperSize::create80x30();
    printPlan.renderDpi = 203.0;

    NativeDrawCommand firstCommand;
    firstCommand.text = QStringLiteral("第一页");
    NativePrintPage firstPage;
    firstPage.pageNumber = 1;
    firstPage.commands.append(firstCommand);
    printPlan.pages.append(firstPage);

    NativeDrawCommand secondCommand;
    secondCommand.text = QStringLiteral("第二页");
    NativePrintPage secondPage;
    secondPage.pageNumber = 2;
    secondPage.commands.append(secondCommand);
    printPlan.pages.append(secondPage);

    PageCollectingPrintEngine printEngine;
    QVERIFY(printEngine.print(printPlan, QStringLiteral("Printer A")));
    QCOMPARE(printEngine.printedPages.size(), 2);
    QCOMPARE(printEngine.printedPages[0].renderDpi, 203.0);
    QCOMPARE(printEngine.printedPages[0].commands.first().text, QStringLiteral("第一页"));
    QCOMPARE(printEngine.printedPages[1].commands.first().text, QStringLiteral("第二页"));
    QCOMPARE(printEngine.printerNames, QStringList({QStringLiteral("Printer A"), QStringLiteral("Printer A")}));
}

void HttpTests::postPrintTagCreatesAcceptedJobAndUsesConfiguredPrinter()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    PrintClientSettings settings;
    settings.defaultPrinter = "Configured Printer";
    TemplateElement configuredText;
    configuredText.id = "configuredText";
    configuredText.type = TemplateElementType::FixedText;
    configuredText.text = "PROFILED";
    configuredText.x = 1.0;
    configuredText.y = 1.0;
    configuredText.width = 10.0;
    configuredText.height = 3.0;

    TemplateLayer layer;
    layer.id = "base";
    layer.elements = {configuredText};

    DeviceProfile profile;
    profile.printerName = "Configured Printer";
    profile.offsetX = 2.0;
    profile.offsetY = 3.0;
    profile.dpi = 203.0;

    TemplateDocument document;
    document.templateKey = "default";
    document.layers = {layer};
    document.deviceProfiles = {profile};
    settings.templateDocuments["default"] = document;
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
    QVERIFY(printEngine.usedPrintPlanEntry);
    QVERIFY(!printEngine.usedSinglePageEntry);
    QCOMPARE(printEngine.lastPrintPlanPageCount, 1);
    QCOMPARE(printEngine.lastPrinterName, QString("Configured Printer"));
    QVERIFY(printEngine.lastPlanCommandCount > 0);
    const auto configuredCommand = std::find_if(
        printEngine.lastPlan.commands.cbegin(),
        printEngine.lastPlan.commands.cend(),
        [](const NativeDrawCommand& command) {
            return command.elementKey == QStringLiteral("configuredText");
        });
    QVERIFY(configuredCommand != printEngine.lastPlan.commands.cend());
    QCOMPARE(configuredCommand->x, 3.0);
    QCOMPARE(configuredCommand->y, 4.0);
    QCOMPARE(printEngine.lastPlan.renderDpi, 203.0);

    const auto payload = QJsonDocument::fromJson(response.body).object();
    QVERIFY(payload["success"].toBool());
    const auto data = payload["data"].toObject();
    QCOMPARE(data["requestId"].toString(), QString("request-1"));
    QCOMPARE(data["accepted"].toInt(), 1);
    QCOMPARE(data["total"].toInt(), 1);
    QCOMPARE(data["printed"].toInt(), 1);
    QCOMPARE(data["failed"].toInt(), 0);
}

void HttpTests::templateLibraryRoutesPersistReadAndRemoveTemplates()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    LocalHttpRouter router(dir.filePath("settings.json"));
    const auto templateJson = TemplateDocumentJson::toJson(createHttpInvoiceTemplateDocument());

    LocalHttpRequest saveRequest;
    saveRequest.method = "POST";
    saveRequest.path = "/templates";
    saveRequest.body = QJsonDocument(templateJson).toJson(QJsonDocument::Compact);

    const auto saveResponse = router.route(saveRequest);
    QCOMPARE(saveResponse.statusCode, 200);

    LocalHttpRequest listRequest;
    listRequest.method = "GET";
    listRequest.path = "/templates";

    const auto listResponse = router.route(listRequest);
    QCOMPARE(listResponse.statusCode, 200);
    const auto listPayload = QJsonDocument::fromJson(listResponse.body).object();
    const auto templates = listPayload["data"].toObject()["templates"].toArray();
    QCOMPARE(templates.size(), 1);
    QCOMPARE(templates.first().toObject()["id"].toString(), QString("template-invoice"));
    QCOMPARE(templates.first().toObject()["templateKey"].toString(), QString("invoice"));

    LocalHttpRequest readRequest;
    readRequest.method = "GET";
    readRequest.path = "/templates/template-invoice";

    const auto readResponse = router.route(readRequest);
    QCOMPARE(readResponse.statusCode, 200);
    const auto readPayload = QJsonDocument::fromJson(readResponse.body).object();
    const auto readTemplate = readPayload["data"].toObject()["template"].toObject();
    QCOMPARE(readTemplate["id"].toString(), QString("template-invoice"));
    QCOMPARE(readTemplate["fieldSchema"].toObject()["fields"].toArray().size(), 1);

    auto updatedDocument = createHttpInvoiceTemplateDocument();
    updatedDocument.name = QString::fromUtf8("客户标签更新版");
    LocalHttpRequest updateRequest;
    updateRequest.method = "PUT";
    updateRequest.path = "/templates/template-invoice";
    updateRequest.body = QJsonDocument(TemplateDocumentJson::toJson(updatedDocument)).toJson(QJsonDocument::Compact);

    const auto updateResponse = router.route(updateRequest);
    QCOMPARE(updateResponse.statusCode, 200);
    const auto updatePayload = QJsonDocument::fromJson(updateResponse.body).object();
    QCOMPARE(updatePayload["data"].toObject()["template"].toObject()["name"].toString(), QString::fromUtf8("客户标签更新版"));

    auto mismatchedDocument = createHttpInvoiceTemplateDocument();
    mismatchedDocument.id = "another-template";
    updateRequest.body = QJsonDocument(TemplateDocumentJson::toJson(mismatchedDocument)).toJson(QJsonDocument::Compact);
    const auto mismatchedUpdateResponse = router.route(updateRequest);
    QCOMPARE(mismatchedUpdateResponse.statusCode, 400);

    LocalHttpRequest removeRequest;
    removeRequest.method = "DELETE";
    removeRequest.path = "/templates/template-invoice";

    const auto removeResponse = router.route(removeRequest);
    QCOMPARE(removeResponse.statusCode, 200);

    const auto emptyListResponse = router.route(listRequest);
    const auto emptyListPayload = QJsonDocument::fromJson(emptyListResponse.body).object();
    QCOMPARE(emptyListPayload["data"].toObject()["templates"].toArray().size(), 0);
}

void HttpTests::postPrintTemplateUsesRequestValuesAndConfiguredPrinter()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    PrintClientSettings settings;
    settings.defaultPrinter = "Configured Printer";

    FieldDefinition customerName;
    customerName.key = "customerName";
    customerName.displayName = QString::fromUtf8("客户名称");
    customerName.required = true;

    TemplateElement customerText;
    customerText.id = "customerNameText";
    customerText.type = TemplateElementType::BoundField;
    customerText.fieldKey = "customerName";
    customerText.x = 1.0;
    customerText.y = 2.0;
    customerText.width = 30.0;
    customerText.height = 5.0;
    customerText.fontSizePt = 9.0;

    TemplateLayer layer;
    layer.id = "base";
    layer.elements = {customerText};

    TemplateDocument document;
    document.id = "template-invoice";
    document.templateKey = "invoice";
    document.fieldSchema = {customerName};
    document.layers = {layer};
    settings.templateDocuments["invoice"] = document;
    QVERIFY(FileSettingsStore(dir.filePath("settings.json")).save(settings));

    RecordingLabelPrintEngine printEngine;
    LocalHttpRouter router(dir.filePath("settings.json"), &printEngine);

    LocalHttpRequest request;
    request.method = "POST";
    request.path = "/print/template";
    request.body = R"json({
        "requestId": "template-request-1",
        "templateKey": "invoice",
        "printerName": "Ignored Printer",
        "executePrint": true,
        "values": {
            "customerName": "张三"
        }
    })json";

    const auto response = router.route(request);
    QCOMPARE(response.statusCode, 200);
    QCOMPARE(printEngine.printCount, 1);
    QVERIFY(printEngine.usedPrintPlanEntry);
    QVERIFY(!printEngine.usedSinglePageEntry);
    QCOMPARE(printEngine.lastPrinterName, QString("Configured Printer"));

    const auto command = std::find_if(
        printEngine.lastPlan.commands.cbegin(),
        printEngine.lastPlan.commands.cend(),
        [](const NativeDrawCommand& item) {
            return item.elementKey == QStringLiteral("customerNameText");
        });
    QVERIFY(command != printEngine.lastPlan.commands.cend());
    QCOMPARE(command->text, QString::fromUtf8("张三"));

    const auto payload = QJsonDocument::fromJson(response.body).object();
    QVERIFY(payload["success"].toBool());
    const auto data = payload["data"].toObject();
    QCOMPARE(data["requestId"].toString(), QString("template-request-1"));
    QCOMPARE(data["accepted"].toInt(), 1);
    QCOMPARE(data["printed"].toInt(), 1);
    QCOMPARE(data["failed"].toInt(), 0);
}

void HttpTests::postPrintTemplateCanUseTemplateLibraryDocument()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    PrintClientSettings settings;
    settings.defaultPrinter = "Configured Printer";
    QVERIFY(FileSettingsStore(dir.filePath("settings.json")).save(settings));

    LocalHttpRouter saveRouter(dir.filePath("settings.json"));
    LocalHttpRequest saveRequest;
    saveRequest.method = "POST";
    saveRequest.path = "/templates";
    saveRequest.body = QJsonDocument(TemplateDocumentJson::toJson(createHttpInvoiceTemplateDocument()))
                           .toJson(QJsonDocument::Compact);
    QCOMPARE(saveRouter.route(saveRequest).statusCode, 200);

    RecordingLabelPrintEngine printEngine;
    LocalHttpRouter router(dir.filePath("settings.json"), &printEngine);

    LocalHttpRequest printRequest;
    printRequest.method = "POST";
    printRequest.path = "/print/template";
    printRequest.body = R"json({
        "requestId": "template-library-request-1",
        "templateKey": "invoice",
        "printerName": "Ignored Printer",
        "executePrint": true,
        "values": {
            "customerName": "李四"
        }
    })json";

    const auto response = router.route(printRequest);
    QCOMPARE(response.statusCode, 200);
    QCOMPARE(printEngine.printCount, 1);
    QCOMPARE(printEngine.lastPrinterName, QString("Configured Printer"));

    const auto command = std::find_if(
        printEngine.lastPlan.commands.cbegin(),
        printEngine.lastPlan.commands.cend(),
        [](const NativeDrawCommand& item) {
            return item.elementKey == QStringLiteral("customerNameText");
        });
    QVERIFY(command != printEngine.lastPlan.commands.cend());
    QCOMPARE(command->text, QString::fromUtf8("李四"));
}

void HttpTests::postPrintTemplateRejectsMissingRequiredFields()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    PrintClientSettings settings;
    settings.defaultPrinter = "Configured Printer";

    FieldDefinition customerName;
    customerName.key = "customerName";
    customerName.displayName = QString::fromUtf8("客户名称");
    customerName.required = true;

    TemplateElement customerText;
    customerText.id = "customerNameText";
    customerText.type = TemplateElementType::BoundField;
    customerText.fieldKey = "customerName";

    TemplateLayer layer;
    layer.id = "base";
    layer.elements = {customerText};

    TemplateDocument document;
    document.id = "template-invoice";
    document.templateKey = "invoice";
    document.fieldSchema = {customerName};
    document.layers = {layer};
    settings.templateDocuments["invoice"] = document;
    QVERIFY(FileSettingsStore(dir.filePath("settings.json")).save(settings));

    RecordingLabelPrintEngine printEngine;
    LocalHttpRouter router(dir.filePath("settings.json"), &printEngine);

    LocalHttpRequest request;
    request.method = "POST";
    request.path = "/print/template";
    request.body = R"json({
        "requestId": "template-request-2",
        "templateKey": "invoice",
        "executePrint": true,
        "values": {}
    })json";

    const auto response = router.route(request);
    QCOMPARE(response.statusCode, 400);
    QCOMPARE(printEngine.printCount, 0);

    const auto payload = QJsonDocument::fromJson(response.body).object();
    QVERIFY(!payload["success"].toBool());
    QVERIFY(payload["message"].toString().contains(QString::fromUtf8("客户名称")));
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

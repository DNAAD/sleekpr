#include "sleekpr/infrastructure/printing/TemplatePrintService.h"

#include "sleekpr/core/templates/FieldPresetStore.h"
#include "sleekpr/core/templates/PaperSpecStore.h"
#include "sleekpr/core/templates/TemplateDocumentJson.h"
#include "sleekpr/core/templates/TemplateInputSchema.h"
#include "sleekpr/core/templates/TemplateLibraryStore.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QSet>
#include <QTemporaryDir>
#include <QtTest>

using namespace sleekpr;

namespace {

core::TemplateDocument productTemplate(const QString& id = QStringLiteral("product"))
{
    core::TemplateElement element;
    element.id = QStringLiteral("productText");
    element.type = core::TemplateElementType::BoundField;
    element.fieldKey = QStringLiteral("productName");
    element.x = 1.0;
    element.y = 2.0;
    element.width = 30.0;
    element.height = 5.0;
    element.fontSizePt = 9.0;

    core::TemplateLayer layer;
    layer.id = QStringLiteral("base");
    layer.elements = {element};

    core::TemplateDocument document;
    document.id = id;
    document.name = QStringLiteral("商品标签");
    document.templateKey = QStringLiteral("product-tag");
    document.layers = {layer};
    return document;
}

infrastructure::TemplatePrintInput productInput()
{
    infrastructure::TemplatePrintInput input;
    input.templateId = QStringLiteral("default");
    input.printerName = QStringLiteral("Label Printer");
    input.values = QJsonObject{{"productName", "request-name"}};
    return input;
}

} // 匿名命名空间

class TemplatePrintServiceTests final : public QObject
{
    Q_OBJECT

private slots:
    void catalogKeysRoundTripDespiteDuplicateNamesAndAliases();
    void catalogAllowsMissingLibrary();
    void catalogReportsUnreadableDocuments();
    void catalogReportsLibraryPathThatIsNotDirectory();
    void findTemplatePreservesLegacyAliasesAndDefault();
    void prepareKeepsSchemaFreeValues();
    void prepareBlocksRequiredFieldsWithoutUsingSamples();
    void prepareUsesRequestThenPresetThenDefault();
    void prepareNeverMergesSampleData();
    void prepareReadsLibraryBesideSettings();
    void prepareReportsMissingTemplateAndPreset();
    void prepareReportsTemplateValidationErrors();
    void prepareReportsTableRenderErrors();
    void preparePreservesPaperMarginsAndProfileDpi_data();
    void preparePreservesPaperMarginsAndProfileDpi();
    void prepareUsesBuiltInPaperDpiForGenericProfile();
    void fieldPresetsReadBesideSettingsAndReportInvalidData();
    void desktopBlocksAllMissingReferencesWhileHttpStaysCompatible();
    void desktopValidatesUsingCopyWithoutAddingInferredContextValues();
    void desktopChecksMergedValuesAndExplicitRequiredRules();
    void desktopChecksRenderedGridRowsWhileHttpStaysCompatible();
    void savesFieldPresetsBesideSettingsAndPropagatesErrors();
    void legacyPlanUsesEmbeddedDocumentWithoutNewValidation();
    void legacyPlanFallsBackToNativeLabels();
    void preflightCollectsFieldsAndGeometryWithoutRenderingInvalidData();
    void preflightPreservesQrTargetAndClearsPreviousReport();
    void preflightWarningsDoNotChangePlanAndPreserveCalibration();
};

void TemplatePrintServiceTests::catalogKeysRoundTripDespiteDuplicateNamesAndAliases()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const infrastructure::TemplatePrintService service(dir.filePath("settings.json"));

    core::PrintClientSettings settings;
    auto embedded = productTemplate(QStringLiteral("shared"));
    settings.templateDocuments.insert(QStringLiteral("default"), embedded);
    auto other = productTemplate(QStringLiteral("other"));
    other.layers.first().elements.first().text = QStringLiteral("other-marker");
    settings.templateDocuments.insert(QStringLiteral("other"), other);
    auto shadow = productTemplate(QStringLiteral("shadow"));
    settings.templateDocuments.insert(QStringLiteral("@library:shared"), shadow);

    // 库模板故意与内嵌模板重名、同 id、同 templateKey，但版式不同，不能被错误去重或选错。
    auto library = productTemplate(QStringLiteral("shared"));
    library.layers.first().elements.first().x = 7.0;
    core::TemplateLibraryStore store(dir.filePath("templates"));
    QVERIFY(store.saveTemplate(library));
    auto libraryDefault = productTemplate(QStringLiteral("default"));
    libraryDefault.layers.first().elements.first().x = 9.0;
    QVERIFY(store.saveTemplate(libraryDefault));

    QString error;
    const auto choices = service.templates(settings, &error);
    QVERIFY2(error.isEmpty(), qPrintable(error));
    QCOMPARE(choices.size(), 5);
    QSet<QString> keys;
    QSet<QString> titles;
    int libraryDocuments = 0;
    for (const auto& choice : choices) {
        QVERIFY(!choice.key.isEmpty());
        QVERIFY(!choice.title.isEmpty());
        QVERIFY(!keys.contains(choice.key));
        QVERIFY(!titles.contains(choice.title));
        keys.insert(choice.key);
        titles.insert(choice.title);
        const auto found = service.findTemplate(settings, choice.key);
        QVERIFY(found.has_value());
        QCOMPARE(core::TemplateDocumentJson::toJson(*found), core::TemplateDocumentJson::toJson(choice.document));
        if (choice.document.layers.first().elements.first().x >= 7.0) {
            ++libraryDocuments;
        }
        auto input = productInput();
        input.templateId = choice.key;
        const auto prepared = service.prepare(settings, input);
        QVERIFY(prepared.has_value());
        QCOMPARE(core::TemplateDocumentJson::toJson(prepared->document), core::TemplateDocumentJson::toJson(choice.document));
    }
    QCOMPARE(libraryDocuments, 2);

    // QHash 插入顺序不得改变列表顺序和选择 key。
    core::PrintClientSettings reordered;
    reordered.templateDocuments.insert(QStringLiteral("other"), other);
    reordered.templateDocuments.insert(QStringLiteral("default"), embedded);
    reordered.templateDocuments.insert(QStringLiteral("@library:shared"), shadow);
    const auto again = service.templates(reordered);
    QCOMPARE(again.size(), choices.size());
    for (qsizetype index = 0; index < choices.size(); ++index) {
        QCOMPARE(again[index].key, choices[index].key);
        QCOMPARE(again[index].title, choices[index].title);
    }
}

void TemplatePrintServiceTests::catalogAllowsMissingLibrary()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const infrastructure::TemplatePrintService service(dir.filePath("settings.json"));
    QString error;
    QVERIFY(service.templates(core::PrintClientSettings{}, &error).isEmpty());
    QVERIFY(error.isEmpty());
    core::PrintClientSettings settings;
    settings.templateDocuments.insert(QStringLiteral("default"), productTemplate());
    QCOMPARE(service.templates(settings, &error).size(), 1);
    QVERIFY(error.isEmpty());
}

void TemplatePrintServiceTests::catalogReportsUnreadableDocuments()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QVERIFY(QDir().mkpath(dir.filePath("templates")));
    QFile file(dir.filePath("templates/broken.sleekpr-template.json"));
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("not-json");
    file.close();

    const infrastructure::TemplatePrintService service(dir.filePath("settings.json"));
    core::PrintClientSettings settings;
    settings.templateDocuments.insert(QStringLiteral("default"), productTemplate());
    QString error;
    QVERIFY(service.templates(settings, &error).isEmpty());
    QVERIFY(!error.isEmpty());
    QVERIFY(error.contains(QStringLiteral("broken")));
}

void TemplatePrintServiceTests::catalogReportsLibraryPathThatIsNotDirectory()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QFile file(dir.filePath("templates"));
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.close();
    const infrastructure::TemplatePrintService service(dir.filePath("settings.json"));
    QString error;
    QVERIFY(service.templates(core::PrintClientSettings{}, &error).isEmpty());
    QVERIFY(!error.isEmpty());
}

void TemplatePrintServiceTests::findTemplatePreservesLegacyAliasesAndDefault()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const infrastructure::TemplatePrintService service(dir.filePath("settings.json"));
    core::PrintClientSettings settings;
    settings.templateDocuments.insert(QStringLiteral("default"), productTemplate(QStringLiteral("embedded-id")));
    for (const auto& alias : {QString(), QStringLiteral(" default "), QStringLiteral("EMBEDDED-ID"), QStringLiteral("PRODUCT-TAG")}) {
        const auto found = service.findTemplate(settings, alias);
        QVERIFY(found.has_value());
        QCOMPARE(found->id, QStringLiteral("embedded-id"));
    }
    QVERIFY(!service.findTemplate(settings, QStringLiteral("missing")).has_value());
}

void TemplatePrintServiceTests::prepareKeepsSchemaFreeValues()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    core::PrintClientSettings settings;
    settings.templateDocuments.insert(QStringLiteral("default"), productTemplate());
    auto input = productInput();
    input.values.insert(QStringLiteral("items"), QJsonArray{QJsonObject{{"name", "item-1"}}});
    input.values.insert(QStringLiteral("metadata"), QJsonObject{{"number", 42}});
    input.values.insert(QStringLiteral("enabled"), false);
    const infrastructure::TemplatePrintService service(dir.filePath("settings.json"));
    QString code;
    QString error;
    const auto prepared = service.prepare(settings, input, &code, &error);
    QVERIFY2(prepared.has_value(), qPrintable(error));
    QCOMPARE(prepared->context.values, input.values);
    QCOMPARE(prepared->templateKey, QStringLiteral("product-tag"));
    QCOMPARE(prepared->printPlan.pages.size(), 1);
    QCOMPARE(prepared->printPlan.pages.first().commands.first().text, QStringLiteral("request-name"));
    QVERIFY(code.isEmpty());
}

void TemplatePrintServiceTests::prepareBlocksRequiredFieldsWithoutUsingSamples()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    auto document = productTemplate();
    core::FieldDefinition field;
    field.key = QStringLiteral("productName");
    field.displayName = QStringLiteral("品名");
    field.required = true;
    field.sampleValue = QStringLiteral("字段样例");
    document.fieldSchema = {field};
    document.sampleData = QJsonObject{{"productName", "sample-name"}};
    core::PrintClientSettings settings;
    settings.templateDocuments.insert(QStringLiteral("default"), document);
    auto input = productInput();
    input.values = {};
    const infrastructure::TemplatePrintService service(dir.filePath("settings.json"));
    QString code;
    QString error;
    QVERIFY(!service.prepare(settings, input, &code, &error).has_value());
    QCOMPARE(code, QStringLiteral("BAD_REQUEST"));
    QCOMPARE(error, QStringLiteral("缺少必填字段：品名"));
    input.values.insert(QStringLiteral("productName"), QStringLiteral("   "));
    QVERIFY(!service.prepare(settings, input, &code, &error).has_value());
    QCOMPARE(code, QStringLiteral("BAD_REQUEST"));
}

void TemplatePrintServiceTests::prepareUsesRequestThenPresetThenDefault()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    auto document = productTemplate();
    core::FieldDefinition field;
    field.key = QStringLiteral("productName");
    field.defaultValue = QStringLiteral("default-name");
    document.fieldSchema = {field};
    core::PrintClientSettings settings;
    settings.templateDocuments.insert(QStringLiteral("default"), document);
    core::FieldPreset preset;
    preset.id = QStringLiteral("preset");
    preset.name = QStringLiteral("字段预设");
    preset.templateId = document.id;
    preset.values = QJsonObject{{"productName", "preset-name"}, {"extra", "preset-extra"}};
    QVERIFY(core::FieldPresetStore(dir.filePath("field-presets.json")).savePreset(preset));

    const infrastructure::TemplatePrintService service(dir.filePath("settings.json"));
    auto input = productInput();
    input.values = {};
    auto prepared = service.prepare(settings, input);
    QVERIFY(prepared.has_value());
    QCOMPARE(prepared->context.values.value("productName").toString(), QStringLiteral("default-name"));
    input.fieldPresetId = QStringLiteral(" preset ");
    prepared = service.prepare(settings, input);
    QVERIFY(prepared.has_value());
    QCOMPARE(prepared->context.values.value("productName").toString(), QStringLiteral("preset-name"));
    QCOMPARE(prepared->context.values.value("extra").toString(), QStringLiteral("preset-extra"));
    input.values = QJsonObject{{"productName", "request-name"}, {"extra", "request-extra"}};
    prepared = service.prepare(settings, input);
    QVERIFY(prepared.has_value());
    QCOMPARE(prepared->context.values.value("productName").toString(), QStringLiteral("request-name"));
    QCOMPARE(prepared->context.values.value("extra").toString(), QStringLiteral("request-extra"));

    // schema 内的 null 继续沿用现有上下文构建器的回退规则。
    input.values.insert(QStringLiteral("productName"), QJsonValue(QJsonValue::Null));
    prepared = service.prepare(settings, input);
    QVERIFY(prepared.has_value());
    QCOMPARE(prepared->context.values.value("productName").toString(), QStringLiteral("preset-name"));
}

void TemplatePrintServiceTests::prepareNeverMergesSampleData()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    auto document = productTemplate();
    document.sampleData = QJsonObject{{"productName", "sample-name"}, {"sampleOnly", "sample-secret"}};
    core::PrintClientSettings settings;
    settings.templateDocuments.insert(QStringLiteral("default"), document);
    const infrastructure::TemplatePrintService service(dir.filePath("settings.json"));
    auto input = productInput();
    input.values = {};
    const auto prepared = service.prepare(settings, input);
    QVERIFY(prepared.has_value());
    QVERIFY(prepared->context.values.isEmpty());
    QVERIFY(prepared->printPlan.pages.first().commands.first().text.isEmpty());
    QCOMPARE(prepared->document.sampleData, document.sampleData);
}

void TemplatePrintServiceTests::prepareReadsLibraryBesideSettings()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    auto document = productTemplate(QStringLiteral("library-product"));
    document.templateKey = QStringLiteral("library-key");
    QVERIFY(core::TemplateLibraryStore(dir.filePath("templates")).saveTemplate(document));
    const infrastructure::TemplatePrintService service(dir.filePath("settings.json"));
    for (const auto& alias : {QStringLiteral("library-product"), QStringLiteral("LIBRARY-PRODUCT"), QStringLiteral("LIBRARY-KEY")}) {
        auto input = productInput();
        input.templateId = alias;
        const auto prepared = service.prepare(core::PrintClientSettings{}, input);
        QVERIFY(prepared.has_value());
        QCOMPARE(prepared->document.id, QStringLiteral("library-product"));
        QCOMPARE(prepared->printPlan.pages.first().commands.first().text, QStringLiteral("request-name"));
    }
}

void TemplatePrintServiceTests::prepareReportsMissingTemplateAndPreset()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const infrastructure::TemplatePrintService service(dir.filePath("settings.json"));
    auto input = productInput();
    input.fieldPresetId = QStringLiteral("missing");
    core::PrintClientSettings settings;
    QString code;
    QString error;
    QVERIFY(!service.prepare(settings, input, &code, &error).has_value());
    QCOMPARE(code, QStringLiteral("TEMPLATE_NOT_FOUND"));
    QCOMPARE(error, QStringLiteral("未找到指定模板。"));
    settings.templateDocuments.insert(QStringLiteral("default"), productTemplate());
    QVERIFY(!service.prepare(settings, input, &code, &error).has_value());
    QCOMPARE(code, QStringLiteral("FIELD_PRESET_NOT_FOUND"));
    QCOMPARE(error, QStringLiteral("未找到指定字段预设。"));
}

void TemplatePrintServiceTests::prepareReportsTemplateValidationErrors()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    auto document = productTemplate();
    document.layers.first().elements.first().x = 1000.0;
    core::PrintClientSettings settings;
    settings.templateDocuments.insert(QStringLiteral("default"), document);
    const infrastructure::TemplatePrintService service(dir.filePath("settings.json"));
    QString code;
    QString error;
    QVERIFY(!service.prepare(settings, productInput(), &code, &error).has_value());
    QCOMPARE(code, QStringLiteral("TEMPLATE_VALIDATION_FAILED"));
    QVERIFY(error.contains(QStringLiteral("超出纸张范围")));
}

void TemplatePrintServiceTests::prepareReportsTableRenderErrors()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    auto document = productTemplate();
    core::TableColumn column;
    column.id = QStringLiteral("name");
    column.title = QStringLiteral("品名");
    column.fieldKey = QStringLiteral("productName");
    column.widthMm = 30.0;
    core::TableElement table;
    table.id = QStringLiteral("itemsTable");
    table.dataPath = QStringLiteral("items");
    table.width = 30.0;
    table.height = 15.0;
    table.columns = {column};
    document.layers.first().tables = {table};
    core::PrintClientSettings settings;
    settings.templateDocuments.insert(QStringLiteral("default"), document);
    const infrastructure::TemplatePrintService service(dir.filePath("settings.json"));
    auto input = productInput();
    input.values.insert(QStringLiteral("items"), QJsonObject{{"productName", "not-an-array"}});
    QString code;
    QString error;
    QVERIFY(!service.prepare(settings, input, &code, &error).has_value());
    QCOMPARE(code, QStringLiteral("TEMPLATE_RENDER_FAILED"));
    QVERIFY(error.contains(QStringLiteral("items")));
}

void TemplatePrintServiceTests::preparePreservesPaperMarginsAndProfileDpi_data()
{
    QTest::addColumn<double>("paperDpi");
    QTest::addColumn<int>("profileKind");
    QTest::addColumn<double>("profileDpi");
    QTest::addColumn<double>("expectedDpi");
    QTest::newRow("paper-203-no-profile") << 203.0 << -1 << 300.0 << 203.0;
    QTest::newRow("paper-300-no-profile") << 300.0 << -1 << 203.0 << 300.0;
    QTest::newRow("paper-203-generic-300") << 203.0 << 0 << 300.0 << 203.0;
    QTest::newRow("paper-300-generic-203") << 300.0 << 0 << 203.0 << 300.0;
    QTest::newRow("paper-203-specific-300") << 203.0 << 1 << 300.0 << 300.0;
    QTest::newRow("paper-300-specific-203") << 300.0 << 1 << 203.0 << 203.0;
    QTest::newRow("unmatched-specific") << 203.0 << 2 << 300.0 << 203.0;
}

void TemplatePrintServiceTests::preparePreservesPaperMarginsAndProfileDpi()
{
    QFETCH(double, paperDpi);
    QFETCH(int, profileKind);
    QFETCH(double, profileDpi);
    QFETCH(double, expectedDpi);
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    core::PaperSpec paper;
    paper.id = QStringLiteral("custom-paper");
    paper.name = QStringLiteral("测试纸张");
    paper.widthMm = 100.0;
    paper.heightMm = 50.0;
    paper.marginLeftMm = 2.0;
    paper.marginTopMm = 3.0;
    paper.marginRightMm = 4.0;
    paper.marginBottomMm = 5.0;
    paper.defaultDpi = paperDpi;
    QVERIFY(core::PaperSpecStore(dir.filePath("paper-specs.json")).savePaperSpec(paper));

    auto document = productTemplate();
    document.paperSpecId = paper.id;
    if (profileKind >= 0) {
        core::DeviceProfile profile;
        profile.id = QStringLiteral("profile");
        profile.dpi = profileDpi;
        profile.offsetX = 0.25;
        profile.offsetY = -0.5;
        if (profileKind == 1) {
            profile.printerName = QStringLiteral("LABEL PRINTER");
            core::DeviceProfile generic;
            generic.id = QStringLiteral("generic-first");
            generic.dpi = 600.0;
            document.deviceProfiles.append(generic);
        } else if (profileKind == 2) {
            profile.printerName = QStringLiteral("Another Printer");
        }
        document.deviceProfiles.append(profile);
    }
    core::PrintClientSettings settings;
    settings.labelOffset.x = 0.5;
    settings.labelOffset.y = 1.25;
    settings.templateDocuments.insert(QStringLiteral("default"), document);
    const infrastructure::TemplatePrintService service(dir.filePath("settings.json"));
    QString error;
    const auto prepared = service.prepare(settings, productInput(), nullptr, &error);
    QVERIFY2(prepared.has_value(), qPrintable(error));
    QCOMPARE(prepared->printPlan.paperSize.widthMm(), 100.0);
    QCOMPARE(prepared->printPlan.paperSize.heightMm(), 50.0);
    QCOMPARE(prepared->printPlan.renderDpi, expectedDpi);
    const auto& command = prepared->printPlan.pages.first().commands.first();
    const bool selectedProfile = profileKind == 0 || profileKind == 1;
    QCOMPARE(command.x, selectedProfile ? 3.75 : 3.5);
    QCOMPARE(command.y, selectedProfile ? 5.75 : 6.25);
}

void TemplatePrintServiceTests::prepareUsesBuiltInPaperDpiForGenericProfile()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    auto document = productTemplate();
    core::DeviceProfile generic;
    generic.id = QStringLiteral("generic");
    generic.dpi = 300.0;
    document.deviceProfiles = {generic};
    core::PrintClientSettings settings;
    settings.templateDocuments.insert(QStringLiteral("default"), document);
    const infrastructure::TemplatePrintService service(dir.filePath("settings.json"));
    const auto prepared = service.prepare(settings, productInput());
    QVERIFY(prepared.has_value());
    QCOMPARE(prepared->printPlan.paperSize.widthMm(), 80.0);
    QCOMPARE(prepared->printPlan.paperSize.heightMm(), 30.0);
    QCOMPARE(prepared->printPlan.renderDpi, 203.0);
}

void TemplatePrintServiceTests::fieldPresetsReadBesideSettingsAndReportInvalidData()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    core::FieldPreset preset;
    preset.id = QStringLiteral("preset");
    preset.name = QStringLiteral("字段预设");
    preset.templateId = QStringLiteral("product");
    preset.values = QJsonObject{{"productName", "preset-name"}};
    const auto path = dir.filePath("field-presets.json");
    QVERIFY(core::FieldPresetStore(path).savePreset(preset));
    const infrastructure::TemplatePrintService service(dir.filePath("settings.json"));
    QString error;
    const auto presets = service.fieldPresets(&error);
    QVERIFY(error.isEmpty());
    QCOMPARE(presets.size(), 1);
    QCOMPARE(presets.first().id, QStringLiteral("preset"));
    QCOMPARE(presets.first().values, preset.values);

    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
    file.write("not-json");
    file.close();
    QVERIFY(service.fieldPresets(&error).isEmpty());
    QVERIFY(!error.isEmpty());
    auto input = productInput();
    input.fieldPresetId = preset.id;
    core::PrintClientSettings settings;
    settings.templateDocuments.insert(QStringLiteral("default"), productTemplate());
    QString code;
    QVERIFY(!service.prepare(settings, input, &code, &error).has_value());
    QCOMPARE(code, QStringLiteral("FIELD_PRESET_NOT_FOUND"));
}

void TemplatePrintServiceTests::desktopBlocksAllMissingReferencesWhileHttpStaysCompatible()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    auto document = productTemplate();
    auto fixed = document.layers.first().elements.first();
    fixed.id = QStringLiteral("businessText");
    fixed.type = core::TemplateElementType::FixedText;
    fixed.text = QStringLiteral("${businessCode}");
    document.layers.first().elements.append(fixed);
    document.sampleData = QJsonObject{{"productName", "sample-name"}, {"businessCode", "sample-code"}};
    core::PrintClientSettings settings;
    settings.templateDocuments.insert(QStringLiteral("default"), document);
    const infrastructure::TemplatePrintService service(dir.filePath("settings.json"));
    auto input = productInput();
    input.values = {};
    QVERIFY(!input.requireCompleteValues);
    const auto http = service.prepare(settings, input);
    QVERIFY(http.has_value());
    QVERIFY(http->context.values.isEmpty());
    input.requireCompleteValues = true;
    QString code;
    QString error;
    QVERIFY(!service.prepare(settings, input, &code, &error).has_value());
    QCOMPARE(code, QStringLiteral("MISSING_TEMPLATE_FIELDS"));
    for (const auto& key : {"productName", "businessCode"}) {
        QVERIFY2(error.contains(QString::fromLatin1(key)), qPrintable(error));
    }
    QVERIFY(!error.contains(QStringLiteral("productText")));
    QVERIFY(!error.contains(QStringLiteral("businessText")));
    input.values = QJsonObject{{"productName", ""}, {"businessCode", ""}};
    QVERIFY(service.prepare(settings, input).has_value());
    input.values = QJsonObject{{"productName", 0}, {"businessCode", false}};
    QVERIFY(service.prepare(settings, input).has_value());
    input.values.insert("businessCode", QJsonValue(QJsonValue::Null));
    QVERIFY(!service.prepare(settings, input, &code, &error).has_value());
    QCOMPARE(code, QStringLiteral("MISSING_TEMPLATE_FIELDS"));
}

void TemplatePrintServiceTests::desktopValidatesUsingCopyWithoutAddingInferredContextValues()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    auto document = productTemplate();
    document.layers.first().elements.first().fieldKey = QStringLiteral("businessName");
    document.sampleData = QJsonObject{{"businessName", "sample-secret"}, {"sampleOnly", 42}};
    const auto before = core::TemplateDocumentJson::toJson(document);
    core::PrintClientSettings settings;
    settings.templateDocuments.insert(QStringLiteral("default"), document);
    const infrastructure::TemplatePrintService service(dir.filePath("settings.json"));
    auto input = productInput();
    input.values = QJsonObject{{"businessName", "business-value"}, {"extra", false}};
    input.requireCompleteValues = true;
    QString code;
    QString error;
    const auto prepared = service.prepare(settings, input, &code, &error);
    QVERIFY2(prepared.has_value(), qPrintable(error));
    QCOMPARE(core::TemplateDocumentJson::toJson(prepared->document), before);
    QCOMPARE(prepared->context.values, input.values);
    QCOMPARE(prepared->printPlan.pages.first().commands.first().text, QStringLiteral("business-value"));
    QCOMPARE(core::TemplateDocumentJson::toJson(settings.templateDocuments.value("default")), before);
    input.requireCompleteValues = false;
    QVERIFY(!service.prepare(settings, input, &code, &error).has_value());
    QCOMPARE(code, QStringLiteral("TEMPLATE_VALIDATION_FAILED"));
    input.requireCompleteValues = true;
    input.values = {};
    QVERIFY(!service.prepare(settings, input, &code, &error).has_value());
    QCOMPARE(code, QStringLiteral("MISSING_TEMPLATE_FIELDS"));
}

void TemplatePrintServiceTests::desktopChecksMergedValuesAndExplicitRequiredRules()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    auto document = productTemplate();
    core::FieldDefinition required;
    required.key = QStringLiteral("productName");
    required.required = true;
    required.defaultValue = QStringLiteral("default-name");
    core::FieldDefinition optional;
    optional.key = QStringLiteral("optional");
    document.fieldSchema = {required, optional};
    auto inferred = document.layers.first().elements.first();
    inferred.id = QStringLiteral("inferred");
    inferred.fieldKey = QStringLiteral("businessName");
    document.layers.first().elements.append(inferred);
    core::FieldPreset preset;
    preset.id = QStringLiteral("desktop");
    preset.name = QStringLiteral("桌面输入");
    preset.templateId = document.id;
    preset.values = QJsonObject{{"businessName", "preset-business"}};
    core::PrintClientSettings settings;
    settings.templateDocuments.insert(QStringLiteral("default"), document);
    const infrastructure::TemplatePrintService service(dir.filePath("settings.json"));
    QVERIFY(service.saveFieldPreset(preset));
    auto input = productInput();
    input.values = {};
    input.requireCompleteValues = true;
    input.fieldPresetId = preset.id;
    const auto prepared = service.prepare(settings, input);
    QVERIFY(prepared.has_value());
    QCOMPARE(prepared->context.values.value("productName").toString(), QStringLiteral("default-name"));
    QCOMPARE(prepared->context.values.value("businessName").toString(), QStringLiteral("preset-business"));
    input.values = QJsonObject{{"productName", " "}, {"businessName", QJsonValue(QJsonValue::Null)}};
    QString code;
    QString error;
    QVERIFY(!service.prepare(settings, input, &code, &error).has_value());
    QCOMPARE(code, QStringLiteral("MISSING_TEMPLATE_FIELDS"));
    QVERIFY(error.contains(QStringLiteral("productName")));
    QVERIFY(error.contains(QStringLiteral("businessName")));
    input.requireCompleteValues = false;
    QVERIFY(!service.prepare(settings, input, &code, &error).has_value());
    QCOMPARE(code, QStringLiteral("BAD_REQUEST"));
    QCOMPARE(error, QStringLiteral("缺少必填字段：productName"));
}

void TemplatePrintServiceTests::desktopChecksRenderedGridRowsWhileHttpStaysCompatible()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    auto document = productTemplate();
    auto grid = document.layers.first().elements.first();
    grid.id = QStringLiteral("grid");
    grid.type = core::TemplateElementType::ArrayGrid;
    grid.dataPath = QStringLiteral("order.items");
    grid.arrayGridRows = 1;
    grid.arrayGridColumns = 1;
    grid.arrayGridCellTemplate = QStringLiteral("${text}:${value}");
    document.layers.first().elements.append(grid);
    core::PrintClientSettings settings;
    settings.templateDocuments.insert(QStringLiteral("default"), document);
    const infrastructure::TemplatePrintService service(dir.filePath("settings.json"));
    auto input = productInput();
    input.values.insert("order", QJsonObject{{"items", QJsonArray{QJsonObject{{"text", "row"}}, QJsonObject{}}}});
    QVERIFY(service.prepare(settings, input).has_value());
    input.requireCompleteValues = true;
    QString code;
    QString error;
    QVERIFY(!service.prepare(settings, input, &code, &error).has_value());
    QCOMPARE(code, QStringLiteral("MISSING_TEMPLATE_FIELDS"));
    for (const auto& part : {QStringLiteral("order.items"), QStringLiteral("第1项"), QStringLiteral("value")}) {
        QVERIFY2(error.contains(part), qPrintable(error));
    }
    QVERIFY(!error.contains(QStringLiteral("grid")));
    const auto issues = core::TemplateInputSchema::validate(document, input.values);
    QCOMPARE(issues.size(), 1);
    QCOMPARE(issues.first().elementIds, QStringList({"grid"}));
    input.values.insert("order.items", QJsonArray{QJsonObject{{"text", 0}, {"value", false}}, QJsonObject{}});
    const auto prepared = service.prepare(settings, input);
    QVERIFY(prepared.has_value());
    QCOMPARE(prepared->context.values, input.values);
    QVERIFY(prepared->document.fieldSchema.isEmpty());
    input.values.insert("order.items", QJsonArray{});
    QVERIFY(service.prepare(settings, input).has_value());
    input.values.insert("order.items", "wrong-array-type");
    QVERIFY(!service.prepare(settings, input, &code, &error).has_value());
    QCOMPARE(code, QStringLiteral("MISSING_TEMPLATE_FIELDS"));
}

void TemplatePrintServiceTests::savesFieldPresetsBesideSettingsAndPropagatesErrors()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const infrastructure::TemplatePrintService service(dir.filePath("settings.json"));
    core::FieldPreset preset;
    preset.id = QStringLiteral("saved");
    preset.name = QStringLiteral("保存预设");
    preset.templateId = QStringLiteral("product");
    preset.values = QJsonObject{{"productName", "saved-name"}, {"enabled", false}};
    QString error;
    QVERIFY2(service.saveFieldPreset(preset, &error), qPrintable(error));
    const auto stored = core::FieldPresetStore(dir.filePath("field-presets.json")).loadPreset(preset.id);
    QVERIFY(stored.has_value());
    QCOMPARE(stored->values, preset.values);
    preset.values.insert("productName", "updated-name");
    QVERIFY(service.saveFieldPreset(preset));
    QCOMPARE(service.fieldPresets().size(), 1);
    QCOMPARE(service.fieldPresets().first().values, preset.values);
    preset.id = QStringLiteral("invalid/id");
    QVERIFY(!service.saveFieldPreset(preset, &error));
    QVERIFY(!error.isEmpty());
    QCOMPARE(service.fieldPresets().size(), 1);
}

void TemplatePrintServiceTests::legacyPlanUsesEmbeddedDocumentWithoutNewValidation()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    auto document = productTemplate();
    core::FieldDefinition required;
    required.key = QStringLiteral("customRequired");
    required.required = true;
    document.fieldSchema = {required};
    document.sampleData = QJsonObject{{"productName", "sample-name"}};
    core::PrintClientSettings settings;
    settings.labelOffset.x = 0.5;
    settings.templateDocuments.insert(QStringLiteral("default"), document);
    core::LabelRenderPlan label;
    label.productName = QStringLiteral("legacy-name");
    const infrastructure::TemplatePrintService service(dir.filePath("settings.json"));
    const auto plan = service.createLegacyPlan(label, settings, QStringLiteral("Label Printer"));
    QCOMPARE(plan.pages.size(), 1);
    QCOMPARE(plan.pages.first().commands.first().text, QStringLiteral("legacy-name"));
    QCOMPARE(plan.pages.first().commands.first().x, 1.5);
    QCOMPARE(plan.renderDpi, 203.0);
}

void TemplatePrintServiceTests::legacyPlanFallsBackToNativeLabels()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    auto library = productTemplate(QStringLiteral("default"));
    library.layers.first().elements.first().text = QStringLiteral("must-not-use-library");
    QVERIFY(core::TemplateLibraryStore(dir.filePath("templates")).saveTemplate(library));
    const infrastructure::TemplatePrintService service(dir.filePath("settings.json"));
    core::LabelRenderPlan label;
    label.productName = QStringLiteral("legacy-name");
    const auto plan = service.createLegacyPlan(label, core::PrintClientSettings{}, QStringLiteral("Label Printer"));
    QCOMPARE(plan.pages.size(), 1);
    QCOMPARE(plan.renderDpi, 300.0);
    bool foundProduct = false;
    for (const auto& command : plan.pages.first().commands) {
        QVERIFY(command.elementKey != QStringLiteral("productText"));
        if (command.text == QStringLiteral("legacy-name")) {
            foundProduct = true;
        }
    }
    QVERIFY(foundProduct);
}

void TemplatePrintServiceTests::preflightCollectsFieldsAndGeometryWithoutRenderingInvalidData()
{
    QTemporaryDir dir;
    auto document = productTemplate();
    document.layers.first().elements.first().x = 90;
    core::PrintClientSettings settings;
    settings.templateDocuments.insert(QStringLiteral("default"), document);
    infrastructure::TemplatePrintService service(dir.filePath("settings.json"));
    auto input = productInput();
    input.values = {};
    input.requireCompleteValues = true;
    core::PrintPreflightReport report;
    QVERIFY(!service.prepare(settings, input, nullptr, nullptr, &report).has_value());
    QCOMPARE(report.paperSizeMm, QSizeF(80, 30));
    QVERIFY(report.issues.size() >= 2);
    bool fieldFound = false;
    bool boundsFound = false;
    for (const auto& issue : report.issues) {
        QVERIFY(issue.elementIds.contains(QStringLiteral("productText")));
        fieldFound |= issue.target == core::PrintPreflightTarget::Field && issue.fieldKey == QStringLiteral("productName");
        boundsFound |= issue.code == QStringLiteral("ELEMENT_OUT_OF_PAPER") && issue.target == core::PrintPreflightTarget::Element;
    }
    QVERIFY(fieldFound && boundsFound);
}

void TemplatePrintServiceTests::preflightPreservesQrTargetAndClearsPreviousReport()
{
    QTemporaryDir dir;
    auto document = productTemplate();
    auto& element = document.layers.first().elements.first();
    element.type = core::TemplateElementType::QrCode;
    element.payload = QString(47, QLatin1Char('X'));
    core::PrintClientSettings settings;
    settings.templateDocuments.insert(QStringLiteral("default"), document);
    infrastructure::TemplatePrintService service(dir.filePath("settings.json"));
    auto input = productInput();
    input.requireCompleteValues = true;
    core::PrintPreflightReport report;
    QVERIFY(!service.prepare(settings, input, nullptr, nullptr, &report).has_value());
    QCOMPARE(report.issues.size(), 1);
    QCOMPARE(report.issues.first().code, QStringLiteral("QR_PAYLOAD_TOO_LONG"));
    QCOMPARE(report.issues.first().elementIds, QStringList{QStringLiteral("productText")});
    settings.templateDocuments[QStringLiteral("default")].layers.first().elements.first().payload = QStringLiteral("ok");
    QVERIFY(service.prepare(settings, input, nullptr, nullptr, &report).has_value());
    QVERIFY(report.issues.isEmpty());
    input.templateId = QStringLiteral("missing");
    QVERIFY(!service.prepare(settings, input, nullptr, nullptr, &report).has_value());
    QCOMPARE(report.issues.size(), 1);
    QCOMPARE(report.issues.first().target, core::PrintPreflightTarget::General);
}

void TemplatePrintServiceTests::preflightWarningsDoNotChangePlanAndPreserveCalibration()
{
    QTemporaryDir dir;
    auto document = productTemplate();
    core::DeviceProfile profile;
    profile.printerName = QStringLiteral("Label Printer");
    profile.offsetX = 0.5;
    profile.offsetY = 0.75;
    profile.scaleX = 1.1;
    profile.scaleY = 0.9;
    document.deviceProfiles = {profile};
    core::PrintClientSettings settings;
    settings.labelOffset.x = 1;
    settings.labelOffset.y = 2;
    settings.templateDocuments.insert(QStringLiteral("default"), document);
    infrastructure::TemplatePrintService service(dir.filePath("settings.json"));
    auto input = productInput();
    input.requireCompleteValues = true;
    input.values = QJsonObject{{"productName", ""}};
    core::PrintPreflightReport report;
    const auto prepared = service.prepare(settings, input, nullptr, nullptr, &report);
    QVERIFY(prepared.has_value());
    QCOMPARE(report.issues.size(), 1);
    QCOMPARE(report.issues.first().severity, core::PrintPreflightSeverity::Warning);
    QCOMPARE(report.issues.first().fieldKey, QStringLiteral("productName"));
    QCOMPARE(report.offsetXmm, 1.5);
    QCOMPARE(report.offsetYmm, 2.75);
    QCOMPARE(report.scaleX, 1.1);
    QCOMPARE(report.scaleY, 0.9);
    const auto withoutReport = service.prepare(settings, input);
    QVERIFY(withoutReport.has_value());
    QCOMPARE(prepared->printPlan.pages.size(), withoutReport->printPlan.pages.size());
    QCOMPARE(prepared->context.values, withoutReport->context.values);
}

QTEST_GUILESS_MAIN(TemplatePrintServiceTests)
#include "tst_template_print_service.moc"

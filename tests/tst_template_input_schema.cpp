#include "sleekpr/core/templates/TemplateInputSchema.h"

#include "sleekpr/core/settings/PrintClientSettingsJson.h"
#include "sleekpr/core/templates/FieldSchemaJson.h"
#include "sleekpr/core/templates/TemplateDocumentJson.h"
#include "sleekpr/core/templates/TemplateLibraryStore.h"
#include "sleekpr/core/templates/TemplateRenderContextBuilder.h"

#include <QJsonArray>
#include <QTemporaryDir>
#include <QtTest>

using namespace sleekpr::core;

namespace {

TemplateElement bound(const QString& id, const QString& key)
{
    TemplateElement element;
    element.id = id;
    element.type = TemplateElementType::BoundField;
    element.fieldKey = key;
    return element;
}

TemplateDocument withElements(const QList<TemplateElement>& elements)
{
    TemplateDocument document;
    TemplateLayer layer;
    layer.id = QStringLiteral("base");
    layer.elements = elements;
    document.layers = {layer};
    return document;
}

TemplateDocument withDisplayNames()
{
    auto element = bound("name", "productName");
    element.layerId = QStringLiteral("base");
    auto document = withElements({element});
    document.id = QStringLiteral("display-name-template");
    document.name = QStringLiteral("名称覆盖模板");
    document.paperSpecId = QStringLiteral("label-80x30");
    document.fieldDisplayNames.insert(QStringLiteral("productName"), QStringLiteral("品名"));
    document.fieldDisplayNames.insert(QStringLiteral("orphan"), QStringLiteral("未绑定名称"));
    document.fieldDisplayNames.insert(QStringLiteral("emptyName"), QString());
    return document;
}

} // 匿名命名空间

class TemplateInputSchemaTests final : public QObject
{
    Q_OBJECT

private slots:
    void infersMissingSchemaAndMergesElementIdsWithoutSamples();
    void preservesExplicitMetadataAndOptionalValues();
    void discoversVisibleReferencesButNotRowVariables();
    void discoversOnlyLegacyTableArraySources();
    void distinguishesAbsentValuesFromIntentionalBlanks_data();
    void distinguishesAbsentValuesFromIntentionalBlanks();
    void resolvesOnlyArrayPathsAndPrefersExactKeys();
    void validatesNonemptyTypes_data();
    void validatesNonemptyTypes();
    void preservesExplicitArrayMetadataButRejectsConflicts();
    void checksOnlyRenderedGridRowsWithoutPromotingCellFields();
    void gridRowsPreferLiteralKeysAndWrapPrimitiveValues();
    void mixedArrayPathRequiresLiteralScalarKey_data();
    void mixedArrayPathRequiresLiteralScalarKey();
    void emptyMarkersMustMatchTypeAndMissingMessageKeepsDisplayName();
    void overridesInferredNamesWithoutChangingInputSemantics();
    void overridesExplicitNamesWithoutChangingSchema();
    void emptyNameOverridesFallBack_data();
    void emptyNameOverridesFallBack();
    void nameOverridesDoNotBecomePrintValues();
    void roundTripsDisplayNamesInDocumentJson();
    void omitsEmptyDisplayNamesFromDocumentJson();
    void importsOptionalDisplayNames();
    void rejectsMalformedDisplayNames_data();
    void rejectsMalformedDisplayNames();
    void roundTripsDisplayNamesInSettingsJson();
    void storesDisplayNamesInTemplateLibrary();
};

void TemplateInputSchemaTests::infersMissingSchemaAndMergesElementIdsWithoutSamples()
{
    auto document = withElements({bound("first", "businessName"), bound("second", "businessName"),
                                  bound("amount", "amount"), bound("enabled", "enabled")});
    document.sampleData = QJsonObject{{"businessName", "sample-secret"}, {"amount", 15}, {"enabled", true}};
    const auto before = TemplateDocumentJson::toJson(document);
    const auto fields = TemplateInputSchema::fields(document);
    QCOMPARE(fields.size(), 3);
    QCOMPARE(fields[0].definition.key, QStringLiteral("businessName"));
    QCOMPARE(fields[0].elementIds, QStringList({"first", "second"}));
    QCOMPARE(fields[1].definition.type, FieldValueType::Number);
    QCOMPARE(fields[2].definition.type, FieldValueType::Boolean);
    for (const auto& field : fields) {
        QVERIFY(field.inferred);
        QVERIFY(field.definition.required);
        QVERIFY(field.definition.defaultValue.isEmpty());
        QVERIFY(field.definition.sampleValue.isEmpty());
    }
    const auto issues = TemplateInputSchema::validate(document, {});
    QCOMPARE(issues.size(), 3);
    QCOMPARE(issues[0].fieldKey, QStringLiteral("businessName"));
    QCOMPARE(issues[0].elementIds, QStringList({"first", "second"}));
    for (const auto& issue : issues) {
        QVERIFY(issue.message.contains(issue.fieldKey));
        QVERIFY(issue.message.contains(QStringLiteral("缺少")));
    }
    QVERIFY(TemplateInputSchema::validate(document, QJsonObject{{"businessName", ""}, {"amount", 0}, {"enabled", false}}).isEmpty());
    QCOMPARE(TemplateDocumentJson::toJson(document), before);
}

void TemplateInputSchemaTests::preservesExplicitMetadataAndOptionalValues()
{
    auto document = withElements({bound("optional", "optional"), bound("required", "required")});
    FieldDefinition optional;
    optional.key = QStringLiteral("optional");
    optional.displayName = QStringLiteral("可选金额");
    optional.type = FieldValueType::Money;
    optional.defaultValue = QStringLiteral("12.50");
    optional.sampleValue = QStringLiteral("99");
    optional.format = QStringLiteral("0.00");
    optional.source = FieldSource::BuiltIn;
    FieldDefinition required;
    required.key = QStringLiteral("required");
    required.type = FieldValueType::List;
    required.required = true;
    document.fieldSchema = {optional, required};
    const auto fields = TemplateInputSchema::fields(document);
    QCOMPARE(fields.size(), 2);
    QVERIFY(!fields[0].inferred);
    QCOMPARE(fields[0].definition.key, optional.key);
    QCOMPARE(fields[0].definition.displayName, optional.displayName);
    QCOMPARE(fields[0].definition.type, optional.type);
    QCOMPARE(fields[0].definition.required, optional.required);
    QCOMPARE(fields[0].definition.defaultValue, optional.defaultValue);
    QCOMPARE(fields[0].definition.sampleValue, optional.sampleValue);
    QCOMPARE(fields[0].definition.format, optional.format);
    QCOMPARE(fields[0].definition.source, optional.source);
    const QJsonObject values{{"required", QJsonArray{1}}};
    QVERIFY(TemplateInputSchema::validate(document, values).isEmpty());
    auto emptyOptional = values;
    emptyOptional.insert("optional", "");
    QVERIFY(TemplateInputSchema::validate(document, emptyOptional).isEmpty());
    const QJsonObject emptyRequired{{"required", QJsonArray{}}};
    const QJsonObject blankRequired{{"required", "   "}};
    QCOMPARE(TemplateInputSchema::validate(document, emptyRequired).size(), 1);
    QCOMPARE(TemplateInputSchema::validate(document, blankRequired).size(), 1);
}

void TemplateInputSchemaTests::discoversVisibleReferencesButNotRowVariables()
{
    auto fixed = bound("fixed", "unusedFixedBinding");
    fixed.type = TemplateElementType::FixedText;
    fixed.text = QStringLiteral("${ name } ${name} ${order.number} ${} ${unfinished");
    auto qr = bound("qr", "unusedQrBinding");
    qr.type = TemplateElementType::QrCode;
    qr.payload = QStringLiteral("https://example.test/${code}/${name}");
    auto staticQr = qr;
    staticQr.id = QStringLiteral("static");
    staticQr.payload = QStringLiteral("static-payload");
    auto dynamicQr = qr;
    dynamicQr.id = QStringLiteral("dynamic");
    dynamicQr.payload = QStringLiteral("  ");
    dynamicQr.fieldKey = QStringLiteral("qrValue");
    auto grid = bound("grid", "unusedGridBinding");
    grid.type = TemplateElementType::ArrayGrid;
    grid.dataPath = QStringLiteral("order.items");
    grid.arrayGridCellTemplate = QStringLiteral("${text}:${value}:${cellOnly}");
    auto hidden = bound("hidden", "hiddenValue");
    hidden.visible = false;
    auto locked = bound("locked", "name");
    locked.locked = true;
    auto document = withElements({fixed, qr, staticQr, dynamicQr, grid, hidden, locked});
    document.layers.first().locked = true;
    TemplateLayer hiddenLayer;
    hiddenLayer.visible = false;
    hiddenLayer.elements = {bound("hidden-layer", "hiddenLayerValue")};
    document.layers.append(hiddenLayer);
    // 数组结构优先于有误的样例类型，样例中的额外字段不生成全局输入。
    document.sampleData = QJsonObject{{"order.items", "not-an-array"}, {"sampleOnly", 12}};
    const auto fields = TemplateInputSchema::fields(document);
    QStringList keys;
    for (const auto& field : fields) {
        keys.append(field.definition.key);
    }
    QCOMPARE(keys, QStringList({"name", "order.number", "code", "qrValue", "order.items"}));
    QCOMPARE(fields[0].elementIds, QStringList({"fixed", "qr", "locked"}));
    QVERIFY(fields[4].arrayPath);
    QCOMPARE(fields[4].definition.type, FieldValueType::List);
    QVERIFY(fields[4].definition.defaultValue.isEmpty());
    QVERIFY(fields[4].definition.sampleValue.isEmpty());
}

void TemplateInputSchemaTests::discoversOnlyLegacyTableArraySources()
{
    auto document = withElements({});
    TableElement table;
    table.id = QStringLiteral("table");
    table.locked = true;
    table.dataPath = QStringLiteral("items");
    TableColumn column;
    column.fieldKey = QStringLiteral("columnOnly");
    table.columns = {column};
    TableRowBand detail;
    detail.id = QStringLiteral("detail");
    detail.dataPath = QStringLiteral("order.details");
    TableRowBand header = detail;
    header.kind = TableRowBandKind::Header;
    header.dataPath = QStringLiteral("headerOnly");
    TableRowBand summary = detail;
    summary.kind = TableRowBandKind::Summary;
    summary.dataPath = QStringLiteral("summaryOnly");
    table.rowBands = {header, detail, summary};
    TableCellTemplate cell;
    cell.fieldKey = QStringLiteral("cellOnly");
    cell.textTemplate = QStringLiteral("${templateOnly}");
    table.cellTemplates = {cell};
    auto hidden = table;
    hidden.visible = false;
    hidden.dataPath = QStringLiteral("hiddenTable");
    document.layers.first().tables = {table, hidden};
    TemplateLayer hiddenLayer = document.layers.first();
    hiddenLayer.visible = false;
    hiddenLayer.tables.first().dataPath = QStringLiteral("hiddenLayerTable");
    document.layers.append(hiddenLayer);
    const auto fields = TemplateInputSchema::fields(document);
    QCOMPARE(fields.size(), 2);
    QCOMPARE(fields[0].definition.key, QStringLiteral("items"));
    QCOMPARE(fields[1].definition.key, QStringLiteral("order.details"));
    for (const auto& field : fields) {
        QVERIFY(field.arrayPath);
        QCOMPARE(field.definition.type, FieldValueType::List);
        QCOMPARE(field.elementIds, QStringList({"table"}));
    }
}

void TemplateInputSchemaTests::distinguishesAbsentValuesFromIntentionalBlanks_data()
{
    QTest::addColumn<QJsonValue>("value");
    QTest::addColumn<bool>("inferredMissing");
    QTest::addColumn<bool>("explicitMissing");
    QTest::newRow("undefined") << QJsonValue(QJsonValue::Undefined) << true << true;
    QTest::newRow("null") << QJsonValue(QJsonValue::Null) << true << true;
    QTest::newRow("empty-text") << QJsonValue("") << false << true;
    QTest::newRow("blank-text") << QJsonValue("   ") << false << true;
    QTest::newRow("empty-array") << QJsonValue(QJsonArray{}) << true << true;
    QTest::newRow("empty-object") << QJsonValue(QJsonObject{}) << true << true;
    QTest::newRow("zero") << QJsonValue(0) << false << false;
    QTest::newRow("false") << QJsonValue(false) << false << false;
}

void TemplateInputSchemaTests::distinguishesAbsentValuesFromIntentionalBlanks()
{
    QFETCH(QJsonValue, value);
    QFETCH(bool, inferredMissing);
    QFETCH(bool, explicitMissing);
    auto document = withElements({bound("valueElement", "value")});
    QJsonObject values;
    values.insert("value", value);
    QCOMPARE(TemplateInputSchema::validate(document, values).size(), inferredMissing ? 1 : 0);
    FieldDefinition definition;
    definition.key = QStringLiteral("value");
    definition.required = true;
    document.fieldSchema = {definition};
    QCOMPARE(TemplateInputSchema::validate(document, values).size(), explicitMissing ? 1 : 0);
    document.fieldSchema.first().required = false;
    QCOMPARE(TemplateInputSchema::validate(document, values).size(), value.isArray() || value.isObject() ? 1 : 0);
}

void TemplateInputSchemaTests::resolvesOnlyArrayPathsAndPrefersExactKeys()
{
    auto grid = bound("grid", "");
    grid.type = TemplateElementType::ArrayGrid;
    grid.dataPath = QStringLiteral("order.items");
    auto document = withElements({grid, bound("text", "order.name")});
    document.layers.first().elements.first().arrayGridCellTemplate = QStringLiteral("${value}");
    QJsonObject values{{"order", QJsonObject{{"items", QJsonArray{1}}, {"name", "nested-name"}}}};
    auto issues = TemplateInputSchema::validate(document, values);
    QCOMPARE(issues.size(), 1);
    QCOMPARE(issues.first().fieldKey, QStringLiteral("order.name"));
    values.insert("order.name", "exact-name");
    QVERIFY(TemplateInputSchema::validate(document, values).isEmpty());
    values.insert("order.items", "wrong-type");
    issues = TemplateInputSchema::validate(document, values);
    QCOMPARE(issues.size(), 1);
    QCOMPARE(issues.first().fieldKey, QStringLiteral("order.items"));
    QCOMPARE(issues.first().elementIds, QStringList({"grid"}));
    values.insert("order.items", QJsonValue(QJsonValue::Null));
    QCOMPARE(TemplateInputSchema::validate(document, values).size(), 1);
    values.insert("order.items", QJsonArray{});
    QVERIFY(TemplateInputSchema::validate(document, values).isEmpty());
}

void TemplateInputSchemaTests::validatesNonemptyTypes_data()
{
    QTest::addColumn<int>("type");
    QTest::addColumn<QJsonValue>("valid");
    QTest::addColumn<QJsonValue>("invalid");
    QTest::newRow("number") << int(FieldValueType::Number) << QJsonValue(" -1.25e2 ") << QJsonValue("12x");
    QTest::newRow("money") << int(FieldValueType::Money) << QJsonValue("0.00") << QJsonValue("NaN");
    QTest::newRow("weight") << int(FieldValueType::Weight) << QJsonValue(0) << QJsonValue(true);
    QTest::newRow("boolean") << int(FieldValueType::Boolean) << QJsonValue(false) << QJsonValue("false");
    QTest::newRow("list") << int(FieldValueType::List) << QJsonValue(QJsonArray{1}) << QJsonValue(QJsonObject{{"x", 1}});
    QTest::newRow("object") << int(FieldValueType::Object) << QJsonValue(QJsonObject{{"x", 1}}) << QJsonValue("{\"x\":1}");
    QTest::newRow("text") << int(FieldValueType::Text) << QJsonValue(0) << QJsonValue(QJsonArray{1});
    QTest::newRow("date") << int(FieldValueType::Date) << QJsonValue("2026-10-19") << QJsonValue(QJsonArray{1});
    QTest::newRow("datetime") << int(FieldValueType::DateTime) << QJsonValue("2026-10-19T12:00:00") << QJsonValue(QJsonObject{{"x", 1}});
}

void TemplateInputSchemaTests::validatesNonemptyTypes()
{
    QFETCH(int, type);
    QFETCH(QJsonValue, valid);
    QFETCH(QJsonValue, invalid);
    auto document = withElements({bound("typed", "typed"), bound("missing", "other")});
    FieldDefinition definition;
    definition.key = QStringLiteral("typed");
    definition.type = static_cast<FieldValueType>(type);
    document.fieldSchema = {definition};
    QVERIFY(TemplateInputSchema::validate(document, QJsonObject{{"typed", valid}, {"other", "ok"}}).isEmpty());
    const auto issues = TemplateInputSchema::validate(document, QJsonObject{{"typed", invalid}});
    QCOMPARE(issues.size(), 2);
    QCOMPARE(issues[0].fieldKey, QStringLiteral("typed"));
    QCOMPARE(issues[0].elementIds, QStringList({"typed"}));
    QVERIFY(issues[0].message.contains(QStringLiteral("typed")));
    QVERIFY(issues[0].message.contains(QStringLiteral("类型")));
    QCOMPARE(issues[1].fieldKey, QStringLiteral("other"));
}

void TemplateInputSchemaTests::preservesExplicitArrayMetadataButRejectsConflicts()
{
    auto grid = bound("grid", "");
    grid.type = TemplateElementType::ArrayGrid;
    grid.dataPath = QStringLiteral("items");
    grid.arrayGridCellTemplate = QStringLiteral("${value}");
    auto document = withElements({bound("text", "items"), grid});
    FieldDefinition definition;
    definition.key = QStringLiteral("items");
    definition.type = FieldValueType::Number;
    definition.defaultValue = QStringLiteral("12");
    document.fieldSchema = {definition};
    const auto field = TemplateInputSchema::fields(document).first();
    QVERIFY(!field.inferred);
    QVERIFY(field.arrayPath);
    QCOMPARE(field.definition.type, FieldValueType::Number);
    QCOMPARE(field.definition.defaultValue, QStringLiteral("12"));
    QCOMPARE(field.elementIds, QStringList({"text", "grid"}));
    QVERIFY(TemplateInputSchema::validate(document, {}).isEmpty());
    QVERIFY(!TemplateInputSchema::validate(document, QJsonObject{{"items", 12}}).isEmpty());
    QVERIFY(!TemplateInputSchema::validate(document, QJsonObject{{"items", QJsonArray{1}}}).isEmpty());
    document.fieldSchema.clear();
    document.sampleData = QJsonObject{{"items", 12}};
    const auto inferred = TemplateInputSchema::fields(document).first();
    QCOMPARE(inferred.definition.type, FieldValueType::List);
    QVERIFY(TemplateInputSchema::validate(document, QJsonObject{{"items", QJsonArray{1}}}).isEmpty());
}

void TemplateInputSchemaTests::checksOnlyRenderedGridRowsWithoutPromotingCellFields()
{
    auto grid = bound("grid", "");
    grid.type = TemplateElementType::ArrayGrid;
    grid.dataPath = QStringLiteral("order.items");
    grid.arrayGridRows = 1;
    grid.arrayGridColumns = 2;
    grid.locked = true;
    grid.arrayGridCellTemplate = QStringLiteral("${text}:${value}:${text}");
    auto hidden = grid;
    hidden.visible = false;
    hidden.dataPath = QStringLiteral("hiddenItems");
    auto document = withElements({grid, hidden});
    document.layers.first().locked = true;
    QJsonObject values{{"order", QJsonObject{{"items", QJsonArray{
        QJsonObject{{"value", QJsonValue(QJsonValue::Null)}},
        QJsonObject{{"text", 0}, {"value", false}},
        QJsonObject{},
    }}}}, {"text", "global-must-not-fill-row"}, {"value", "global-must-not-fill-row"}};
    const auto issues = TemplateInputSchema::validate(document, values);
    QCOMPARE(issues.size(), 2);
    for (const auto& issue : issues) {
        QCOMPARE(issue.fieldKey, QStringLiteral("order.items"));
        QCOMPARE(issue.elementIds, QStringList({"grid"}));
        QVERIFY(issue.message.contains(QStringLiteral("order.items")));
        QVERIFY(issue.message.contains(QStringLiteral("第1项")));
    }
    QVERIFY(issues[0].message.contains(QStringLiteral("text")));
    QVERIFY(issues[1].message.contains(QStringLiteral("value")));
    QCOMPARE(TemplateInputSchema::fields(document).size(), 1);
    values.insert("order.items", QJsonArray{});
    QVERIFY(TemplateInputSchema::validate(document, values).isEmpty());
    document.layers.first().visible = false;
    QVERIFY(TemplateInputSchema::validate(document, {}).isEmpty());
}

void TemplateInputSchemaTests::gridRowsPreferLiteralKeysAndWrapPrimitiveValues()
{
    auto grid = bound("grid", "");
    grid.type = TemplateElementType::ArrayGrid;
    grid.dataPath = QStringLiteral("items");
    grid.arrayGridRows = 0;
    grid.arrayGridColumns = -1;
    grid.arrayGridCellTemplate = QStringLiteral("${product.name}");
    auto document = withElements({grid});
    QJsonObject row{{"product.name", QJsonValue(QJsonValue::Null)}, {"product", QJsonObject{{"name", "nested"}}}};
    auto issues = TemplateInputSchema::validate(document, QJsonObject{{"items", QJsonArray{row, QJsonObject{}}}});
    QCOMPARE(issues.size(), 1);
    QVERIFY(issues.first().message.contains(QStringLiteral("product.name")));
    row.insert("product.name", "");
    QVERIFY(TemplateInputSchema::validate(document, QJsonObject{{"items", QJsonArray{row}}}).isEmpty());
    row.remove("product.name");
    QVERIFY(TemplateInputSchema::validate(document, QJsonObject{{"items", QJsonArray{row}}}).isEmpty());

    auto& element = document.layers.first().elements.first();
    element.arrayGridColumns = 6;
    element.arrayGridCellTemplate = QStringLiteral("   ");
    issues = TemplateInputSchema::validate(document, QJsonObject{{"items", QJsonArray{
        0, false, "", QJsonValue(QJsonValue::Null), QJsonObject{{"value", "object-value"}}, QJsonObject{},
    }}});
    QCOMPARE(issues.size(), 2);
    QVERIFY(issues[0].message.contains(QStringLiteral("第4项")));
    QVERIFY(issues[1].message.contains(QStringLiteral("第6项")));
    for (const auto& issue : issues) {
        QVERIFY(issue.message.contains(QStringLiteral("value")));
    }
}

void TemplateInputSchemaTests::mixedArrayPathRequiresLiteralScalarKey_data()
{
    QTest::addColumn<int>("scalarType");
    QTest::newRow("bound") << int(TemplateElementType::BoundField);
    QTest::newRow("fixed") << int(TemplateElementType::FixedText);
    QTest::newRow("qr") << int(TemplateElementType::QrCode);
}

void TemplateInputSchemaTests::mixedArrayPathRequiresLiteralScalarKey()
{
    QFETCH(int, scalarType);
    auto scalar = bound("scalar", "order.items");
    scalar.type = static_cast<TemplateElementType>(scalarType);
    scalar.text = QStringLiteral("${order.items}");
    auto grid = bound("grid", "");
    grid.type = TemplateElementType::ArrayGrid;
    grid.dataPath = QStringLiteral("order.items");
    grid.arrayGridCellTemplate = QStringLiteral("${value}");
    auto document = withElements({scalar, grid});
    QJsonObject values{{"order", QJsonObject{{"items", QJsonArray{0}}}}};
    auto issues = TemplateInputSchema::validate(document, values);
    QCOMPARE(issues.size(), 1);
    QCOMPARE(issues.first().fieldKey, QStringLiteral("order.items"));
    QCOMPARE(issues.first().elementIds, QStringList({"scalar", "grid"}));
    values.insert("order.items", QJsonArray{0});
    QVERIFY(TemplateInputSchema::validate(document, values).isEmpty());
    FieldDefinition optional;
    optional.key = QStringLiteral("order.items");
    optional.type = FieldValueType::List;
    document.fieldSchema = {optional};
    values.remove("order.items");
    QCOMPARE(TemplateInputSchema::validate(document, values).size(), 1);
    QVERIFY(TemplateInputSchema::validate(document, {}).isEmpty());
}

void TemplateInputSchemaTests::emptyMarkersMustMatchTypeAndMissingMessageKeepsDisplayName()
{
    auto document = withElements({bound("name", "productName"), bound("list", "items"), bound("object", "metadata")});
    FieldDefinition name;
    name.key = QStringLiteral("productName");
    name.displayName = QStringLiteral("品名");
    name.required = true;
    FieldDefinition list;
    list.key = QStringLiteral("items");
    list.type = FieldValueType::List;
    FieldDefinition object;
    object.key = QStringLiteral("metadata");
    object.type = FieldValueType::Object;
    document.fieldSchema = {name, list, object};
    const auto missing = TemplateInputSchema::validate(document, {});
    QCOMPARE(missing.size(), 1);
    QVERIFY(missing.first().message.contains(QStringLiteral("品名")));
    QVERIFY(missing.first().message.contains(QStringLiteral("productName")));
    document.fieldSchema.first().required = false;
    QVERIFY(TemplateInputSchema::validate(document, QJsonObject{{"productName", ""}, {"items", QJsonArray{}}, {"metadata", QJsonObject{}}}).isEmpty());
    QCOMPARE(TemplateInputSchema::validate(document, QJsonObject{{"productName", QJsonArray{}}, {"items", QJsonObject{}}, {"metadata", QJsonArray{}}}).size(), 3);
    QCOMPARE(TemplateInputSchema::validate(document, QJsonObject{{"items", ""}, {"metadata", ""}}).size(), 2);
    document.fieldSchema.clear();
    document.sampleData = QJsonObject{{"productName", "example"}, {"items", QJsonArray{}}, {"metadata", QJsonObject{}}};
    QVERIFY(TemplateInputSchema::validate(document, QJsonObject{{"productName", ""}, {"items", QJsonArray{}}, {"metadata", QJsonObject{}}}).isEmpty());
    QCOMPARE(TemplateInputSchema::validate(document, QJsonObject{{"productName", QJsonArray{}}, {"items", QJsonObject{}}, {"metadata", QJsonArray{}}}).size(), 3);
}

void TemplateInputSchemaTests::overridesInferredNamesWithoutChangingInputSemantics()
{
    auto document = withElements({bound("first", "productName"), bound("second", "productName"),
                                  bound("amount", "amount"), bound("enabled", "enabled")});
    document.sampleData = QJsonObject{{"productName", "sample-only"}, {"amount", 12}, {"enabled", true}};
    const auto original = TemplateInputSchema::fields(document);
    const auto sampleData = document.sampleData;
    document.fieldDisplayNames = {{QStringLiteral("productName"), QStringLiteral("  品名  ")},
                                 {QStringLiteral("amount"), QStringLiteral("金额")},
                                 {QStringLiteral("orphan"), QStringLiteral("未绑定名称")}};
    const auto fields = TemplateInputSchema::fields(document);
    QCOMPARE(fields.size(), original.size());
    for (qsizetype index = 0; index < fields.size(); ++index) {
        auto expected = original[index].definition;
        if (document.fieldDisplayNames.contains(expected.key)) {
            expected.displayName = document.fieldDisplayNames.value(expected.key).trimmed();
        }
        QCOMPARE(FieldSchemaJson::toJson({fields[index].definition}), FieldSchemaJson::toJson({expected}));
        QVERIFY(fields[index].inferred);
        QVERIFY(fields[index].definition.required);
        QCOMPARE(fields[index].arrayPath, original[index].arrayPath);
        QCOMPARE(fields[index].elementIds, original[index].elementIds);
    }
    QVERIFY(document.fieldSchema.isEmpty());
    QCOMPARE(document.sampleData, sampleData);
    const auto missing = TemplateInputSchema::validate(document, {});
    QCOMPARE(missing.size(), 3);
    QCOMPARE(missing[0].fieldKey, QStringLiteral("productName"));
    QCOMPARE(missing[0].elementIds, QStringList({"first", "second"}));
    QVERIFY(missing[0].message.contains(QStringLiteral("品名")));
    QVERIFY(missing[0].message.contains(QStringLiteral("productName")));
    // 名称不能把推断字段变成显式必填，主动留空仍然有效。
    QVERIFY(TemplateInputSchema::validate(document, QJsonObject{{"productName", ""}, {"amount", "   "}, {"enabled", false}}).isEmpty());
    const auto invalid = TemplateInputSchema::validate(document, QJsonObject{{"productName", ""}, {"amount", "wrong"}, {"enabled", false}});
    QCOMPARE(invalid.size(), 1);
    QCOMPARE(invalid.first().fieldKey, QStringLiteral("amount"));
}

void TemplateInputSchemaTests::overridesExplicitNamesWithoutChangingSchema()
{
    auto document = withElements({bound("amount", "amount"), bound("items", "items")});
    FieldDefinition amount;
    amount.key = QStringLiteral("amount");
    amount.displayName = QStringLiteral("原金额");
    amount.type = FieldValueType::Money;
    amount.defaultValue = QStringLiteral("12.50");
    amount.sampleValue = QStringLiteral("99");
    amount.format = QStringLiteral("0.00");
    amount.source = FieldSource::BuiltIn;
    FieldDefinition items;
    items.key = QStringLiteral("items");
    items.displayName = QStringLiteral("原明细");
    items.type = FieldValueType::List;
    items.required = true;
    items.defaultValue = QStringLiteral("not-a-print-value");
    document.fieldSchema = {amount, items};
    const auto originalSchema = FieldSchemaJson::toJson(document.fieldSchema);
    document.fieldDisplayNames = {{QStringLiteral("amount"), QStringLiteral("  新金额  ")},
                                 {QStringLiteral("items"), QStringLiteral("新明细")},
                                 {QStringLiteral("orphan"), QStringLiteral("未绑定名称")}};
    const auto fields = TemplateInputSchema::fields(document);
    QCOMPARE(fields.size(), 2);
    amount.displayName = QStringLiteral("新金额");
    items.displayName = QStringLiteral("新明细");
    QCOMPARE(FieldSchemaJson::toJson({fields[0].definition, fields[1].definition}), FieldSchemaJson::toJson({amount, items}));
    QVERIFY(!fields[0].inferred);
    QVERIFY(!fields[1].inferred);
    QCOMPARE(fields[0].elementIds, QStringList({"amount"}));
    QCOMPARE(fields[1].elementIds, QStringList({"items"}));
    QCOMPARE(FieldSchemaJson::toJson(document.fieldSchema), originalSchema);
    const QJsonObject valid{{"items", QJsonArray{1}}};
    QVERIFY(TemplateInputSchema::validate(document, valid).isEmpty());
    auto blankOptional = valid;
    blankOptional.insert("amount", "");
    QVERIFY(TemplateInputSchema::validate(document, blankOptional).isEmpty());
    QCOMPARE(TemplateInputSchema::validate(document, {}).size(), 1);
    QCOMPARE(TemplateInputSchema::validate(document, QJsonObject{{"items", QJsonArray{}}}).size(), 1);
    QCOMPARE(TemplateInputSchema::validate(document, QJsonObject{{"items", "   "}}).size(), 1);
    auto wrongType = valid;
    wrongType.insert("amount", "wrong");
    const auto issues = TemplateInputSchema::validate(document, wrongType);
    QCOMPARE(issues.size(), 1);
    QCOMPARE(issues.first().fieldKey, QStringLiteral("amount"));
    QVERIFY(issues.first().message.contains(QStringLiteral("新金额")));
    QCOMPARE(FieldSchemaJson::toJson(document.fieldSchema), originalSchema);
}

void TemplateInputSchemaTests::emptyNameOverridesFallBack_data()
{
    QTest::addColumn<QString>("overrideName");
    QTest::newRow("empty") << QString();
    QTest::newRow("whitespace") << QStringLiteral(" \t ");
}

void TemplateInputSchemaTests::emptyNameOverridesFallBack()
{
    QFETCH(QString, overrideName);
    auto document = withElements({bound("name", "productName")});
    document.fieldDisplayNames.insert(QStringLiteral("productName"), overrideName);
    QCOMPARE(TemplateInputSchema::fields(document).first().definition.displayName, QStringLiteral("productName"));
    FieldDefinition definition;
    definition.key = QStringLiteral("productName");
    definition.displayName = QStringLiteral("原品名");
    document.fieldSchema = {definition};
    QCOMPARE(TemplateInputSchema::fields(document).first().definition.displayName, definition.displayName);
    document.fieldDisplayNames.insert(QStringLiteral("productName"), QStringLiteral("覆盖品名"));
    QCOMPARE(TemplateInputSchema::fields(document).first().definition.displayName, QStringLiteral("覆盖品名"));
    document.fieldDisplayNames.remove(QStringLiteral("productName"));
    QCOMPARE(TemplateInputSchema::fields(document).first().definition.displayName, definition.displayName);
    QCOMPARE(document.fieldSchema.first().displayName, definition.displayName);
}

void TemplateInputSchemaTests::nameOverridesDoNotBecomePrintValues()
{
    const auto document = withDisplayNames();
    QList<FieldDefinition> definitions;
    for (const auto& field : TemplateInputSchema::fields(document)) {
        definitions.append(field.definition);
        QVERIFY(field.definition.defaultValue.isEmpty());
        QVERIFY(field.definition.sampleValue.isEmpty());
    }
    const QJsonObject request{{"productName", "actual-product"}};
    const auto context = TemplateRenderContextBuilder::build(definitions, {}, request);
    QCOMPARE(context.values, request);
    QVERIFY(!context.hasMissingRequiredFields());
    const auto blank = TemplateRenderContextBuilder::build(definitions, {}, QJsonObject{{"productName", ""}});
    QCOMPARE(blank.values, QJsonObject({{"productName", ""}}));
    const auto missing = TemplateRenderContextBuilder::build(definitions, {}, {});
    QCOMPARE(missing.values.value("productName"), QJsonValue(""));
    QCOMPARE(missing.values.size(), 1);
    QVERIFY(missing.hasMissingRequiredFields());
    QCOMPARE(TemplateInputSchema::validate(document, {}).size(), 1);
}

void TemplateInputSchemaTests::roundTripsDisplayNamesInDocumentJson()
{
    const auto document = withDisplayNames();
    const auto json = TemplateDocumentJson::toJson(document);
    const QJsonObject names{{"productName", QStringLiteral("品名")},
                            {"orphan", QStringLiteral("未绑定名称")}, {"emptyName", ""}};
    QCOMPARE(json.value("fieldDisplayNames"), QJsonValue(names));
    const auto restored = TemplateDocumentJson::fromJson(json);
    QCOMPARE(restored.fieldDisplayNames, document.fieldDisplayNames);
    QVERIFY(restored.fieldSchema.isEmpty());
    QVERIFY(restored.sampleData.isEmpty());
    QCOMPARE(TemplateDocumentJson::toJson(restored), json);
    const auto fields = TemplateInputSchema::fields(restored);
    QCOMPARE(fields.size(), 1);
    QCOMPARE(fields.first().definition.displayName, QStringLiteral("品名"));
}

void TemplateInputSchemaTests::omitsEmptyDisplayNamesFromDocumentJson()
{
    auto document = withDisplayNames();
    document.fieldDisplayNames.clear();
    const auto legacy = TemplateDocumentJson::toJson(document);
    QVERIFY(!legacy.contains("fieldDisplayNames"));
    const auto restored = TemplateDocumentJson::fromJson(legacy);
    QVERIFY(restored.fieldDisplayNames.isEmpty());
    QCOMPARE(TemplateDocumentJson::toJson(restored), legacy);
    auto explicitEmpty = legacy;
    explicitEmpty.insert("fieldDisplayNames", QJsonObject{});
    const auto restoredEmpty = TemplateDocumentJson::fromJson(explicitEmpty);
    QVERIFY(restoredEmpty.fieldDisplayNames.isEmpty());
    QCOMPARE(TemplateDocumentJson::toJson(restoredEmpty), legacy);
}

void TemplateInputSchemaTests::importsOptionalDisplayNames()
{
    auto document = withDisplayNames();
    document.fieldDisplayNames.clear();
    auto json = TemplateDocumentJson::toJson(document);
    QString error;
    QVERIFY2(TemplateDocumentJson::validateForImport(json, &error), qPrintable(error));
    json.insert("fieldDisplayNames", QJsonObject{});
    QVERIFY2(TemplateDocumentJson::validateForImport(json, &error), qPrintable(error));
    json.insert("fieldDisplayNames", QJsonObject{{"productName", QStringLiteral("  品名  ")},
                                                {"orphan", ""}, {"blankName", "   "}});
    QVERIFY2(TemplateDocumentJson::validateForImport(json, &error), qPrintable(error));
    QVERIFY(TemplateDocumentJson::validateForImport(json, nullptr));
    const auto restored = TemplateDocumentJson::fromJson(json);
    QVERIFY(restored.fieldSchema.isEmpty());
    const auto fields = TemplateInputSchema::fields(restored);
    QCOMPARE(fields.size(), 1);
    QCOMPARE(fields.first().definition.displayName, QStringLiteral("品名"));
}

void TemplateInputSchemaTests::rejectsMalformedDisplayNames_data()
{
    QTest::addColumn<QJsonValue>("names");
    QTest::newRow("null-map") << QJsonValue(QJsonValue::Null);
    QTest::newRow("array-map") << QJsonValue(QJsonArray{});
    QTest::newRow("text-map") << QJsonValue("name");
    QTest::newRow("number-map") << QJsonValue(1);
    QTest::newRow("bool-map") << QJsonValue(false);
    QTest::newRow("null-name") << QJsonValue(QJsonObject{{"productName", QJsonValue(QJsonValue::Null)}});
    QTest::newRow("number-name") << QJsonValue(QJsonObject{{"productName", 12}});
    QTest::newRow("bool-name") << QJsonValue(QJsonObject{{"productName", true}});
    QTest::newRow("array-name") << QJsonValue(QJsonObject{{"productName", QJsonArray{}}});
    QTest::newRow("object-name") << QJsonValue(QJsonObject{{"productName", QJsonObject{}}});
    QTest::newRow("empty-key") << QJsonValue(QJsonObject{{"", "name"}});
    QTest::newRow("blank-key") << QJsonValue(QJsonObject{{"   ", "name"}});
    QTest::newRow("leading-space-key") << QJsonValue(QJsonObject{{" productName", "name"}});
    QTest::newRow("trailing-space-key") << QJsonValue(QJsonObject{{"productName ", "name"}});
    QTest::newRow("tab-key") << QJsonValue(QJsonObject{{"\tproductName", "name"}});
}

void TemplateInputSchemaTests::rejectsMalformedDisplayNames()
{
    QFETCH(QJsonValue, names);
    auto json = TemplateDocumentJson::toJson(withDisplayNames());
    json.insert("fieldDisplayNames", names);
    QString error;
    QVERIFY(!TemplateDocumentJson::validateForImport(json, &error));
    QVERIFY2(error.contains(QStringLiteral("fieldDisplayNames")), qPrintable(error));
    QVERIFY(!TemplateDocumentJson::validateForImport(json, nullptr));
}

void TemplateInputSchemaTests::roundTripsDisplayNamesInSettingsJson()
{
    const auto document = withDisplayNames();
    PrintClientSettings settings;
    settings.templateDocuments.insert(document.templateKey, document);
    const auto json = PrintClientSettingsJson::toJson(settings);
    const auto restored = PrintClientSettingsJson::fromJson(json);
    QVERIFY(restored.templateDocuments.contains(document.templateKey));
    const auto saved = restored.templateDocuments.value(document.templateKey);
    QCOMPARE(saved.fieldDisplayNames, document.fieldDisplayNames);
    QVERIFY(saved.fieldSchema.isEmpty());
    QVERIFY(saved.sampleData.isEmpty());
    QCOMPARE(PrintClientSettingsJson::toJson(restored), json);
}

void TemplateInputSchemaTests::storesDisplayNamesInTemplateLibrary()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    TemplateLibraryStore store(directory.path());
    auto document = withDisplayNames();
    QString error;
    QVERIFY2(store.saveTemplate(document, &error), qPrintable(error));
    const auto restored = store.loadTemplate(document.id, &error);
    QVERIFY2(restored.has_value(), qPrintable(error));
    QCOMPARE(restored->fieldDisplayNames, document.fieldDisplayNames);
    QCOMPARE(TemplateDocumentJson::toJson(*restored), TemplateDocumentJson::toJson(document));
    document.fieldDisplayNames.clear();
    QVERIFY2(store.saveTemplate(document, &error), qPrintable(error));
    const auto cleared = store.loadTemplate(document.id, &error);
    QVERIFY2(cleared.has_value(), qPrintable(error));
    QVERIFY(cleared->fieldDisplayNames.isEmpty());
    QVERIFY(!TemplateDocumentJson::toJson(*cleared).contains("fieldDisplayNames"));
}

QTEST_GUILESS_MAIN(TemplateInputSchemaTests)
#include "tst_template_input_schema.moc"

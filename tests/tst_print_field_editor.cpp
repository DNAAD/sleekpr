#include "sleekpr/app/AppTheme.h"
#include "sleekpr/app/PrintFieldEditorPanel.h"

#include <QCheckBox>
#include <QCoreApplication>
#include <QEvent>
#include <QFormLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QRegularExpression>
#include <QScrollArea>
#include <QScrollBar>
#include <QSignalSpy>
#include <QTabBar>
#include <QTabWidget>
#include <QTextCursor>
#include <QtTest>

using namespace sleekpr;

namespace {

core::FieldDefinition field(const QString& key, core::FieldValueType type)
{
    core::FieldDefinition definition;
    definition.key = key;
    definition.displayName = key;
    definition.type = type;
    return definition;
}

core::TemplateDocument documentWithFields()
{
    core::TemplateDocument document;
    document.fieldSchema = {
        field(QStringLiteral("name"), core::FieldValueType::Text),
        field(QStringLiteral("quantity"), core::FieldValueType::Number),
        field(QStringLiteral("price"), core::FieldValueType::Money),
        field(QStringLiteral("weight"), core::FieldValueType::Weight),
        field(QStringLiteral("enabled"), core::FieldValueType::Boolean),
        field(QStringLiteral("date"), core::FieldValueType::Date),
        field(QStringLiteral("time"), core::FieldValueType::DateTime),
        field(QStringLiteral("items"), core::FieldValueType::List),
        field(QStringLiteral("details"), core::FieldValueType::Object),
    };
    return document;
}

QJsonObject valuesWithUnknownFields()
{
    return {
        {QStringLiteral("name"), QStringLiteral("original")},
        {QStringLiteral("header_items"), QJsonArray{QStringLiteral("A"), QStringLiteral("B")}},
        {QStringLiteral("placeholder"), QStringLiteral("${external_value}")},
        {QStringLiteral("unknown_object"), QJsonObject{{QStringLiteral("nested"), 7}}},
        {QStringLiteral("unknown_null"), QJsonValue::Null},
    };
}

core::TemplateDocument documentWithReferences()
{
    core::TemplateDocument document;
    core::TemplateLayer layer;
    layer.id = QStringLiteral("main");
    core::TemplateElement product;
    product.id = QStringLiteral("product");
    product.type = core::TemplateElementType::BoundField;
    product.fieldKey = QStringLiteral("productName");
    core::TemplateElement text;
    text.id = QStringLiteral("text");
    text.text = QStringLiteral("${customer} / ${quantity} / ${enabled} / ${details}");
    core::TemplateElement qr;
    qr.id = QStringLiteral("qr");
    qr.type = core::TemplateElementType::QrCode;
    qr.fieldKey = QStringLiteral("qrPayload");
    core::TemplateElement qrTemplate;
    qrTemplate.id = QStringLiteral("qrTemplate");
    qrTemplate.type = core::TemplateElementType::QrCode;
    qrTemplate.payload = QStringLiteral("https://example.test/${token}");
    core::TemplateElement grid;
    grid.id = QStringLiteral("grid");
    grid.type = core::TemplateElementType::ArrayGrid;
    grid.dataPath = QStringLiteral("order.items");
    grid.arrayGridCellTemplate = QStringLiteral("${rowName}:${rowValue}");
    layer.elements = {product, text, qr, qrTemplate, grid};
    document.layers = {layer};
    document.sampleData = {
        {QStringLiteral("productName"), QStringLiteral("sample-product")},
        {QStringLiteral("customer"), QStringLiteral("sample-customer")},
        {QStringLiteral("quantity"), 12},
        {QStringLiteral("enabled"), true},
        {QStringLiteral("details"), QJsonObject{{QStringLiteral("sample"), 1}}},
        {QStringLiteral("order"), QJsonObject{{QStringLiteral("items"), QJsonArray{1, 2}}}},
        {QStringLiteral("sampleOnly"), QStringLiteral("not-a-field")},
    };
    return document;
}

} // 匿名命名空间

class PrintFieldEditorTests final : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase() { app::applySleekprTheme(*qApp); }
    void documentDoesNotPopulateSampleOrDefaultValues();
    void schemaFreeDocumentOnlyOffersJson();
    void unknownFieldsSurviveBothModeDirections();
    void untouchedSchemaValuesKeepTheirOriginalJsonTypes();
    void invalidJsonCannotBeDiscardedByModeSwitch();
    void jsonRequiresObjectAndHonorsUtf8SizeLimit();
    void jsonErrorColumnsCountCharactersNotUtf8Bytes();
    void numbersRemainEmptyAndAcceptZero_data();
    void numbersRemainEmptyAndAcceptZero();
    void incompleteNumbersAreNotSilentlyConverted();
    void falseAndUnsetBooleanRemainDistinct();
    void structuredFieldsRoundTripAndReportErrors();
    void dateTextIsPreservedWithoutInferringValues();
    void setValuesAndModeSwitchDoNotEmitEdits();
    void narrowPanelDoesNotForceHorizontalScrolling();
    void jsonModeRejectsWrongSchemaTypesWithoutCoercion();
    void formLabelsRemainVisible();
    void referencesWithoutSchemaCreateEmptyForm();
    void inferredTextRequiresExplicitEmptyChoice();
    void inferredEmptyChoicesPreserveDraftsAcrossModes();
    void inferredEmptyMarkers_data();
    void inferredEmptyMarkers();
    void inferredPresetZeroAndFalseAreNotEmpty();
    void explicitSchemaDoesNotOfferEmptyBypass();
    void nestedArrayValuesAreDisplayOnlyUntilEdited();
    void nestedArrayLiteralKeyTakesPriority();
    void inferredTypesRejectInvalidJson();
    void inferredLabelsAndEmptyControlsFitNarrowPanel();
    void focusFieldSwitchesModeAndScrollsWithoutEditing();
    void focusFieldUsesEmptyCheckboxWithoutChangingDraft();
    void focusUnknownFieldDoesNotEditValues();
    void focusFieldPreservesInvalidJsonAndLocatesParseError();
    void focusJsonErrorUsesUtf8ParseOffset_data();
    void focusJsonErrorUsesUtf8ParseOffset();
    void focusJsonErrorUsesOneBasedCoordinates();
    void focusJsonErrorSwitchesValidFormWithoutEditing();
    void focusJsonErrorPreservesInvalidForm_data();
    void focusJsonErrorPreservesInvalidForm();
    void invalidFieldKeyRecordsFirstFormFailureAndClears();
    void invalidFieldKeyRecordsJsonTypeFailureAndClears();
    void fieldTitlesUseDisplayNameWithoutRenamingValues();
    void fieldErrorsUseDisplayNamesAndKeepInvalidKeys();
};

void PrintFieldEditorTests::documentDoesNotPopulateSampleOrDefaultValues()
{
    app::PrintFieldEditorPanel panel;
    auto document = documentWithFields();
    document.sampleData = {{QStringLiteral("name"), QStringLiteral("sample")}};
    document.fieldSchema[0].defaultValue = QStringLiteral("default");
    document.fieldSchema[0].sampleValue = QStringLiteral("field-sample");
    panel.setDocument(document);
    bool ok = false;
    QVERIFY(panel.values(&ok).isEmpty());
    QVERIFY(ok);
    auto* name = panel.findChild<QLineEdit*>(QStringLiteral("printField_name"));
    QVERIFY(name != nullptr);
    QVERIFY(name->text().isEmpty());
    auto* tabs = panel.findChild<QTabWidget*>(QStringLiteral("printFieldModeTabs"));
    QVERIFY(tabs != nullptr);
    QCOMPARE(tabs->currentIndex(), 0);
}

void PrintFieldEditorTests::schemaFreeDocumentOnlyOffersJson()
{
    app::PrintFieldEditorPanel panel;
    panel.setDocument({});
    auto* tabs = panel.findChild<QTabWidget*>(QStringLiteral("printFieldModeTabs"));
    auto* json = panel.findChild<QPlainTextEdit*>(QStringLiteral("printValuesJsonEdit"));
    QVERIFY(tabs != nullptr && json != nullptr);
    QCOMPARE(tabs->count(), 1);
    json->setPlainText(QStringLiteral("{\"items\":[1,2],\"external\":false}"));
    bool ok = false;
    const auto result = panel.values(&ok);
    QVERIFY(ok);
    QCOMPARE(result.value(QStringLiteral("items")).toArray(), (QJsonArray{1, 2}));
    QCOMPARE(result.value(QStringLiteral("external")), QJsonValue(false));
}

void PrintFieldEditorTests::unknownFieldsSurviveBothModeDirections()
{
    app::PrintFieldEditorPanel panel;
    panel.setDocument(documentWithFields());
    auto expected = valuesWithUnknownFields();
    panel.setValues(expected);
    auto* name = panel.findChild<QLineEdit*>(QStringLiteral("printField_name"));
    auto* tabs = panel.findChild<QTabWidget*>(QStringLiteral("printFieldModeTabs"));
    auto* json = panel.findChild<QPlainTextEdit*>(QStringLiteral("printValuesJsonEdit"));
    QVERIFY(name != nullptr && tabs != nullptr && json != nullptr);
    name->setText(QStringLiteral("form-edit"));
    expected.insert(QStringLiteral("name"), QStringLiteral("form-edit"));
    tabs->setCurrentIndex(1);
    QCOMPARE(QJsonDocument::fromJson(json->toPlainText().toUtf8()).object(), expected);
    expected.insert(QStringLiteral("json_only"), QJsonArray{0, false, QJsonObject{}});
    expected.insert(QStringLiteral("name"), QStringLiteral("json-edit"));
    json->setPlainText(QString::fromUtf8(QJsonDocument(expected).toJson()));
    tabs->setCurrentIndex(0);
    QCOMPARE(name->text(), QStringLiteral("json-edit"));
    QCOMPARE(panel.values(), expected);
    tabs->setCurrentIndex(1);
    tabs->setCurrentIndex(0);
    QCOMPARE(panel.values(), expected);
}

void PrintFieldEditorTests::untouchedSchemaValuesKeepTheirOriginalJsonTypes()
{
    app::PrintFieldEditorPanel panel;
    panel.setDocument(documentWithFields());
    const QJsonObject original{
        {QStringLiteral("name"), QJsonArray{1, 2}},
        {QStringLiteral("quantity"), QJsonValue::Null},
        {QStringLiteral("enabled"), false},
    };
    panel.setValues(original);
    auto* tabs = panel.findChild<QTabWidget*>(QStringLiteral("printFieldModeTabs"));
    QVERIFY(tabs != nullptr);
    QCOMPARE(panel.values(), original);
    tabs->setCurrentIndex(1);
    tabs->setCurrentIndex(0);
    QCOMPARE(panel.values(), original);
}

void PrintFieldEditorTests::invalidJsonCannotBeDiscardedByModeSwitch()
{
    app::PrintFieldEditorPanel panel;
    panel.setDocument(documentWithFields());
    panel.setValues(valuesWithUnknownFields());
    auto* tabs = panel.findChild<QTabWidget*>(QStringLiteral("printFieldModeTabs"));
    auto* json = panel.findChild<QPlainTextEdit*>(QStringLiteral("printValuesJsonEdit"));
    auto* label = panel.findChild<QLabel*>(QStringLiteral("printValuesErrorLabel"));
    QVERIFY(tabs != nullptr && json != nullptr && label != nullptr);
    tabs->setCurrentIndex(1);
    const auto invalid = QString::fromUtf8("{\n  \"name\": \"金\",\n  \"items\": [\n}");
    json->setPlainText(invalid);
    bool ok = true;
    QString error;
    QVERIFY(panel.values(&ok, &error).isEmpty());
    QVERIFY(!ok);
    QVERIFY(error.contains(QString::fromUtf8("行")) && error.contains(QString::fromUtf8("列")));
    QCOMPARE(label->text(), error);
    QCOMPARE(label->property("statusRole").toString(), QStringLiteral("danger"));
    tabs->setCurrentIndex(0);
    QCOMPARE(tabs->currentIndex(), 1);
    QCOMPARE(json->toPlainText(), invalid);
    json->setPlainText(QStringLiteral("{\"name\":\"repaired\",\"extra\":[9]}"));
    tabs->setCurrentIndex(0);
    QCOMPARE(tabs->currentIndex(), 0);
    QCOMPARE(panel.values().value(QStringLiteral("extra")).toArray(), QJsonArray{9});
    QVERIFY(label->text().isEmpty());
}

void PrintFieldEditorTests::jsonRequiresObjectAndHonorsUtf8SizeLimit()
{
    app::PrintFieldEditorPanel panel;
    auto* json = panel.findChild<QPlainTextEdit*>(QStringLiteral("printValuesJsonEdit"));
    QVERIFY(json != nullptr);
    for (const auto& text : {QStringLiteral("[]"), QStringLiteral("null"), QStringLiteral("42"), QString()}) {
        json->setPlainText(text);
        bool ok = true;
        QString error;
        panel.values(&ok, &error);
        QVERIFY(!ok);
        QVERIFY(!error.isEmpty());
    }
    // 按 UTF-8 字节限制体积，中文不能因为字符数较少绕过上限。
    const auto oversized = QStringLiteral("{\"text\":\"") + QString(400000, QChar(0x91d1)) + QStringLiteral("\"}");
    QVERIFY(oversized.size() < 1024 * 1024);
    json->setPlainText(oversized);
    bool ok = true;
    QString error;
    panel.values(&ok, &error);
    QVERIFY(!ok);
    QVERIFY(error.contains(QStringLiteral("1 MB")));
    json->setPlainText(QStringLiteral("{}"));
    panel.values(&ok, &error);
    QVERIFY(ok);
    QVERIFY(error.isEmpty());
}

void PrintFieldEditorTests::jsonErrorColumnsCountCharactersNotUtf8Bytes()
{
    app::PrintFieldEditorPanel panel;
    auto* json = panel.findChild<QPlainTextEdit*>(QStringLiteral("printValuesJsonEdit"));
    QVERIFY(json != nullptr);
    const QRegularExpression position(QString::fromUtf8("第 (\\d+) 行，第 (\\d+) 列"));
    json->setPlainText(QStringLiteral("{\"name\":\"x\",\"bad\":@}"));
    QString asciiError;
    panel.values(nullptr, &asciiError);
    json->setPlainText(QString::fromUtf8("{\"name\":\"金\",\"bad\":@}"));
    QString chineseError;
    panel.values(nullptr, &chineseError);
    const auto asciiMatch = position.match(asciiError);
    const auto chineseMatch = position.match(chineseError);
    QVERIFY(asciiMatch.hasMatch() && chineseMatch.hasMatch());
    QCOMPARE(asciiMatch.captured(1), chineseMatch.captured(1));
    QCOMPARE(asciiMatch.captured(2), chineseMatch.captured(2));
}

void PrintFieldEditorTests::numbersRemainEmptyAndAcceptZero_data()
{
    QTest::addColumn<QString>("key");
    QTest::newRow("number") << QStringLiteral("quantity");
    QTest::newRow("money") << QStringLiteral("price");
    QTest::newRow("weight") << QStringLiteral("weight");
}

void PrintFieldEditorTests::numbersRemainEmptyAndAcceptZero()
{
    QFETCH(QString, key);
    app::PrintFieldEditorPanel panel;
    panel.setDocument(documentWithFields());
    auto* number = panel.findChild<QLineEdit*>(QStringLiteral("printField_") + key);
    QVERIFY(number != nullptr && number->validator() != nullptr);
    QVERIFY(number->text().isEmpty());
    QVERIFY(!panel.values().contains(key));
    number->setText(QStringLiteral("0"));
    bool ok = false;
    QCOMPARE(panel.values(&ok).value(key), QJsonValue(0));
    QVERIFY(ok);
    number->clear();
    QVERIFY(!panel.values(&ok).contains(key));
    QVERIFY(ok);
}

void PrintFieldEditorTests::incompleteNumbersAreNotSilentlyConverted()
{
    app::PrintFieldEditorPanel panel;
    panel.setDocument(documentWithFields());
    auto* number = panel.findChild<QLineEdit*>(QStringLiteral("printField_quantity"));
    auto* tabs = panel.findChild<QTabWidget*>(QStringLiteral("printFieldModeTabs"));
    QVERIFY(number != nullptr && tabs != nullptr);
    for (const auto& invalid : {QStringLiteral("-"), QStringLiteral("1e"), QStringLiteral("12oops")}) {
        number->setText(invalid);
        bool ok = true;
        QString error;
        panel.values(&ok, &error);
        QVERIFY(!ok);
        QVERIFY(error.contains(QStringLiteral("quantity")));
        tabs->setCurrentIndex(1);
        QCOMPARE(tabs->currentIndex(), 0);
        QCOMPARE(number->text(), invalid);
    }
}

void PrintFieldEditorTests::falseAndUnsetBooleanRemainDistinct()
{
    app::PrintFieldEditorPanel panel;
    panel.setDocument(documentWithFields());
    auto* checkbox = panel.findChild<QCheckBox*>(QStringLiteral("printField_enabled"));
    QVERIFY(checkbox != nullptr);
    QVERIFY(!panel.values().contains(QStringLiteral("enabled")));
    panel.setValues({{QStringLiteral("enabled"), false}});
    QCOMPARE(checkbox->checkState(), Qt::Unchecked);
    QCOMPARE(panel.values().value(QStringLiteral("enabled")), QJsonValue(false));
    checkbox->setCheckState(Qt::Checked);
    QCOMPARE(panel.values().value(QStringLiteral("enabled")), QJsonValue(true));
    checkbox->setCheckState(Qt::Unchecked);
    QCOMPARE(panel.values().value(QStringLiteral("enabled")), QJsonValue(false));
    checkbox->setCheckState(Qt::PartiallyChecked);
    QVERIFY(!panel.values().contains(QStringLiteral("enabled")));
}

void PrintFieldEditorTests::structuredFieldsRoundTripAndReportErrors()
{
    app::PrintFieldEditorPanel panel;
    panel.setDocument(documentWithFields());
    panel.setValues(valuesWithUnknownFields());
    auto* list = panel.findChild<QPlainTextEdit*>(QStringLiteral("printField_items"));
    auto* object = panel.findChild<QPlainTextEdit*>(QStringLiteral("printField_details"));
    auto* tabs = panel.findChild<QTabWidget*>(QStringLiteral("printFieldModeTabs"));
    QVERIFY(list != nullptr && object != nullptr && tabs != nullptr);
    list->setPlainText(QStringLiteral("[0,false,{\"code\":\"A\"}]"));
    object->setPlainText(QStringLiteral("{\"nested\":[1,2]}"));
    bool ok = false;
    auto result = panel.values(&ok);
    QVERIFY(ok);
    QCOMPARE(result.value(QStringLiteral("items")).toArray(), (QJsonArray{0, false, QJsonObject{{QStringLiteral("code"), QStringLiteral("A")}}}));
    QCOMPARE(result.value(QStringLiteral("details")).toObject().value(QStringLiteral("nested")).toArray(), (QJsonArray{1, 2}));
    QCOMPARE(result.value(QStringLiteral("header_items")), valuesWithUnknownFields().value(QStringLiteral("header_items")));
    const auto invalid = QStringLiteral("[\n  1,\n  @\n]");
    list->setPlainText(invalid);
    QString error;
    panel.values(&ok, &error);
    QVERIFY(!ok);
    QVERIFY(error.contains(QStringLiteral("items")));
    QVERIFY(error.contains(QString::fromUtf8("第 3 行")) && error.contains(QString::fromUtf8("列")));
    tabs->setCurrentIndex(1);
    QCOMPARE(tabs->currentIndex(), 0);
    QCOMPARE(list->toPlainText(), invalid);
    list->setPlainText(QStringLiteral("{}"));
    panel.values(&ok, &error);
    QVERIFY(!ok);
    list->setPlainText(QStringLiteral("[]"));
    object->setPlainText(QStringLiteral("[]"));
    panel.values(&ok, &error);
    QVERIFY(!ok);
    object->setPlainText(QStringLiteral("{}"));
    tabs->setCurrentIndex(1);
    tabs->setCurrentIndex(0);
    panel.values(&ok, &error);
    QVERIFY(ok);
}

void PrintFieldEditorTests::dateTextIsPreservedWithoutInferringValues()
{
    app::PrintFieldEditorPanel panel;
    auto document = documentWithFields();
    document.fieldSchema[0].required = true;
    document.fieldSchema[5].format = QStringLiteral("yyyy-MM-dd");
    panel.setDocument(document);
    auto* date = panel.findChild<QLineEdit*>(QStringLiteral("printField_date"));
    auto* time = panel.findChild<QLineEdit*>(QStringLiteral("printField_time"));
    QVERIFY(date != nullptr && time != nullptr);
    date->setText(QStringLiteral("2026-10-17"));
    time->setText(QStringLiteral("2026-10-17T10:30:00+08:00"));
    bool ok = false;
    auto result = panel.values(&ok);
    QVERIFY(ok);
    QCOMPARE(result.value(QStringLiteral("date")).toString(), QStringLiteral("2026-10-17"));
    QCOMPARE(result.value(QStringLiteral("time")).toString(), QStringLiteral("2026-10-17T10:30:00+08:00"));
    QVERIFY(!result.contains(QStringLiteral("name")));
    date->setText(QStringLiteral("let-core-report-the-format"));
    result = panel.values(&ok);
    QVERIFY(ok);
    QCOMPARE(result.value(QStringLiteral("date")).toString(), QStringLiteral("let-core-report-the-format"));
}

void PrintFieldEditorTests::setValuesAndModeSwitchDoNotEmitEdits()
{
    app::PrintFieldEditorPanel panel;
    QSignalSpy edits(&panel, &app::PrintFieldEditorPanel::valuesEdited);
    panel.setDocument(documentWithFields());
    panel.setValues(valuesWithUnknownFields());
    QCOMPARE(edits.count(), 0);
    auto* tabs = panel.findChild<QTabWidget*>(QStringLiteral("printFieldModeTabs"));
    auto* json = panel.findChild<QPlainTextEdit*>(QStringLiteral("printValuesJsonEdit"));
    auto* name = panel.findChild<QLineEdit*>(QStringLiteral("printField_name"));
    QVERIFY(tabs != nullptr && json != nullptr && name != nullptr);
    tabs->setCurrentIndex(1);
    json->setPlainText(QStringLiteral(" \n") + json->toPlainText());
    tabs->setCurrentIndex(0);
    QCOMPARE(edits.count(), 0);
    name->setText(QStringLiteral("changed"));
    QCOMPARE(edits.count(), 1);
    panel.setValues({{QStringLiteral("name"), QStringLiteral("replacement")}, {QStringLiteral("enabled"), false}});
    QCOMPARE(edits.count(), 1);
    tabs->setCurrentIndex(1);
    json->setPlainText(QStringLiteral("{"));
    QCOMPARE(edits.count(), 2);
}

void PrintFieldEditorTests::narrowPanelDoesNotForceHorizontalScrolling()
{
    app::PrintFieldEditorPanel panel;
    auto document = documentWithFields();
    document.fieldSchema[0].displayName = QString(160, QLatin1Char('X'));
    panel.setDocument(document);
    panel.resize(300, 440);
    panel.show();
    QTest::qWait(50);
    QCOMPARE(panel.width(), 300);
    auto* form = panel.findChild<QFormLayout*>();
    QVERIFY(form != nullptr);
    auto* scroll = panel.findChild<QScrollArea*>();
    QVERIFY(scroll != nullptr);
    QCOMPARE(scroll->horizontalScrollBar()->maximum(), 0);
    for (const auto& definition : document.fieldSchema) {
        auto* input = panel.findChild<QWidget*>(QStringLiteral("printField_") + definition.key);
        QVERIFY(input != nullptr);
        QVERIFY(input->width() <= 300);
    }
}

void PrintFieldEditorTests::jsonModeRejectsWrongSchemaTypesWithoutCoercion()
{
    app::PrintFieldEditorPanel panel;
    panel.setDocument(documentWithFields());
    auto* tabs = panel.findChild<QTabWidget*>(QStringLiteral("printFieldModeTabs"));
    auto* json = panel.findChild<QPlainTextEdit*>(QStringLiteral("printValuesJsonEdit"));
    QVERIFY(tabs != nullptr && json != nullptr);
    tabs->setCurrentIndex(1);
    for (const auto& invalid : {QStringLiteral("{\"quantity\":\"oops\"}"), QStringLiteral("{\"enabled\":\"yes\"}"),
             QStringLiteral("{\"items\":{}}"), QStringLiteral("{\"details\":[]}")}) {
        json->setPlainText(invalid);
        bool ok = true;
        QString error;
        panel.values(&ok, &error);
        QVERIFY(!ok);
        QVERIFY(!error.isEmpty());
    }
    json->setPlainText(QStringLiteral("{\"quantity\":\"12.50\",\"enabled\":false,\"items\":[],\"details\":{}}"));
    bool ok = false;
    const auto value = panel.values(&ok);
    QVERIFY(ok);
    QCOMPARE(value[QStringLiteral("quantity")], QJsonValue(QStringLiteral("12.50")));
}

void PrintFieldEditorTests::formLabelsRemainVisible()
{
    app::PrintFieldEditorPanel panel;
    panel.setDocument(documentWithFields());
    panel.resize(300, 500);
    panel.show();
    QTest::qWait(30);
    auto* form = panel.findChild<QFormLayout*>();
    auto* input = panel.findChild<QWidget*>(QStringLiteral("printField_name"));
    QVERIFY(form != nullptr && input != nullptr);
    auto* title = qobject_cast<QLabel*>(form->labelForField(input->parentWidget()));
    QVERIFY(title != nullptr);
    QVERIFY(title->width() > 20);
    QVERIFY(title->height() > 0);
    QVERIFY(title->isVisible());
}

void PrintFieldEditorTests::referencesWithoutSchemaCreateEmptyForm()
{
    app::PrintFieldEditorPanel panel;
    panel.setDocument(documentWithReferences());
    auto* tabs = panel.findChild<QTabWidget*>(QStringLiteral("printFieldModeTabs"));
    QVERIFY(tabs != nullptr);
    QCOMPARE(tabs->count(), 2);
    QCOMPARE(tabs->currentIndex(), 0);
    for (const auto* key : {"productName", "customer", "quantity", "qrPayload", "token"}) {
        auto* input = panel.findChild<QLineEdit*>(QStringLiteral("printField_") + QLatin1String(key));
        QVERIFY2(input != nullptr, key);
        QVERIFY(input->text().isEmpty());
    }
    auto* items = panel.findChild<QPlainTextEdit*>(QStringLiteral("printField_order.items"));
    auto* details = panel.findChild<QPlainTextEdit*>(QStringLiteral("printField_details"));
    auto* enabled = panel.findChild<QCheckBox*>(QStringLiteral("printField_enabled"));
    QVERIFY(items != nullptr && details != nullptr && enabled != nullptr);
    QVERIFY(items->toPlainText().isEmpty() && details->toPlainText().isEmpty());
    QCOMPARE(enabled->checkState(), Qt::PartiallyChecked);
    for (const auto* key : {"rowName", "rowValue", "sampleOnly"}) {
        QVERIFY(panel.findChild<QWidget*>(QStringLiteral("printField_") + QLatin1String(key)) == nullptr);
    }
    for (auto* empty : panel.findChildren<QCheckBox*>()) {
        if (empty->objectName().endsWith(QStringLiteral("_allowEmpty"))) {
            QVERIFY(!empty->isChecked());
        }
    }
    bool ok = false;
    QVERIFY(panel.values(&ok).isEmpty());
    QVERIFY(ok);
    tabs->setCurrentIndex(1);
    tabs->setCurrentIndex(0);
    QVERIFY(panel.values(&ok).isEmpty());
    QVERIFY(ok);
}

void PrintFieldEditorTests::inferredTextRequiresExplicitEmptyChoice()
{
    app::PrintFieldEditorPanel panel;
    panel.setDocument(documentWithReferences());
    auto* input = panel.findChild<QLineEdit*>(QStringLiteral("printField_productName"));
    auto* empty = panel.findChild<QCheckBox*>(QStringLiteral("printField_productName_allowEmpty"));
    QVERIFY(input != nullptr && empty != nullptr);
    QSignalSpy edits(&panel, &app::PrintFieldEditorPanel::valuesEdited);
    QVERIFY(!empty->isChecked());
    QVERIFY(!panel.values().contains(QStringLiteral("productName")));
    input->setText(QStringLiteral("draft"));
    input->clear();
    QVERIFY(!panel.values().contains(QStringLiteral("productName")));
    empty->setChecked(true);
    QVERIFY(!input->isEnabled());
    QCOMPARE(panel.values().value(QStringLiteral("productName")), QJsonValue(QString()));
    empty->setChecked(false);
    QVERIFY(input->isEnabled());
    QVERIFY(!panel.values().contains(QStringLiteral("productName")));
    QCOMPARE(edits.count(), 4);
}

void PrintFieldEditorTests::inferredEmptyChoicesPreserveDraftsAcrossModes()
{
    app::PrintFieldEditorPanel panel;
    const auto document = documentWithReferences();
    panel.setDocument(document);
    auto expected = valuesWithUnknownFields();
    expected.insert(QStringLiteral("productName"), QStringLiteral("preset"));
    panel.setValues(expected);
    auto* input = panel.findChild<QLineEdit*>(QStringLiteral("printField_productName"));
    auto* empty = panel.findChild<QCheckBox*>(QStringLiteral("printField_productName_allowEmpty"));
    auto* tabs = panel.findChild<QTabWidget*>(QStringLiteral("printFieldModeTabs"));
    QVERIFY(input != nullptr && empty != nullptr && tabs != nullptr);
    input->setText(QStringLiteral("draft"));
    empty->setChecked(true);
    expected.insert(QStringLiteral("productName"), QString());
    QCOMPARE(panel.values(), expected);
    QSignalSpy edits(&panel, &app::PrintFieldEditorPanel::valuesEdited);
    tabs->setCurrentIndex(1);
    tabs->setCurrentIndex(0);
    QCOMPARE(edits.count(), 0);
    QVERIFY(empty->isChecked() && !input->isEnabled());
    empty->setChecked(false);
    QCOMPARE(input->text(), QStringLiteral("draft"));
    expected.insert(QStringLiteral("productName"), QStringLiteral("draft"));
    QCOMPARE(panel.values(), expected);
    panel.setDocument(document);
    QCOMPARE(panel.values(), expected);
}

void PrintFieldEditorTests::inferredEmptyMarkers_data()
{
    QTest::addColumn<QString>("key");
    QTest::addColumn<QJsonValue>("marker");
    QTest::newRow("text") << QStringLiteral("productName") << QJsonValue(QString());
    QTest::newRow("number") << QStringLiteral("quantity") << QJsonValue(QString());
    QTest::newRow("boolean") << QStringLiteral("enabled") << QJsonValue(QString());
    QTest::newRow("list") << QStringLiteral("order.items") << QJsonValue(QJsonArray{});
    QTest::newRow("object") << QStringLiteral("details") << QJsonValue(QJsonObject{});
}

void PrintFieldEditorTests::inferredEmptyMarkers()
{
    QFETCH(QString, key);
    QFETCH(QJsonValue, marker);
    app::PrintFieldEditorPanel panel;
    panel.setDocument(documentWithReferences());
    auto* empty = panel.findChild<QCheckBox*>(QStringLiteral("printField_") + key + QStringLiteral("_allowEmpty"));
    auto* input = panel.findChild<QWidget*>(QStringLiteral("printField_") + key);
    auto* tabs = panel.findChild<QTabWidget*>(QStringLiteral("printFieldModeTabs"));
    QVERIFY(empty != nullptr && input != nullptr && tabs != nullptr);
    empty->setChecked(true);
    bool ok = false;
    QCOMPARE(panel.values(&ok).value(key), marker);
    QVERIFY(ok && !input->isEnabled());
    empty->setChecked(false);
    QVERIFY(!panel.values(&ok).contains(key));
    QVERIFY(ok);
    QSignalSpy edits(&panel, &app::PrintFieldEditorPanel::valuesEdited);
    panel.setValues({{key, marker}, {QStringLiteral("unknown"), QJsonArray{0, false}}});
    QCOMPARE(edits.count(), 0);
    QVERIFY(empty->isChecked() && !input->isEnabled());
    QCOMPARE(panel.values(&ok).value(key), marker);
    QVERIFY(ok);
    tabs->setCurrentIndex(1);
    tabs->setCurrentIndex(0);
    QCOMPARE(edits.count(), 0);
    QVERIFY(empty->isChecked());
    empty->setChecked(false);
    const auto result = panel.values(&ok);
    QVERIFY(ok && !result.contains(key));
    QCOMPARE(result.value(QStringLiteral("unknown")), QJsonValue(QJsonArray{0, false}));
}

void PrintFieldEditorTests::inferredPresetZeroAndFalseAreNotEmpty()
{
    app::PrintFieldEditorPanel panel;
    panel.setDocument(documentWithReferences());
    const QJsonObject preset{{QStringLiteral("quantity"), 0}, {QStringLiteral("enabled"), false}};
    QSignalSpy edits(&panel, &app::PrintFieldEditorPanel::valuesEdited);
    panel.setValues(preset);
    QCOMPARE(edits.count(), 0);
    for (const auto* key : {"quantity", "enabled"}) {
        const auto name = QStringLiteral("printField_") + QLatin1String(key);
        auto* empty = panel.findChild<QCheckBox*>(name + QStringLiteral("_allowEmpty"));
        auto* input = panel.findChild<QWidget*>(name);
        QVERIFY(empty != nullptr && input != nullptr);
        QVERIFY(!empty->isChecked() && input->isEnabled());
        empty->setChecked(true);
        QCOMPARE(panel.values().value(QLatin1String(key)), QJsonValue(QString()));
        empty->setChecked(false);
        QCOMPARE(panel.values(), preset);
    }
    QCOMPARE(panel.findChild<QLineEdit*>(QStringLiteral("printField_quantity"))->text(), QStringLiteral("0"));
    QCOMPARE(panel.findChild<QCheckBox*>(QStringLiteral("printField_enabled"))->checkState(), Qt::Unchecked);
}

void PrintFieldEditorTests::explicitSchemaDoesNotOfferEmptyBypass()
{
    app::PrintFieldEditorPanel panel;
    auto document = documentWithReferences();
    auto required = field(QStringLiteral("productName"), core::FieldValueType::Text);
    required.required = true;
    required.defaultValue = QStringLiteral("never-use-default");
    document.fieldSchema = {required, field(QStringLiteral("quantity"), core::FieldValueType::Number)};
    panel.setDocument(document);
    QVERIFY(panel.findChild<QLineEdit*>(QStringLiteral("printField_customer")) != nullptr);
    QVERIFY(panel.findChild<QCheckBox*>(QStringLiteral("printField_productName_allowEmpty")) == nullptr);
    QVERIFY(panel.findChild<QCheckBox*>(QStringLiteral("printField_quantity_allowEmpty")) == nullptr);
    QCOMPARE(panel.findChildren<QLineEdit*>(QStringLiteral("printField_productName")).size(), 1);
    QVERIFY(panel.values().isEmpty());
}

void PrintFieldEditorTests::nestedArrayValuesAreDisplayOnlyUntilEdited()
{
    app::PrintFieldEditorPanel panel;
    panel.setDocument(documentWithReferences());
    QJsonObject expected{
        {QStringLiteral("order"), QJsonObject{{QStringLiteral("items"), QJsonArray{QJsonObject{{QStringLiteral("rowName"), "A"}}}},
            {QStringLiteral("keep"), false}}},
        {QStringLiteral("unknown"), QJsonArray{0, false}},
    };
    panel.setValues(expected);
    auto* items = panel.findChild<QPlainTextEdit*>(QStringLiteral("printField_order.items"));
    auto* tabs = panel.findChild<QTabWidget*>(QStringLiteral("printFieldModeTabs"));
    auto* product = panel.findChild<QLineEdit*>(QStringLiteral("printField_productName"));
    QVERIFY(items != nullptr && tabs != nullptr && product != nullptr);
    QCOMPARE(QJsonDocument::fromJson(items->toPlainText().toUtf8()).array(),
        (QJsonArray{QJsonObject{{QStringLiteral("rowName"), "A"}}}));
    QCOMPARE(panel.values(), expected);
    tabs->setCurrentIndex(1);
    tabs->setCurrentIndex(0);
    panel.setDocument(documentWithReferences());
    QCOMPARE(panel.values(), expected);
    QVERIFY(!panel.values().contains(QStringLiteral("order.items")));
    items = panel.findChild<QPlainTextEdit*>(QStringLiteral("printField_order.items"));
    product = panel.findChild<QLineEdit*>(QStringLiteral("printField_productName"));
    product->setText(QStringLiteral("edited"));
    expected.insert(QStringLiteral("productName"), QStringLiteral("edited"));
    QCOMPARE(panel.values(), expected);
    items->setPlainText(QStringLiteral("[0,false]"));
    expected.insert(QStringLiteral("order.items"), QJsonArray{0, false});
    QCOMPARE(panel.values(), expected);
    tabs->setCurrentIndex(1);
    tabs->setCurrentIndex(0);
    QCOMPARE(panel.values(), expected);
}

void PrintFieldEditorTests::nestedArrayLiteralKeyTakesPriority()
{
    app::PrintFieldEditorPanel panel;
    panel.setDocument(documentWithReferences());
    const QJsonObject preset{
        {QStringLiteral("order"), QJsonObject{{QStringLiteral("items"), QJsonArray{1}}}},
        {QStringLiteral("order.items"), QJsonArray{2}},
    };
    panel.setValues(preset);
    auto* items = panel.findChild<QPlainTextEdit*>(QStringLiteral("printField_order.items"));
    QVERIFY(items != nullptr);
    QCOMPARE(QJsonDocument::fromJson(items->toPlainText().toUtf8()).array(), QJsonArray{2});
    QCOMPARE(panel.values(), preset);
}

void PrintFieldEditorTests::inferredTypesRejectInvalidJson()
{
    app::PrintFieldEditorPanel panel;
    panel.setDocument(documentWithReferences());
    auto* tabs = panel.findChild<QTabWidget*>(QStringLiteral("printFieldModeTabs"));
    auto* json = panel.findChild<QPlainTextEdit*>(QStringLiteral("printValuesJsonEdit"));
    QVERIFY(tabs != nullptr && json != nullptr);
    tabs->setCurrentIndex(1);
    for (const auto* invalid : {"{\"quantity\":\"oops\"}", "{\"enabled\":0}", "{\"details\":[]}",
             "{\"order.items\":{}}", "{\"order\":{\"items\":{}}}", "{\"details\":\"\"}", "{\"order.items\":\"\"}"}) {
        json->setPlainText(QLatin1String(invalid));
        bool ok = true;
        QString error;
        panel.values(&ok, &error);
        QVERIFY2(!ok && !error.isEmpty(), invalid);
    }
    json->setPlainText(QStringLiteral("{\"quantity\":\"\",\"enabled\":\"\",\"details\":{},\"order.items\":[]}"));
    bool ok = false;
    panel.values(&ok);
    QVERIFY(ok);
    tabs->setCurrentIndex(0);
    QCOMPARE(tabs->currentIndex(), 0);
    for (const auto* key : {"quantity", "enabled", "details", "order.items"}) {
        auto* empty = panel.findChild<QCheckBox*>(QStringLiteral("printField_") + QLatin1String(key) + QStringLiteral("_allowEmpty"));
        QVERIFY(empty != nullptr && empty->isChecked());
    }
}

void PrintFieldEditorTests::inferredLabelsAndEmptyControlsFitNarrowPanel()
{
    app::PrintFieldEditorPanel panel;
    auto document = documentWithReferences();
    const auto longKey = QString(120, QLatin1Char('X'));
    document.layers[0].elements[0].fieldKey = longKey;
    auto required = field(QStringLiteral("required"), core::FieldValueType::Text);
    required.displayName = QString(100, QChar(0x540d));
    required.required = true;
    document.fieldSchema = {required};
    panel.setDocument(document);
    panel.resize(300, 500);
    panel.show();
    QTest::qWait(30);
    QCOMPARE(panel.width(), 300);
    auto* scroll = panel.findChild<QScrollArea*>();
    auto* form = panel.findChild<QFormLayout*>();
    QVERIFY(scroll != nullptr && form != nullptr);
    QCOMPARE(scroll->horizontalScrollBar()->maximum(), 0);
    for (const auto& key : {longKey, QStringLiteral("required")}) {
        auto* input = panel.findChild<QWidget*>(QStringLiteral("printField_") + key);
        QVERIFY(input != nullptr);
        auto* label = qobject_cast<QLabel*>(form->labelForField(input->parentWidget()));
        QVERIFY(label != nullptr && label->isVisible());
        QVERIFY(label->width() > 20 && label->height() > 0);
        QVERIFY(label->wordWrap());
        QVERIFY(label->text().contains(key == longKey ? longKey : required.displayName));
        QVERIFY(label->geometry().right() < scroll->widget()->width());
        if (key == longKey) {
            auto* empty = panel.findChild<QCheckBox*>(QStringLiteral("printField_") + key + QStringLiteral("_allowEmpty"));
            QVERIFY(empty != nullptr && empty->isVisible());
            QVERIFY(empty->mapTo(scroll->widget(), QPoint(empty->width(), 0)).x() <= scroll->widget()->width());
        } else {
            QVERIFY(label->text().contains(QLatin1Char('*')));
        }
    }
}

void PrintFieldEditorTests::focusFieldSwitchesModeAndScrollsWithoutEditing()
{
    app::PrintFieldEditorPanel panel;
    core::TemplateDocument document;
    for (int index = 0; index < 32; ++index) {
        document.fieldSchema.append(field(QStringLiteral("row%1").arg(index, 2, 10, QLatin1Char('0')),
            core::FieldValueType::Text));
    }
    panel.setDocument(document);
    auto expected = valuesWithUnknownFields();
    expected.insert(QStringLiteral("row31"), QStringLiteral("json-draft"));
    panel.setValues(expected);
    auto* tabs = panel.findChild<QTabWidget*>(QStringLiteral("printFieldModeTabs"));
    auto* scroll = panel.findChild<QScrollArea*>(QStringLiteral("printFieldsScrollArea"));
    auto* input = panel.findChild<QLineEdit*>(QStringLiteral("printField_row31"));
    QVERIFY(tabs != nullptr && scroll != nullptr && input != nullptr);
    tabs->setCurrentIndex(1);
    panel.resize(330, 260);
    panel.show();
    panel.activateWindow();
    QVERIFY(QTest::qWaitForWindowExposed(&panel));
    QSignalSpy edits(&panel, &app::PrintFieldEditorPanel::valuesEdited);

    QVERIFY(panel.focusField(QStringLiteral("row31")));

    QCOMPARE(tabs->currentWidget(), scroll);
    QTRY_VERIFY(input->hasFocus());
    QTRY_VERIFY(scroll->verticalScrollBar()->value() > 0);
    QVERIFY(scroll->viewport()->rect().contains(input->mapTo(scroll->viewport(), input->rect().center())));
    QCOMPARE(input->text(), QStringLiteral("json-draft"));
    QCOMPARE(panel.values(), expected);
    QCOMPARE(edits.count(), 0);
}

void PrintFieldEditorTests::focusFieldUsesEmptyCheckboxWithoutChangingDraft()
{
    app::PrintFieldEditorPanel panel;
    panel.setDocument(documentWithReferences());
    panel.setValues({{QStringLiteral("productName"), QStringLiteral("retained-draft")},
        {QStringLiteral("unknown"), QJsonArray{0, false}}});
    auto* input = panel.findChild<QLineEdit*>(QStringLiteral("printField_productName"));
    auto* empty = panel.findChild<QCheckBox*>(QStringLiteral("printField_productName_allowEmpty"));
    auto* tabs = panel.findChild<QTabWidget*>(QStringLiteral("printFieldModeTabs"));
    QVERIFY(input != nullptr && empty != nullptr && tabs != nullptr);
    empty->setChecked(true);
    const auto expected = panel.values();
    tabs->setCurrentIndex(1);
    panel.resize(330, 300);
    panel.show();
    panel.activateWindow();
    QVERIFY(QTest::qWaitForWindowExposed(&panel));
    QSignalSpy edits(&panel, &app::PrintFieldEditorPanel::valuesEdited);

    QVERIFY(panel.focusField(QStringLiteral("productName")));

    QTRY_VERIFY(empty->hasFocus());
    QVERIFY(empty->isChecked());
    QVERIFY(!input->isEnabled());
    QCOMPARE(input->text(), QStringLiteral("retained-draft"));
    QCOMPARE(panel.values(), expected);
    QCOMPARE(edits.count(), 0);
}

void PrintFieldEditorTests::focusUnknownFieldDoesNotEditValues()
{
    app::PrintFieldEditorPanel panel;
    panel.setDocument(documentWithFields());
    const auto expected = valuesWithUnknownFields();
    panel.setValues(expected);
    QSignalSpy edits(&panel, &app::PrintFieldEditorPanel::valuesEdited);
    QVERIFY(!panel.focusField(QStringLiteral("not-in-template")));
    QCOMPARE(panel.values(), expected);
    QCOMPARE(edits.count(), 0);
}

void PrintFieldEditorTests::focusFieldPreservesInvalidJsonAndLocatesParseError()
{
    app::PrintFieldEditorPanel panel;
    panel.setDocument(documentWithFields());
    panel.setValues(valuesWithUnknownFields());
    auto* tabs = panel.findChild<QTabWidget*>(QStringLiteral("printFieldModeTabs"));
    auto* json = panel.findChild<QPlainTextEdit*>(QStringLiteral("printValuesJsonEdit"));
    QVERIFY(tabs != nullptr && json != nullptr);
    tabs->setCurrentIndex(1);
    const auto invalid = QString::fromUtf8("{\n  \"name\": \"金😀\", \"bad\": @\n}");
    json->setPlainText(invalid);
    QJsonParseError error;
    QJsonDocument::fromJson(invalid.toUtf8(), &error);
    QVERIFY(error.error != QJsonParseError::NoError);
    // 解析器给出 UTF-8 字节偏移，光标使用 UTF-16 位置，不能从中文报错反解坐标。
    const auto expectedPosition = QString::fromUtf8(invalid.toUtf8().left(error.offset)).size();
    json->moveCursor(QTextCursor::Start);
    panel.resize(400, 300);
    panel.show();
    panel.activateWindow();
    QVERIFY(QTest::qWaitForWindowExposed(&panel));
    QSignalSpy edits(&panel, &app::PrintFieldEditorPanel::valuesEdited);

    QVERIFY(!panel.focusField(QStringLiteral("name")));

    QCOMPARE(tabs->currentIndex(), 1);
    QTRY_VERIFY(json->hasFocus());
    QCOMPARE(json->toPlainText(), invalid);
    QCOMPARE(json->textCursor().position(), expectedPosition);
    bool ok = true;
    panel.values(&ok);
    QVERIFY(!ok);
    QCOMPARE(edits.count(), 0);
}

void PrintFieldEditorTests::focusJsonErrorUsesUtf8ParseOffset_data()
{
    QTest::addColumn<QString>("invalid");
    QTest::newRow("chinese-and-surrogate") << QString::fromUtf8("{\"name\":\"金😀\",\"bad\":@}");
    QTest::newRow("multiline") << QString::fromUtf8("{\n  \"name\": \"金\",\n  \"items\": [\n    @\n  ]\n}");
}

void PrintFieldEditorTests::focusJsonErrorUsesUtf8ParseOffset()
{
    QFETCH(QString, invalid);
    app::PrintFieldEditorPanel panel;
    auto* json = panel.findChild<QPlainTextEdit*>(QStringLiteral("printValuesJsonEdit"));
    QVERIFY(json != nullptr);
    json->setPlainText(invalid);
    QJsonParseError error;
    QJsonDocument::fromJson(invalid.toUtf8(), &error);
    QVERIFY(error.error != QJsonParseError::NoError);
    const auto expectedPosition = QString::fromUtf8(invalid.toUtf8().left(error.offset)).size();
    json->moveCursor(QTextCursor::Start);
    panel.resize(400, 300);
    panel.show();
    panel.activateWindow();
    QVERIFY(QTest::qWaitForWindowExposed(&panel));
    QSignalSpy edits(&panel, &app::PrintFieldEditorPanel::valuesEdited);

    panel.focusJsonError();

    QTRY_VERIFY(json->hasFocus());
    QCOMPARE(json->textCursor().position(), expectedPosition);
    QCOMPARE(json->toPlainText(), invalid);
    QCOMPARE(edits.count(), 0);
}

void PrintFieldEditorTests::focusJsonErrorUsesOneBasedCoordinates()
{
    app::PrintFieldEditorPanel panel;
    auto* json = panel.findChild<QPlainTextEdit*>(QStringLiteral("printValuesJsonEdit"));
    QVERIFY(json != nullptr);
    const auto invalid = QStringLiteral("{\n  \"name\": \"draft\",\n    \"bad\": @\n}");
    json->setPlainText(invalid);
    QSignalSpy edits(&panel, &app::PrintFieldEditorPanel::valuesEdited);

    panel.focusJsonError(3, 5);

    QCOMPARE(json->textCursor().blockNumber(), 2);
    QCOMPARE(json->textCursor().positionInBlock(), 4);
    QCOMPARE(json->toPlainText(), invalid);
    QCOMPARE(edits.count(), 0);
}

void PrintFieldEditorTests::focusJsonErrorSwitchesValidFormWithoutEditing()
{
    app::PrintFieldEditorPanel panel;
    panel.setDocument(documentWithFields());
    const auto expected = valuesWithUnknownFields();
    panel.setValues(expected);
    auto* tabs = panel.findChild<QTabWidget*>(QStringLiteral("printFieldModeTabs"));
    auto* json = panel.findChild<QPlainTextEdit*>(QStringLiteral("printValuesJsonEdit"));
    QVERIFY(tabs != nullptr && json != nullptr);
    panel.resize(400, 300);
    panel.show();
    panel.activateWindow();
    QVERIFY(QTest::qWaitForWindowExposed(&panel));
    QSignalSpy edits(&panel, &app::PrintFieldEditorPanel::valuesEdited);

    panel.focusJsonError(2, 3);

    QCOMPARE(tabs->currentIndex(), 1);
    QTRY_VERIFY(json->hasFocus());
    QCOMPARE(json->textCursor().blockNumber(), 1);
    QCOMPARE(json->textCursor().positionInBlock(), 2);
    QCOMPARE(panel.values(), expected);
    QCOMPARE(edits.count(), 0);
}

void PrintFieldEditorTests::focusJsonErrorPreservesInvalidForm_data()
{
    QTest::addColumn<QString>("key");
    QTest::addColumn<QString>("invalid");
    QTest::newRow("number-intermediate") << QStringLiteral("quantity") << QStringLiteral("-");
    QTest::newRow("object-json") << QStringLiteral("details") << QStringLiteral("{\n  \"bad\": @\n}");
}

void PrintFieldEditorTests::focusJsonErrorPreservesInvalidForm()
{
    QFETCH(QString, key);
    QFETCH(QString, invalid);
    app::PrintFieldEditorPanel panel;
    panel.setDocument(documentWithFields());
    panel.setValues(valuesWithUnknownFields());
    auto* tabs = panel.findChild<QTabWidget*>(QStringLiteral("printFieldModeTabs"));
    auto* json = panel.findChild<QPlainTextEdit*>(QStringLiteral("printValuesJsonEdit"));
    auto* input = panel.findChild<QWidget*>(QStringLiteral("printField_") + key);
    auto* scroll = panel.findChild<QScrollArea*>(QStringLiteral("printFieldsScrollArea"));
    QVERIFY(tabs != nullptr && json != nullptr && input != nullptr && scroll != nullptr);
    const auto previousJson = json->toPlainText();
    if (auto* line = qobject_cast<QLineEdit*>(input)) line->setText(invalid);
    else qobject_cast<QPlainTextEdit*>(input)->setPlainText(invalid);
    panel.resize(330, 260);
    panel.show();
    panel.activateWindow();
    QVERIFY(QTest::qWaitForWindowExposed(&panel));
    tabs->tabBar()->setFocus();
    QSignalSpy edits(&panel, &app::PrintFieldEditorPanel::valuesEdited);

    panel.focusJsonError(1, 1);

    QCOMPARE(tabs->currentIndex(), 0);
    QTRY_VERIFY(input->hasFocus());
    QVERIFY(scroll->viewport()->rect().contains(input->mapTo(scroll->viewport(), input->rect().center())));
    // 错误提示和模式回退的排版完成后也必须保持输入框中心可见，不能只保证文本光标露出。
    QCoreApplication::sendPostedEvents(nullptr, QEvent::LayoutRequest);
    QCoreApplication::processEvents();
    QVERIFY(input->hasFocus());
    QVERIFY(scroll->viewport()->rect().contains(input->mapTo(scroll->viewport(), input->rect().center())));
    if (const auto* line = qobject_cast<QLineEdit*>(input)) QCOMPARE(line->text(), invalid);
    else QCOMPARE(qobject_cast<QPlainTextEdit*>(input)->toPlainText(), invalid);
    QCOMPARE(json->toPlainText(), previousJson);
    bool ok = true;
    panel.values(&ok);
    QVERIFY(!ok);
    QCOMPARE(panel.invalidFieldKey(), key);
    QCOMPARE(edits.count(), 0);
}

void PrintFieldEditorTests::invalidFieldKeyRecordsFirstFormFailureAndClears()
{
    app::PrintFieldEditorPanel panel;
    panel.setDocument(documentWithFields());
    auto* quantity = panel.findChild<QLineEdit*>(QStringLiteral("printField_quantity"));
    auto* details = panel.findChild<QPlainTextEdit*>(QStringLiteral("printField_details"));
    QVERIFY(quantity != nullptr && details != nullptr);
    quantity->setText(QStringLiteral("-"));
    details->setPlainText(QStringLiteral("["));
    bool ok = true;
    panel.values(&ok);
    QVERIFY(!ok);
    QCOMPARE(panel.invalidFieldKey(), QStringLiteral("quantity"));
    quantity->setText(QStringLiteral("0"));
    panel.values(&ok);
    QVERIFY(!ok);
    QCOMPARE(panel.invalidFieldKey(), QStringLiteral("details"));
    details->setPlainText(QStringLiteral("{}"));
    panel.values(&ok);
    QVERIFY(ok);
    QVERIFY(panel.invalidFieldKey().isEmpty());
}

void PrintFieldEditorTests::invalidFieldKeyRecordsJsonTypeFailureAndClears()
{
    app::PrintFieldEditorPanel panel;
    panel.setDocument(documentWithFields());
    auto* tabs = panel.findChild<QTabWidget*>(QStringLiteral("printFieldModeTabs"));
    auto* json = panel.findChild<QPlainTextEdit*>(QStringLiteral("printValuesJsonEdit"));
    QVERIFY(tabs != nullptr && json != nullptr);
    tabs->setCurrentIndex(1);
    json->setPlainText(QStringLiteral("{\"quantity\":\"oops\",\"enabled\":0}"));
    bool ok = true;
    panel.values(&ok);
    QVERIFY(!ok);
    QCOMPARE(panel.invalidFieldKey(), QStringLiteral("quantity"));
    json->setPlainText(QStringLiteral("{\"quantity\":0,\"enabled\":0}"));
    panel.values(&ok);
    QVERIFY(!ok);
    QCOMPARE(panel.invalidFieldKey(), QStringLiteral("enabled"));
    json->setPlainText(QStringLiteral("{\"bad\":@}"));
    panel.values(&ok);
    QVERIFY(!ok);
    QVERIFY(panel.invalidFieldKey().isEmpty());
    json->setPlainText(QStringLiteral("{\"quantity\":0,\"enabled\":false}"));
    panel.values(&ok);
    QVERIFY(ok);
    QVERIFY(panel.invalidFieldKey().isEmpty());
    json->setPlainText(QStringLiteral("{\"text\":\"") + QString(1024 * 1024, QLatin1Char('x')) + QStringLiteral("\"}"));
    panel.values(&ok);
    QVERIFY(!ok);
    QVERIFY(panel.invalidFieldKey().isEmpty());
}

void PrintFieldEditorTests::fieldTitlesUseDisplayNameWithoutRenamingValues()
{
    app::PrintFieldEditorPanel panel;
    auto document = documentWithReferences();
    panel.setDocument(document);
    const QJsonObject values{{"productName", "actual-product"}, {"quantity", 0}, {"enabled", false}};
    panel.setValues(values);
    auto* input = panel.findChild<QLineEdit*>(QStringLiteral("printField_productName"));
    auto* form = panel.findChild<QFormLayout*>();
    QVERIFY(input != nullptr && form != nullptr);
    auto* title = qobject_cast<QLabel*>(form->labelForField(input->parentWidget()));
    QVERIFY(title != nullptr);
    QCOMPARE(title->text(), QString::fromUtf8("商品名称 *"));
    QCOMPARE(panel.values(), values);
    document.fieldDisplayNames.insert(QStringLiteral("productName"), QString::fromUtf8("门店品名"));
    panel.setDocument(document);
    input = panel.findChild<QLineEdit*>(QStringLiteral("printField_productName"));
    title = qobject_cast<QLabel*>(form->labelForField(input->parentWidget()));
    QCOMPARE(title->text(), QString::fromUtf8("门店品名 *"));
    QCOMPARE(input->toolTip(), QStringLiteral("productName"));
    QCOMPARE(panel.values(), values);
    document.fieldDisplayNames.clear();
    core::FieldDefinition declared;
    declared.key = QStringLiteral("productName");
    declared.displayName = declared.key;
    declared.required = true;
    document.fieldSchema = {declared};
    panel.setDocument(document);
    input = panel.findChild<QLineEdit*>(QStringLiteral("printField_productName"));
    title = qobject_cast<QLabel*>(form->labelForField(input->parentWidget()));
    QCOMPARE(title->text(), QStringLiteral("productName *"));
    QCOMPARE(panel.values(), values);
}

void PrintFieldEditorTests::fieldErrorsUseDisplayNamesAndKeepInvalidKeys()
{
    app::PrintFieldEditorPanel panel;
    auto document = documentWithFields();
    document.fieldDisplayNames.insert(QStringLiteral("quantity"), QString::fromUtf8("打印数量"));
    panel.setDocument(document);
    auto* input = panel.findChild<QLineEdit*>(QStringLiteral("printField_quantity"));
    QVERIFY(input != nullptr);
    input->setText(QStringLiteral("-"));
    bool ok = true;
    QString error;
    panel.values(&ok, &error);
    QVERIFY(!ok);
    QVERIFY(error.contains(QString::fromUtf8("打印数量")));
    QVERIFY(!error.contains(QStringLiteral("quantity")));
    QCOMPARE(panel.invalidFieldKey(), QStringLiteral("quantity"));
    document.fieldDisplayNames.insert(QStringLiteral("quantity"), QString::fromUtf8("标签数量"));
    panel.refreshDisplayNames(document);
    QCOMPARE(input->text(), QStringLiteral("-"));
    auto* inlineError = panel.findChild<QLabel*>(QStringLiteral("printField_quantity_error"));
    QVERIFY(inlineError != nullptr);
    QVERIFY(inlineError->text().contains(QString::fromUtf8("标签数量")));

    panel.setValues({});
    auto* tabs = panel.findChild<QTabWidget*>(QStringLiteral("printFieldModeTabs"));
    QVERIFY(tabs != nullptr);
    tabs->setCurrentIndex(1);
    panel.findChild<QPlainTextEdit*>(QStringLiteral("printValuesJsonEdit"))->setPlainText(QStringLiteral("{\"quantity\":\"bad\"}"));
    panel.values(&ok, &error);
    QVERIFY(!ok);
    QVERIFY(error.contains(QString::fromUtf8("标签数量")));
    QVERIFY(!error.contains(QStringLiteral("quantity")));
    QCOMPARE(panel.invalidFieldKey(), QStringLiteral("quantity"));
}

QTEST_MAIN(PrintFieldEditorTests)
#include "tst_print_field_editor.moc"

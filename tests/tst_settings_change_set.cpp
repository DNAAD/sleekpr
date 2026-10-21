#include "sleekpr/core/settings/SettingsChangeSet.h"
#include "sleekpr/core/settings/PrintClientSettingsJson.h"

#include <QtTest>

using namespace sleekpr::core;

namespace {

PrintClientSettings baseSettings()
{
    PrintClientSettings settings;
    settings.defaultPrinter = QStringLiteral("old");
    TemplateDocument document;
    document.id = QStringLiteral("tag");
    document.name = QStringLiteral("base");
    settings.templateDocuments.insert(QStringLiteral("default"), document);
    return settings;
}

} // 匿名命名空间

class SettingsChangeSetTests final : public QObject
{
    Q_OBJECT
private slots:
    void mergesTemplateWithoutRestoringOldPrinter();
    void rejectsOverlappingTemplateAtomically();
    void acceptsIdempotentTarget();
    void keepsUnknownFieldsAndNormalizesDefaults();
    void distinguishesMissingEntryFromNull();
    void handlesLiteralMapKeysAndRemoval();
    void mergesIndependentHttpLimitFields();
    void unchangedEditPreservesExternalChanges();
};

void SettingsChangeSetTests::mergesTemplateWithoutRestoringOldPrinter()
{
    const auto base = baseSettings();
    auto edited = base;
    edited.templateDocuments[QStringLiteral("default")].name = QStringLiteral("edited");
    edited.defaultPrinter = QStringLiteral("must-not-write");
    auto latest = PrintClientSettingsJson::toJson(base);
    latest.insert(QStringLiteral("defaultPrinter"), QStringLiteral("new"));
    const auto changes = SettingsChangeSet::between(base, edited, SettingsEditScope::TemplatesOnly);
    const auto result = changes.merge(latest);
    QVERIFY(result.conflicts.isEmpty());
    QVERIFY(result.changed);
    const auto actual = PrintClientSettingsJson::fromJson(result.document);
    QCOMPARE(actual.defaultPrinter, QStringLiteral("new"));
    QCOMPARE(actual.templateDocuments.value(QStringLiteral("default")).name, QStringLiteral("edited"));
}

void SettingsChangeSetTests::rejectsOverlappingTemplateAtomically()
{
    const auto base = baseSettings();
    auto edited = base;
    edited.defaultPrinter = QStringLiteral("local");
    edited.templateDocuments[QStringLiteral("default")].name = QStringLiteral("local");
    auto external = base;
    external.templateDocuments[QStringLiteral("default")].name = QStringLiteral("external");
    const auto latest = PrintClientSettingsJson::toJson(external);
    const auto result = SettingsChangeSet::between(base, edited, SettingsEditScope::AllChangedFields).merge(latest);
    QVERIFY(!result.conflicts.isEmpty());
    QVERIFY(!result.changed);
    QCOMPARE(result.document, latest);
}

void SettingsChangeSetTests::acceptsIdempotentTarget()
{
    const auto base = baseSettings();
    auto edited = base;
    edited.defaultPrinter = QStringLiteral("new");
    const auto latest = PrintClientSettingsJson::toJson(edited);
    const auto result = SettingsChangeSet::between(base, edited, SettingsEditScope::AllChangedFields).merge(latest);
    QVERIFY(result.conflicts.isEmpty());
    QVERIFY(!result.changed);
    QCOMPARE(result.document, latest);
}

void SettingsChangeSetTests::keepsUnknownFieldsAndNormalizesDefaults()
{
    PrintClientSettings base;
    auto edited = base;
    edited.labelOffset.x = 2.5;
    const QJsonObject latest{{"future", QJsonObject{{"enabled", true}}},
        {"labelOffset", QJsonObject{{"futureOffset", 7}}}};
    const auto result = SettingsChangeSet::between(base, edited, SettingsEditScope::AllChangedFields).merge(latest);
    QVERIFY(result.conflicts.isEmpty());
    QCOMPARE(result.document.value(QStringLiteral("future")), latest.value(QStringLiteral("future")));
    QCOMPARE(result.document.value(QStringLiteral("labelOffset")).toObject().value(QStringLiteral("futureOffset")).toInt(), 7);
    QCOMPARE(PrintClientSettingsJson::fromJson(result.document).labelOffset.x, 2.5);
}

void SettingsChangeSetTests::distinguishesMissingEntryFromNull()
{
    PrintClientSettings base;
    auto edited = base;
    edited.templateDocuments.insert(QStringLiteral("tag"), baseSettings().templateDocuments.value(QStringLiteral("default")));
    const QJsonObject latest{{"templateDocuments", QJsonObject{{"tag", QJsonValue::Null}}}};
    const auto result = SettingsChangeSet::between(base, edited, SettingsEditScope::TemplatesOnly).merge(latest);
    QVERIFY(!result.conflicts.isEmpty());
    QCOMPARE(result.document, latest);
}

void SettingsChangeSetTests::handlesLiteralMapKeysAndRemoval()
{
    auto base = baseSettings();
    const auto key = QString::fromUtf8("a.b/中文");
    base.templateDocuments.insert(key, base.templateDocuments.value(QStringLiteral("default")));
    auto edited = base;
    edited.templateDocuments.remove(QStringLiteral("default"));
    edited.templateDocuments[key].name = QStringLiteral("literal-key");
    const auto result = SettingsChangeSet::between(base, edited, SettingsEditScope::TemplatesOnly).merge(PrintClientSettingsJson::toJson(base));
    QVERIFY(result.conflicts.isEmpty());
    const auto documents = result.document.value(QStringLiteral("templateDocuments")).toObject();
    QVERIFY(!documents.contains(QStringLiteral("default")));
    QCOMPARE(documents.value(key).toObject().value(QStringLiteral("name")).toString(), QStringLiteral("literal-key"));
    QVERIFY(!documents.contains(QStringLiteral("a")));
}

void SettingsChangeSetTests::mergesIndependentHttpLimitFields()
{
    const auto base = baseSettings();
    auto edited = base;
    edited.localHttpLimits.maxPreviewPages = 12;
    auto external = base;
    external.localHttpLimits.maxPreviewResponseBytes = 1024;
    const auto result = SettingsChangeSet::between(base, edited, SettingsEditScope::AllChangedFields)
        .merge(PrintClientSettingsJson::toJson(external));
    QVERIFY(result.conflicts.isEmpty());
    const auto actual = PrintClientSettingsJson::fromJson(result.document);
    QVERIFY(actual.localHttpLimits.maxPreviewPages.has_value());
    QVERIFY(actual.localHttpLimits.maxPreviewResponseBytes.has_value());
    QCOMPARE(actual.localHttpLimits.maxPreviewPages.value(), 12);
    QCOMPARE(actual.localHttpLimits.maxPreviewResponseBytes.value(), qsizetype(1024));
}

void SettingsChangeSetTests::unchangedEditPreservesExternalChanges()
{
    const auto base = baseSettings();
    auto external = base;
    external.defaultPrinter = QStringLiteral("external");
    const auto latest = PrintClientSettingsJson::toJson(external);
    const auto changes = SettingsChangeSet::between(base, base, SettingsEditScope::AllChangedFields);
    QVERIFY(changes.changes.isEmpty());
    const auto result = changes.merge(latest);
    QVERIFY(!result.changed);
    QCOMPARE(result.document, latest);
}

QTEST_GUILESS_MAIN(SettingsChangeSetTests)
#include "tst_settings_change_set.moc"

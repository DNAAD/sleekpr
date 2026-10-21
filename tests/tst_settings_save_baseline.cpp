#include "sleekpr/core/settings/SettingsSaveBaseline.h"
#include "sleekpr/core/settings/PrintClientSettingsJson.h"
#include <QtTest>

using namespace sleekpr::core;

namespace {
SettingsReadResult observed()
{
    SettingsReadResult result;
    result.status = SettingsReadStatus::Ready;
    result.settings.defaultPrinter = QStringLiteral("old");
    result.settings.localHttpLimits.maxHeaderBytes = 16 * 1024;
    TemplateDocument document;
    document.id = QStringLiteral("tag");
    document.name = QStringLiteral("base");
    result.settings.templateDocuments.insert(QStringLiteral("a.b/c"), document);
    result.sourceJson = PrintClientSettingsJson::toJson(result.settings);
    return result;
}
}

class SettingsSaveBaselineTests final : public QObject
{
    Q_OBJECT
private slots:
    void acknowledgesOnlySubmittedPaths();
    void ownWriteThenRevertUsesAcknowledgedValue();
    void failedWriteDoesNotAdvance();
    void preservesUnknownAndLiteralPaths();
    void optionalFallbackIsNotAnEdit();
};

void SettingsSaveBaselineTests::acknowledgesOnlySubmittedPaths()
{
    auto base = observed();
    SettingsSaveBaseline state(base);
    auto edited = base.settings;
    edited.defaultPrinter = QStringLiteral("new");
    auto applied = state.changesFor(edited, SettingsEditScope::AllChangedFields);
    auto actual = edited;
    actual.localHttpLimits.maxHeaderBytes = 24 * 1024;
    auto snapshot = base;
    snapshot.settings = actual;
    snapshot.sourceJson = PrintClientSettingsJson::toJson(actual);
    state.acknowledge(applied, {SettingsWriteStatus::Saved, snapshot});
    edited.localHttpLimits.maxHeaderBytes = 32 * 1024;
    const auto changes = state.changesFor(edited, SettingsEditScope::AllChangedFields);
    QCOMPARE(changes.changes.size(), 1);
    QCOMPARE(changes.changes.first().path, QStringList({QStringLiteral("localHttpLimits"), QStringLiteral("maxHeaderBytes")}));
    QCOMPARE(changes.changes.first().expected.toInt(), 16 * 1024);
    QCOMPARE(changes.changes.first().value.toInt(), 32 * 1024);
    QVERIFY(!changes.merge(snapshot.sourceJson).conflicts.isEmpty());
}

void SettingsSaveBaselineTests::ownWriteThenRevertUsesAcknowledgedValue()
{
    const auto base = observed();
    SettingsSaveBaseline state(base);
    auto edited = base.settings;
    edited.defaultPrinter = QStringLiteral("A");
    const auto applied = state.changesFor(edited, SettingsEditScope::AllChangedFields);
    auto snapshot = base;
    snapshot.settings = edited;
    snapshot.sourceJson = PrintClientSettingsJson::toJson(edited);
    state.acknowledge(applied, {SettingsWriteStatus::Saved, snapshot});
    const auto reverted = state.changesFor(base.settings, SettingsEditScope::AllChangedFields);
    QCOMPARE(reverted.changes.size(), 1);
    QCOMPARE(reverted.changes.first().expected.toString(), QStringLiteral("A"));
    QCOMPARE(reverted.changes.first().value.toString(), QStringLiteral("old"));
}

void SettingsSaveBaselineTests::failedWriteDoesNotAdvance()
{
    const auto base = observed();
    SettingsSaveBaseline state(base);
    auto edited = base.settings;
    edited.defaultPrinter = QStringLiteral("A");
    state.acknowledge(state.changesFor(edited, SettingsEditScope::AllChangedFields), {SettingsWriteStatus::WriteFailed, base});
    QCOMPARE(state.expectedSettings().defaultPrinter, QStringLiteral("old"));
}

void SettingsSaveBaselineTests::preservesUnknownAndLiteralPaths()
{
    auto base = observed();
    auto documents = base.sourceJson.value("templateDocuments").toObject();
    auto document = documents.value("a.b/c").toObject();
    document.insert("future", 1);
    documents.insert("a.b/c", document);
    base.sourceJson.insert("templateDocuments", documents);
    SettingsSaveBaseline state(base);
    auto edited = base.settings;
    edited.templateDocuments[QStringLiteral("a.b/c")].name = QStringLiteral("changed");
    const auto changes = state.changesFor(edited, SettingsEditScope::TemplatesOnly);
    QCOMPARE(changes.changes.first().path, QStringList({QStringLiteral("templateDocuments"), QStringLiteral("a.b/c")}));
    auto raw = changes.merge(base.sourceJson).document;
    QCOMPARE(raw.value("templateDocuments").toObject().value("a.b/c").toObject().value("future").toInt(), 1);
    auto snapshot = base;
    snapshot.sourceJson = raw;
    snapshot.settings = PrintClientSettingsJson::fromJson(raw);
    state.acknowledge(changes, {SettingsWriteStatus::Saved, snapshot});
    document = raw.value("templateDocuments").toObject().value("a.b/c").toObject();
    document.insert("future", 2);
    documents.insert("a.b/c", document);
    raw.insert("templateDocuments", documents);
    edited.templateDocuments[QStringLiteral("a.b/c")].name = QStringLiteral("next");
    QVERIFY(!state.changesFor(edited, SettingsEditScope::TemplatesOnly).merge(raw).conflicts.isEmpty());
    edited.templateDocuments.remove(QStringLiteral("a.b/c"));
    QCOMPARE(state.changesFor(edited, SettingsEditScope::TemplatesOnly).changes.size(), 1);
}

void SettingsSaveBaselineTests::optionalFallbackIsNotAnEdit()
{
    auto base = observed();
    base.settings.localHttpLimits.maxHeaderBytes.reset();
    base.sourceJson = PrintClientSettingsJson::toJson(base.settings);
    SettingsSaveBaseline state(base);
    QCOMPARE(state.changesFor(base.settings, SettingsEditScope::AllChangedFields).changes.size(), 0);
}

QTEST_GUILESS_MAIN(SettingsSaveBaselineTests)
#include "tst_settings_save_baseline.moc"

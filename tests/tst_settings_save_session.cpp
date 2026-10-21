#include "sleekpr/infrastructure/settings/SettingsSaveSession.h"
#include "sleekpr/core/settings/PrintClientSettingsJson.h"
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSaveFile>
#include <QTemporaryDir>
#include <QtTest>

using namespace sleekpr;
namespace {
bool writeBytes(const QString& path, const QByteArray& bytes, QString* error = nullptr)
{
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit()) {
        if (error) *error = file.errorString();
        return false;
    }
    return true;
}
core::PrintClientSettings initial()
{
    core::PrintClientSettings value;
    value.defaultPrinter = QStringLiteral("old");
    core::TemplateDocument document;
    document.id = QStringLiteral("tag"); document.name = QStringLiteral("base");
    value.templateDocuments.insert(QStringLiteral("default"), document);
    return value;
}
}

class SettingsSaveSessionTests final : public QObject
{
    Q_OBJECT
private slots:
    void advancesOnlyAfterSuccessAndKeepsExternalValues();
    void conflictBlocksUntilSuccessfulReload();
    void corruptInitialSnapshotCannotAutoSave();
    void explicitBackupRestoreUpdatesBaseline();
    void failedReloadKeepsPriorAcknowledgement();
    void unknownTemplateExtensionRemainsProtected();
    void templateEditsPreserveUnknownFieldsByElementIdentity();
};

void SettingsSaveSessionTests::advancesOnlyAfterSuccessAndKeepsExternalValues()
{
    QTemporaryDir dir;
    const auto path = dir.filePath("settings.json");
    infrastructure::SettingsRepository repository(path);
    QVERIFY(repository.replace(initial()).ok());
    bool failMain = true;
    infrastructure::SettingsSaveSession session(path, [&](const auto& target, const auto& bytes, auto* error) {
        return target == path && failMain ? false : writeBytes(target, bytes, error);
    });
    auto base = session.open();
    auto edited = base.expectedSettings;
    edited.defaultPrinter = QStringLiteral("new");
    const auto failed = session.saveEdited(edited, core::SettingsEditScope::AllChangedFields);
    QVERIFY(failed.writeResult.has_value());
    QCOMPARE(failed.writeResult->status, core::SettingsWriteStatus::WriteFailed);
    QCOMPARE(failed.expectedSettings.defaultPrinter, QStringLiteral("old"));
    failMain = false;
    QVERIFY(session.saveEdited(edited, core::SettingsEditScope::AllChangedFields).writeResult->ok());
    auto external = edited;
    external.defaultPrinter = QStringLiteral("external");
    QVERIFY(repository.replace(external).ok());
    edited.templateDocuments[QStringLiteral("default")].name = QStringLiteral("local");
    const auto saved = session.saveEdited(edited, core::SettingsEditScope::TemplatesOnly);
    QVERIFY(saved.writeResult->ok());
    QCOMPARE(saved.observed.settings.defaultPrinter, QStringLiteral("external"));
    QCOMPARE(saved.expectedSettings.defaultPrinter, QStringLiteral("new"));
}

void SettingsSaveSessionTests::conflictBlocksUntilSuccessfulReload()
{
    QTemporaryDir dir; const auto path = dir.filePath("settings.json");
    infrastructure::SettingsRepository repository(path); QVERIFY(repository.replace(initial()).ok());
    infrastructure::SettingsSaveSession session(path); auto edited = session.open().expectedSettings;
    auto external = edited; external.defaultPrinter = QStringLiteral("external"); QVERIFY(repository.replace(external).ok());
    edited.defaultPrinter = QStringLiteral("local");
    QCOMPARE(session.saveEdited(edited, core::SettingsEditScope::AllChangedFields).writeResult->status, core::SettingsWriteStatus::Conflict);
    QVERIFY(writeBytes(path, "{broken"));
    QCOMPARE(session.reload().observed.status, core::SettingsReadStatus::InvalidJson);
    QCOMPARE(session.saveEdited(edited, core::SettingsEditScope::AllChangedFields).writeResult->status, core::SettingsWriteStatus::Conflict);
    QVERIFY(repository.replace(external).ok());
    edited = session.reload().expectedSettings; edited.defaultPrinter = QStringLiteral("confirmed");
    QVERIFY(session.saveEdited(edited, core::SettingsEditScope::AllChangedFields).writeResult->ok());
}

void SettingsSaveSessionTests::corruptInitialSnapshotCannotAutoSave()
{
    QTemporaryDir dir; const auto path = dir.filePath("settings.json"); QVERIFY(writeBytes(path, "{broken"));
    infrastructure::SettingsSaveSession session(path);
    QCOMPARE(session.open().observed.status, core::SettingsReadStatus::InvalidJson);
    QCOMPARE(session.saveEdited(initial(), core::SettingsEditScope::AllChangedFields).writeResult->status, core::SettingsWriteStatus::ReadFailed);
    QFile file(path); QVERIFY(file.open(QIODevice::ReadOnly)); QCOMPARE(file.readAll(), QByteArray("{broken"));
}

void SettingsSaveSessionTests::explicitBackupRestoreUpdatesBaseline()
{
    QTemporaryDir dir; const auto path = dir.filePath("settings.json");
    infrastructure::SettingsRepository repository(path); QVERIFY(repository.replace(initial()).ok());
    auto changed = initial(); changed.defaultPrinter = QStringLiteral("second"); QVERIFY(repository.replace(changed).ok());
    QVERIFY(writeBytes(path, "{broken"));
    infrastructure::SettingsSaveSession session(path); const auto broken = session.open();
    const auto restored = session.restoreBackup(broken.observed.sourceDigest);
    QVERIFY(restored.writeResult->ok()); QVERIFY(restored.baselineReset);
    QCOMPARE(restored.expectedSettings.defaultPrinter, QStringLiteral("old"));
}

void SettingsSaveSessionTests::failedReloadKeepsPriorAcknowledgement()
{
    QTemporaryDir dir; const auto path = dir.filePath("settings.json");
    infrastructure::SettingsRepository repository(path); QVERIFY(repository.replace(initial()).ok());
    infrastructure::SettingsSaveSession session(path); auto edited = session.open().expectedSettings;
    edited.defaultPrinter = QStringLiteral("saved"); QVERIFY(session.saveEdited(edited, core::SettingsEditScope::AllChangedFields).writeResult->ok());
    QVERIFY(writeBytes(path, "{broken")); const auto failure = session.reload();
    QVERIFY(!failure.baselineReset); QCOMPARE(failure.expectedSettings.defaultPrinter, QStringLiteral("saved"));
    QCOMPARE(failure.observed.status, core::SettingsReadStatus::InvalidJson);
}

void SettingsSaveSessionTests::unknownTemplateExtensionRemainsProtected()
{
    QTemporaryDir dir; const auto path = dir.filePath("settings.json");
    auto raw = core::PrintClientSettingsJson::toJson(initial());
    auto documents = raw.value("templateDocuments").toObject(); auto document = documents.value("default").toObject();
    document.insert("future", 1); documents.insert("default", document); raw.insert("templateDocuments", documents);
    QVERIFY(writeBytes(path, QJsonDocument(raw).toJson()));
    infrastructure::SettingsSaveSession session(path); auto edited = session.open().expectedSettings;
    edited.defaultPrinter = QStringLiteral("mine"); QVERIFY(session.saveEdited(edited, core::SettingsEditScope::AllChangedFields).writeResult->ok());
    infrastructure::SettingsRepository repository(path); raw = repository.read().sourceJson;
    documents = raw.value("templateDocuments").toObject(); document.insert("future", 2);
    documents.insert("default", document); raw.insert("templateDocuments", documents); QVERIFY(writeBytes(path, QJsonDocument(raw).toJson()));
    edited.templateDocuments[QStringLiteral("default")].name = QStringLiteral("local");
    QCOMPARE(session.saveEdited(edited, core::SettingsEditScope::TemplatesOnly).writeResult->status, core::SettingsWriteStatus::Conflict);
}

void SettingsSaveSessionTests::templateEditsPreserveUnknownFieldsByElementIdentity()
{
    QTemporaryDir dir; const auto path = dir.filePath("settings.json"); auto settings = initial();
    core::TemplateLayer layer; layer.id = QStringLiteral("layer");
    core::TemplateElement a, b; a.id = QStringLiteral("a"); b.id = QStringLiteral("b"); layer.elements = {a, b};
    settings.templateDocuments[QStringLiteral("default")].layers = {layer};
    auto raw = core::PrintClientSettingsJson::toJson(settings); auto docs = raw.value("templateDocuments").toObject();
    auto doc = docs.value("default").toObject(); auto layers = doc.value("layers").toArray(); auto item = layers[0].toObject();
    auto elements = item.value("elements").toArray(); auto first = elements[0].toObject(); first.insert("future", 7);
    elements[0] = first; item.insert("elements", elements); layers[0] = item; doc.insert("layers", layers);
    docs.insert("default", doc); raw.insert("templateDocuments", docs); QVERIFY(writeBytes(path, QJsonDocument(raw).toJson()));
    infrastructure::SettingsSaveSession session(path); auto edited = session.open().expectedSettings;
    edited.templateDocuments[QStringLiteral("default")].layers[0].elements.move(0, 1);
    const auto saved = session.saveEdited(edited, core::SettingsEditScope::TemplatesOnly);
    QVERIFY(saved.writeResult->ok());
    const auto result = saved.observed.sourceJson.value("templateDocuments").toObject().value("default").toObject()
        .value("layers").toArray()[0].toObject().value("elements").toArray();
    QVERIFY(!result[0].toObject().contains("future")); QCOMPARE(result[1].toObject().value("future").toInt(), 7);
}

QTEST_GUILESS_MAIN(SettingsSaveSessionTests)
#include "tst_settings_save_session.moc"

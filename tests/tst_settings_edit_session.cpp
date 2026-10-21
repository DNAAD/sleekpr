#include "sleekpr/app/SettingsEditSession.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include "sleekpr/core/settings/PrintClientSettingsJson.h"
#include <QSaveFile>
#include <QTemporaryDir>
#include <QtTest>

using namespace sleekpr;

namespace {

core::PrintClientSettings initialSettings()
{
    core::PrintClientSettings settings;
    settings.defaultPrinter = QStringLiteral("old");
    core::TemplateDocument document;
    document.id = QStringLiteral("tag");
    document.name = QStringLiteral("base");
    settings.templateDocuments.insert(QStringLiteral("default"), document);
    return settings;
}

bool writeBytes(const QString& path, const QByteArray& bytes, QString* error = nullptr)
{
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit()) {
        if (error != nullptr) *error = file.errorString();
        return false;
    }
    return true;
}

} // 匿名命名空间

class SettingsEditSessionTests final : public QObject
{
    Q_OBJECT
private slots:
    void advancesBaselineOnlyAfterSuccess();
    void preservesExternalPrinterWhenSavingTemplate();
    void conflictBlocksUntilSuccessfulReload();
    void corruptInitialSnapshotCannotAutoSave();
    void explicitBackupRestoreUpdatesBaseline();
    void templateEditsPreserveUnknownFieldsByElementIdentity();
    void externalUnknownTemplateEditConflicts();
};

void SettingsEditSessionTests::advancesBaselineOnlyAfterSuccess()
{
    QTemporaryDir dir;
    const auto path = dir.filePath(QStringLiteral("settings.json"));
    infrastructure::SettingsRepository repository(path);
    QVERIFY(repository.replace(initialSettings()).ok());
    bool failMain = true;
    app::SettingsEditSession session(path, [&](const QString& target, const QByteArray& bytes, QString* error) {
        if (target == path && failMain) return false;
        return writeBytes(target, bytes, error);
    });
    const auto oldDigest = session.baseline().sourceDigest;
    auto edited = session.baseline().settings;
    edited.defaultPrinter = QStringLiteral("new");
    QCOMPARE(session.saveEdited(edited, core::SettingsEditScope::AllChangedFields).status, core::SettingsWriteStatus::WriteFailed);
    QCOMPARE(session.baseline().sourceDigest, oldDigest);
    failMain = false;
    QVERIFY(session.saveEdited(edited, core::SettingsEditScope::AllChangedFields).ok());
    QCOMPARE(session.baseline().settings.defaultPrinter, QStringLiteral("new"));
    QVERIFY(session.baseline().sourceDigest != oldDigest);
}

void SettingsEditSessionTests::preservesExternalPrinterWhenSavingTemplate()
{
    QTemporaryDir dir;
    const auto path = dir.filePath(QStringLiteral("settings.json"));
    infrastructure::SettingsRepository repository(path);
    QVERIFY(repository.replace(initialSettings()).ok());
    app::SettingsEditSession session(path);
    auto edited = session.baseline().settings;
    auto external = edited;
    external.defaultPrinter = QStringLiteral("external");
    QVERIFY(repository.replace(external).ok());
    edited.templateDocuments[QStringLiteral("default")].name = QStringLiteral("local");
    QVERIFY(session.saveEdited(edited, core::SettingsEditScope::TemplatesOnly).ok());
    QCOMPARE(session.baseline().settings.defaultPrinter, QStringLiteral("external"));
    QCOMPARE(repository.read().settings.templateDocuments.value(QStringLiteral("default")).name, QStringLiteral("local"));
}

void SettingsEditSessionTests::conflictBlocksUntilSuccessfulReload()
{
    QTemporaryDir dir;
    const auto path = dir.filePath(QStringLiteral("settings.json"));
    infrastructure::SettingsRepository repository(path);
    QVERIFY(repository.replace(initialSettings()).ok());
    app::SettingsEditSession session(path);
    auto edited = session.baseline().settings;
    auto external = edited;
    external.defaultPrinter = QStringLiteral("external");
    QVERIFY(repository.replace(external).ok());
    edited.defaultPrinter = QStringLiteral("local");
    QCOMPARE(session.saveEdited(edited, core::SettingsEditScope::AllChangedFields).status, core::SettingsWriteStatus::Conflict);
    const auto oldDigest = session.baseline().sourceDigest;
    QVERIFY(writeBytes(path, QByteArray("{broken")));
    QCOMPARE(session.saveEdited(edited, core::SettingsEditScope::AllChangedFields).status, core::SettingsWriteStatus::Conflict);
    QCOMPARE(session.reload().status, core::SettingsReadStatus::InvalidJson);
    QCOMPARE(session.baseline().sourceDigest, oldDigest);
    QVERIFY(repository.replace(external).ok());
    QCOMPARE(session.reload().status, core::SettingsReadStatus::Ready);
    edited = session.baseline().settings;
    edited.defaultPrinter = QStringLiteral("confirmed");
    QVERIFY(session.saveEdited(edited, core::SettingsEditScope::AllChangedFields).ok());
}

void SettingsEditSessionTests::corruptInitialSnapshotCannotAutoSave()
{
    QTemporaryDir dir;
    const auto path = dir.filePath(QStringLiteral("settings.json"));
    QVERIFY(writeBytes(path, QByteArray("{broken")));
    app::SettingsEditSession session(path);
    QCOMPARE(session.baseline().status, core::SettingsReadStatus::InvalidJson);
    QCOMPARE(session.saveEdited(initialSettings(), core::SettingsEditScope::AllChangedFields).status, core::SettingsWriteStatus::ReadFailed);
    QFile file(path);
    QVERIFY(file.open(QIODevice::ReadOnly));
    QCOMPARE(file.readAll(), QByteArray("{broken"));
}

void SettingsEditSessionTests::explicitBackupRestoreUpdatesBaseline()
{
    QTemporaryDir dir;
    const auto path = dir.filePath(QStringLiteral("settings.json"));
    infrastructure::SettingsRepository repository(path);
    const auto settings = initialSettings();
    QVERIFY(repository.replace(settings).ok());
    auto changed = settings;
    changed.defaultPrinter = QStringLiteral("second");
    QVERIFY(repository.replace(changed).ok());
    QVERIFY(writeBytes(path, QByteArray("{broken")));
    app::SettingsEditSession session(path);
    QVERIFY(session.restoreBackup(session.baseline().sourceDigest).ok());
    QCOMPARE(session.baseline().status, core::SettingsReadStatus::Ready);
    QCOMPARE(session.baseline().settings.defaultPrinter, QStringLiteral("old"));
}

void SettingsEditSessionTests::templateEditsPreserveUnknownFieldsByElementIdentity()
{
    QTemporaryDir directory;
    const auto path = directory.filePath(QStringLiteral("settings.json"));
    auto settings = initialSettings();
    core::TemplateLayer layer;
    layer.id = QStringLiteral("base");
    core::TemplateElement a, b;
    a.id = QStringLiteral("a");
    b.id = QStringLiteral("b");
    layer.elements = {a, b};
    settings.templateDocuments[QStringLiteral("default")].layers = {layer};
    auto source = core::PrintClientSettingsJson::toJson(settings);
    auto documents = source.value("templateDocuments").toObject();
    auto document = documents.value("default").toObject();
    document.remove("paperSpecId");
    document.insert("futureTemplate", QJsonObject{{"enabled", true}});
    auto layers = document.value("layers").toArray();
    auto rawLayer = layers[0].toObject();
    auto elements = rawLayer.value("elements").toArray();
    auto rawA = elements[0].toObject();
    rawA.insert("futureElement", QStringLiteral("belongs-to-a"));
    elements[0] = rawA;
    rawLayer.insert("elements", elements);
    rawLayer.insert("futureLayer", 7);
    layers[0] = rawLayer;
    document.insert("layers", layers);
    documents.insert("default", document);
    source.insert("templateDocuments", documents);
    QVERIFY(writeBytes(path, QJsonDocument(source).toJson()));
    app::SettingsEditSession session(path);
    auto edited = session.baseline().settings;
    auto& local = edited.templateDocuments[QStringLiteral("default")];
    local.name = QStringLiteral("edited");
    local.layers[0].elements.move(0, 1);
    // 仅补齐已知缺省值不算外部冲突，扩展字段仍必须随元素身份保存。
    document.insert("paperSpecId", QStringLiteral("label-80x30"));
    documents.insert("default", document);
    source.insert("templateDocuments", documents);
    QVERIFY(writeBytes(path, QJsonDocument(source).toJson()));
    const auto result = session.saveEdited(edited, core::SettingsEditScope::TemplatesOnly);
    QVERIFY(result.ok());
    const auto actual = result.snapshot.sourceJson.value("templateDocuments").toObject().value("default").toObject();
    QCOMPARE(actual.value("futureTemplate"), document.value("futureTemplate"));
    const auto actualLayer = actual.value("layers").toArray()[0].toObject();
    QCOMPARE(actualLayer.value("futureLayer").toInt(), 7);
    const auto actualElements = actualLayer.value("elements").toArray();
    QCOMPARE(actualElements[0].toObject().value("id").toString(), QStringLiteral("b"));
    QVERIFY(!actualElements[0].toObject().contains("futureElement"));
    QCOMPARE(actualElements[1].toObject().value("futureElement").toString(), QStringLiteral("belongs-to-a"));
}

void SettingsEditSessionTests::externalUnknownTemplateEditConflicts()
{
    QTemporaryDir directory;
    const auto path = directory.filePath(QStringLiteral("settings.json"));
    auto source = core::PrintClientSettingsJson::toJson(initialSettings());
    auto documents = source.value("templateDocuments").toObject();
    auto document = documents.value("default").toObject();
    document.insert("futureTemplate", 1);
    documents.insert("default", document);
    source.insert("templateDocuments", documents);
    QVERIFY(writeBytes(path, QJsonDocument(source).toJson()));
    app::SettingsEditSession session(path);
    auto edited = session.baseline().settings;
    edited.templateDocuments[QStringLiteral("default")].name = QStringLiteral("local");
    document.insert("futureTemplate", 2);
    documents.insert("default", document);
    source.insert("templateDocuments", documents);
    const auto external = QJsonDocument(source).toJson();
    QVERIFY(writeBytes(path, external));
    QCOMPARE(session.saveEdited(edited, core::SettingsEditScope::TemplatesOnly).status, core::SettingsWriteStatus::Conflict);
    QFile file(path);
    QVERIFY(file.open(QIODevice::ReadOnly));
    QCOMPARE(file.readAll(), external);
}

QTEST_GUILESS_MAIN(SettingsEditSessionTests)
#include "tst_settings_edit_session.moc"

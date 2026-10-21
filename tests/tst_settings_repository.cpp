#include "sleekpr/infrastructure/settings/SettingsRepository.h"
#include "sleekpr/core/settings/FileSettingsStore.h"

#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QLockFile>
#include <QSaveFile>
#include <QTemporaryDir>
#include <QtTest>

using namespace sleekpr;

namespace {

QByteArray bytesAt(const QString& path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray{};
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

} // 匿名命名空间

class SettingsRepositoryTests final : public QObject
{
    Q_OBJECT
private slots:
    void staleTemplateIsRejectedAndFileUnchanged();
    void independentChangesSurvive();
    void replacementUsesSameLockAsPatch();
    void corruptFileBlocksAutomaticPatch();
    void explicitReplacementPreservesCorruptBytes();
    void backupFailureDoesNotReplaceMainFile();
    void failedCommitKeepsFileAndSnapshot();
    void restoreRejectsChangedDigest();
    void missingFileInitializesWithoutError();
    void noOpAndIdempotentSaveDoNotWrite();
    void unreadablePathCannotBeOverwritten();
    void corruptFileRestoresOnlyOnExplicitRequest();
    void legacyFacadeHonorsTheSameLock();
    void failedRestoreKeepsOriginalBackupAvailable();
};

void SettingsRepositoryTests::staleTemplateIsRejectedAndFileUnchanged()
{
    QTemporaryDir dir;
    const auto path = dir.filePath(QStringLiteral("settings.json"));
    infrastructure::SettingsRepository store(path);
    const auto base = initialSettings();
    QVERIFY(store.replace(base).ok());
    auto edited = base;
    edited.templateDocuments[QStringLiteral("default")].name = QStringLiteral("local");
    auto external = base;
    external.templateDocuments[QStringLiteral("default")].name = QStringLiteral("external");
    QVERIFY(store.replace(external).ok());
    const auto original = bytesAt(path);
    const auto result = store.apply(core::SettingsChangeSet::between(base, edited, core::SettingsEditScope::TemplatesOnly));
    QCOMPARE(result.status, core::SettingsWriteStatus::Conflict);
    QVERIFY(!result.conflicts.isEmpty());
    QCOMPARE(bytesAt(path), original);
}

void SettingsRepositoryTests::independentChangesSurvive()
{
    QTemporaryDir dir;
    const auto path = dir.filePath(QStringLiteral("settings.json"));
    infrastructure::SettingsRepository store(path);
    const auto base = initialSettings();
    QVERIFY(store.replace(base).ok());
    auto external = base;
    external.defaultPrinter = QStringLiteral("external");
    QVERIFY(store.replace(external).ok());
    auto edited = base;
    edited.templateDocuments[QStringLiteral("default")].name = QStringLiteral("local");
    const auto result = store.apply(core::SettingsChangeSet::between(base, edited, core::SettingsEditScope::TemplatesOnly));
    QVERIFY(result.ok());
    QCOMPARE(store.read().settings.defaultPrinter, QStringLiteral("external"));
    QCOMPARE(store.read().settings.templateDocuments.value(QStringLiteral("default")).name, QStringLiteral("local"));
}

void SettingsRepositoryTests::replacementUsesSameLockAsPatch()
{
    QTemporaryDir dir;
    const auto path = dir.filePath(QStringLiteral("settings.json"));
    infrastructure::SettingsRepository store(path);
    const auto base = initialSettings();
    QVERIFY(store.replace(base).ok());
    QLockFile lock(path + QStringLiteral(".lock"));
    QVERIFY(lock.tryLock(0));
    auto edited = base;
    edited.defaultPrinter = QStringLiteral("changed");
    QCOMPARE(store.replace(edited).status, core::SettingsWriteStatus::Busy);
    QCOMPARE(store.apply(core::SettingsChangeSet::between(base, edited, core::SettingsEditScope::AllChangedFields)).status,
        core::SettingsWriteStatus::Busy);
    QCOMPARE(store.read().settings.defaultPrinter, base.defaultPrinter);
}

void SettingsRepositoryTests::corruptFileBlocksAutomaticPatch()
{
    QTemporaryDir dir;
    const auto path = dir.filePath(QStringLiteral("settings.json"));
    const QByteArray damaged("{broken-config");
    QVERIFY(writeBytes(path, damaged));
    infrastructure::SettingsRepository store(path);
    QCOMPARE(store.read().status, core::SettingsReadStatus::InvalidJson);
    core::PrintClientSettings edited;
    edited.defaultPrinter = QStringLiteral("must-not-save");
    const auto result = store.apply(core::SettingsChangeSet::between({}, edited, core::SettingsEditScope::AllChangedFields));
    QCOMPARE(result.status, core::SettingsWriteStatus::ReadFailed);
    QCOMPARE(bytesAt(path), damaged);
}

void SettingsRepositoryTests::explicitReplacementPreservesCorruptBytes()
{
    QTemporaryDir dir;
    const auto path = dir.filePath(QStringLiteral("settings.json"));
    infrastructure::SettingsRepository store(path);
    auto settings = initialSettings();
    QVERIFY(store.replace(settings).ok());
    settings.defaultPrinter = QStringLiteral("second");
    QVERIFY(store.replace(settings).ok());
    const auto goodBackup = bytesAt(path + QStringLiteral(".bak"));
    const QByteArray damaged("[corrupt]");
    QVERIFY(writeBytes(path, damaged));
    settings.defaultPrinter = QStringLiteral("recovered");
    QVERIFY(store.replace(settings).ok());
    QCOMPARE(bytesAt(path + QStringLiteral(".corrupt.bak")), damaged);
    QCOMPARE(bytesAt(path + QStringLiteral(".bak")), goodBackup);
    QCOMPARE(store.read().settings.defaultPrinter, QStringLiteral("recovered"));
}

void SettingsRepositoryTests::backupFailureDoesNotReplaceMainFile()
{
    QTemporaryDir dir;
    const auto path = dir.filePath(QStringLiteral("settings.json"));
    infrastructure::SettingsRepository store(path);
    auto settings = initialSettings();
    QVERIFY(store.replace(settings).ok());
    const auto original = bytesAt(path);
    QVERIFY(QDir().mkdir(path + QStringLiteral(".bak")));
    settings.defaultPrinter = QStringLiteral("changed");
    QCOMPARE(store.replace(settings).status, core::SettingsWriteStatus::BackupFailed);
    QCOMPARE(bytesAt(path), original);
}

void SettingsRepositoryTests::failedCommitKeepsFileAndSnapshot()
{
    QTemporaryDir dir;
    const auto path = dir.filePath(QStringLiteral("settings.json"));
    infrastructure::SettingsRepository store(path);
    const auto settings = initialSettings();
    QVERIFY(store.replace(settings).ok());
    const auto original = bytesAt(path);
    const auto snapshot = store.read();
    infrastructure::SettingsRepository failing(path, [path](const QString& target, const QByteArray& bytes, QString* error) {
        if (target == path) { if (error != nullptr) *error = QStringLiteral("forced failure"); return false; }
        return writeBytes(target, bytes, error);
    });
    auto edited = settings;
    edited.defaultPrinter = QStringLiteral("changed");
    const auto result = failing.apply(core::SettingsChangeSet::between(settings, edited, core::SettingsEditScope::AllChangedFields));
    QCOMPARE(result.status, core::SettingsWriteStatus::WriteFailed);
    QCOMPARE(result.snapshot.sourceDigest, snapshot.sourceDigest);
    QCOMPARE(bytesAt(path), original);
}

void SettingsRepositoryTests::restoreRejectsChangedDigest()
{
    QTemporaryDir dir;
    const auto path = dir.filePath(QStringLiteral("settings.json"));
    infrastructure::SettingsRepository store(path);
    auto settings = initialSettings();
    QVERIFY(store.replace(settings).ok());
    settings.defaultPrinter = QStringLiteral("second");
    QVERIFY(store.replace(settings).ok());
    const auto digest = store.read().sourceDigest;
    settings.defaultPrinter = QStringLiteral("third");
    QVERIFY(store.replace(settings).ok());
    QCOMPARE(store.restoreBackup(digest).status, core::SettingsWriteStatus::Conflict);
    QCOMPARE(store.read().settings.defaultPrinter, QStringLiteral("third"));
}

void SettingsRepositoryTests::missingFileInitializesWithoutError()
{
    QTemporaryDir dir;
    const auto path = dir.filePath(QStringLiteral("nested/settings.json"));
    infrastructure::SettingsRepository store(path);
    QCOMPARE(store.read().status, core::SettingsReadStatus::Missing);
    core::PrintClientSettings settings;
    settings.defaultPrinter = QStringLiteral("new");
    const auto result = store.apply(core::SettingsChangeSet::between({}, settings, core::SettingsEditScope::AllChangedFields));
    QCOMPARE(result.status, core::SettingsWriteStatus::Saved);
    QCOMPARE(store.read().status, core::SettingsReadStatus::Ready);
    QCOMPARE(core::FileSettingsStore(path).load().defaultPrinter, settings.defaultPrinter);
}

void SettingsRepositoryTests::noOpAndIdempotentSaveDoNotWrite()
{
    QTemporaryDir dir;
    const auto path = dir.filePath(QStringLiteral("settings.json"));
    infrastructure::SettingsRepository store(path);
    const auto settings = initialSettings();
    QVERIFY(store.replace(settings).ok());
    int writes = 0;
    infrastructure::SettingsRepository counted(path, [&writes](const QString& target, const QByteArray& bytes, QString* error) {
        ++writes;
        return writeBytes(target, bytes, error);
    });
    QCOMPARE(counted.replace(settings).status, core::SettingsWriteStatus::Unchanged);
    QCOMPARE(counted.apply({}).status, core::SettingsWriteStatus::Unchanged);
    QCOMPARE(writes, 0);
}

void SettingsRepositoryTests::unreadablePathCannotBeOverwritten()
{
    QTemporaryDir dir;
    const auto path = dir.filePath(QStringLiteral("settings.json"));
    QVERIFY(QDir().mkdir(path));
    infrastructure::SettingsRepository store(path);
    QCOMPARE(store.read().status, core::SettingsReadStatus::IoError);
    QCOMPARE(store.replace(initialSettings()).status, core::SettingsWriteStatus::ReadFailed);
    QVERIFY(QFileInfo(path).isDir());
}

void SettingsRepositoryTests::corruptFileRestoresOnlyOnExplicitRequest()
{
    QTemporaryDir dir;
    const auto path = dir.filePath(QStringLiteral("settings.json"));
    infrastructure::SettingsRepository store(path);
    auto settings = initialSettings();
    QVERIFY(store.replace(settings).ok());
    settings.defaultPrinter = QStringLiteral("second");
    QVERIFY(store.replace(settings).ok());
    const QByteArray damaged("{broken");
    QVERIFY(writeBytes(path, damaged));
    const auto snapshot = store.read();
    QCOMPARE(snapshot.status, core::SettingsReadStatus::InvalidJson);
    QCOMPARE(bytesAt(path), damaged);
    QVERIFY(store.restoreBackup(snapshot.sourceDigest).ok());
    QCOMPARE(store.read().settings.defaultPrinter, QStringLiteral("old"));
    QCOMPARE(bytesAt(path + QStringLiteral(".corrupt.bak")), damaged);
}

void SettingsRepositoryTests::legacyFacadeHonorsTheSameLock()
{
    QTemporaryDir dir;
    const auto path = dir.filePath(QStringLiteral("settings.json"));
    infrastructure::SettingsRepository store(path);
    QVERIFY(store.replace(initialSettings()).ok());
    QLockFile lock(path + QStringLiteral(".lock"));
    QVERIFY(lock.tryLock(0));
    QVERIFY(!core::FileSettingsStore(path).save({}));
}

void SettingsRepositoryTests::failedRestoreKeepsOriginalBackupAvailable()
{
    QTemporaryDir directory;
    const auto path = directory.filePath(QStringLiteral("settings.json"));
    infrastructure::SettingsRepository repository(path);
    auto settings = initialSettings();
    QVERIFY(repository.replace(settings).ok());
    settings.defaultPrinter = QStringLiteral("second");
    QVERIFY(repository.replace(settings).ok());
    const auto main = bytesAt(path);
    const auto backup = bytesAt(path + QStringLiteral(".bak"));
    const auto digest = repository.read().sourceDigest;
    infrastructure::SettingsRepository failing(path, [path](const QString& target, const QByteArray& bytes, QString* error) {
        if (target == path) return false;
        return writeBytes(target, bytes, error);
    });
    QCOMPARE(failing.restoreBackup(digest).status, core::SettingsWriteStatus::WriteFailed);
    QCOMPARE(bytesAt(path), main);
    QCOMPARE(bytesAt(path + QStringLiteral(".bak")), backup);
    QVERIFY(repository.restoreBackup(digest).ok());
    QCOMPARE(repository.read().settings.defaultPrinter, QStringLiteral("old"));
    QCOMPARE(bytesAt(path + QStringLiteral(".restore.bak")), main);
}

QTEST_GUILESS_MAIN(SettingsRepositoryTests)
#include "tst_settings_repository.moc"

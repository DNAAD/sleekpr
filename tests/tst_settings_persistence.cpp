#include "sleekpr/app/SettingsPersistenceService.h"
#include "support/SettingsPersistenceTestSupport.h"
#include <QFile>
#include <QJsonDocument>
#include <QLockFile>
#include <QTemporaryDir>
#include <QTimer>
#include <stdexcept>
#include <QtTest>

using namespace sleekpr;
using namespace sleekpr::app;

class SettingsPersistenceTests final : public QObject
{
    Q_OBJECT
private slots:
    void retainsOnePendingIntentPerSession();
    void guiHeartbeatWhileWriteBlocked();
    void revertWhileFirstWriteRunningIsNotDropped();
    void sessionsConflictOnlyOnSameTarget();
    void unknownExtensionConflictSurvivesOwnAcknowledgement();
    void reloadBarrierKeepsCompletedSaveOnReadFailure();
    void restoreUsesLatestObservedDigest();
    void detachedObserverDoesNotCancelAcceptedSave();
    void reentrantNotificationDoesNotLoseNewSubmission();
    void writeFailurePausesUntilExplicitRetry_data();
    void writeFailurePausesUntilExplicitRetry();
    void lockAndReadFailuresPreserveDraft();
    void suspendedSavesDoNotHideControlWork();
    void controlRequestSealsNewEditsUntilResult();
    void writerExceptionDoesNotEraseConfirmedBaseline();
};

void SettingsPersistenceTests::retainsOnePendingIntentPerSession()
{
    QTemporaryDir dir; const auto path = dir.filePath("settings.json");
    infrastructure::SettingsRepository repo(path); const auto base = test::sampleSettings(); QVERIFY(repo.replace(base).ok());
    test::SettingsWriteGate gate{path}; SettingsPersistenceService service(path, gate.writer()); test::ReleaseSettingsGate unblock{gate};
    const auto a = service.openSession(QStringLiteral("designer")); const auto b = service.openSession(QStringLiteral("settings"));
    QVERIFY(a && b); QTRY_VERIFY(service.isIdle());
    auto draft = base; draft.defaultPrinter = QStringLiteral("A"); gate.armed = true;
    QVERIFY(service.requestSave(*a, draft, core::SettingsEditScope::AllChangedFields));
    QTRY_VERIFY(gate.entered.available() > 0);
    for (int i = 1; i <= 100; ++i) { draft.defaultPrinter = QString::number(i); QVERIFY(service.requestSave(*a, draft, core::SettingsEditScope::AllChangedFields)); }
    auto other = base; other.labelOffset.x = 5;
    QVERIFY(service.requestSave(*b, other, core::SettingsEditScope::AllChangedFields));
    QVERIFY(service.view(*a).running); QVERIFY(service.view(*a).pending); QCOMPARE(gate.writes.load(), 1);
    gate.release.release(); QTRY_VERIFY(service.isIdle());
    QCOMPARE(repo.read().settings.defaultPrinter, QStringLiteral("100")); QCOMPARE(repo.read().settings.labelOffset.x, 5.0);
    QCOMPARE(gate.writes.load(), 3); QCOMPARE(service.view(*a).savedRevision, quint64(101));
}

void SettingsPersistenceTests::guiHeartbeatWhileWriteBlocked()
{
    QTemporaryDir dir; const auto path = dir.filePath("settings.json");
    infrastructure::SettingsRepository repo(path); auto draft = test::sampleSettings(); QVERIFY(repo.replace(draft).ok());
    test::SettingsWriteGate gate{path}; SettingsPersistenceService service(path, gate.writer()); test::ReleaseSettingsGate unblock{gate};
    auto id = service.openSession(QStringLiteral("test")); QVERIFY(id); QTRY_VERIFY(service.isIdle());
    bool completionOnGui = true;
    connect(&service, &SettingsPersistenceService::sessionChanged, this, [&](auto) { completionOnGui &= QThread::currentThread() == gate.guiThread; });
    gate.armed = true; draft.defaultPrinter = QStringLiteral("new");
    QVERIFY(service.requestSave(*id, draft, core::SettingsEditScope::AllChangedFields)); QTRY_VERIFY(gate.entered.available() > 0);
    int heartbeat = 0; QTimer::singleShot(0, this, [&] { ++heartbeat; }); QTRY_COMPARE(heartbeat, 1);
    QVERIFY(service.hasRunningWork()); QVERIFY(gate.offGui.load()); QVERIFY(!service.shutdownIfIdle());
    gate.release.release(); QTRY_VERIFY(service.isIdle()); QVERIFY(completionOnGui);
}

void SettingsPersistenceTests::revertWhileFirstWriteRunningIsNotDropped()
{
    QTemporaryDir dir; const auto path = dir.filePath("settings.json"); auto base = test::sampleSettings();
    infrastructure::SettingsRepository repo(path); QVERIFY(repo.replace(base).ok());
    test::SettingsWriteGate gate{path}; SettingsPersistenceService service(path, gate.writer()); test::ReleaseSettingsGate unblock{gate};
    auto id = service.openSession(QStringLiteral("test")); QVERIFY(id); QTRY_VERIFY(service.isIdle());
    auto a = base; a.defaultPrinter = QStringLiteral("A"); gate.armed = true;
    QVERIFY(service.requestSave(*id, a, core::SettingsEditScope::AllChangedFields)); QTRY_VERIFY(gate.entered.available() > 0);
    QVERIFY(service.requestSave(*id, base, core::SettingsEditScope::AllChangedFields));
    gate.release.release(); QTRY_VERIFY(service.isIdle()); QCOMPARE(repo.read().settings.defaultPrinter, QStringLiteral("old"));
    QCOMPARE(gate.writes.load(), 2);
}

void SettingsPersistenceTests::sessionsConflictOnlyOnSameTarget()
{
    QTemporaryDir dir; const auto path = dir.filePath("settings.json"); auto base = test::sampleSettings();
    infrastructure::SettingsRepository repo(path); QVERIFY(repo.replace(base).ok()); SettingsPersistenceService service(path);
    auto a = service.openSession(QStringLiteral("A")), b = service.openSession(QStringLiteral("B")); QVERIFY(a && b); QTRY_VERIFY(service.isIdle());
    auto draft = base; draft.defaultPrinter = QStringLiteral("A"); QVERIFY(service.requestSave(*a, draft, core::SettingsEditScope::AllChangedFields)); QTRY_VERIFY(service.isIdle());
    draft.defaultPrinter = QStringLiteral("B"); QVERIFY(service.requestSave(*b, draft, core::SettingsEditScope::AllChangedFields)); QTRY_VERIFY(!service.hasRunningWork());
    QCOMPARE(service.view(*b).phase, SettingsSavePhase::Conflict); QVERIFY(!service.retry(*b));
    QCOMPARE(repo.read().settings.defaultPrinter, QStringLiteral("A"));
}

void SettingsPersistenceTests::unknownExtensionConflictSurvivesOwnAcknowledgement()
{
    QTemporaryDir dir; const auto path = dir.filePath("settings.json"); auto base = test::sampleSettings();
    infrastructure::SettingsRepository repo(path); QVERIFY(repo.replace(base).ok()); SettingsPersistenceService service(path);
    auto id = service.openSession(QStringLiteral("test")); QVERIFY(id); QTRY_VERIFY(service.isIdle());
    auto draft = base; draft.defaultPrinter = QStringLiteral("mine"); QVERIFY(service.requestSave(*id, draft, core::SettingsEditScope::AllChangedFields)); QTRY_VERIFY(service.isIdle());
    auto raw = repo.read().sourceJson; auto docs = raw.value("templateDocuments").toObject(); auto doc = docs.value("default").toObject();
    doc.insert("future", 2); docs.insert("default", doc); raw.insert("templateDocuments", docs); QVERIFY(test::writeSettingsBytes(path, QJsonDocument(raw).toJson()));
    draft.templateDocuments[QStringLiteral("default")].name = QStringLiteral("local");
    QVERIFY(service.requestSave(*id, draft, core::SettingsEditScope::TemplatesOnly)); QTRY_VERIFY(!service.hasRunningWork());
    QCOMPARE(service.view(*id).phase, SettingsSavePhase::Conflict);
}

void SettingsPersistenceTests::reloadBarrierKeepsCompletedSaveOnReadFailure()
{
    QTemporaryDir dir; const auto path = dir.filePath("settings.json"); auto draft = test::sampleSettings();
    infrastructure::SettingsRepository repo(path); QVERIFY(repo.replace(draft).ok());
    test::SettingsWriteGate gate{path};
    bool breakRead = false;
    auto writer = gate.writer();
    SettingsPersistenceService service(path, [&](const auto& target, const auto& bytes, auto* error) {
        const bool ok = writer(target, bytes, error);
        if (target == path && breakRead && ok) test::writeSettingsBytes(path, "{broken");
        return ok;
    }); test::ReleaseSettingsGate unblock{gate};
    auto id = service.openSession(QStringLiteral("test")); QVERIFY(id); QTRY_VERIFY(service.isIdle());
    draft.defaultPrinter = QStringLiteral("A"); gate.armed = true; breakRead = true;
    QVERIFY(service.requestSave(*id, draft, core::SettingsEditScope::AllChangedFields)); QTRY_VERIFY(gate.entered.available() > 0);
    auto b = draft; b.defaultPrinter = QStringLiteral("B"); QVERIFY(service.requestSave(*id, b, core::SettingsEditScope::AllChangedFields));
    QVERIFY(service.requestReload(*id)); gate.release.release();
    QTRY_VERIFY(!service.hasRunningWork() && !service.hasControlWork());
    const auto view = service.view(*id); QCOMPARE(view.phase, SettingsSavePhase::Failed); QVERIFY(view.lastOutcome);
    QCOMPARE(view.lastOutcome->storage.expectedSettings.defaultPrinter, QStringLiteral("A"));
    QVERIFY(!view.pending); QCOMPARE(gate.writes.load(), 1);
}

void SettingsPersistenceTests::restoreUsesLatestObservedDigest()
{
    QTemporaryDir dir; const auto path = dir.filePath("settings.json"); auto draft = test::sampleSettings(); infrastructure::SettingsRepository repo(path);
    QVERIFY(repo.replace(draft).ok()); draft.defaultPrinter = QStringLiteral("second"); QVERIFY(repo.replace(draft).ok());
    QVERIFY(test::writeSettingsBytes(path, "{broken")); SettingsPersistenceService service(path);
    auto id = service.openSession(QStringLiteral("test")); QVERIFY(id); QTRY_VERIFY(!service.hasControlWork());
    const auto view = service.view(*id); QVERIFY(view.lastOutcome); const auto digest = view.lastOutcome->storage.observed.sourceDigest;
    QVERIFY(service.requestRestore(*id, digest)); QTRY_VERIFY(service.isIdle()); QCOMPARE(repo.read().settings.defaultPrinter, QStringLiteral("old"));
}

void SettingsPersistenceTests::detachedObserverDoesNotCancelAcceptedSave()
{
    QTemporaryDir dir; const auto path = dir.filePath("settings.json"); auto draft = test::sampleSettings(); infrastructure::SettingsRepository repo(path); QVERIFY(repo.replace(draft).ok());
    test::SettingsWriteGate gate{path}; SettingsPersistenceService service(path, gate.writer()); test::ReleaseSettingsGate unblock{gate};
    auto id = service.openSession(QStringLiteral("test")); QVERIFY(id); QTRY_VERIFY(service.isIdle());
    gate.armed = true; draft.defaultPrinter = QStringLiteral("A"); QVERIFY(service.requestSave(*id, draft, core::SettingsEditScope::AllChangedFields)); QTRY_VERIFY(gate.entered.available() > 0);
    draft.defaultPrinter = QStringLiteral("B"); QVERIFY(service.requestSave(*id, draft, core::SettingsEditScope::AllChangedFields)); service.detachSession(*id);
    gate.release.release(); QTRY_VERIFY(service.isIdle()); QCOMPARE(repo.read().settings.defaultPrinter, QStringLiteral("B"));
}

void SettingsPersistenceTests::reentrantNotificationDoesNotLoseNewSubmission()
{
    QTemporaryDir dir; const auto path = dir.filePath("settings.json"); auto draft = test::sampleSettings(); infrastructure::SettingsRepository repo(path); QVERIFY(repo.replace(draft).ok());
    SettingsPersistenceService service(path); auto id = service.openSession(QStringLiteral("test")); QVERIFY(id); QTRY_VERIFY(service.isIdle());
    bool submitted = false;
    connect(&service, &SettingsPersistenceService::sessionChanged, this, [&](auto changed) {
        if (changed == *id && !submitted && service.view(*id).savedRevision == 1) {
            submitted = true; draft.defaultPrinter = QStringLiteral("B"); service.requestSave(*id, draft, core::SettingsEditScope::AllChangedFields);
        }
    });
    draft.defaultPrinter = QStringLiteral("A"); QVERIFY(service.requestSave(*id, draft, core::SettingsEditScope::AllChangedFields));
    QTRY_VERIFY(submitted && service.isIdle()); QCOMPARE(repo.read().settings.defaultPrinter, QStringLiteral("B"));
}

void SettingsPersistenceTests::writeFailurePausesUntilExplicitRetry_data()
{
    QTest::addColumn<bool>("backup"); QTest::newRow("main") << false; QTest::newRow("backup") << true;
}
void SettingsPersistenceTests::writeFailurePausesUntilExplicitRetry()
{
    QFETCH(bool, backup); QTemporaryDir dir; const auto path = dir.filePath("settings.json"); auto draft = test::sampleSettings();
    infrastructure::SettingsRepository repo(path); QVERIFY(repo.replace(draft).ok()); test::SettingsWriteGate gate{path};
    SettingsPersistenceService service(path, gate.writer()); test::ReleaseSettingsGate unblock{gate};
    auto id = service.openSession(QStringLiteral("test")); QVERIFY(id); QTRY_VERIFY(service.isIdle());
    gate.failBackup = backup; gate.failMain = !backup; draft.defaultPrinter = QStringLiteral("A");
    QVERIFY(service.requestSave(*id, draft, core::SettingsEditScope::AllChangedFields)); QTRY_COMPARE(service.view(*id).phase, SettingsSavePhase::Failed);
    QCOMPARE(repo.read().settings.defaultPrinter, QStringLiteral("old"));
    draft.defaultPrinter = QStringLiteral("B"); QVERIFY(service.requestSave(*id, draft, core::SettingsEditScope::AllChangedFields));
    QVERIFY(!service.hasRunningWork()); gate.failBackup = false; gate.failMain = false;
    QVERIFY(service.retry(*id)); QTRY_VERIFY(service.isIdle()); QCOMPARE(repo.read().settings.defaultPrinter, QStringLiteral("B"));
}

void SettingsPersistenceTests::lockAndReadFailuresPreserveDraft()
{
    QTemporaryDir dir; const auto path = dir.filePath("settings.json"); auto draft = test::sampleSettings(); infrastructure::SettingsRepository repo(path); QVERIFY(repo.replace(draft).ok());
    SettingsPersistenceService service(path); auto id = service.openSession(QStringLiteral("test")); QVERIFY(id); QTRY_VERIFY(service.isIdle());
    QLockFile lock(path + ".lock"); QVERIFY(lock.tryLock()); draft.defaultPrinter = QStringLiteral("A");
    QVERIFY(service.requestSave(*id, draft, core::SettingsEditScope::AllChangedFields)); QTRY_COMPARE(service.view(*id).phase, SettingsSavePhase::Failed);
    QVERIFY(service.view(*id).lastOutcome); QCOMPARE(service.view(*id).lastOutcome->storage.writeResult->status, core::SettingsWriteStatus::Busy);
    lock.unlock(); QVERIFY(service.retry(*id)); QTRY_VERIFY(service.isIdle());
    QVERIFY(test::writeSettingsBytes(path, "{broken")); draft.defaultPrinter = QStringLiteral("B"); QVERIFY(service.requestSave(*id, draft, core::SettingsEditScope::AllChangedFields));
    QTRY_COMPARE(service.view(*id).phase, SettingsSavePhase::Failed); QVERIFY(!service.retry(*id));
}

void SettingsPersistenceTests::suspendedSavesDoNotHideControlWork()
{
    QTemporaryDir dir; const auto path = dir.filePath("settings.json"); auto draft = test::sampleSettings(); infrastructure::SettingsRepository repo(path); QVERIFY(repo.replace(draft).ok());
    SettingsPersistenceService service(path); auto id = service.openSession(QStringLiteral("test")); QVERIFY(id); QTRY_VERIFY(service.isIdle());
    service.setPendingSuspended(true); draft.defaultPrinter = QStringLiteral("A"); QVERIFY(service.requestSave(*id, draft, core::SettingsEditScope::AllChangedFields));
    QVERIFY(!service.isIdle()); QVERIFY(!service.hasRunningWork()); QVERIFY(service.view(*id).pending);
    service.setPendingSuspended(false); QTRY_VERIFY(service.isIdle()); QCOMPARE(repo.read().settings.defaultPrinter, QStringLiteral("A"));
    QVERIFY(service.requestReload(*id)); QVERIFY(service.hasControlWork()); QTRY_VERIFY(service.isIdle());
    service.setAcceptingSaves(false); QVERIFY(!service.requestSave(*id, draft, core::SettingsEditScope::AllChangedFields));
}

void SettingsPersistenceTests::controlRequestSealsNewEditsUntilResult()
{
    QTemporaryDir dir; const auto path = dir.filePath("settings.json"); auto draft = test::sampleSettings();
    infrastructure::SettingsRepository repo(path); QVERIFY(repo.replace(draft).ok());
    test::SettingsWriteGate gate{path}; SettingsPersistenceService service(path, gate.writer()); test::ReleaseSettingsGate unblock{gate};
    auto id = service.openSession(QStringLiteral("test")); QVERIFY(id); QTRY_VERIFY(service.isIdle());
    gate.armed = true; draft.defaultPrinter = QStringLiteral("A"); QVERIFY(service.requestSave(*id, draft, core::SettingsEditScope::AllChangedFields));
    QTRY_VERIFY(gate.entered.available() > 0); QVERIFY(service.requestReload(*id));
    QCOMPARE(service.view(*id).phase, SettingsSavePhase::Loading);
    draft.defaultPrinter = QStringLiteral("must-not-be-accepted"); QVERIFY(!service.requestSave(*id, draft, core::SettingsEditScope::AllChangedFields));
    gate.release.release(); QTRY_VERIFY(service.isIdle()); QCOMPARE(service.view(*id).epoch, quint64(2));
    QCOMPARE(repo.read().settings.defaultPrinter, QStringLiteral("A"));
}

void SettingsPersistenceTests::writerExceptionDoesNotEraseConfirmedBaseline()
{
    QTemporaryDir dir; const auto path = dir.filePath("settings.json"); auto initial = test::sampleSettings();
    initial.localHttpLimits.maxHeaderBytes = 24 * 1024;
    infrastructure::SettingsRepository repo(path); QVERIFY(repo.replace(initial).ok());
    std::atomic<bool> fail{true};
    SettingsPersistenceService service(path, [&](const auto& target, const auto& bytes, auto* error) {
        if (fail) throw std::runtime_error("模拟存储异常");
        return test::writeSettingsBytes(target, bytes, error);
    });
    auto id = service.openSession(QStringLiteral("test")); QVERIFY(id); QTRY_VERIFY(service.isIdle());
    auto edited = initial; edited.labelOffset.x = 2;
    QVERIFY(service.requestSave(*id, edited, core::SettingsEditScope::AllChangedFields)); QTRY_COMPARE(service.view(*id).phase, SettingsSavePhase::Failed);
    const auto view = service.view(*id); QVERIFY(view.lastOutcome);
    QCOMPARE(view.lastOutcome->storage.expectedSettings.defaultPrinter, QStringLiteral("old"));
    QCOMPARE(view.lastOutcome->storage.expectedSettings.localHttpLimits.maxHeaderBytes.value(), qsizetype(24 * 1024));
    fail = false; QVERIFY(service.retry(*id)); QTRY_VERIFY(service.isIdle()); QCOMPARE(repo.read().settings.labelOffset.x, 2.0);
}

QTEST_GUILESS_MAIN(SettingsPersistenceTests)
#include "tst_settings_persistence.moc"

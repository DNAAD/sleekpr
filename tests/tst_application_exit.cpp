#include "sleekpr/app/ApplicationExitCoordinator.h"
#include "sleekpr/app/ApplicationExitDialog.h"
#include "sleekpr/app/AppTheme.h"
#include "support/SettingsPersistenceTestSupport.h"
#include <QDialogButtonBox>
#include <QFileInfo>
#include <QMessageBox>
#include <QPushButton>
#include <QTemporaryDir>
#include <QTimer>
#include <QtTest>

using namespace sleekpr;
using namespace sleekpr::app;
namespace {
struct ExitFixture
{
    QTemporaryDir dir;
    QString path = dir.filePath(QStringLiteral("settings.json"));
    infrastructure::SettingsRepository repository{path};
    test::SettingsWriteGate gate{path};
    SettingsPersistenceService service{path, gate.writer()};
    ExitDraftState draft;
    int quitCalls = 0, flushes = 0;
    bool admission = false, paused = false;
    std::function<void()> onFlush, onManual;
    ApplicationExitCoordinator coordinator{service, [this](bool value) { admission = value; }, [this] { ++quitCalls; }};
    SettingsSessionId id = 0;
    ExitFixture()
    {
        repository.replace(test::sampleSettings()); id = service.openSession(QString::fromUtf8("编辑器")).value();
        coordinator.addParticipant({QStringLiteral("editor"), QString::fromUtf8("编辑器"),
            [this] { ++flushes; if (onFlush) onFlush(); }, [this] { return draft; },
            [this] { if (onManual) onManual(); }, [this](bool value) { paused = value; }, [] {}});
    }
    ~ExitFixture() { gate.release.release(32); }
    bool submit(const QString& name)
    {
        auto settings = test::sampleSettings(); settings.defaultPrinter = name;
        return service.requestSave(id, settings, core::SettingsEditScope::AllChangedFields).has_value();
    }
};
}
class ApplicationExitTests final : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase() { applySleekprTheme(*qApp); }
    void waitsForAcceptedSaveAndPendingTimers();
    void invalidOrFailedDraftBlocksExit();
    void conflictCannotBeRetriedThroughExit();
    void manualDraftRequiresExplicitChoice();
    void skipManualDraftStillFinishesPreviouslyAcceptedSaves();
    void repeatedExitDoesNotFlushTwice();
    void cancelExitInvalidatesOldCompletion();
    void discardWaitsForRunningWrite();
    void cancelAfterDiscardRestoresPendingIntent();
    void cancelReadyDiscardDoesNotLosePending();
    void acceptedTimerPrintFinishesBeforeQuit();
    void dialogDefaultsToReturnAndConfirmsDiscard();
};

void ApplicationExitTests::waitsForAcceptedSaveAndPendingTimers()
{
    ExitFixture f; QTRY_VERIFY(f.service.isIdle()); f.gate.armed = true;
    f.onFlush = [&] { QVERIFY(f.submit(QStringLiteral("flushed"))); };
    f.coordinator.requestExit(); QTRY_VERIFY(f.gate.entered.available() > 0);
    QCOMPARE(f.coordinator.state(), ExitState::WaitingForSaves); QVERIFY(f.paused && f.admission); QCOMPARE(f.quitCalls, 0);
    f.gate.release.release(); QTRY_COMPARE(f.quitCalls, 1); QCOMPARE(f.repository.read().settings.defaultPrinter, QStringLiteral("flushed"));
}

void ApplicationExitTests::invalidOrFailedDraftBlocksExit()
{
    ExitFixture f; QTRY_VERIFY(f.service.isIdle()); f.draft.invalidDraft = true;
    f.coordinator.requestExit(); QCOMPARE(f.coordinator.state(), ExitState::Blocked); QCOMPARE(f.quitCalls, 0);
    f.coordinator.retryAndContinue(); QCOMPARE(f.coordinator.state(), ExitState::Blocked);
    f.coordinator.cancelExit(); f.draft.invalidDraft = false; f.gate.failMain = true;
    QVERIFY(f.submit(QStringLiteral("A"))); QTRY_COMPARE(f.service.view(f.id).phase, SettingsSavePhase::Failed);
    f.coordinator.requestExit(); QCOMPARE(f.coordinator.state(), ExitState::Blocked); QCOMPARE(f.quitCalls, 0);
    f.gate.failMain = false; f.coordinator.retryAndContinue(); QTRY_COMPARE(f.quitCalls, 1);
    QCOMPARE(f.repository.read().settings.defaultPrinter, QStringLiteral("A"));
}

void ApplicationExitTests::conflictCannotBeRetriedThroughExit()
{
    ExitFixture f; QTRY_VERIFY(f.service.isIdle()); auto external = test::sampleSettings(); external.defaultPrinter = QStringLiteral("external");
    QVERIFY(f.repository.replace(external).ok()); QVERIFY(f.submit(QStringLiteral("local")));
    QTRY_COMPARE(f.service.view(f.id).phase, SettingsSavePhase::Conflict);
    f.coordinator.requestExit(); QCOMPARE(f.coordinator.state(), ExitState::Blocked); f.coordinator.retryAndContinue();
    QCOMPARE(f.coordinator.state(), ExitState::Blocked); QCOMPARE(f.repository.read().settings.defaultPrinter, QStringLiteral("external"));
    f.coordinator.discardAndContinue(); QTRY_COMPARE(f.quitCalls, 1);
}

void ApplicationExitTests::manualDraftRequiresExplicitChoice()
{
    ExitFixture f; QTRY_VERIFY(f.service.isIdle()); f.draft.manualDraft = true;
    ApplicationExitDialog dialog(f.coordinator);
    f.onManual = [&] { QVERIFY(f.submit(QStringLiteral("manual"))); f.draft.manualDraft = false; };
    f.coordinator.requestExit(); QCOMPARE(f.coordinator.state(), ExitState::Collecting); QCOMPARE(f.quitCalls, 0);
    if (!qEnvironmentVariableIsEmpty("SLEEKPR_EXIT_SCREENSHOT")) {
        const QFileInfo output(qEnvironmentVariable("SLEEKPR_EXIT_SCREENSHOT")); dialog.resize(720, 370);
        QVERIFY(dialog.grab().save(output.absolutePath() + QLatin1Char('/') + output.completeBaseName() + QStringLiteral("-manual.png")));
        for (auto* button : dialog.findChildren<QPushButton*>()) if (button->isVisibleTo(&dialog)) {
            QVERIFY(dialog.rect().contains(QRect(button->mapTo(&dialog, QPoint()), button->size())));
            QVERIFY(button->fontMetrics().horizontalAdvance(button->text()) + 16 <= button->width());
        }
    }
    f.coordinator.saveManualAndContinue(); QTRY_COMPARE(f.quitCalls, 1);
    QCOMPARE(f.repository.read().settings.defaultPrinter, QStringLiteral("manual"));
}

void ApplicationExitTests::skipManualDraftStillFinishesPreviouslyAcceptedSaves()
{
    ExitFixture f; QTRY_VERIFY(f.service.isIdle()); f.gate.armed = true; QVERIFY(f.submit(QStringLiteral("accepted")));
    QTRY_VERIFY(f.gate.entered.available() > 0); f.draft.manualDraft = true;
    f.coordinator.requestExit(); f.coordinator.continueWithoutSavingManual();
    QCOMPARE(f.coordinator.state(), ExitState::WaitingForSaves); QCOMPARE(f.quitCalls, 0);
    f.gate.release.release(); QTRY_COMPARE(f.quitCalls, 1);
    QCOMPARE(f.repository.read().settings.defaultPrinter, QStringLiteral("accepted"));
}

void ApplicationExitTests::repeatedExitDoesNotFlushTwice()
{
    ExitFixture f; QTRY_VERIFY(f.service.isIdle()); f.draft.manualDraft = true;
    f.coordinator.requestExit(); const auto generation = f.coordinator.generation(); f.coordinator.requestExit();
    QCOMPARE(f.flushes, 1); QCOMPARE(f.coordinator.generation(), generation); f.coordinator.continueWithoutSavingManual();
    QTRY_COMPARE(f.quitCalls, 1);
}

void ApplicationExitTests::cancelExitInvalidatesOldCompletion()
{
    ExitFixture f; QTRY_VERIFY(f.service.isIdle()); f.gate.armed = true; QVERIFY(f.submit(QStringLiteral("A")));
    QTRY_VERIFY(f.gate.entered.available() > 0); f.coordinator.requestExit(); f.coordinator.cancelExit();
    QCOMPARE(f.coordinator.state(), ExitState::Running); QVERIFY(!f.paused && !f.admission);
    f.gate.release.release(); QTRY_VERIFY(f.service.isIdle()); QCOMPARE(f.quitCalls, 0);
    f.coordinator.requestExit(); QTRY_COMPARE(f.quitCalls, 1);
}

void ApplicationExitTests::discardWaitsForRunningWrite()
{
    ExitFixture f; QTRY_VERIFY(f.service.isIdle()); f.gate.armed = true; QVERIFY(f.submit(QStringLiteral("A")));
    QTRY_VERIFY(f.gate.entered.available() > 0); QVERIFY(f.submit(QStringLiteral("B")));
    f.coordinator.requestExit(); f.coordinator.discardAndContinue(); QCOMPARE(f.quitCalls, 0);
    f.gate.release.release(); QTRY_COMPARE(f.quitCalls, 1);
    QCOMPARE(f.repository.read().settings.defaultPrinter, QStringLiteral("A")); QCOMPARE(f.gate.writes.load(), 1);
}

void ApplicationExitTests::cancelAfterDiscardRestoresPendingIntent()
{
    ExitFixture f; QTRY_VERIFY(f.service.isIdle()); f.gate.armed = true; QVERIFY(f.submit(QStringLiteral("A")));
    QTRY_VERIFY(f.gate.entered.available() > 0); QVERIFY(f.submit(QStringLiteral("B")));
    f.coordinator.requestExit(); f.coordinator.discardAndContinue(); f.coordinator.cancelExit(); f.gate.release.release();
    QTRY_VERIFY(f.service.isIdle()); QCOMPARE(f.quitCalls, 0); QCOMPARE(f.repository.read().settings.defaultPrinter, QStringLiteral("B"));
}

void ApplicationExitTests::cancelReadyDiscardDoesNotLosePending()
{
    ExitFixture f; QTRY_VERIFY(f.service.isIdle()); f.service.setPendingSuspended(true); QVERIFY(f.submit(QStringLiteral("A")));
    f.coordinator.requestExit(); f.coordinator.discardAndContinue(); QCOMPARE(f.coordinator.state(), ExitState::Ready);
    f.coordinator.cancelExit(); QTRY_VERIFY(f.service.isIdle()); QCOMPARE(f.quitCalls, 0);
    QCOMPARE(f.repository.read().settings.defaultPrinter, QStringLiteral("A"));
}

void ApplicationExitTests::acceptedTimerPrintFinishesBeforeQuit()
{
    ExitFixture f; QTRY_VERIFY(f.service.isIdle()); int submissions = 0; f.draft.activeSubmission = true;
    QTimer::singleShot(0, &f.coordinator, [&] { ++submissions; f.draft.activeSubmission = false; f.coordinator.refresh(); });
    f.coordinator.requestExit(); QCOMPARE(f.coordinator.state(), ExitState::WaitingForSaves); QCOMPARE(f.quitCalls, 0);
    QTRY_COMPARE(f.quitCalls, 1); QCOMPARE(submissions, 1);
}

void ApplicationExitTests::dialogDefaultsToReturnAndConfirmsDiscard()
{
    ExitFixture f; QTRY_VERIFY(f.service.isIdle()); f.draft.invalidDraft = true;
    ApplicationExitDialog dialog(f.coordinator); f.coordinator.requestExit();
    if (!qEnvironmentVariableIsEmpty("SLEEKPR_EXIT_SCREENSHOT")) {
        f.draft.issues = {QString::fromUtf8("配置写入失败，当前草稿已保留，请返回编辑或确认处理。路径及错误说明会在此完整显示。").repeated(6)};
        f.coordinator.refresh(); dialog.resize(720, 370);
        QTRY_VERIFY(dialog.isVisible());
        QVERIFY(dialog.grab().save(qEnvironmentVariable("SLEEKPR_EXIT_SCREENSHOT")));
    }
    auto* back = dialog.findChild<QPushButton*>(QStringLiteral("exitReturnButton"));
    auto* discard = dialog.findChild<QPushButton*>(QStringLiteral("exitDiscardButton")); QVERIFY(back && discard); QVERIFY(back->isDefault());
    discard->click(); auto* confirm = dialog.findChild<QMessageBox*>(); QTRY_VERIFY(dialog.findChild<QMessageBox*>() != nullptr);
    confirm = dialog.findChild<QMessageBox*>(); QCOMPARE(confirm->defaultButton(), confirm->button(QMessageBox::No));
    confirm->button(QMessageBox::No)->click(); QCOMPARE(f.quitCalls, 0); QCOMPARE(f.coordinator.state(), ExitState::Blocked);
    back->click(); QCOMPARE(f.coordinator.state(), ExitState::Running); QVERIFY(!dialog.isVisible());
}

QTEST_MAIN(ApplicationExitTests)
#include "tst_application_exit.moc"

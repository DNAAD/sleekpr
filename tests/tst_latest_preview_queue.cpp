#include "sleekpr/app/LatestPreviewQueue.h"
#include <QCoreApplication>
#include <QEvent>
#include <QProcess>
#include <QSemaphore>
#include <QSignalSpy>
#include <QThread>
#include <QTimer>
#include <QtTest>
#include <atomic>

using namespace sleekpr;

namespace {

struct ReleaseOnExit
{
    QSemaphore& gate;
    ~ReleaseOnExit() { gate.release(8); }
};

infrastructure::PreviewWorkResult taggedResult(int tag)
{
    infrastructure::PreviewWorkResult result;
    result.planningNanoseconds = tag;
    return result;
}

} // 匿名命名空间

class PreviewQueueTests final : public QObject
{
    Q_OBJECT
private slots:
    void publishesOnlyNewestResult();
    void retainsOneRunningAndOnePending();
    void invalidatedQueueDropsResults();
    void destroyedOwnerIsNeverCalled();
    void workRunsOffGuiThread();
    void maximumConcurrencyIsTwo();
    void applicationExitDrainsPreviewWorkSafely();
    void destroyedQueuedOwnerSkipsWork();
    void queuedCancellationReleasesSnapshot();
};

void PreviewQueueTests::publishesOnlyNewestResult()
{
    infrastructure::PreviewExecutor executor;
    app::LatestPreviewQueue queue(executor);
    QSignalSpy spy(&queue, &app::LatestPreviewQueue::resultReady);
    const auto generation = queue.submit([] { return taggedResult(2); });
    QTRY_COMPARE_WITH_TIMEOUT(spy.size(), 1, 1000);
    QCOMPARE(spy.first().first().toULongLong(), generation);
    QCOMPARE(qvariant_cast<infrastructure::PreviewWorkResult>(spy.first().last()).planningNanoseconds, 2);
}

void PreviewQueueTests::retainsOneRunningAndOnePending()
{
    QSemaphore started, release;
    std::atomic<int> bRuns = 0;
    infrastructure::PreviewExecutor executor;
    ReleaseOnExit unblock{release};
    app::LatestPreviewQueue queue(executor);
    QSignalSpy spy(&queue, &app::LatestPreviewQueue::resultReady);
    queue.submit([&] { started.release(); release.acquire(); return taggedResult(1); });
    QVERIFY(started.tryAcquire(1, 2000));
    queue.submit([&] { ++bRuns; return taggedResult(2); });
    const auto newest = queue.submit([] { return taggedResult(3); });
    release.release();
    QTRY_COMPARE_WITH_TIMEOUT(spy.size(), 1, 2000);
    QCOMPARE(spy.first().first().toULongLong(), newest);
    QCOMPARE(qvariant_cast<infrastructure::PreviewWorkResult>(spy.first().last()).planningNanoseconds, 3);
    QCOMPARE(bRuns.load(), 0);
}

void PreviewQueueTests::invalidatedQueueDropsResults()
{
    QSemaphore started, release;
    infrastructure::PreviewExecutor executor;
    ReleaseOnExit unblock{release};
    app::LatestPreviewQueue queue(executor);
    QSignalSpy spy(&queue, &app::LatestPreviewQueue::resultReady);
    queue.submit([&] { started.release(); release.acquire(); return taggedResult(1); });
    QVERIFY(started.tryAcquire(1, 2000));
    queue.invalidate();
    release.release();
    executor.shutdown();
    QCoreApplication::sendPostedEvents(&executor, QEvent::MetaCall);
    QCOMPARE(spy.size(), 0);
}

void PreviewQueueTests::destroyedOwnerIsNeverCalled()
{
    QSemaphore started, release;
    int calls = 0;
    infrastructure::PreviewExecutor executor;
    ReleaseOnExit unblock{release};
    auto queue = std::make_unique<app::LatestPreviewQueue>(executor);
    connect(queue.get(), &app::LatestPreviewQueue::resultReady, this, [&](auto, const auto&) { ++calls; });
    queue->submit([&] { started.release(); release.acquire(); return taggedResult(1); });
    QVERIFY(started.tryAcquire(1, 2000));
    queue.reset();
    release.release();
    executor.shutdown();
    QCoreApplication::sendPostedEvents(&executor, QEvent::MetaCall);
    QCOMPARE(calls, 0);
}

void PreviewQueueTests::workRunsOffGuiThread()
{
    auto* gui = QThread::currentThread();
    std::atomic<bool> offGui = false;
    infrastructure::PreviewExecutor executor;
    app::LatestPreviewQueue queue(executor);
    QSignalSpy spy(&queue, &app::LatestPreviewQueue::resultReady);
    bool completionOnGui = false;
    connect(&queue, &app::LatestPreviewQueue::resultReady, this, [&](auto, const auto&) {
        completionOnGui = QThread::currentThread() == gui;
    });
    queue.submit([&] { offGui = QThread::currentThread() != gui; return taggedResult(1); });
    QTRY_COMPARE_WITH_TIMEOUT(spy.size(), 1, 2000);
    QVERIFY(offGui.load());
    QVERIFY(completionOnGui);
}

void PreviewQueueTests::maximumConcurrencyIsTwo()
{
    QSemaphore started, release;
    std::atomic<int> active = 0, peak = 0;
    int finished = 0;
    infrastructure::PreviewExecutor executor;
    ReleaseOnExit unblock{release};
    for (int i = 0; i < 3; ++i) executor.submit([&] {
        const int count = ++active;
        int old = peak.load();
        while (count > old && !peak.compare_exchange_weak(old, count)) {}
        started.release();
        release.acquire();
        --active;
        return taggedResult(1);
    }, [&](auto) { ++finished; });
    QVERIFY(started.tryAcquire(2, 2000));
    QCOMPARE(peak.load(), 2);
    release.release(3);
    QTRY_COMPARE_WITH_TIMEOUT(finished, 3, 2000);
    QCOMPARE(peak.load(), 2);
}

void PreviewQueueTests::applicationExitDrainsPreviewWorkSafely()
{
    QProcess child;
    child.start(QCoreApplication::applicationFilePath(), {QStringLiteral("--exit-child")});
    QVERIFY(child.waitForStarted());
    QVERIFY(child.waitForFinished(15000));
    QCOMPARE(child.exitStatus(), QProcess::NormalExit);
    QCOMPARE(child.exitCode(), 0);
}

void PreviewQueueTests::destroyedQueuedOwnerSkipsWork()
{
    QSemaphore started, release;
    std::atomic<int> computations = 0;
    infrastructure::PreviewExecutor executor;
    ReleaseOnExit unblock{release};
    for (int i = 0; i < 2; ++i) executor.submit([&] {
        started.release(); release.acquire(); return taggedResult(1);
    }, [](auto) {});
    QVERIFY(started.tryAcquire(2, 2000));
    auto queue = std::make_unique<app::LatestPreviewQueue>(executor);
    queue->submit([&] { ++computations; return taggedResult(1); });
    queue.reset();
    release.release(2);
    executor.shutdown();
    QCOMPARE(computations.load(), 0);
}

void PreviewQueueTests::queuedCancellationReleasesSnapshot()
{
    QSemaphore started, release;
    infrastructure::PreviewExecutor executor;
    ReleaseOnExit unblock{release};
    for (int i = 0; i < 2; ++i) executor.submit([&] {
        started.release(); release.acquire(); return taggedResult(1);
    }, [](auto) {});
    QVERIFY(started.tryAcquire(2, 2000));
    auto retained = std::make_shared<int>(42);
    const std::weak_ptr<int> snapshot = retained;
    auto queue = std::make_unique<app::LatestPreviewQueue>(executor);
    queue->submit([retained] { return taggedResult(*retained); });
    retained.reset();
    QVERIFY(!snapshot.expired());
    queue.reset();
    QVERIFY(snapshot.expired());
}

int main(int argc, char** argv)
{
    QCoreApplication application(argc, argv);
    if (application.arguments().contains(QStringLiteral("--exit-child"))) {
        QSemaphore started, release;
        std::atomic<bool> drained = false;
        {
            infrastructure::PreviewExecutor executor;
            ReleaseOnExit unblock{release};
            app::LatestPreviewQueue queue(executor);
            queue.submit([&] { started.release(); release.acquire(); drained = true; return taggedResult(1); });
            if (!started.tryAcquire(1, 2000)) return 3;
            QObject::connect(&application, &QCoreApplication::aboutToQuit, &application, [&] { release.release(); });
            QTimer::singleShot(0, &application, &QCoreApplication::quit);
            application.exec();
        }
        return drained ? 0 : 4;
    }
    PreviewQueueTests tests;
    return QTest::qExec(&tests, argc, argv);
}

#include "tst_latest_preview_queue.moc"

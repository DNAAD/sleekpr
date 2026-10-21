#include "sleekpr/infrastructure/diagnostics/ClientPerformanceMetrics.h"
#include <QJsonDocument>
#include <QThread>
#include <QtTest>

using namespace sleekpr::infrastructure;

class ClientPerformanceMetricsTests final : public QObject
{
    Q_OBJECT
private slots:
    void recordsDeterministicPercentiles();
    void retainsBoundedSamples();
    void concurrentRecordingKeepsCounts();
    void scopeRecordsOnlyKnownStagesAndTiming();
    void savingAndEnqueueAreMeasuredSeparately();
};

void ClientPerformanceMetricsTests::recordsDeterministicPercentiles()
{
    ClientPerformanceMetrics metrics;
    for (int i = 1; i <= 20; ++i) metrics.record(PerformanceStage::Planning, i * 1000000);
    const auto result = metrics.snapshot().value(QStringLiteral("planning")).toObject();
    QCOMPARE(result.value(QStringLiteral("count")).toInteger(), 20);
    QCOMPARE(result.value(QStringLiteral("p50Ms")).toDouble(), 10.0);
    QCOMPARE(result.value(QStringLiteral("p95Ms")).toDouble(), 19.0);
}

void ClientPerformanceMetricsTests::retainsBoundedSamples()
{
    ClientPerformanceMetrics metrics;
    for (int i = 0; i < 600; ++i) metrics.record(PerformanceStage::Saving, i);
    const auto result = metrics.snapshot().value(QStringLiteral("saving")).toObject();
    QCOMPARE(result.value(QStringLiteral("count")).toInteger(), 600);
    QCOMPARE(result.value(QStringLiteral("retained")).toInt(), ClientPerformanceMetrics::RetainedSamples);
    QCOMPARE(result.value(QStringLiteral("p50Ms")).toDouble(), 343.0 / 1000000.0);
}

void ClientPerformanceMetricsTests::concurrentRecordingKeepsCounts()
{
    ClientPerformanceMetrics metrics;
    QList<QThread*> workers;
    for (int i = 0; i < 4; ++i) {
        auto* worker = QThread::create([&] { for (int n = 0; n < 250; ++n) metrics.record(PerformanceStage::Rasterization, n); });
        workers.append(worker);
        worker->start();
    }
    for (auto* worker : workers) { QVERIFY(worker->wait(5000)); delete worker; }
    QCOMPARE(metrics.snapshot().value(QStringLiteral("rasterization")).toObject().value(QStringLiteral("count")).toInteger(), 1000);
}

void ClientPerformanceMetricsTests::scopeRecordsOnlyKnownStagesAndTiming()
{
    ClientPerformanceMetrics metrics;
    { PerformanceScope scope(PerformanceStage::Publication, metrics); }
    metrics.record(static_cast<PerformanceStage>(999), 3);
    metrics.record(PerformanceStage::Publication, -1);
    const auto result = metrics.snapshot();
    QCOMPARE(result.size(), static_cast<int>(PerformanceStage::Count));
    QCOMPARE(result.value(QStringLiteral("publication")).toObject().value(QStringLiteral("count")).toInteger(), 1);
    QCOMPARE(result.value(QStringLiteral("publication")).toObject().keys(),
        QStringList({QStringLiteral("count"), QStringLiteral("p50Ms"), QStringLiteral("p95Ms"), QStringLiteral("retained")}));
}

void ClientPerformanceMetricsTests::savingAndEnqueueAreMeasuredSeparately()
{
    ClientPerformanceMetrics metrics;
    for (int i = 0; i < 600; ++i) metrics.record(PerformanceStage::SaveEnqueue, 1000000);
    metrics.record(PerformanceStage::Saving, 9000000);
    const auto result = metrics.snapshot();
    QCOMPARE(result.value(QStringLiteral("saveEnqueue")).toObject().value(QStringLiteral("retained")).toInt(), 512);
    QCOMPARE(result.value(QStringLiteral("saveEnqueue")).toObject().value(QStringLiteral("p95Ms")).toDouble(), 1.0);
    QCOMPARE(result.value(QStringLiteral("saving")).toObject().value(QStringLiteral("p95Ms")).toDouble(), 9.0);
}

QTEST_GUILESS_MAIN(ClientPerformanceMetricsTests)
#include "tst_client_performance_metrics.moc"

#include "sleekpr/app/LatestPreviewQueue.h"
#include "sleekpr/infrastructure/preview/TemplatePreviewJob.h"
#include "sleekpr/infrastructure/diagnostics/ClientPerformanceMetrics.h"
#include <QJsonDocument>
#include <QApplication>
#include <QSignalSpy>
#include <QThread>
#include <QtTest>
#include <atomic>

using namespace sleekpr;

namespace {

infrastructure::TemplatePreviewSnapshot labelSnapshot()
{
    infrastructure::TemplatePreviewSnapshot result;
    result.resolved.templateKey = QStringLiteral("default");
    result.resolved.document.id = QStringLiteral("snapshot-label");
    core::TemplateElement text;
    text.id = QStringLiteral("text");
    text.type = core::TemplateElementType::FixedText;
    text.text = QStringLiteral("NATIVE SNAPSHOT");
    text.width = 40;
    text.height = 8;
    text.fontSizePt = 9;
    core::TemplateLayer layer;
    layer.id = QStringLiteral("base");
    layer.elements = {text};
    result.resolved.document.layers = {layer};
    result.resolved.deviceProfile.dpi = 203;
    return result;
}

} // 匿名命名空间

class TemplatePreviewJobTests final : public QObject
{
    Q_OBJECT
private slots:
    void jobPlansAndRendersQtImage();
    void jobRasterizesSelectedPageAndScaling();
    void jobRunsOnWorkerWithoutControls();
    void jobRecordsTimingsWithoutInputContent();
    void jobPropagatesImageBudgetAsResourceError();
    void jobPropagatesPlanningBudgetBeforeRaster();
};

void TemplatePreviewJobTests::jobPlansAndRendersQtImage()
{
    const auto result = infrastructure::TemplatePreviewJob::run(labelSnapshot());
    QVERIFY(result.errorCode.isEmpty());
    QVERIFY(result.plan.has_value());
    QCOMPARE(result.plan->renderDpi, 203.0);
    QVERIFY(!result.image.isNull());
    QCOMPARE(result.image.width(), qRound(80 * 203.0 / 25.4));
    QCOMPARE(result.image.height(), qRound(30 * 203.0 / 25.4));
    bool ink = false;
    for (int y = 0; y < result.image.height() && !ink; ++y) {
        for (int x = 0; x < result.image.width(); ++x) {
            if (result.image.pixelColor(x, y).lightness() < 128) { ink = true; break; }
        }
    }
    QVERIFY(ink);
}

void TemplatePreviewJobTests::jobRasterizesSelectedPageAndScaling()
{
    auto snapshot = labelSnapshot();
    core::NativePrintDrawingPlan plan;
    plan.paperSize = core::LabelPaperSize(40, 20);
    plan.renderDpi = 203;
    core::NativeDrawCommand rectangle;
    rectangle.type = core::NativeDrawCommandType::Rectangle;
    rectangle.x = 20;
    rectangle.y = 5;
    rectangle.width = 5;
    rectangle.height = 5;
    plan.pages = {{1, {}}, {2, {rectangle}}};
    snapshot.preparedPlan = plan;
    snapshot.pageIndex = 1;
    snapshot.zoom = 1.5;
    snapshot.devicePixelRatio = 2;
    const auto result = infrastructure::TemplatePreviewJob::run(snapshot);
    QVERIFY(result.plan.has_value());
    QVERIFY(!result.image.isNull());
    QCOMPARE(result.plan->renderDpi, 203.0);
    const double scale = 203 * 1.5 * 2 / 25.4;
    QCOMPARE(result.image.size(), QSize(qRound(40 * scale), qRound(20 * scale)));
    QCOMPARE(result.image.devicePixelRatio(), 2.0);
    bool edgeInk = false;
    // 非整数像素上的细线可能落在相邻像素，检查局部边框而不绑定 Qt 光栅器的取整方向。
    for (int y = qRound(5 * scale) - 2; y <= qRound(5 * scale) + 2; ++y) {
        for (int x = qRound(20 * scale) - 2; x <= qRound(20 * scale) + 2; ++x) {
            edgeInk = edgeInk || result.image.pixelColor(x, y).lightness() < 128;
        }
    }
    QVERIFY(edgeInk);
}

void TemplatePreviewJobTests::jobRunsOnWorkerWithoutControls()
{
    const auto snapshot = labelSnapshot();
    auto* guiThread = QThread::currentThread();
    std::atomic<bool> worker = false;
    app::LatestPreviewQueue queue(infrastructure::PreviewExecutor::applicationInstance());
    QSignalSpy spy(&queue, &app::LatestPreviewQueue::resultReady);
    queue.submit([snapshot, guiThread, &worker] {
        worker = QThread::currentThread() != guiThread;
        return infrastructure::TemplatePreviewJob::run(snapshot);
    });
    QTRY_COMPARE_WITH_TIMEOUT(spy.size(), 1, 5000);
    QVERIFY(worker.load());
    const auto result = qvariant_cast<infrastructure::PreviewWorkResult>(spy.first().last());
    QVERIFY(result.plan.has_value());
    QVERIFY(!result.image.isNull());
}

void TemplatePreviewJobTests::jobRecordsTimingsWithoutInputContent()
{
    const auto count = [](const char* stage) {
        return infrastructure::ClientPerformanceMetrics::applicationMetrics().snapshot().value(QString::fromLatin1(stage))
            .toObject().value(QStringLiteral("count")).toInteger();
    };
    const auto plans = count("planning");
    const auto rasters = count("rasterization");
    QVERIFY(infrastructure::TemplatePreviewJob::run(labelSnapshot()).plan.has_value());
    QCOMPARE(count("planning"), plans + 1);
    QCOMPARE(count("rasterization"), rasters + 1);
    QVERIFY(!QJsonDocument(infrastructure::ClientPerformanceMetrics::applicationMetrics().snapshot()).toJson().contains("NATIVE SNAPSHOT"));
}

void TemplatePreviewJobTests::jobPropagatesImageBudgetAsResourceError()
{
    auto snapshot = labelSnapshot();
    snapshot.zoom = 1000;
    const auto result = infrastructure::TemplatePreviewJob::run(snapshot);
    QCOMPARE(result.errorCode, QStringLiteral("REQUEST_TOO_LARGE"));
    QVERIFY(result.image.isNull());
}

void TemplatePreviewJobTests::jobPropagatesPlanningBudgetBeforeRaster()
{
    auto snapshot = labelSnapshot();
    snapshot.planningBudget.maxCommands = 0;
    const auto result = infrastructure::TemplatePreviewJob::run(snapshot);
    QCOMPARE(result.errorCode, QStringLiteral("REQUEST_TOO_LARGE"));
    QVERIFY(!result.plan.has_value());
    QVERIFY(result.image.isNull());
}

int main(int argc, char** argv)
{
    QApplication application(argc, argv);
    infrastructure::PreviewExecutor executor;
    TemplatePreviewJobTests tests;
    return QTest::qExec(&tests, argc, argv);
}

#include "tst_template_preview_job.moc"

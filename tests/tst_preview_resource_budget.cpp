#include "sleekpr/core/printing/PreviewResourceBudget.h"
#include <QtTest>
#include <limits>

using namespace sleekpr::core;

class PreviewResourceBudgetTests final : public QObject
{
    Q_OBJECT
private slots:
    void rejectsSideBeforeAllocation();
    void rejectsPixelProductBeforeAllocation();
    void rejectsNonFiniteGeometry();
    void includesZoomAndDpr();
    void acceptsExactBudgetBoundary();
    void rejectsImageBytesBeforeAllocation();
    void rejectsUnrepresentableResolutionMetadata();
    void planningCountersAccumulateWithoutOverflow();
    void planningCellEstimateDoesNotConsumeTwice();
};

void PreviewResourceBudgetTests::rejectsSideBeforeAllocation()
{
    const PreviewResourceBudget budget;
    QCOMPARE(budget.maxSidePixels, 16384);
    QCOMPARE(budget.maxPixels, quint64(64000000));
    QCOMPARE(checkPreviewImageRequest({QSizeF(16385, 1), 25.4}).error, PreviewImageError::ResourceLimit);
}

void PreviewResourceBudgetTests::rejectsPixelProductBeforeAllocation()
{
    QCOMPARE(checkPreviewImageRequest({QSizeF(9000, 9000), 25.4}).error, PreviewImageError::ResourceLimit);
}

void PreviewResourceBudgetTests::rejectsNonFiniteGeometry()
{
    const auto nan = std::numeric_limits<double>::quiet_NaN();
    const auto inf = std::numeric_limits<double>::infinity();
    for (const auto size : {QSizeF(nan, 30), QSizeF(80, inf), QSizeF(0, 30), QSizeF(80, -1)}) {
        QCOMPARE(checkPreviewImageRequest({size, 203}).error, PreviewImageError::InvalidInput);
    }
    QCOMPARE(checkPreviewImageRequest({QSizeF(80, 30), nan}).error, PreviewImageError::InvalidInput);
    QCOMPARE(checkPreviewImageRequest({QSizeF(80, 30), 203, inf}).error, PreviewImageError::InvalidInput);
    QCOMPARE(checkPreviewImageRequest({QSizeF(80, 30), 203, 1, 0}).error, PreviewImageError::InvalidInput);
}

void PreviewResourceBudgetTests::includesZoomAndDpr()
{
    const auto result = checkPreviewImageRequest({QSizeF(80, 30), 25.4, 1.5, 2});
    QVERIFY(result.ok());
    QCOMPARE(result.pixelSize, QSize(240, 90));
    QCOMPARE(result.rasterDpi, 76.2);
    QCOMPARE(checkPreviewImageRequest({QSizeF(8000, 30), 25.4, 2, 2}).error, PreviewImageError::ResourceLimit);
}

void PreviewResourceBudgetTests::acceptsExactBudgetBoundary()
{
    // 边界只计算预算，不分配 6400 万像素的真实图像。
    const auto result = checkPreviewImageRequest({QSizeF(8000, 8000), 25.4});
    QVERIFY(result.ok());
    QCOMPARE(result.pixelSize, QSize(8000, 8000));
    const auto side = checkPreviewImageRequest({QSizeF(16384, 1), 25.4});
    QVERIFY(side.ok());
    QCOMPARE(side.pixelSize, QSize(16384, 1));
}

void PreviewResourceBudgetTests::rejectsImageBytesBeforeAllocation()
{
    PreviewResourceBudget budget;
    budget.maxImageBytes = 399;
    QCOMPARE(checkPreviewImageRequest({QSizeF(10, 10), 25.4}, budget).error, PreviewImageError::ResourceLimit);
    budget.maxImageBytes = 400;
    QVERIFY(checkPreviewImageRequest({QSizeF(10, 10), 25.4}, budget).ok());
}

void PreviewResourceBudgetTests::rejectsUnrepresentableResolutionMetadata()
{
    // 即使极小纸张的像素数不大，DPM 元数据也不能溢出 int。
    QCOMPARE(checkPreviewImageRequest({QSizeF(0.00000001, 0.00000001), 1e12}).error, PreviewImageError::ResourceLimit);
}

void PreviewResourceBudgetTests::planningCountersAccumulateWithoutOverflow()
{
    PreviewPlanningBudget budget;
    budget.maxPages = 2;
    QVERIFY(budget.consumePages(1));
    QVERIFY(budget.consumePages(1));
    QVERIFY(!budget.consumePages(1));
    QCOMPARE(budget.exceeded, PreviewPlanningLimit::Pages);
    QCOMPARE(budget.usedPages, 2);
    PreviewPlanningBudget commands;
    QVERIFY(commands.consumeCommands(199999));
    QVERIFY(!commands.consumeCommands(std::numeric_limits<qsizetype>::max()));
    QCOMPARE(commands.exceeded, PreviewPlanningLimit::Commands);
    QCOMPARE(commands.usedCommands, 199999);
}

void PreviewResourceBudgetTests::planningCellEstimateDoesNotConsumeTwice()
{
    PreviewPlanningBudget budget;
    budget.maxCells = 3;
    QVERIFY(budget.ensureCells(3));
    QCOMPARE(budget.usedCells, 0);
    QVERIFY(budget.consumeCells(2));
    QVERIFY(!budget.ensureCells(2));
    QCOMPARE(budget.exceeded, PreviewPlanningLimit::Cells);
}

QTEST_GUILESS_MAIN(PreviewResourceBudgetTests)
#include "tst_preview_resource_budget.moc"

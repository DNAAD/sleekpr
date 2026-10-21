#include "sleekpr/infrastructure/preview/LabelPreviewImageRenderer.h"
#include <QIODevice>
#include <QtTest>

using namespace sleekpr;

namespace {

core::NativeLabelDrawingPlan squarePlan()
{
    core::NativeLabelDrawingPlan plan;
    plan.paperSize = core::LabelPaperSize(8, 8);
    core::NativeDrawCommand rectangle;
    rectangle.type = core::NativeDrawCommandType::Rectangle;
    rectangle.x = 1;
    rectangle.y = 1;
    rectangle.width = 4;
    rectangle.height = 4;
    rectangle.backgroundColor = QStringLiteral("#202020");
    plan.commands = {rectangle};
    return plan;
}

} // 匿名命名空间

class PreviewImageRendererTests final : public QObject
{
    Q_OBJECT
private slots:
    void budgetRejectsBeforeAllocator();
    void typedImageIncludesZoomAndDprOnce();
    void allocationFailureIsStructured();
    void pngEncodingFailureIsStructured();
    void pngEncoderCannotReturnEmptySuccess();
    void pngOutputIsBoundedDuringWriting();
    void pngAcceptsExactEncodedBoundary();
    void legacyWrapperReturnsEmptyForRejectedImage();
};

void PreviewImageRendererTests::budgetRejectsBeforeAllocator()
{
    int allocations = 0;
    infrastructure::LabelPreviewImageRenderer renderer({}, [&](const QSize& size) {
        ++allocations;
        return QImage(size, QImage::Format_ARGB32_Premultiplied);
    });
    const auto result = renderer.tryRenderImage(squarePlan(), {QSizeF(20000, 1), 25.4});
    QCOMPARE(result.error, core::PreviewImageError::ResourceLimit);
    QCOMPARE(allocations, 0);
    QVERIFY(result.image.isNull());
}

void PreviewImageRendererTests::typedImageIncludesZoomAndDprOnce()
{
    const auto result = infrastructure::LabelPreviewImageRenderer().tryRenderImage(squarePlan(), {QSizeF(8, 8), 25.4, 2, 2});
    QVERIFY(result.ok());
    QCOMPARE(result.image.size(), QSize(32, 32));
    QCOMPARE(result.image.devicePixelRatio(), 2.0);
    QCOMPARE(result.image.pixelColor(8, 8), QColor(QStringLiteral("#202020")));
    QCOMPARE(result.image.pixelColor(1, 1), QColor(Qt::white));
}

void PreviewImageRendererTests::allocationFailureIsStructured()
{
    infrastructure::LabelPreviewImageRenderer renderer({}, [](const QSize&) { return QImage{}; });
    const auto result = renderer.tryRenderImage(squarePlan(), {QSizeF(8, 8), 25.4});
    QCOMPARE(result.error, core::PreviewImageError::AllocationFailed);
    QVERIFY(!result.errorMessage.isEmpty());
}

void PreviewImageRendererTests::pngEncodingFailureIsStructured()
{
    infrastructure::LabelPreviewImageRenderer renderer([](const QImage&, QIODevice*) { return false; });
    const auto result = renderer.tryRenderPng(squarePlan(), {QSizeF(8, 8), 25.4}, {}, 10000);
    QCOMPARE(result.error, core::PreviewImageError::EncodeFailed);
    QVERIFY(result.png.isEmpty());
}

void PreviewImageRendererTests::pngEncoderCannotReturnEmptySuccess()
{
    infrastructure::LabelPreviewImageRenderer renderer([](const QImage&, QIODevice*) { return true; });
    QCOMPARE(renderer.tryRenderPng(squarePlan(), {QSizeF(8, 8), 25.4}, {}, 10000).error, core::PreviewImageError::EncodeFailed);
}

void PreviewImageRendererTests::pngOutputIsBoundedDuringWriting()
{
    bool rejected = false;
    infrastructure::LabelPreviewImageRenderer renderer([&](const QImage&, QIODevice* device) {
        rejected = device->write(QByteArray(1024, 'x')) < 0;
        return true;
    });
    const auto result = renderer.tryRenderPng(squarePlan(), {QSizeF(8, 8), 25.4}, {}, 10);
    QCOMPARE(result.error, core::PreviewImageError::ResourceLimit);
    QVERIFY(rejected);
    QVERIFY(result.png.isEmpty());
}

void PreviewImageRendererTests::pngAcceptsExactEncodedBoundary()
{
    infrastructure::LabelPreviewImageRenderer renderer;
    const auto initial = renderer.tryRenderPng(squarePlan(), {QSizeF(8, 8), 25.4}, {}, 10000);
    QVERIFY(initial.ok());
    QVERIFY(!initial.png.isEmpty());
    QVERIFY(renderer.tryRenderPng(squarePlan(), {QSizeF(8, 8), 25.4}, {}, initial.png.size()).ok());
    QCOMPARE(renderer.tryRenderPng(squarePlan(), {QSizeF(8, 8), 25.4}, {}, initial.png.size() - 1).error, core::PreviewImageError::ResourceLimit);
}

void PreviewImageRendererTests::legacyWrapperReturnsEmptyForRejectedImage()
{
    auto plan = squarePlan();
    plan.paperSize = core::LabelPaperSize(16385, 1);
    infrastructure::LabelPreviewImageRenderer renderer;
    QVERIFY(renderer.renderImage(plan, 25.4).isNull());
    QVERIFY(renderer.renderPng(plan, 25.4).isEmpty());
}

QTEST_MAIN(PreviewImageRendererTests)
#include "tst_preview_image_renderer.moc"

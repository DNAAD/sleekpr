#include "sleekpr/app/AppTheme.h"
#include "sleekpr/app/PrintPreviewPanel.h"
#include "sleekpr/core/native/NativePlanJsonSerializer.h"
#include "sleekpr/core/templates/TemplateDocumentRenderer.h"

#include <QComboBox>
#include <QCoreApplication>
#include <QFontMetrics>
#include <QImage>
#include <QJsonArray>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QtTest>

using namespace sleekpr;

namespace {

core::TemplateDocument locatorDocument()
{
    core::TemplateDocument document;
    core::TemplateLayer layer;
    layer.id = QStringLiteral("main");
    core::TemplateElement first;
    first.id = QStringLiteral("first");
    first.type = core::TemplateElementType::BoundField;
    first.fieldKey = QStringLiteral("shared");
    first.x = 2;
    first.y = 2;
    first.width = 10;
    first.height = 4;
    auto second = first;
    second.id = QStringLiteral("second");
    second.x = 18;
    second.y = 3;
    second.width = 8;
    second.height = 5;
    layer.elements = {first, second};
    document.layers = {layer};
    return document;
}

core::NativeDrawCommand rectangle(const QString& key, double x, double y, double width, double height)
{
    core::NativeDrawCommand command;
    command.type = core::NativeDrawCommandType::Rectangle;
    command.elementKey = key;
    command.x = x;
    command.y = y;
    command.width = width;
    command.height = height;
    return command;
}

core::NativePrintDrawingPlan calibratedPlan()
{
    core::NativePrintDrawingPlan plan;
    // 故意与模板原坐标分离，防止定位框忽略已校准的正式绘制命令。
    plan.pages = {{1, {
        rectangle(QStringLiteral("first"), 30, 12, 9, 4),
        rectangle(QStringLiteral("second"), 52, 18, 8, 5),
    }}};
    return plan;
}

QLabel* previewImage(app::PrintPreviewPanel& panel)
{
    return panel.findChild<QLabel*>(QStringLiteral("workbenchPreviewImage"));
}

QJsonArray serializedPages(const core::NativePrintDrawingPlan& plan)
{
    QJsonArray result;
    for (const auto& page : plan.pages) {
        auto label = plan.firstPageAsLabelPlan();
        label.commands = page.commands;
        result.append(core::NativePlanJsonSerializer().toJson(label));
    }
    return result;
}

QImage paintedImage(QLabel& image)
{
    // 抓取控件实际绘制结果，不能只验证状态属性而漏掉没有画出来的叠加层。
    return image.grab().toImage().convertToFormat(QImage::Format_ARGB32);
}

QPoint paperPoint(const QLabel& image, const QImage& capture, const QPointF& millimeters,
    const QSizeF& paperMm = QSizeF(80, 30))
{
    const auto pixmap = image.pixmap();
    const auto displayedSize = pixmap.isNull() ? QSizeF(image.size()) : pixmap.deviceIndependentSize();
    const QPointF origin((image.width() - displayedSize.width()) / 2.0,
        (image.height() - displayedSize.height()) / 2.0);
    // 分别使用实际位图宽高和 DPR，不假设纸张恰好被相同的整数像素比例整除。
    const QPointF logical(origin.x() + millimeters.x() * displayedSize.width() / paperMm.width(),
        origin.y() + millimeters.y() * displayedSize.height() / paperMm.height());
    return QPoint(qRound(logical.x() * capture.devicePixelRatio()),
        qRound(logical.y() * capture.devicePixelRatio()));
}

bool changedNear(const QImage& before, const QImage& after, const QPoint& point, int radius = 4)
{
    if (before.size() != after.size()) return false;
    const auto area = QRect(point - QPoint(radius, radius), QSize(radius * 2 + 1, radius * 2 + 1))
        .intersected(before.rect());
    for (int y = area.top(); y <= area.bottom(); ++y) {
        for (int x = area.left(); x <= area.right(); ++x) {
            if (before.pixel(x, y) != after.pixel(x, y)) return true;
        }
    }
    return false;
}

bool hasInkNear(const QImage& image, const QPoint& point, int radius = 3)
{
    const auto area = QRect(point - QPoint(radius, radius), QSize(radius * 2 + 1, radius * 2 + 1))
        .intersected(image.rect());
    for (int y = area.top(); y <= area.bottom(); ++y) {
        for (int x = area.left(); x <= area.right(); ++x) {
            const auto color = image.pixelColor(x, y);
            if (color.red() < 245 || color.green() < 245 || color.blue() < 245) return true;
        }
    }
    return false;
}

bool hasDiagnosticTitle(const app::PrintPreviewPanel& panel)
{
    for (const auto* label : panel.findChildren<QLabel*>()) {
        if (label->isVisible() && label->text() == QString::fromUtf8("版式定位（非打印预览）")) return true;
    }
    return false;
}

} // 匿名命名空间

class PrintPreviewTests final : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase() { app::applySleekprTheme(*qApp); }
    void highlightsAllTargetsWithoutChangingPlanOrPixmap();
    void highlightsUseRendererCalibratedCoordinates();
    void arrayGridHighlightsEveryCommandWithoutPrefixCollision();
    void rotatedCommandUsesItsBoundingShape_data();
    void rotatedCommandUsesItsBoundingShape();
    void highlightTracksZoomAndWindowResize();
    void locatingEnsuresTargetIsVisible();
    void firstPageHighlightDoesNotLeakOntoOtherPages();
    void setPlanClearsPreviousTargets();
    void emptyElementFallbackScalesAfterOffset();
    void diagnosticIsNotPrintPreviewAndNeverUsesOldData();
    void diagnosticUsesOnlyCurrentTemplate();
    void diagnosticFallbackScalesAfterOffset();
    void clearHighlightRetainsDiagnosticUntilClear();
    void fullyOutsideElementShowsPaperEdgeIndicator_data();
    void fullyOutsideElementShowsPaperEdgeIndicator();
    void diagnosticTitleRemainsFullyVisibleInNarrowPanel_data();
    void diagnosticTitleRemainsFullyVisibleInNarrowPanel();
};

void PrintPreviewTests::highlightsAllTargetsWithoutChangingPlanOrPixmap()
{
    app::PrintPreviewPanel panel;
    const auto plan = calibratedPlan();
    const auto original = serializedPages(plan);
    panel.resize(720, 360);
    panel.show();
    QVERIFY(QTest::qWaitForWindowExposed(&panel));
    panel.setPlan(plan);
    QTest::qWait(30);
    auto* image = previewImage(panel);
    QVERIFY(image != nullptr && !image->pixmap().isNull());
    const auto baseline = paintedImage(*image);
    const auto cacheKey = image->pixmap().cacheKey();

    panel.setElementHighlight(locatorDocument(), {QStringLiteral("first"), QStringLiteral("second")}, 7, 3, 1.4, 0.8);

    QCOMPARE(panel.property("printPreflightHighlightCount").toInt(), 2);
    QVERIFY(panel.property("printPreflightDiagnostic").isValid());
    QVERIFY(!panel.property("printPreflightDiagnostic").toBool());
    const auto highlighted = paintedImage(*image);
    QVERIFY(changedNear(baseline, highlighted, paperPoint(*image, highlighted, QPointF(30, 14))));
    QVERIFY(changedNear(baseline, highlighted, paperPoint(*image, highlighted, QPointF(52, 20))));
    QVERIFY(!changedNear(baseline, highlighted, paperPoint(*image, highlighted, QPointF(2, 4))));
    QCOMPARE(image->pixmap().cacheKey(), cacheKey);
    QCOMPARE(serializedPages(plan), original);

    panel.setElementHighlight(locatorDocument(), {QStringLiteral("first"), QStringLiteral("second")});
    QTest::qWait(20);
    QCOMPARE(image->pixmap().cacheKey(), cacheKey);
    panel.clearElementHighlight();
    QCOMPARE(panel.property("printPreflightHighlightCount").toInt(), 0);
    QCOMPARE(image->pixmap().cacheKey(), cacheKey);
    QCOMPARE(paintedImage(*image), baseline);
}

void PrintPreviewTests::highlightsUseRendererCalibratedCoordinates()
{
    auto document = locatorDocument();
    document.layers[0].elements[0].x = 4;
    document.layers[0].elements[0].y = 3;
    core::DeviceProfile profile;
    profile.offsetX = 2;
    profile.offsetY = 1;
    profile.scaleX = 2;
    profile.scaleY = 1.5;
    const auto plan = core::TemplateDocumentRenderer().renderPrint(document, {}, {1, 2}, profile, {});
    QVERIFY(!plan.pages.isEmpty() && !plan.pages.first().commands.isEmpty());
    const auto& command = plan.pages.first().commands.first();
    QCOMPARE(command.x, 14.0);
    QCOMPARE(command.y, 9.0);
    QCOMPARE(command.width, 20.0);
    QCOMPARE(command.height, 6.0);
    app::PrintPreviewPanel panel;
    panel.resize(720, 360);
    panel.show();
    QVERIFY(QTest::qWaitForWindowExposed(&panel));
    panel.setPlan(plan);
    QTest::qWait(30);
    auto* image = previewImage(panel);
    QVERIFY(image != nullptr);
    const auto baseline = paintedImage(*image);
    const auto cacheKey = image->pixmap().cacheKey();

    panel.setElementHighlight(document, {QStringLiteral("first")}, 3, 3, 2, 1.5);

    const auto highlighted = paintedImage(*image);
    QVERIFY(changedNear(baseline, highlighted, paperPoint(*image, highlighted, QPointF(14, 12))));
    QVERIFY(changedNear(baseline, highlighted, paperPoint(*image, highlighted, QPointF(34, 12))));
    QVERIFY(!changedNear(baseline, highlighted, paperPoint(*image, highlighted, QPointF(4, 5))));
    QCOMPARE(image->pixmap().cacheKey(), cacheKey);
}

void PrintPreviewTests::arrayGridHighlightsEveryCommandWithoutPrefixCollision()
{
    auto document = locatorDocument();
    auto grid = document.layers[0].elements[0];
    grid.id = QStringLiteral("grid");
    grid.type = core::TemplateElementType::ArrayGrid;
    grid.x = 4;
    grid.y = 4;
    grid.width = 24;
    grid.height = 12;
    grid.dataPath = QStringLiteral("items");
    grid.arrayGridRows = 1;
    grid.arrayGridColumns = 2;
    grid.arrayGridCellTemplate = QStringLiteral("${text}");
    document.layers[0].elements = {grid};
    core::DeviceProfile profile;
    profile.offsetX = 3;
    profile.offsetY = 2;
    profile.scaleX = 1.5;
    core::TemplateRenderContext context;
    context.values = {{QStringLiteral("items"), QJsonArray{
        QJsonObject{{QStringLiteral("text"), QStringLiteral("A")}},
        QJsonObject{{QStringLiteral("text"), QStringLiteral("B")}},
    }}};
    auto plan = core::TemplateDocumentRenderer().renderPrint(document, {}, {}, profile, context);
    QCOMPARE(plan.pages.first().commands.size(), 4);
    plan.pages[0].commands.append(rectangle(QStringLiteral("grid-other.cell0.border"), 65, 4, 8, 4));
    app::PrintPreviewPanel panel;
    panel.resize(720, 360);
    panel.show();
    QVERIFY(QTest::qWaitForWindowExposed(&panel));
    panel.setPlan(plan);
    QTest::qWait(30);
    auto* image = previewImage(panel);
    QVERIFY(image != nullptr);
    const auto baseline = paintedImage(*image);
    const auto cacheKey = image->pixmap().cacheKey();

    panel.setElementHighlight(document, {QStringLiteral("grid")}, 3, 2, 1.5, 1);

    QVERIFY(panel.property("printPreflightHighlightCount").toInt() > 0);
    const auto highlighted = paintedImage(*image);
    QVERIFY(changedNear(baseline, highlighted, paperPoint(*image, highlighted, QPointF(10.5, 12))));
    QVERIFY(changedNear(baseline, highlighted, paperPoint(*image, highlighted, QPointF(46.5, 12))));
    // 两个有内边距的文本命令也必须定位，不能只匹配网格的第一条边框命令。
    QVERIFY(changedNear(baseline, highlighted, paperPoint(*image, highlighted, QPointF(29.4, 9))));
    QVERIFY(!changedNear(baseline, highlighted, paperPoint(*image, highlighted, QPointF(65, 6))));
    QCOMPARE(image->pixmap().cacheKey(), cacheKey);
}

void PrintPreviewTests::rotatedCommandUsesItsBoundingShape_data()
{
    QTest::addColumn<double>("rotation");
    QTest::addColumn<QPointF>("edge");
    QTest::newRow("right-angle") << 90.0 << QPointF(34, 8);
    QTest::newRow("oblique") << 45.0 << QPointF(34.585786, 10.585786);
}

void PrintPreviewTests::rotatedCommandUsesItsBoundingShape()
{
    QFETCH(double, rotation);
    QFETCH(QPointF, edge);
    auto plan = calibratedPlan();
    auto& command = plan.pages[0].commands[0];
    command.x = 30;
    command.y = 10;
    command.width = 12;
    command.height = 4;
    command.rotationDegrees = rotation;
    app::PrintPreviewPanel panel;
    panel.resize(720, 360);
    panel.show();
    QVERIFY(QTest::qWaitForWindowExposed(&panel));
    panel.setPlan(plan);
    QTest::qWait(30);
    auto* image = previewImage(panel);
    QVERIFY(image != nullptr);
    const auto baseline = paintedImage(*image);

    panel.setElementHighlight(locatorDocument(), {QStringLiteral("first")});

    const auto highlighted = paintedImage(*image);
    QVERIFY(changedNear(baseline, highlighted, paperPoint(*image, highlighted, edge)));
    QVERIFY(!changedNear(baseline, highlighted, paperPoint(*image, highlighted, QPointF(30, 10))));
}

void PrintPreviewTests::highlightTracksZoomAndWindowResize()
{
    app::PrintPreviewPanel panel;
    panel.resize(720, 360);
    panel.show();
    QVERIFY(QTest::qWaitForWindowExposed(&panel));
    panel.setPlan(calibratedPlan());
    QTest::qWait(30);
    auto* image = previewImage(panel);
    auto* zoom = panel.findChild<QComboBox*>(QStringLiteral("workbenchPreviewZoom"));
    QVERIFY(image != nullptr && zoom != nullptr);
    const auto initialSize = image->pixmap().size();
    panel.setElementHighlight(locatorDocument(), {QStringLiteral("first")});
    zoom->setCurrentIndex(zoom->findData(2.0));
    QTRY_VERIFY(image->pixmap().size() != initialSize);
    QTest::qWait(30);
    auto highlighted = paintedImage(*image);
    auto cacheKey = image->pixmap().cacheKey();
    panel.clearElementHighlight();
    auto baseline = paintedImage(*image);
    QVERIFY(changedNear(baseline, highlighted, paperPoint(*image, highlighted, QPointF(30, 14))));
    QCOMPARE(image->pixmap().cacheKey(), cacheKey);

    panel.setElementHighlight(locatorDocument(), {QStringLiteral("first")});
    zoom->setCurrentIndex(0);
    QTest::qWait(30);
    const auto beforeResize = image->pixmap().size();
    panel.resize(540, 420);
    QTRY_VERIFY(image->pixmap().size() != beforeResize);
    QTest::qWait(30);
    QCOMPARE(panel.property("printPreflightHighlightCount").toInt(), 1);
    highlighted = paintedImage(*image);
    cacheKey = image->pixmap().cacheKey();
    panel.clearElementHighlight();
    baseline = paintedImage(*image);
    QVERIFY(changedNear(baseline, highlighted, paperPoint(*image, highlighted, QPointF(39, 14))));
    QCOMPARE(image->pixmap().cacheKey(), cacheKey);
}

void PrintPreviewTests::locatingEnsuresTargetIsVisible()
{
    auto plan = calibratedPlan();
    plan.pages[0].commands = {rectangle(QStringLiteral("first"), 72, 24, 5, 4)};
    app::PrintPreviewPanel panel;
    panel.resize(440, 240);
    panel.show();
    QVERIFY(QTest::qWaitForWindowExposed(&panel));
    panel.setPlan(plan);
    auto* image = previewImage(panel);
    auto* zoom = panel.findChild<QComboBox*>(QStringLiteral("workbenchPreviewZoom"));
    auto* scroll = panel.findChild<QScrollArea*>(QStringLiteral("workbenchPreviewScrollArea"));
    QVERIFY(image != nullptr && zoom != nullptr && scroll != nullptr);
    zoom->setCurrentIndex(zoom->findData(2.0));
    QTest::qWait(30);
    scroll->horizontalScrollBar()->setValue(0);
    scroll->verticalScrollBar()->setValue(0);
    const auto displayed = image->pixmap().deviceIndependentSize();
    const QPoint center(qRound(displayed.width() * 74.5 / 80), qRound(displayed.height() * 26 / 30));
    QVERIFY(!scroll->viewport()->rect().contains(image->mapTo(scroll->viewport(), center)));

    panel.setElementHighlight(locatorDocument(), {QStringLiteral("first")});

    QTRY_VERIFY(scroll->viewport()->rect().contains(image->mapTo(scroll->viewport(), center)));
}

void PrintPreviewTests::firstPageHighlightDoesNotLeakOntoOtherPages()
{
    auto plan = calibratedPlan();
    plan.pages.append({2, {rectangle(QStringLiteral("first"), 5, 5, 9, 4)}});
    app::PrintPreviewPanel panel;
    panel.resize(720, 360);
    panel.show();
    QVERIFY(QTest::qWaitForWindowExposed(&panel));
    panel.setPlan(plan);
    QTest::qWait(30);
    auto* image = previewImage(panel);
    auto* next = panel.findChild<QPushButton*>(QStringLiteral("workbenchPageNextButton"));
    auto* previous = panel.findChild<QPushButton*>(QStringLiteral("workbenchPagePreviousButton"));
    QVERIFY(image != nullptr && next != nullptr && previous != nullptr);
    panel.setElementHighlight(locatorDocument(), {QStringLiteral("first")});

    next->click();
    QTest::qWait(20);
    QVERIFY(previous->isEnabled() && !next->isEnabled());
    const auto secondPage = paintedImage(*image);
    const auto cacheKey = image->pixmap().cacheKey();
    panel.clearElementHighlight();
    QCOMPARE(paintedImage(*image), secondPage);
    QCOMPARE(image->pixmap().cacheKey(), cacheKey);

    panel.setElementHighlight(locatorDocument(), {QStringLiteral("first")});
    QTRY_VERIFY(!previous->isEnabled() && next->isEnabled());
    QCOMPARE(panel.property("printPreflightHighlightCount").toInt(), 1);
}

void PrintPreviewTests::setPlanClearsPreviousTargets()
{
    app::PrintPreviewPanel panel;
    panel.resize(720, 360);
    panel.show();
    QVERIFY(QTest::qWaitForWindowExposed(&panel));
    panel.setPlan(calibratedPlan());
    QTest::qWait(30);
    auto* image = previewImage(panel);
    QVERIFY(image != nullptr);
    const auto baseline = paintedImage(*image);
    panel.setElementHighlight(locatorDocument(), {QStringLiteral("first")});
    QCOMPARE(panel.property("printPreflightHighlightCount").toInt(), 1);

    panel.setPlan(calibratedPlan());

    QCOMPARE(panel.property("printPreflightHighlightCount").toInt(), 0);
    QVERIFY(!panel.property("printPreflightDiagnostic").toBool());
    QCOMPARE(paintedImage(*image), baseline);
}

void PrintPreviewTests::emptyElementFallbackScalesAfterOffset()
{
    auto plan = calibratedPlan();
    plan.pages[0].commands = {rectangle(QStringLiteral("unrelated"), 65, 20, 5, 4)};
    app::PrintPreviewPanel panel;
    panel.resize(720, 360);
    panel.show();
    QVERIFY(QTest::qWaitForWindowExposed(&panel));
    panel.setPlan(plan);
    QTest::qWait(30);
    auto* image = previewImage(panel);
    QVERIFY(image != nullptr);
    const auto baseline = paintedImage(*image);
    const auto cacheKey = image->pixmap().cacheKey();

    panel.setElementHighlight(locatorDocument(), {QStringLiteral("first")}, 3, 1, 2, 1.5);

    QCOMPARE(panel.property("printPreflightHighlightCount").toInt(), 1);
    const auto highlighted = paintedImage(*image);
    // (2 + 3) * 2 = 10，(2 + 1) * 1.5 = 4.5，尺寸为 20 x 6 毫米。
    QVERIFY(changedNear(baseline, highlighted, paperPoint(*image, highlighted, QPointF(10, 7.5))));
    QVERIFY(changedNear(baseline, highlighted, paperPoint(*image, highlighted, QPointF(30, 7.5))));
    QVERIFY(!changedNear(baseline, highlighted, paperPoint(*image, highlighted, QPointF(7, 7))));
    QCOMPARE(image->pixmap().cacheKey(), cacheKey);
}

void PrintPreviewTests::diagnosticIsNotPrintPreviewAndNeverUsesOldData()
{
    app::PrintPreviewPanel panel;
    panel.resize(720, 360);
    panel.show();
    QVERIFY(QTest::qWaitForWindowExposed(&panel));
    auto plan = calibratedPlan();
    auto blackPage = rectangle(QStringLiteral("old-business-data"), 0, 0, 80, 30);
    blackPage.backgroundColor = QStringLiteral("#000000");
    plan.pages[0].commands = {blackPage};
    panel.setPlan(plan);
    panel.setElementHighlight(locatorDocument(), {QStringLiteral("first")});
    auto document = locatorDocument();
    document.layers[0].elements[0].type = core::TemplateElementType::FixedText;
    document.layers[0].elements[0].text = QStringLiteral("DO-NOT-RENDER-DATA");
    document.layers[0].elements[1].type = core::TemplateElementType::QrCode;
    document.layers[0].elements[1].payload = QStringLiteral("https://example.test/private");

    panel.setDiagnosticDocument(document, QSizeF(80, 30));
    QTest::qWait(30);

    auto* image = previewImage(panel);
    QVERIFY(image != nullptr);
    QVERIFY(panel.property("printPreflightDiagnostic").toBool());
    QCOMPARE(panel.property("printPreflightHighlightCount").toInt(), 0);
    QVERIFY(hasDiagnosticTitle(panel));
    const auto diagnostic = paintedImage(*image);
    QVERIFY(!diagnostic.isNull());
    const auto interior = diagnostic.pixelColor(paperPoint(*image, diagnostic, QPointF(40, 25)));
    QVERIFY(interior.red() > 230 && interior.green() > 230 && interior.blue() > 230);
    // 相同版式只改正文和二维码载荷，诊断图必须逐像素相同，不能偷偷渲染样例或业务数据。
    document.layers[0].elements[0].text = QStringLiteral("OTHER-PRIVATE-TEXT");
    document.layers[0].elements[1].payload = QStringLiteral("different-qr-data");
    document.sampleData = {{QStringLiteral("shared"), QStringLiteral("NEVER-USE-SAMPLE")}};
    panel.setDiagnosticDocument(document, QSizeF(80, 30));
    QTest::qWait(20);
    QCOMPARE(paintedImage(*image), diagnostic);

    panel.setPlan(calibratedPlan());
    QVERIFY(!panel.property("printPreflightDiagnostic").toBool());
    QVERIFY(!hasDiagnosticTitle(panel));
}

void PrintPreviewTests::diagnosticUsesOnlyCurrentTemplate()
{
    app::PrintPreviewPanel panel;
    panel.resize(720, 360);
    panel.show();
    QVERIFY(QTest::qWaitForWindowExposed(&panel));
    auto document = locatorDocument();
    document.layers[0].elements.removeLast();
    panel.setDiagnosticDocument(document, QSizeF(80, 30));
    QTest::qWait(30);
    auto* image = previewImage(panel);
    QVERIFY(image != nullptr);
    const auto oldDocument = paintedImage(*image);
    document.layers[0].elements[0].x = 50;
    document.layers[0].elements[0].y = 18;

    panel.setDiagnosticDocument(document, QSizeF(80, 30));
    QTest::qWait(20);

    const auto current = paintedImage(*image);
    QVERIFY(changedNear(oldDocument, current, paperPoint(*image, current, QPointF(2, 4))));
    QVERIFY(changedNear(oldDocument, current, paperPoint(*image, current, QPointF(50, 20))));
    QVERIFY(panel.property("printPreflightDiagnostic").toBool());
}

void PrintPreviewTests::diagnosticFallbackScalesAfterOffset()
{
    app::PrintPreviewPanel panel;
    panel.resize(720, 360);
    panel.show();
    QVERIFY(QTest::qWaitForWindowExposed(&panel));
    panel.setDiagnosticDocument(locatorDocument(), QSizeF(80, 30), 3, 1, 2, 1.5);
    QTest::qWait(30);
    auto* image = previewImage(panel);
    QVERIFY(image != nullptr);
    const auto baseline = paintedImage(*image);
    // 诊断底图本身也必须先偏移后缩放，不能只有叠加框采用正确的变换顺序。
    QVERIFY(hasInkNear(baseline, paperPoint(*image, baseline, QPointF(10, 7.5))));
    QVERIFY(hasInkNear(baseline, paperPoint(*image, baseline, QPointF(30, 7.5))));
    QVERIFY(!hasInkNear(baseline, paperPoint(*image, baseline, QPointF(7, 7))));

    panel.setElementHighlight(locatorDocument(), {QStringLiteral("first")}, 3, 1, 2, 1.5);

    const auto highlighted = paintedImage(*image);
    QVERIFY(panel.property("printPreflightDiagnostic").toBool());
    QCOMPARE(panel.property("printPreflightHighlightCount").toInt(), 1);
    QVERIFY(changedNear(baseline, highlighted, paperPoint(*image, highlighted, QPointF(10, 7.5))));
    QVERIFY(changedNear(baseline, highlighted, paperPoint(*image, highlighted, QPointF(30, 7.5))));
    QVERIFY(!changedNear(baseline, highlighted, paperPoint(*image, highlighted, QPointF(7, 7))));
}

void PrintPreviewTests::clearHighlightRetainsDiagnosticUntilClear()
{
    app::PrintPreviewPanel panel;
    panel.resize(720, 360);
    panel.show();
    QVERIFY(QTest::qWaitForWindowExposed(&panel));
    panel.setDiagnosticDocument(locatorDocument(), QSizeF(80, 30));
    QTest::qWait(30);
    auto* image = previewImage(panel);
    auto* next = panel.findChild<QPushButton*>(QStringLiteral("workbenchPageNextButton"));
    auto* previous = panel.findChild<QPushButton*>(QStringLiteral("workbenchPagePreviousButton"));
    QVERIFY(image != nullptr && next != nullptr && previous != nullptr);
    const auto baseline = paintedImage(*image);
    const auto cacheKey = image->pixmap().cacheKey();
    panel.setElementHighlight(locatorDocument(), {QStringLiteral("first")});
    QCOMPARE(panel.property("printPreflightHighlightCount").toInt(), 1);

    panel.clearElementHighlight();

    QCOMPARE(panel.property("printPreflightHighlightCount").toInt(), 0);
    QVERIFY(panel.property("printPreflightDiagnostic").toBool());
    QCOMPARE(image->pixmap().cacheKey(), cacheKey);
    QCOMPARE(paintedImage(*image), baseline);
    panel.setElementHighlight(locatorDocument(), {QStringLiteral("second")});
    panel.clear();
    QCOMPARE(panel.property("printPreflightHighlightCount").toInt(), 0);
    QVERIFY(!panel.property("printPreflightDiagnostic").toBool());
    QVERIFY(image->pixmap().isNull());
    QVERIFY(!next->isEnabled() && !previous->isEnabled());
    QVERIFY(!hasDiagnosticTitle(panel));
}

void PrintPreviewTests::fullyOutsideElementShowsPaperEdgeIndicator_data()
{
    QTest::addColumn<bool>("diagnostic");
    QTest::newRow("print-plan") << false;
    QTest::newRow("diagnostic") << true;
}

void PrintPreviewTests::fullyOutsideElementShowsPaperEdgeIndicator()
{
    QFETCH(bool, diagnostic);
    auto document = locatorDocument();
    document.layers[0].elements.removeLast();
    document.layers[0].elements[0].x = 95;
    document.layers[0].elements[0].y = 12;
    auto plan = calibratedPlan();
    plan.pages[0].commands = {rectangle(QStringLiteral("first"), 95, 12, 10, 4)};
    const auto original = serializedPages(plan);
    app::PrintPreviewPanel panel;
    panel.resize(720, 360);
    panel.show();
    QVERIFY(QTest::qWaitForWindowExposed(&panel));
    if (diagnostic) panel.setDiagnosticDocument(document, QSizeF(80, 30));
    else panel.setPlan(plan);
    QTest::qWait(30);
    auto* image = previewImage(panel);
    QVERIFY(image != nullptr);
    const auto baseline = paintedImage(*image);
    const auto cacheKey = image->pixmap().cacheKey();

    panel.setElementHighlight(document, {QStringLiteral("first")});

    QCOMPARE(panel.property("printPreflightHighlightCount").toInt(), 1);
    const auto highlighted = paintedImage(*image);
    // 元素位于纸张右外侧，纸内右边缘必须出现反馈，不能只让计数变成 1。
    QVERIFY(changedNear(baseline, highlighted, paperPoint(*image, highlighted, QPointF(78.5, 14)), 10));
    QCOMPARE(image->pixmap().cacheKey(), cacheKey);
    QCOMPARE(serializedPages(plan), original);
    panel.clearElementHighlight();
    QCOMPARE(paintedImage(*image), baseline);
}

void PrintPreviewTests::diagnosticTitleRemainsFullyVisibleInNarrowPanel_data()
{
    QTest::addColumn<int>("width");
    QTest::newRow("compact") << 300;
    QTest::newRow("narrow") << 360;
    QTest::newRow("workbench") << 734;
}

void PrintPreviewTests::diagnosticTitleRemainsFullyVisibleInNarrowPanel()
{
    QFETCH(int, width);
    app::PrintPreviewPanel panel;
    panel.resize(width, 280);
    panel.show();
    QVERIFY(QTest::qWaitForWindowExposed(&panel));
    panel.setDiagnosticDocument(locatorDocument(), QSizeF(80, 30));
    QCoreApplication::processEvents();
    QCOMPARE(panel.width(), width);
    QLabel* title = nullptr;
    for (auto* label : panel.findChildren<QLabel*>()) {
        if (label->text() == QString::fromUtf8("版式定位（非打印预览）")) title = label;
    }
    QVERIFY(title != nullptr && title->isVisible());
    const auto contents = title->contentsRect();
    const auto required = title->fontMetrics().boundingRect(QRect(0, 0, contents.width(), 10000),
        Qt::TextWordWrap | Qt::AlignLeft | Qt::AlignTop, title->text());
    // 按完整文案计算所需高度，不能用仅检查 text() 的方式放过实际被裁掉的后半句。
    QVERIFY2(contents.height() >= required.height(), qPrintable(QStringLiteral("title height %1 < required %2")
        .arg(contents.height()).arg(required.height())));
    QVERIFY(contents.width() >= required.width());
    const QRect titleRect(title->mapTo(&panel, QPoint()), title->size());
    QVERIFY(panel.rect().contains(titleRect));
    const auto* zoom = panel.findChild<QComboBox*>(QStringLiteral("workbenchPreviewZoom"));
    const auto* previous = panel.findChild<QPushButton*>(QStringLiteral("workbenchPagePreviousButton"));
    const auto* next = panel.findChild<QPushButton*>(QStringLiteral("workbenchPageNextButton"));
    QVERIFY(zoom != nullptr && previous != nullptr && next != nullptr);
    const QWidget* controls[] = {zoom, previous, next};
    for (const auto* control : controls) {
        const QRect controlRect(control->mapTo(&panel, QPoint()), control->size());
        QVERIFY(panel.rect().contains(controlRect));
        QVERIFY(!titleRect.intersects(controlRect));
    }
}

QTEST_MAIN(PrintPreviewTests)
#include "tst_print_preview.moc"

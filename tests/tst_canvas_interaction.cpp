#include "sleekpr/app/TemplatePreviewLabel.h"

#include <QApplication>
#include <QContextMenuEvent>
#include <QImage>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPixmap>
#include <QRegion>
#include <QtTest>

#include <array>
#include <limits>

using sleekpr::app::TemplatePreviewLabel;

namespace {

void prepareCanvas(TemplatePreviewLabel& label, qreal dpr = 1.0, bool rulers = true)
{
    QPixmap image(qRound(400 * dpr), qRound(200 * dpr));
    image.setDevicePixelRatio(dpr);
    image.fill(Qt::white);
    label.setPixmap(image);
    label.setAlignment(Qt::AlignLeft | Qt::AlignTop);
    label.setRulerPaperSizeMm(QSizeF(80, 40));
    label.setRulerVisible(rulers);
    label.resize(label.printableImageOriginPx().x() + 420, label.printableImageOriginPx().y() + 220);
    label.setDraggingEnabled(true);
}

void sendMouse(TemplatePreviewLabel& label, QEvent::Type type, QPoint position,
    Qt::MouseButton button = Qt::NoButton, Qt::MouseButtons buttons = Qt::NoButton)
{
    QMouseEvent event(type, QPointF(position), QPointF(label.mapToGlobal(position)), button, buttons, Qt::NoModifier);
    QApplication::sendEvent(&label, &event);
}

void press(TemplatePreviewLabel& label, QPoint position)
{
    sendMouse(label, QEvent::MouseButtonPress, position, Qt::LeftButton, Qt::LeftButton);
}

void release(TemplatePreviewLabel& label, QPoint position)
{
    sendMouse(label, QEvent::MouseButtonRelease, position, Qt::LeftButton);
}

QImage canvasImage(TemplatePreviewLabel& label)
{
    QImage image(label.size(), QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    label.render(&image);
    return image;
}

const std::array<Qt::Edges, 8> resizeEdges = {
    Qt::TopEdge | Qt::LeftEdge, Qt::TopEdge, Qt::TopEdge | Qt::RightEdge,
    Qt::RightEdge, Qt::BottomEdge | Qt::RightEdge, Qt::BottomEdge,
    Qt::BottomEdge | Qt::LeftEdge, Qt::LeftEdge};

} // 匿名命名空间

class CanvasInteractionTests final : public QObject
{
    Q_OBJECT

private slots:
    void handlesUseLogicalPixelsAndRulerOffset_data();
    void handlesUseLogicalPixelsAndRulerOffset();
    void smallSelectionKeepsCenterAvailableForMoving_data();
    void smallSelectionKeepsCenterAvailableForMoving();
    void smallSelectionHandlesDoNotExpandActualOutline();
    void handlesRequireAValidEnabledSelection();
    void handleHoverUsesResizeCursors();
    void overlaysNeverModifyPreviewPixmap_data();
    void overlaysNeverModifyPreviewPixmap();
    void editorSelectionReplacesLegacyOutline();
    void clickFinishesAfterSelectionCallback_data();
    void clickFinishesAfterSelectionCallback();
    void dragKeepsIncrementalDeltaSemantics();
    void rejectedPressAndFloatingActionDoNotFinishDrag();
    void escapeCancelsDragExactlyOnce();
    void interruptionCancelsDrag_data();
    void interruptionCancelsDrag();
    void doubleClickEndsOldGestureBeforeCallback();
    void existingKeyboardAndContextCallbacksRemainAvailable();
};

void CanvasInteractionTests::handlesUseLogicalPixelsAndRulerOffset_data()
{
    QTest::addColumn<qreal>("dpr");
    QTest::addColumn<bool>("rulers");
    QTest::newRow("normal") << qreal(1.0) << true;
    QTest::newRow("fractional") << qreal(1.5) << true;
    QTest::newRow("retina") << qreal(2.0) << true;
    QTest::newRow("without-rulers") << qreal(2.0) << false;
}

void CanvasInteractionTests::handlesUseLogicalPixelsAndRulerOffset()
{
    QFETCH(qreal, dpr);
    QFETCH(bool, rulers);
    TemplatePreviewLabel label;
    prepareCanvas(label, dpr, rulers);
    label.setEditorSelectionRectsMm({QRectF(10, 8, 20, 12)}, QRectF(10, 8, 20, 12));
    QCOMPARE(label.printableImageSizePx(), QSize(400, 200));
    const auto origin = label.printableImageOriginPx();
    const std::array<QPoint, 8> anchors = {
        QPoint(50, 40), QPoint(100, 40), QPoint(150, 40), QPoint(150, 70),
        QPoint(150, 100), QPoint(100, 100), QPoint(50, 100), QPoint(50, 70)};
    QSize handleSize;
    for (std::size_t index = 0; index < resizeEdges.size(); ++index) {
        const auto handle = label.editorResizeHandleRect(resizeEdges[index]);
        QVERIFY(!handle.isEmpty());
        QCOMPARE(handle.center(), origin + anchors[index]);
        QCOMPARE(label.editorResizeHandleAt(handle.center()), resizeEdges[index]);
        if (handleSize.isEmpty()) {
            handleSize = handle.size();
        }
        QCOMPARE(handle.size(), handleSize);
        QVERIFY(handle.width() <= 12);
    }
    QCOMPARE(label.editorResizeHandleAt(origin + QPoint(100, 70)), Qt::Edges{});
    QCOMPARE(label.mapToPrintableImagePx(origin + QPoint(100, 70)), QPoint(100, 70));
}

void CanvasInteractionTests::smallSelectionKeepsCenterAvailableForMoving_data()
{
    QTest::addColumn<qreal>("dpr");
    QTest::addColumn<bool>("rulers");
    QTest::addColumn<QSizeF>("elementSizeMm");
    const std::array<QSizeF, 3> sizes = {QSizeF(9.2, 1.5), QSizeF(1.5, 9.2), QSizeF(0.8, 0.6)};
    for (const auto dpr : {qreal(1.0), qreal(1.5), qreal(2.0)}) {
        for (const auto rulers : {false, true}) {
            for (const auto& size : sizes) {
                const auto name = QStringLiteral("%1x%2-dpr%3-rulers%4")
                                      .arg(size.width()).arg(size.height()).arg(dpr).arg(rulers);
                QTest::newRow(qPrintable(name)) << dpr << rulers << size;
            }
        }
    }
}

void CanvasInteractionTests::smallSelectionKeepsCenterAvailableForMoving()
{
    QFETCH(qreal, dpr);
    QFETCH(bool, rulers);
    QFETCH(QSizeF, elementSizeMm);
    TemplatePreviewLabel label;
    prepareCanvas(label, dpr, rulers);
    const QRectF bounds(QPointF(10, 8), elementSizeMm);
    label.setEditorSelectionRectsMm({bounds}, bounds);
    const auto origin = label.printableImageOriginPx();
    const auto center = origin + QPoint(qRound(bounds.center().x() * 5), qRound(bounds.center().y() * 5));
    // 9.2x1.5 毫米在此仅占 46x7.5 逻辑像素，中心和周围小范围必须保留给移动。
    for (int dy = -3; dy <= 3; ++dy) {
        for (int dx = -3; dx <= 3; ++dx) {
            QCOMPARE(label.editorResizeHandleAt(center + QPoint(dx, dy)), Qt::Edges{});
        }
    }

    TemplatePreviewLabel reference;
    prepareCanvas(reference, 1.0, rulers);
    reference.setEditorSelectionRectsMm({bounds}, bounds);
    QRegion occupiedHandles;
    for (const auto edges : resizeEdges) {
        const auto handle = label.editorResizeHandleRect(edges);
        QVERIFY(!handle.isEmpty());
        QCOMPARE(handle, reference.editorResizeHandleRect(edges));
        QCOMPARE(label.editorResizeHandleAt(handle.center()), edges);
        QVERIFY(!occupiedHandles.contains(handle.center()));
        occupiedHandles += handle;
    }

    Qt::Edges pressedHandle;
    QPoint accumulatedDelta;
    QList<bool> finishes;
    label.setDragStartCallback([&](QPoint position, Qt::KeyboardModifiers) {
        pressedHandle = label.editorResizeHandleAt(position);
        return true;
    });
    label.setDragDeltaCallback([&](QPoint delta) { accumulatedDelta += delta; });
    label.setDragFinishedCallback([&](bool cancelled) { finishes.append(cancelled); });
    sendMouse(label, QEvent::MouseMove, center);
    QCOMPARE(label.cursor().shape(), Qt::OpenHandCursor);
    press(label, center);
    QCOMPARE(pressedHandle, Qt::Edges{});
    QCOMPARE(label.cursor().shape(), Qt::ClosedHandCursor);
    const QPoint delta(QApplication::startDragDistance() + 4, 2);
    sendMouse(label, QEvent::MouseMove, center + delta, Qt::NoButton, Qt::LeftButton);
    QCOMPARE(accumulatedDelta, delta);
    QCOMPARE(label.cursor().shape(), Qt::ClosedHandCursor);
    release(label, center + delta);
    QCOMPARE(finishes, QList<bool>({false}));
}

void CanvasInteractionTests::smallSelectionHandlesDoNotExpandActualOutline()
{
    TemplatePreviewLabel label;
    prepareCanvas(label, 2.0);
    const QRectF bounds(10, 8, 9.2, 1.5);
    const auto originalPixmapKey = label.pixmap().cacheKey();
    label.setEditorSelectionRectsMm({bounds}, bounds, false);
    const auto actualOutline = canvasImage(label);
    label.setEditorSelectionRectsMm({bounds}, bounds);
    const auto withHandles = canvasImage(label);
    QRegion handleArea;
    for (const auto edges : resizeEdges) {
        handleArea += label.editorResizeHandleRect(edges).adjusted(-1, -1, 1, 1);
    }
    // 开启手柄只允许新增八个点，不得扩大真实选区轮廓，也不能改写原生预览图。
    for (int y = 0; y < actualOutline.height(); ++y) {
        for (int x = 0; x < actualOutline.width(); ++x) {
            if (!handleArea.contains(QPoint(x, y))) {
                QCOMPARE(withHandles.pixel(x, y), actualOutline.pixel(x, y));
            }
        }
    }
    QCOMPARE(label.pixmap().cacheKey(), originalPixmapKey);
}

void CanvasInteractionTests::handlesRequireAValidEnabledSelection()
{
    TemplatePreviewLabel label;
    prepareCanvas(label);
    QVERIFY(label.editorResizeHandleRect(Qt::TopEdge).isEmpty());
    const QRectF bounds(10, 8, 20, 12);
    label.setEditorSelectionRectsMm({bounds}, bounds, false);
    QVERIFY(label.editorResizeHandleRect(Qt::TopEdge).isEmpty());
    label.setEditorSelectionRectsMm({bounds}, bounds);
    QVERIFY(!label.editorResizeHandleRect(Qt::TopEdge).isEmpty());
    QVERIFY(label.editorResizeHandleRect(Qt::Edges{}).isEmpty());
    QVERIFY(label.editorResizeHandleRect(Qt::LeftEdge | Qt::RightEdge).isEmpty());
    label.setEditorSelectionRectsMm({}, bounds);
    QVERIFY(label.editorResizeHandleRect(Qt::TopEdge).isEmpty());
    label.setEditorSelectionRectsMm({QRectF(std::numeric_limits<double>::quiet_NaN(), 0, 10, 10)}, bounds);
    QVERIFY(label.editorResizeHandleRect(Qt::TopEdge).isEmpty());
    label.setRulerPaperSizeMm({});
    label.setEditorSelectionRectsMm({bounds}, bounds);
    QVERIFY(label.editorResizeHandleRect(Qt::TopEdge).isEmpty());
}

void CanvasInteractionTests::handleHoverUsesResizeCursors()
{
    TemplatePreviewLabel label;
    prepareCanvas(label, 2.0);
    const QRectF bounds(10, 8, 20, 12);
    label.setEditorSelectionRectsMm({bounds}, bounds);
    const std::array<Qt::CursorShape, 8> shapes = {
        Qt::SizeFDiagCursor, Qt::SizeVerCursor, Qt::SizeBDiagCursor, Qt::SizeHorCursor,
        Qt::SizeFDiagCursor, Qt::SizeVerCursor, Qt::SizeBDiagCursor, Qt::SizeHorCursor};
    for (std::size_t index = 0; index < resizeEdges.size(); ++index) {
        sendMouse(label, QEvent::MouseMove, label.editorResizeHandleRect(resizeEdges[index]).center());
        QCOMPARE(label.cursor().shape(), shapes[index]);
    }
    sendMouse(label, QEvent::MouseMove, label.printableImageOriginPx() + QPoint(300, 150));
    QCOMPARE(label.cursor().shape(), Qt::OpenHandCursor);
    label.setEditorSelectionRectsMm({bounds}, bounds, false);
    sendMouse(label, QEvent::MouseMove, label.printableImageOriginPx() + QPoint(50, 40));
    QCOMPARE(label.cursor().shape(), Qt::OpenHandCursor);
}

void CanvasInteractionTests::overlaysNeverModifyPreviewPixmap_data()
{
    QTest::addColumn<bool>("rulers");
    QTest::newRow("rulers") << true;
    QTest::newRow("no-rulers") << false;
}

void CanvasInteractionTests::overlaysNeverModifyPreviewPixmap()
{
    QFETCH(bool, rulers);
    TemplatePreviewLabel label;
    prepareCanvas(label, 2.0, rulers);
    const auto nativeImage = label.pixmap().toImage();
    const auto nativeKey = label.pixmap().cacheKey();
    const auto baseline = canvasImage(label);
    label.setEditorSelectionRectsMm({QRectF(10, 8, 20, 12), QRectF(35, 8, 10, 12)}, QRectF(10, 8, 35, 12));
    label.setEditorMarqueeRectMm(QRectF(60, 32, -8, -6));
    label.setEditorSnapGuidesMm({QLineF(65, -10, 65, 50), QLineF(0, 28, 80, 28)});
    const auto decorated = canvasImage(label);
    const auto origin = label.printableImageOriginPx();
    QVERIFY(decorated != baseline);
    QVERIFY(decorated.copy(QRect(origin + QPoint(45, 35), QSize(20, 20)))
        != baseline.copy(QRect(origin + QPoint(45, 35), QSize(20, 20))));
    QVERIFY(decorated.copy(QRect(origin + QPoint(260, 130), QSize(40, 30)))
        != baseline.copy(QRect(origin + QPoint(260, 130), QSize(40, 30))));
    QVERIFY(decorated.copy(QRect(origin + QPoint(320, 40), QSize(10, 30)))
        != baseline.copy(QRect(origin + QPoint(320, 40), QSize(10, 30))));
    if (rulers) {
        // 超出纸张的吸附线不能覆盖标尺，覆盖层清除后也必须完全回到原始显示。
        const QRect rulerStrip(origin.x() + 320, 0, 10, origin.y());
        QCOMPARE(decorated.copy(rulerStrip), baseline.copy(rulerStrip));
    }
    QCOMPARE(label.pixmap().cacheKey(), nativeKey);
    QCOMPARE(label.pixmap().toImage(), nativeImage);
    QCOMPARE(label.designAidCommandCount(), 0);
    label.setEditorSelectionRectsMm({}, {});
    label.setEditorMarqueeRectMm({});
    label.setEditorSnapGuidesMm({});
    QCOMPARE(canvasImage(label), baseline);
}

void CanvasInteractionTests::editorSelectionReplacesLegacyOutline()
{
    TemplatePreviewLabel label;
    prepareCanvas(label);
    sleekpr::core::NativeDrawCommand command;
    command.elementKey = QStringLiteral("old-selected");
    command.x = 4;
    command.y = 4;
    command.width = 8;
    command.height = 6;
    label.setDesignAidCommands({command});
    const auto baseline = canvasImage(label);
    label.setDesignAidSelectedElementKey(command.elementKey);
    label.setCanvasFocusRectMm(QRectF(4, 4, 8, 6));
    const QRect oldOutline(label.printableImageOriginPx() + QPoint(18, 18), QSize(45, 35));
    QVERIFY(canvasImage(label).copy(oldOutline) != baseline.copy(oldOutline));
    label.setEditorSelectionRectsMm({QRectF(30, 20, 10, 8)}, QRectF(30, 20, 10, 8));
    QCOMPARE(canvasImage(label).copy(oldOutline), baseline.copy(oldOutline));
}

void CanvasInteractionTests::clickFinishesAfterSelectionCallback_data()
{
    QTest::addColumn<bool>("disableInClick");
    QTest::newRow("normal") << false;
    QTest::newRow("reentrant-disable") << true;
}

void CanvasInteractionTests::clickFinishesAfterSelectionCallback()
{
    QFETCH(bool, disableInClick);
    TemplatePreviewLabel label;
    prepareCanvas(label);
    QStringList callbacks;
    label.setDragStartCallback([&](QPoint, Qt::KeyboardModifiers) { callbacks.append(QStringLiteral("start")); return true; });
    label.setCanvasClickCallback([&](QPoint, Qt::KeyboardModifiers) {
        callbacks.append(QStringLiteral("click"));
        if (disableInClick) {
            label.setDraggingEnabled(false);
        }
    });
    label.setDragFinishedCallback([&](bool cancelled) { callbacks.append(cancelled ? QStringLiteral("cancel") : QStringLiteral("finish")); });
    const QPoint point(150, 100);
    press(label, point);
    release(label, point);
    release(label, point);
    QCOMPARE(callbacks, QStringList({QStringLiteral("start"), QStringLiteral("click"), QStringLiteral("finish")}));
    QCOMPARE(label.cursor().shape(), disableInClick ? Qt::ArrowCursor : Qt::OpenHandCursor);
}

void CanvasInteractionTests::dragKeepsIncrementalDeltaSemantics()
{
    TemplatePreviewLabel label;
    prepareCanvas(label);
    label.setEditorSelectionRectsMm({QRectF(10, 8, 20, 12)}, QRectF(10, 8, 20, 12));
    QList<QPoint> deltas;
    QList<bool> finishes;
    int clicks = 0;
    label.setDragDeltaCallback([&](QPoint delta) { deltas.append(delta); });
    label.setDragFinishedCallback([&](bool cancelled) { finishes.append(cancelled); });
    label.setCanvasClickCallback([&](QPoint, Qt::KeyboardModifiers) { ++clicks; });
    const auto start = label.editorResizeHandleRect(Qt::RightEdge).center();
    const QPoint firstDelta(QApplication::startDragDistance() + 2, 0);
    press(label, start);
    sendMouse(label, QEvent::MouseMove, start + QPoint(1, 0), Qt::NoButton, Qt::LeftButton);
    QVERIFY(deltas.isEmpty());
    sendMouse(label, QEvent::MouseMove, start + firstDelta, Qt::NoButton, Qt::LeftButton);
    QCOMPARE(label.cursor().shape(), Qt::SizeHorCursor);
    sendMouse(label, QEvent::MouseMove, start + firstDelta + QPoint(3, 2), Qt::NoButton, Qt::LeftButton);
    release(label, start + firstDelta + QPoint(3, 2));
    QCOMPARE(deltas, QList<QPoint>({firstDelta, QPoint(3, 2)}));
    QCOMPARE(finishes, QList<bool>({false}));
    QCOMPARE(clicks, 0);
}

void CanvasInteractionTests::rejectedPressAndFloatingActionDoNotFinishDrag()
{
    TemplatePreviewLabel label;
    prepareCanvas(label);
    int finishes = 0;
    int actions = 0;
    label.setDragFinishedCallback([&](bool) { ++finishes; });
    label.setDragStartCallback([](QPoint, Qt::KeyboardModifiers) { return false; });
    press(label, QPoint(200, 150));
    release(label, QPoint(200, 150));
    QCOMPARE(finishes, 0);
    label.setCanvasFocusRectMm(QRectF(10, 8, 20, 12));
    label.setCanvasFloatingActions({{QStringLiteral("table-action"), QStringLiteral("B"), true}});
    label.setCanvasFloatingActionCallback([&](QString) { ++actions; });
    const auto action = label.canvasFloatingActionRect(QStringLiteral("table-action"));
    QVERIFY(!action.isEmpty());
    press(label, action.center());
    release(label, action.center());
    QCOMPARE(actions, 1);
    QCOMPARE(finishes, 0);
}

void CanvasInteractionTests::escapeCancelsDragExactlyOnce()
{
    TemplatePreviewLabel label;
    prepareCanvas(label);
    QList<bool> finishes;
    int clicks = 0;
    int shortcuts = 0;
    label.setDragFinishedCallback([&](bool cancelled) { finishes.append(cancelled); });
    label.setCanvasClickCallback([&](QPoint, Qt::KeyboardModifiers) { ++clicks; });
    label.setCanvasShortcutCallback([&](int, Qt::KeyboardModifiers) { ++shortcuts; return true; });
    press(label, QPoint(150, 100));
    sendMouse(label, QEvent::MouseMove, QPoint(200, 100), Qt::NoButton, Qt::LeftButton);
    QTest::keyClick(&label, Qt::Key_Escape);
    release(label, QPoint(200, 100));
    QCOMPARE(finishes, QList<bool>({true}));
    QCOMPARE(clicks, 0);
    QCOMPARE(shortcuts, 0);
    QCOMPARE(label.cursor().shape(), Qt::OpenHandCursor);
}

void CanvasInteractionTests::interruptionCancelsDrag_data()
{
    QTest::addColumn<int>("interruption");
    QTest::newRow("ungrab") << int(QEvent::UngrabMouse);
    QTest::newRow("hide") << int(QEvent::Hide);
    QTest::newRow("deactivate") << int(QEvent::WindowDeactivate);
    QTest::newRow("disabled-widget") << -1;
    QTest::newRow("disabled-dragging") << -2;
    QTest::newRow("lost-button") << -3;
}

void CanvasInteractionTests::interruptionCancelsDrag()
{
    QFETCH(int, interruption);
    TemplatePreviewLabel label;
    prepareCanvas(label);
    QList<bool> finishes;
    label.setDragFinishedCallback([&](bool cancelled) { finishes.append(cancelled); });
    press(label, QPoint(150, 100));
    if (interruption == -1) {
        label.setEnabled(false);
    } else if (interruption == -2) {
        label.setDraggingEnabled(false);
    } else if (interruption == -3) {
        sendMouse(label, QEvent::MouseMove, QPoint(160, 100));
    } else {
        QEvent event(static_cast<QEvent::Type>(interruption));
        QApplication::sendEvent(&label, &event);
    }
    release(label, QPoint(150, 100));
    QCOMPARE(finishes, QList<bool>({true}));
    QVERIFY(label.cursor().shape() != Qt::ClosedHandCursor);
}

void CanvasInteractionTests::doubleClickEndsOldGestureBeforeCallback()
{
    TemplatePreviewLabel label;
    prepareCanvas(label);
    QStringList callbacks;
    label.setDragFinishedCallback([&](bool cancelled) { callbacks.append(cancelled ? QStringLiteral("cancel") : QStringLiteral("finish")); });
    label.setDoubleClickCallback([&](QPoint) { callbacks.append(QStringLiteral("double")); });
    label.setCanvasClickCallback([&](QPoint, Qt::KeyboardModifiers) { callbacks.append(QStringLiteral("click")); });
    press(label, QPoint(150, 100));
    sendMouse(label, QEvent::MouseButtonDblClick, QPoint(150, 100), Qt::LeftButton, Qt::LeftButton);
    release(label, QPoint(150, 100));
    QCOMPARE(callbacks, QStringList({QStringLiteral("cancel"), QStringLiteral("double")}));
    QCOMPARE(label.cursor().shape(), Qt::OpenHandCursor);
    callbacks.clear();
    sendMouse(label, QEvent::MouseButtonDblClick, QPoint(150, 100), Qt::LeftButton, Qt::LeftButton);
    release(label, QPoint(150, 100));
    QCOMPARE(callbacks, QStringList({QStringLiteral("double")}));
}

void CanvasInteractionTests::existingKeyboardAndContextCallbacksRemainAvailable()
{
    TemplatePreviewLabel label;
    prepareCanvas(label);
    QPoint nudge;
    int contextMenus = 0;
    int shortcuts = 0;
    label.setKeyboardNudgeCallback([&](QPoint direction, Qt::KeyboardModifiers) { nudge = direction; });
    label.setCanvasShortcutCallback([&](int key, Qt::KeyboardModifiers) { ++shortcuts; return key == Qt::Key_B; });
    label.setContextMenuCallback([&](QPoint, QPoint) { ++contextMenus; });
    QTest::keyClick(&label, Qt::Key_Right);
    QCOMPARE(nudge, QPoint(1, 0));
    QTest::keyClick(&label, Qt::Key_B);
    QCOMPARE(shortcuts, 2);
    QContextMenuEvent context(QContextMenuEvent::Mouse, QPoint(100, 100), label.mapToGlobal(QPoint(100, 100)));
    QApplication::sendEvent(&label, &context);
    QCOMPARE(contextMenus, 1);
}

QTEST_MAIN(CanvasInteractionTests)
#include "tst_canvas_interaction.moc"

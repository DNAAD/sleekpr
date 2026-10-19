#include <QtTest/QtTest>

#include "sleekpr/core/templates/TemplateDocumentJson.h"
#include "sleekpr/core/templates/TemplateSelectionEditModel.h"

#include <QSet>

#include <limits>

using namespace sleekpr::core;

namespace {

TemplateElement element(const QString& id, const QRectF& rect, int zIndex = 0)
{
    TemplateElement result;
    result.id = id;
    result.layerId = QStringLiteral("main");
    result.zIndex = zIndex;
    result.x = rect.x();
    result.y = rect.y();
    result.width = rect.width();
    result.height = rect.height();
    return result;
}

TemplateDocument documentWith(const QList<TemplateElement>& elements)
{
    TemplateLayer layer;
    layer.id = QStringLiteral("main");
    layer.elements = elements;
    TemplateDocument document;
    document.layers.append(layer);
    return document;
}

QRectF rectOf(const TemplateElement& value)
{
    return {value.x, value.y, value.width, value.height};
}

QJsonObject propertiesWithoutIdentityAndPosition(TemplateElement value)
{
    // 只剔除允许变化的身份和位置，以完整序列化结果验证其余属性均被保留。
    value.id.clear();
    value.layerId.clear();
    value.zIndex = 0;
    value.x = 0.0;
    value.y = 0.0;
    return TemplateDocumentJson::toJson(documentWith({value}));
}

} // 匿名命名空间

class SelectionTests final : public QObject
{
    Q_OBJECT

private slots:
    void boundsExcludeProtectedItemsAndTables();
    void emptySelectionDoesNotEdit();
    void moveClampsWholeSelectionAndKeepsRelativePositions();
    void finiteExtremeOffsetIsClampedBeforeAddition();
    void transformsDoNotChangeProtectedItems();
    void oversizedMoveAndPasteAreRejectedAtomically();
    void resizeScalesGeometryButNotStyle();
    void resizeEnforcesMinimumByScalingWholeSelection();
    void resizeClampsTargetToPaper();
    void resizeRejectsImpossibleMinimum();
    void noOpTransformsReturnFalse();
    void alignMultiple_data();
    void alignMultiple();
    void alignSingleToPaper_data();
    void alignSingleToPaper();
    void distributeUsesEqualEdgeGaps_data();
    void distributeUsesEqualEdgeGaps();
    void distributeRequiresThreeEditableDistinctItems();
    void distributeOverlappingItemsStaysWithinReference();
    void alignClampsAsOneSelection();
    void duplicatePreservesPropertiesAndPaintOrder();
    void duplicatePreservesLayerOrderAndStableZIndexTies();
    void duplicateFiltersProtectedSources();
    void pastePreservesInputOrderAndClampsTogether();
    void pasteRejectsUnwritableTarget_data();
    void pasteRejectsUnwritableTarget();
    void pasteSkipsHiddenAndLockedItems();
    void pasteIdsRemainUniqueAcrossCalls();
    void pasteRejectsZIndexOverflow();
    void invalidPaperIsRejected_data();
    void invalidPaperIsRejected();
    void invalidOffsetIsRejected_data();
    void invalidOffsetIsRejected();
    void invalidGeometryIsRejectedAtomically_data();
    void invalidGeometryIsRejectedAtomically();
    void invalidResizeTargetIsRejected_data();
    void invalidResizeTargetIsRejected();
};

void SelectionTests::boundsExcludeProtectedItemsAndTables()
{
    auto document = documentWith({
        element("a", {1, 2, 10, 5}), element("b", {15, 8, 8, 4}),
        element("hidden", {40, 20, 10, 5}), element("locked", {50, 25, 10, 5})});
    document.layers[0].elements[2].visible = false;
    document.layers[0].elements[3].locked = true;
    TableElement table;
    table.id = QStringLiteral("table");
    document.layers[0].tables.append(table);
    TemplateLayer lockedLayer;
    lockedLayer.id = QStringLiteral("locked-layer");
    lockedLayer.locked = true;
    lockedLayer.elements.append(element("layer-locked", {60, 20, 10, 5}));
    document.layers.append(lockedLayer);
    auto hiddenLayer = lockedLayer;
    hiddenLayer.id = QStringLiteral("hidden-layer");
    hiddenLayer.locked = false;
    hiddenLayer.visible = false;
    hiddenLayer.elements[0].id = QStringLiteral("layer-hidden");
    document.layers.append(hiddenLayer);

    const QStringList ids{"b", "a", "a", "hidden", "locked", "layer-locked", "layer-hidden", "table", "missing"};
    QCOMPARE(TemplateSelectionEditModel::bounds(document, ids), QRectF(1, 2, 22, 10));
    QCOMPARE(TemplateSelectionEditModel::bounds(document, {"locked", "table"}), QRectF());
}

void SelectionTests::emptySelectionDoesNotEdit()
{
    auto document = documentWith({element("a", {1, 2, 10, 5})});
    const auto before = TemplateDocumentJson::toJson(document);
    QCOMPARE(TemplateSelectionEditModel::bounds(document, {}), QRectF());
    QVERIFY(!TemplateSelectionEditModel::move(document, {}, {1, 1}, {80, 30}));
    QVERIFY(!TemplateSelectionEditModel::resize(document, {}, {0, 0, 20, 10}, {80, 30}));
    QVERIFY(!TemplateSelectionEditModel::align(document, {}, SelectionAlignment::Left, {80, 30}));
    QVERIFY(TemplateSelectionEditModel::duplicate(document, {"missing"}, "main", {1, 1}, {80, 30}).isEmpty());
    QVERIFY(TemplateSelectionEditModel::paste(document, {}, "main", {1, 1}, {80, 30}).isEmpty());
    QCOMPARE(TemplateDocumentJson::toJson(document), before);
}

void SelectionTests::moveClampsWholeSelectionAndKeepsRelativePositions()
{
    auto document = documentWith({element("a", {10, 3, 10, 4}), element("b", {25, 12, 5, 6})});
    QVERIFY(TemplateSelectionEditModel::move(document, {"b", "a", "a"}, {100, -100}, {80, 30}));
    QCOMPARE(rectOf(document.layers[0].elements[0]), QRectF(60, 0, 10, 4));
    QCOMPARE(rectOf(document.layers[0].elements[1]), QRectF(75, 9, 5, 6));
    QVERIFY(!TemplateSelectionEditModel::move(document, {"a", "b"}, {100, 0}, {80, 30}));
    QVERIFY(TemplateSelectionEditModel::move(document, {"a", "b"}, {-100, 100}, {80, 30}));
    QCOMPARE(rectOf(document.layers[0].elements[0]), QRectF(0, 15, 10, 4));
    QCOMPARE(rectOf(document.layers[0].elements[1]), QRectF(15, 24, 5, 6));
}

void SelectionTests::finiteExtremeOffsetIsClampedBeforeAddition()
{
    auto document = documentWith({element("a", {1, 2, 10, 5})});
    const auto max = std::numeric_limits<double>::max();
    QVERIFY(TemplateSelectionEditModel::move(document, {"a"}, {max, -max}, {80, 30}));
    QCOMPARE(rectOf(document.layers[0].elements[0]), QRectF(70, 0, 10, 5));
    const auto ids = TemplateSelectionEditModel::paste(document, document.layers[0].elements, "main", {-max, max}, {80, 30});
    QCOMPARE(ids.size(), 1);
    QCOMPARE(rectOf(document.layers[0].elements[1]), QRectF(0, 25, 10, 5));
}

void SelectionTests::transformsDoNotChangeProtectedItems()
{
    auto document = documentWith({element("a", {1, 2, 10, 5}), element("locked", {40, 20, 10, 5}),
        element("hidden", {60, 20, 10, 5})});
    document.layers[0].elements[1].locked = true;
    document.layers[0].elements[2].visible = false;
    auto lockedLayer = document.layers[0];
    lockedLayer.id = QStringLiteral("locked-layer");
    lockedLayer.locked = true;
    lockedLayer.elements = {element("layer-locked", {10, 10, 8, 5})};
    document.layers.append(lockedLayer);
    auto hiddenLayer = lockedLayer;
    hiddenLayer.id = QStringLiteral("hidden-layer");
    hiddenLayer.locked = false;
    hiddenLayer.visible = false;
    hiddenLayer.elements[0].id = QStringLiteral("layer-hidden");
    document.layers.append(hiddenLayer);
    const QStringList ids{"a", "locked", "hidden", "layer-locked", "layer-hidden"};

    QVERIFY(TemplateSelectionEditModel::move(document, ids, {5, 5}, {80, 30}));
    QVERIFY(TemplateSelectionEditModel::resize(document, ids, {2, 3, 20, 10}, {80, 30}));
    QVERIFY(TemplateSelectionEditModel::align(document, ids, SelectionAlignment::Right, {80, 30}));
    QCOMPARE(rectOf(document.layers[0].elements[0]), QRectF(60, 3, 20, 10));
    QCOMPARE(rectOf(document.layers[0].elements[1]), QRectF(40, 20, 10, 5));
    QCOMPARE(rectOf(document.layers[0].elements[2]), QRectF(60, 20, 10, 5));
    QCOMPARE(rectOf(document.layers[1].elements[0]), QRectF(10, 10, 8, 5));
    QCOMPARE(rectOf(document.layers[2].elements[0]), QRectF(10, 10, 8, 5));
}

void SelectionTests::oversizedMoveAndPasteAreRejectedAtomically()
{
    auto document = documentWith({element("a", {0, 0, 10, 5}), element("b", {90, 0, 5, 5})});
    const auto before = TemplateDocumentJson::toJson(document);
    QVERIFY(!TemplateSelectionEditModel::move(document, {"a", "b"}, {2, 3}, {80, 30}));
    QVERIFY(TemplateSelectionEditModel::duplicate(document, {"a", "b"}, "main", {0, 0}, {80, 30}).isEmpty());
    QVERIFY(TemplateSelectionEditModel::paste(document, document.layers[0].elements, "main", {0, 0}, {80, 30}).isEmpty());
    QCOMPARE(TemplateDocumentJson::toJson(document), before);
}

void SelectionTests::resizeScalesGeometryButNotStyle()
{
    auto a = element("a", {10, 5, 10, 5});
    a.fontSizePt = 9.0;
    a.rotationDegrees = 90.0;
    a.verticalText = true;
    a.fieldKey = QStringLiteral("productName");
    a.autoFitFont = true;
    auto document = documentWith({a, element("b", {25, 15, 5, 5})});
    QVERIFY(TemplateSelectionEditModel::resize(document, {"a", "b"}, {20, 3, 40, 24}, {80, 40}));
    QCOMPARE(rectOf(document.layers[0].elements[0]), QRectF(20, 3, 20, 8));
    QCOMPARE(rectOf(document.layers[0].elements[1]), QRectF(50, 19, 10, 8));
    QCOMPARE(document.layers[0].elements[0].fontSizePt, 9.0);
    QCOMPARE(document.layers[0].elements[0].rotationDegrees, 90.0);
    QVERIFY(document.layers[0].elements[0].verticalText);
    QVERIFY(document.layers[0].elements[0].autoFitFont);
    QCOMPARE(document.layers[0].elements[0].fieldKey, QStringLiteral("productName"));
}

void SelectionTests::resizeEnforcesMinimumByScalingWholeSelection()
{
    auto document = documentWith({element("a", {0, 0, 1, 2}), element("b", {10, 5, 2, 1})});
    QVERIFY(TemplateSelectionEditModel::resize(document, {"a", "b"}, {10, 10, 0.1, 0.1}, {80, 30}));
    QCOMPARE(rectOf(document.layers[0].elements[0]), QRectF(10, 10, 0.5, 1));
    QCOMPARE(rectOf(document.layers[0].elements[1]), QRectF(15, 12.5, 1, 0.5));
}

void SelectionTests::resizeClampsTargetToPaper()
{
    auto document = documentWith({element("a", {2, 2, 10, 5}), element("b", {17, 12, 5, 5})});
    QVERIFY(TemplateSelectionEditModel::resize(document, {"a", "b"}, {-20, 100, 160, 60}, {80, 30}));
    QCOMPARE(rectOf(document.layers[0].elements[0]), QRectF(0, 0, 40, 10));
    QCOMPARE(rectOf(document.layers[0].elements[1]), QRectF(60, 20, 20, 10));
}

void SelectionTests::resizeRejectsImpossibleMinimum()
{
    auto document = documentWith({element("a", {0, 0, 0.1, 1}), element("b", {79, 0, 1, 1})});
    const auto before = TemplateDocumentJson::toJson(document);
    QVERIFY(!TemplateSelectionEditModel::resize(document, {"a", "b"}, {0, 0, 80, 30}, {80, 30}));
    QCOMPARE(TemplateDocumentJson::toJson(document), before);
}

void SelectionTests::noOpTransformsReturnFalse()
{
    auto document = documentWith({element("a", {0, 0, 10, 5}), element("b", {15, 0, 10, 5})});
    const auto before = TemplateDocumentJson::toJson(document);
    QVERIFY(!TemplateSelectionEditModel::move(document, {"a", "b"}, {0, 0}, {80, 30}));
    QVERIFY(!TemplateSelectionEditModel::resize(document, {"a", "b"}, {0, 0, 25, 5}, {80, 30}));
    QVERIFY(!TemplateSelectionEditModel::align(document, {"a", "b"}, SelectionAlignment::Top, {80, 30}));
    QVERIFY(!TemplateSelectionEditModel::align(document, {"a"}, SelectionAlignment::Left, {80, 30}));
    QVERIFY(!TemplateSelectionEditModel::align(document, {"a"}, static_cast<SelectionAlignment>(999), {80, 30}));
    QCOMPARE(TemplateDocumentJson::toJson(document), before);
}

void SelectionTests::alignMultiple_data()
{
    QTest::addColumn<int>("alignment");
    QTest::addColumn<QPointF>("first");
    QTest::addColumn<QPointF>("second");
    QTest::newRow("left") << int(SelectionAlignment::Left) << QPointF(10, 4) << QPointF(10, 20);
    QTest::newRow("horizontal-center") << int(SelectionAlignment::HorizontalCenter) << QPointF(30, 4) << QPointF(25, 20);
    QTest::newRow("right") << int(SelectionAlignment::Right) << QPointF(50, 4) << QPointF(40, 20);
    QTest::newRow("top") << int(SelectionAlignment::Top) << QPointF(10, 4) << QPointF(40, 4);
    QTest::newRow("vertical-center") << int(SelectionAlignment::VerticalCenter) << QPointF(10, 13) << QPointF(40, 12);
    QTest::newRow("bottom") << int(SelectionAlignment::Bottom) << QPointF(10, 22) << QPointF(40, 20);
}

void SelectionTests::alignMultiple()
{
    QFETCH(int, alignment);
    QFETCH(QPointF, first);
    QFETCH(QPointF, second);
    auto document = documentWith({element("a", {10, 4, 10, 3}), element("b", {40, 20, 20, 5})});
    QVERIFY(TemplateSelectionEditModel::align(document, {"a", "b"}, static_cast<SelectionAlignment>(alignment), {80, 30}));
    QCOMPARE(rectOf(document.layers[0].elements[0]), QRectF(first, QSizeF(10, 3)));
    QCOMPARE(rectOf(document.layers[0].elements[1]), QRectF(second, QSizeF(20, 5)));
}

void SelectionTests::alignSingleToPaper_data()
{
    QTest::addColumn<int>("alignment");
    QTest::addColumn<QPointF>("position");
    QTest::newRow("left") << int(SelectionAlignment::Left) << QPointF(0, 4);
    QTest::newRow("horizontal-center") << int(SelectionAlignment::HorizontalCenter) << QPointF(30, 4);
    QTest::newRow("right") << int(SelectionAlignment::Right) << QPointF(60, 4);
    QTest::newRow("top") << int(SelectionAlignment::Top) << QPointF(10, 0);
    QTest::newRow("vertical-center") << int(SelectionAlignment::VerticalCenter) << QPointF(10, 11);
    QTest::newRow("bottom") << int(SelectionAlignment::Bottom) << QPointF(10, 22);
}

void SelectionTests::alignSingleToPaper()
{
    QFETCH(int, alignment);
    QFETCH(QPointF, position);
    auto document = documentWith({element("a", {10, 4, 20, 8})});
    QVERIFY(TemplateSelectionEditModel::align(document, {"a"}, static_cast<SelectionAlignment>(alignment), {80, 30}));
    QCOMPARE(rectOf(document.layers[0].elements[0]), QRectF(position, QSizeF(20, 8)));
}

void SelectionTests::distributeUsesEqualEdgeGaps_data()
{
    QTest::addColumn<bool>("horizontal");
    QTest::newRow("horizontal") << true;
    QTest::newRow("vertical") << false;
}

void SelectionTests::distributeUsesEqualEdgeGaps()
{
    QFETCH(bool, horizontal);
    auto a = element("a", {2, 4, 10, 5});
    auto b = element("b", {22, 14, 5, 2});
    auto c = element("c", {42, 9, 15, 4});
    if (!horizontal) {
        a = element("a", {4, 2, 5, 10});
        b = element("b", {14, 22, 2, 5});
        c = element("c", {9, 42, 4, 15});
    }
    auto document = documentWith({c, a, b});
    const auto alignment = horizontal ? SelectionAlignment::DistributeHorizontally : SelectionAlignment::DistributeVertically;
    QVERIFY(TemplateSelectionEditModel::align(document, {"b", "c", "a", "a"}, alignment, {80, 80}));
    QCOMPARE(rectOf(document.layers[0].elements[0]), rectOf(c));
    QCOMPARE(rectOf(document.layers[0].elements[1]), rectOf(a));
    QCOMPARE(rectOf(document.layers[0].elements[2]), horizontal ? QRectF(24.5, 14, 5, 2) : QRectF(14, 24.5, 2, 5));
}

void SelectionTests::distributeRequiresThreeEditableDistinctItems()
{
    auto document = documentWith({element("a", {1, 2, 10, 5}), element("b", {20, 10, 10, 5}),
        element("locked", {40, 20, 10, 5})});
    document.layers[0].elements[2].locked = true;
    const auto before = TemplateDocumentJson::toJson(document);
    QVERIFY(!TemplateSelectionEditModel::align(document, {"a", "b", "a", "locked"}, SelectionAlignment::DistributeHorizontally, {80, 30}));
    QVERIFY(!TemplateSelectionEditModel::align(document, {"a"}, SelectionAlignment::DistributeVertically, {80, 30}));
    QCOMPARE(TemplateDocumentJson::toJson(document), before);
}

void SelectionTests::distributeOverlappingItemsStaysWithinReference()
{
    auto document = documentWith({element("a", {0, 0, 10, 3}), element("b", {2, 5, 10, 3}),
        element("c", {6, 10, 10, 3})});
    QVERIFY(TemplateSelectionEditModel::align(document, {"a", "b", "c"}, SelectionAlignment::DistributeHorizontally, {80, 30}));
    QCOMPARE(rectOf(document.layers[0].elements[0]), QRectF(0, 0, 10, 3));
    QCOMPARE(rectOf(document.layers[0].elements[1]), QRectF(3, 5, 10, 3));
    QCOMPARE(rectOf(document.layers[0].elements[2]), QRectF(6, 10, 10, 3));

    // 特殊重叠关系无法在原选区中保持顺序和等间距时，不能偷偷扩大选区或颠倒顺序。
    document = documentWith({element("a", {0, 0, 1, 3}), element("b", {1, 5, 100, 3}),
        element("c", {2, 10, 100, 3})});
    const auto before = TemplateDocumentJson::toJson(document);
    QVERIFY(!TemplateSelectionEditModel::align(document, {"a", "b", "c"}, SelectionAlignment::DistributeHorizontally, {200, 30}));
    QCOMPARE(TemplateDocumentJson::toJson(document), before);
}

void SelectionTests::alignClampsAsOneSelection()
{
    auto document = documentWith({element("a", {-5, -4, 10, 5}), element("b", {20, 10, 20, 5})});
    QVERIFY(TemplateSelectionEditModel::align(document, {"a", "b"}, SelectionAlignment::Left, {80, 30}));
    QCOMPARE(rectOf(document.layers[0].elements[0]), QRectF(0, 0, 10, 5));
    QCOMPARE(rectOf(document.layers[0].elements[1]), QRectF(0, 14, 20, 5));
}

void SelectionTests::duplicatePreservesPropertiesAndPaintOrder()
{
    auto a = element("a", {10, 3, 10, 4}, 8);
    a.type = TemplateElementType::ArrayGrid;
    a.displayName = QStringLiteral("自定义网格");
    a.text = QStringLiteral("固定文本\n${productName}");
    a.fieldKey = QStringLiteral("productName");
    a.payload = QStringLiteral("https://example.test/${id}");
    a.fontSizePt = 9.5;
    a.bold = true;
    a.rotationDegrees = 90;
    a.verticalText = true;
    a.dataPath = QStringLiteral("rows");
    a.arrayGridRows = 4;
    a.arrayGridColumns = 6;
    a.arrayGridRowHeightMm = 2.5;
    a.arrayGridCellTemplate = QStringLiteral("${name}\n${weight}");
    a.arrayGridDrawBorders = false;
    a.maxLines = 3;
    a.ellipsis = true;
    a.autoFitFont = true;
    a.autoFitMinFontSizePt = 2.5;
    a.autoFitMaxFontSizePt = 15;
    const auto b = element("b", {25, 12, 5, 6}, 2);
    auto document = documentWith({a, b});
    TemplateLayer destination;
    destination.id = QStringLiteral("destination");
    destination.elements = {element("existing", {0, 0, 1, 1}, 20)};
    TableElement table;
    table.id = QStringLiteral("table");
    table.zIndex = 42;
    destination.tables.append(table);
    document.layers.append(destination);

    const auto ids = TemplateSelectionEditModel::duplicate(document, {"a", "b", "a"}, "destination", {100, -100}, {80, 30});
    QCOMPARE(ids.size(), 2);
    QCOMPARE(document.layers[0].elements.size(), 2);
    QCOMPARE(rectOf(document.layers[0].elements[0]), rectOf(a));
    const auto& copiedB = document.layers[1].elements[1];
    const auto& copiedA = document.layers[1].elements[2];
    QCOMPARE(copiedB.id, ids[0]);
    QCOMPARE(copiedA.id, ids[1]);
    QVERIFY(copiedA.id != a.id && copiedB.id != b.id && copiedA.id != copiedB.id);
    QCOMPARE(copiedA.layerId, QStringLiteral("destination"));
    QCOMPARE(copiedB.layerId, QStringLiteral("destination"));
    QCOMPARE(copiedB.zIndex, 43);
    QCOMPARE(copiedA.zIndex, 44);
    QCOMPARE(rectOf(copiedA), QRectF(60, 0, 10, 4));
    QCOMPARE(rectOf(copiedB), QRectF(75, 9, 5, 6));
    QCOMPARE(propertiesWithoutIdentityAndPosition(copiedA), propertiesWithoutIdentityAndPosition(a));
    QCOMPARE(propertiesWithoutIdentityAndPosition(copiedB), propertiesWithoutIdentityAndPosition(b));
}

void SelectionTests::duplicateFiltersProtectedSources()
{
    auto document = documentWith({element("a", {1, 2, 10, 5}), element("locked", {40, 20, 10, 5})});
    document.layers[0].elements[1].locked = true;
    auto hiddenLayer = document.layers[0];
    hiddenLayer.id = QStringLiteral("hidden-layer");
    hiddenLayer.visible = false;
    hiddenLayer.elements = {element("hidden", {0, 0, 10, 5})};
    document.layers.append(hiddenLayer);
    auto lockedLayer = hiddenLayer;
    lockedLayer.id = QStringLiteral("locked-layer");
    lockedLayer.visible = true;
    lockedLayer.locked = true;
    lockedLayer.elements[0].id = QStringLiteral("layer-locked");
    document.layers.append(lockedLayer);
    const auto ids = TemplateSelectionEditModel::duplicate(document, {"a", "locked", "hidden", "layer-locked"}, "main", {2, 2}, {80, 30});
    QCOMPARE(ids.size(), 1);
    QCOMPARE(document.layers[0].elements.size(), 3);
    QCOMPARE(rectOf(document.layers[0].elements.last()), QRectF(3, 4, 10, 5));
}

void SelectionTests::duplicatePreservesLayerOrderAndStableZIndexTies()
{
    auto a = element("a", {1, 2, 10, 5}, 5);
    a.text = QStringLiteral("a");
    auto b = element("b", {12, 2, 10, 5}, 5);
    b.text = QStringLiteral("b");
    auto c = element("c", {23, 2, 10, 5}, -10);
    c.text = QStringLiteral("c");
    auto document = documentWith({a, b});
    TemplateLayer secondLayer;
    secondLayer.id = QStringLiteral("second");
    secondLayer.elements = {c};
    document.layers.append(secondLayer);
    const auto ids = TemplateSelectionEditModel::duplicate(document, {"c", "b", "a"}, "main", {1, 1}, {80, 30});
    QCOMPARE(ids.size(), 3);
    QCOMPARE(document.layers[0].elements[2].text, QStringLiteral("a"));
    QCOMPARE(document.layers[0].elements[3].text, QStringLiteral("b"));
    QCOMPARE(document.layers[0].elements[4].text, QStringLiteral("c"));
    QCOMPARE(document.layers[0].elements[2].zIndex, 6);
    QCOMPARE(document.layers[0].elements[3].zIndex, 7);
    QCOMPARE(document.layers[0].elements[4].zIndex, 8);
}

void SelectionTests::pastePreservesInputOrderAndClampsTogether()
{
    auto first = element("clipboard-a", {10, 3, 10, 4}, 99);
    first.type = TemplateElementType::BoundField;
    first.fieldKey = QStringLiteral("weight");
    first.bold = true;
    first.ellipsis = true;
    first.autoFitFont = true;
    auto second = element("clipboard-b", {25, 12, 5, 6}, -3);
    second.type = TemplateElementType::QrCode;
    second.payload = QStringLiteral("${serial}");
    auto document = documentWith({element("existing", {0, 0, 1, 1}, 10)});

    const auto ids = TemplateSelectionEditModel::paste(document, {first, second}, "main", {100, -100}, {80, 30});
    QCOMPARE(ids.size(), 2);
    const auto& pastedFirst = document.layers[0].elements[1];
    const auto& pastedSecond = document.layers[0].elements[2];
    QCOMPARE(pastedFirst.id, ids[0]);
    QCOMPARE(pastedSecond.id, ids[1]);
    QCOMPARE(pastedFirst.zIndex, 11);
    QCOMPARE(pastedSecond.zIndex, 12);
    QCOMPARE(rectOf(pastedFirst), QRectF(60, 0, 10, 4));
    QCOMPARE(rectOf(pastedSecond), QRectF(75, 9, 5, 6));
    QCOMPARE(propertiesWithoutIdentityAndPosition(pastedFirst), propertiesWithoutIdentityAndPosition(first));
    QCOMPARE(propertiesWithoutIdentityAndPosition(pastedSecond), propertiesWithoutIdentityAndPosition(second));
}

void SelectionTests::pasteRejectsUnwritableTarget_data()
{
    QTest::addColumn<QString>("target");
    QTest::addColumn<bool>("locked");
    QTest::addColumn<bool>("visible");
    QTest::newRow("missing") << QStringLiteral("missing") << false << true;
    QTest::newRow("empty") << QString() << false << true;
    QTest::newRow("locked") << QStringLiteral("main") << true << true;
    QTest::newRow("hidden") << QStringLiteral("main") << false << false;
}

void SelectionTests::pasteRejectsUnwritableTarget()
{
    QFETCH(QString, target);
    QFETCH(bool, locked);
    QFETCH(bool, visible);
    auto document = documentWith({element("a", {1, 2, 10, 5})});
    document.layers[0].locked = locked;
    document.layers[0].visible = visible;
    const auto before = TemplateDocumentJson::toJson(document);
    QVERIFY(TemplateSelectionEditModel::paste(document, {element("copy", {0, 0, 10, 5})}, target, {2, 2}, {80, 30}).isEmpty());
    QVERIFY(TemplateSelectionEditModel::duplicate(document, {"a"}, target, {2, 2}, {80, 30}).isEmpty());
    QCOMPARE(TemplateDocumentJson::toJson(document), before);
}

void SelectionTests::pasteSkipsHiddenAndLockedItems()
{
    auto document = documentWith({});
    auto hidden = element("hidden", {0, 0, 1, 1});
    hidden.visible = false;
    auto locked = element("locked", {0, 0, 1, 1});
    locked.locked = true;
    const auto ids = TemplateSelectionEditModel::paste(document, {hidden, element("a", {1, 2, 10, 5}), locked}, "main", {0, 0}, {80, 30});
    QCOMPARE(ids.size(), 1);
    QCOMPARE(rectOf(document.layers[0].elements[0]), QRectF(1, 2, 10, 5));
    QCOMPARE(document.layers[0].elements[0].zIndex, 0);
}

void SelectionTests::pasteIdsRemainUniqueAcrossCalls()
{
    auto document = documentWith({element("a", {0, 0, 10, 5})});
    TableElement table;
    table.id = QStringLiteral("table");
    document.layers[0].tables.append(table);
    const auto first = TemplateSelectionEditModel::paste(document, document.layers[0].elements, "main", {1, 1}, {80, 30});
    const auto second = TemplateSelectionEditModel::paste(document, document.layers[0].elements, "main", {1, 1}, {80, 30});
    QCOMPARE(first.size(), 1);
    QCOMPARE(second.size(), 2);
    QSet<QString> seen{QStringLiteral("table")};
    for (const auto& value : document.layers[0].elements) {
        QVERIFY(!value.id.isEmpty());
        QVERIFY(!seen.contains(value.id));
        seen.insert(value.id);
    }
}

void SelectionTests::pasteRejectsZIndexOverflow()
{
    auto document = documentWith({element("a", {0, 0, 10, 5}, std::numeric_limits<int>::max() - 1)});
    const auto before = TemplateDocumentJson::toJson(document);
    const QList<TemplateElement> incoming{element("first", {1, 1, 1, 1}), element("second", {2, 2, 1, 1})};
    QVERIFY(TemplateSelectionEditModel::paste(document, incoming, "main", {0, 0}, {80, 30}).isEmpty());
    QCOMPARE(TemplateDocumentJson::toJson(document), before);
    document.layers[0].elements[0].zIndex = std::numeric_limits<int>::max();
    QVERIFY(TemplateSelectionEditModel::duplicate(document, {"a"}, "main", {0, 0}, {80, 30}).isEmpty());
}

void SelectionTests::invalidPaperIsRejected_data()
{
    QTest::addColumn<QSizeF>("paper");
    const auto nan = std::numeric_limits<double>::quiet_NaN();
    const auto inf = std::numeric_limits<double>::infinity();
    QTest::newRow("zero-width") << QSizeF(0, 30);
    QTest::newRow("negative-height") << QSizeF(80, -30);
    QTest::newRow("nan-width") << QSizeF(nan, 30);
    QTest::newRow("nan-height") << QSizeF(80, nan);
    QTest::newRow("infinite-width") << QSizeF(inf, 30);
    QTest::newRow("infinite-height") << QSizeF(80, inf);
}

void SelectionTests::invalidPaperIsRejected()
{
    QFETCH(QSizeF, paper);
    auto document = documentWith({element("a", {1, 2, 10, 5})});
    const auto before = TemplateDocumentJson::toJson(document);
    QVERIFY(!TemplateSelectionEditModel::move(document, {"a"}, {1, 1}, paper));
    QVERIFY(!TemplateSelectionEditModel::resize(document, {"a"}, {0, 0, 20, 10}, paper));
    QVERIFY(!TemplateSelectionEditModel::align(document, {"a"}, SelectionAlignment::Left, paper));
    QVERIFY(TemplateSelectionEditModel::duplicate(document, {"a"}, "main", {1, 1}, paper).isEmpty());
    QVERIFY(TemplateSelectionEditModel::paste(document, document.layers[0].elements, "main", {1, 1}, paper).isEmpty());
    QCOMPARE(TemplateDocumentJson::toJson(document), before);
}

void SelectionTests::invalidOffsetIsRejected_data()
{
    QTest::addColumn<QPointF>("offset");
    const auto nan = std::numeric_limits<double>::quiet_NaN();
    const auto inf = std::numeric_limits<double>::infinity();
    QTest::newRow("nan-x") << QPointF(nan, 0);
    QTest::newRow("nan-y") << QPointF(0, nan);
    QTest::newRow("infinite-x") << QPointF(inf, 0);
    QTest::newRow("infinite-y") << QPointF(0, -inf);
}

void SelectionTests::invalidOffsetIsRejected()
{
    QFETCH(QPointF, offset);
    auto document = documentWith({element("a", {1, 2, 10, 5})});
    const auto before = TemplateDocumentJson::toJson(document);
    QVERIFY(!TemplateSelectionEditModel::move(document, {"a"}, offset, {80, 30}));
    QVERIFY(TemplateSelectionEditModel::duplicate(document, {"a"}, "main", offset, {80, 30}).isEmpty());
    QVERIFY(TemplateSelectionEditModel::paste(document, document.layers[0].elements, "main", offset, {80, 30}).isEmpty());
    QCOMPARE(TemplateDocumentJson::toJson(document), before);
}

void SelectionTests::invalidGeometryIsRejectedAtomically_data()
{
    QTest::addColumn<QRectF>("rect");
    const auto nan = std::numeric_limits<double>::quiet_NaN();
    const auto inf = std::numeric_limits<double>::infinity();
    const auto max = std::numeric_limits<double>::max();
    QTest::newRow("negative-width") << QRectF(1, 2, -10, 5);
    QTest::newRow("negative-height") << QRectF(1, 2, 10, -5);
    QTest::newRow("zero-width") << QRectF(1, 2, 0, 5);
    QTest::newRow("zero-height") << QRectF(1, 2, 10, 0);
    QTest::newRow("nan-x") << QRectF(nan, 2, 10, 5);
    QTest::newRow("nan-width") << QRectF(1, 2, nan, 5);
    QTest::newRow("infinite-y") << QRectF(1, inf, 10, 5);
    QTest::newRow("infinite-height") << QRectF(1, 2, 10, inf);
    QTest::newRow("overflow-right") << QRectF(max, 2, max, 5);
}

void SelectionTests::invalidGeometryIsRejectedAtomically()
{
    QFETCH(QRectF, rect);
    auto document = documentWith({element("valid", {1, 2, 10, 5}), element("invalid", rect)});
    const auto before = TemplateDocumentJson::toJson(document);
    const QStringList ids{"valid", "invalid"};
    QCOMPARE(TemplateSelectionEditModel::bounds(document, ids), QRectF());
    QVERIFY(!TemplateSelectionEditModel::move(document, ids, {1, 1}, {80, 30}));
    QVERIFY(!TemplateSelectionEditModel::resize(document, ids, {0, 0, 20, 10}, {80, 30}));
    QVERIFY(!TemplateSelectionEditModel::align(document, ids, SelectionAlignment::Left, {80, 30}));
    QVERIFY(TemplateSelectionEditModel::duplicate(document, ids, "main", {1, 1}, {80, 30}).isEmpty());
    QVERIFY(TemplateSelectionEditModel::paste(document, document.layers[0].elements, "main", {1, 1}, {80, 30}).isEmpty());
    QCOMPARE(TemplateDocumentJson::toJson(document), before);
}

void SelectionTests::invalidResizeTargetIsRejected_data()
{
    QTest::addColumn<QRectF>("target");
    const auto nan = std::numeric_limits<double>::quiet_NaN();
    const auto inf = std::numeric_limits<double>::infinity();
    QTest::newRow("negative-width") << QRectF(1, 2, -10, 5);
    QTest::newRow("negative-height") << QRectF(1, 2, 10, -5);
    QTest::newRow("zero-width") << QRectF(1, 2, 0, 5);
    QTest::newRow("nan-x") << QRectF(nan, 2, 10, 5);
    QTest::newRow("nan-height") << QRectF(1, 2, 10, nan);
    QTest::newRow("infinite-width") << QRectF(1, 2, inf, 5);
}

void SelectionTests::invalidResizeTargetIsRejected()
{
    QFETCH(QRectF, target);
    auto document = documentWith({element("a", {1, 2, 10, 5})});
    const auto before = TemplateDocumentJson::toJson(document);
    QVERIFY(!TemplateSelectionEditModel::resize(document, {"a"}, target, {80, 30}));
    QCOMPARE(TemplateDocumentJson::toJson(document), before);
}

QTEST_APPLESS_MAIN(SelectionTests)
#include "tst_selection.moc"

#include "sleekpr/core/templates/TemplateDocumentRenderer.h"
#include "sleekpr/core/templates/TableElementLayout.h"
#include <QJsonArray>
#include <QtTest>
#include <limits>

using namespace sleekpr::core;

namespace {

TemplateDocument documentWithElements(int count)
{
    TemplateDocument document;
    TemplateLayer layer;
    layer.id = QStringLiteral("base");
    for (int i = 0; i < count; ++i) {
        TemplateElement element;
        element.id = QStringLiteral("element-%1").arg(i);
        element.type = TemplateElementType::FixedText;
        element.text = QStringLiteral("LABEL");
        element.width = 10;
        element.height = 5;
        layer.elements.append(element);
    }
    document.layers = {layer};
    return document;
}

TableElement simpleTable(const QString& id, bool complex = false)
{
    TableElement table;
    table.id = id;
    table.dataPath = QStringLiteral("rows");
    table.width = 20;
    table.height = 10;
    table.headerRowHeightMm = 5;
    table.detailRowHeightMm = 5;
    table.repeatHeaderOnPage = true;
    TableColumn column;
    column.id = QStringLiteral("value");
    column.title = QStringLiteral("名称");
    column.fieldKey = QStringLiteral("value");
    column.widthMm = 20;
    column.wrapText = complex;
    table.columns = {column};
    return table;
}

TemplateRenderContext rowsContext(int count)
{
    TemplateRenderContext context;
    QJsonArray rows;
    for (int i = 0; i < count; ++i) rows.append(QJsonObject{{QStringLiteral("value"), QStringLiteral("A")}});
    context.values.insert(QStringLiteral("rows"), rows);
    return context;
}

TemplatePrintRenderResult render(const TemplateDocument& document, const TemplateRenderContext& context, PreviewPlanningBudget* budget)
{
    return TemplateDocumentRenderer().tryRenderPrint(document, LabelRenderPlan{}, LabelOffset{}, DeviceProfile{}, context, budget);
}

} // 匿名命名空间

class PreviewPlanningBudgetTests final : public QObject
{
    Q_OBJECT
private slots:
    void limitsCommandsBeforeAppending();
    void arrayGridRejectsBeforeExpansion();
    void hugeGridCapacityDoesNotOverflow();
    void legacyTablesRespectRemainingPages();
    void complexTablesRespectRemainingPages();
    void layoutLimitsCellsBeforeMeasurement();
    void countsCellsAcrossTables();
    void countsFinalPagesRatherThanOverlaidTableSum();
    void sharedBudgetCountsDocuments();
    void normalPrintDoesNotUsePreviewLimits();
    void pageEstimateDoesNotConsume();
};

void PreviewPlanningBudgetTests::limitsCommandsBeforeAppending()
{
    PreviewPlanningBudget budget;
    budget.maxCommands = 1;
    const auto result = render(documentWithElements(2), {}, &budget);
    QVERIFY(!result.success());
    QCOMPARE(budget.exceeded, PreviewPlanningLimit::Commands);
    QVERIFY(!result.errorMessage.isEmpty());
}

void PreviewPlanningBudgetTests::arrayGridRejectsBeforeExpansion()
{
    auto document = documentWithElements(1);
    auto& element = document.layers.first().elements.first();
    element.type = TemplateElementType::ArrayGrid;
    element.arrayGridRows = 10;
    element.arrayGridColumns = 10;
    element.arrayGridDrawBorders = true;
    element.dataPath = QStringLiteral("rows");
    PreviewPlanningBudget budget;
    budget.maxCells = 5;
    QVERIFY(!render(document, rowsContext(1), &budget).success());
    QCOMPARE(budget.exceeded, PreviewPlanningLimit::Cells);
    QCOMPARE(budget.usedCommands, 0);
}

void PreviewPlanningBudgetTests::hugeGridCapacityDoesNotOverflow()
{
    auto document = documentWithElements(1);
    auto& element = document.layers.first().elements.first();
    element.type = TemplateElementType::ArrayGrid;
    element.arrayGridRows = std::numeric_limits<int>::max();
    element.arrayGridColumns = std::numeric_limits<int>::max();
    element.arrayGridDrawBorders = true;
    PreviewPlanningBudget budget;
    budget.maxCells = 10;
    QVERIFY(!render(document, {}, &budget).success());
    QCOMPARE(budget.exceeded, PreviewPlanningLimit::Cells);
    QCOMPARE(budget.usedCells, 0);
}

void PreviewPlanningBudgetTests::legacyTablesRespectRemainingPages()
{
    auto document = documentWithElements(0);
    document.layers.first().tables = {simpleTable(QStringLiteral("legacy"))};
    PreviewPlanningBudget budget;
    budget.maxPages = 1;
    QVERIFY(!render(document, rowsContext(2), &budget).success());
    QCOMPARE(budget.exceeded, PreviewPlanningLimit::Pages);
}

void PreviewPlanningBudgetTests::complexTablesRespectRemainingPages()
{
    auto document = documentWithElements(0);
    document.layers.first().tables = {simpleTable(QStringLiteral("complex"), true)};
    PreviewPlanningBudget budget;
    budget.maxPages = 1;
    QVERIFY(!render(document, rowsContext(2), &budget).success());
    QCOMPARE(budget.exceeded, PreviewPlanningLimit::Pages);
}

void PreviewPlanningBudgetTests::layoutLimitsCellsBeforeMeasurement()
{
    PreviewPlanningBudget budget;
    budget.maxCells = 1;
    const auto result = TableElementLayout::layout(simpleTable(QStringLiteral("complex"), true), rowsContext(3), &budget);
    QVERIFY(!result.success());
    QCOMPARE(budget.exceeded, PreviewPlanningLimit::Cells);
    QVERIFY(result.pages.isEmpty());
}

void PreviewPlanningBudgetTests::countsCellsAcrossTables()
{
    auto document = documentWithElements(0);
    document.layers.first().tables = {simpleTable(QStringLiteral("one")), simpleTable(QStringLiteral("two"))};
    PreviewPlanningBudget budget;
    budget.maxCells = 3;
    QVERIFY(!render(document, rowsContext(1), &budget).success());
    QCOMPARE(budget.exceeded, PreviewPlanningLimit::Cells);
}

void PreviewPlanningBudgetTests::countsFinalPagesRatherThanOverlaidTableSum()
{
    auto document = documentWithElements(0);
    document.layers.first().tables = {simpleTable(QStringLiteral("one")), simpleTable(QStringLiteral("two"), true)};
    PreviewPlanningBudget budget;
    budget.maxPages = 3;
    const auto result = render(document, rowsContext(3), &budget);
    QVERIFY2(result.success(), qPrintable(result.errorMessage));
    QCOMPARE(result.plan.pages.size(), 3);
    QCOMPARE(budget.usedPages, 3);
}

void PreviewPlanningBudgetTests::sharedBudgetCountsDocuments()
{
    PreviewPlanningBudget budget;
    budget.maxPages = 1;
    QVERIFY(render(documentWithElements(1), {}, &budget).success());
    QVERIFY(!render(documentWithElements(1), {}, &budget).success());
    QCOMPARE(budget.exceeded, PreviewPlanningLimit::Pages);
}

void PreviewPlanningBudgetTests::normalPrintDoesNotUsePreviewLimits()
{
    const auto document = documentWithElements(2);
    PreviewPlanningBudget budget;
    budget.maxCommands = 1;
    QVERIFY(!render(document, {}, &budget).success());
    const auto normal = render(document, {}, nullptr);
    QVERIFY(normal.success());
    QCOMPARE(normal.plan.pages.first().commands.size(), 2);
}

void PreviewPlanningBudgetTests::pageEstimateDoesNotConsume()
{
    PreviewPlanningBudget budget;
    budget.maxPages = 2;
    QVERIFY(budget.ensurePages(2));
    QCOMPARE(budget.usedPages, 0);
    QVERIFY(budget.consumePages(1));
    QVERIFY(!budget.ensurePages(2));
    QCOMPARE(budget.exceeded, PreviewPlanningLimit::Pages);
}

QTEST_MAIN(PreviewPlanningBudgetTests)
#include "tst_preview_planning_budget.moc"

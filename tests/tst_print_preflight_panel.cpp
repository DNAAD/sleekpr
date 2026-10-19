#include "sleekpr/app/AppTheme.h"
#include "sleekpr/app/PrintPreflightPanel.h"

#include <QAccessible>
#include <QListWidget>
#include <QRegularExpression>
#include <QScrollBar>
#include <QToolButton>
#include <QVBoxLayout>
#include <QtTest>

using namespace sleekpr;

namespace {

core::PrintPreflightReport mixedReport()
{
    core::PrintPreflightIssue printer;
    printer.severity = core::PrintPreflightSeverity::Warning;
    printer.code = QStringLiteral("printer-capabilities");
    printer.message = QStringLiteral("打印机未提供纸张能力信息");
    printer.target = core::PrintPreflightTarget::Printer;

    core::PrintPreflightIssue field;
    field.code = QStringLiteral("required-field");
    field.message = QStringLiteral("商品名称不能为空");
    field.target = core::PrintPreflightTarget::Field;
    field.fieldKey = QStringLiteral("productName");
    field.elementIds = {QStringLiteral("title"), QStringLiteral("barcode")};
    field.line = 7;
    field.column = 12;

    core::PrintPreflightIssue copies;
    copies.severity = core::PrintPreflightSeverity::Warning;
    copies.code = QStringLiteral("many-copies");
    copies.message = QStringLiteral("本次打印份数较多");
    copies.target = core::PrintPreflightTarget::Copies;

    core::PrintPreflightIssue element;
    element.code = QStringLiteral("element-outside-paper");
    element.message = QStringLiteral("条码超出纸张边界");
    element.target = core::PrintPreflightTarget::Element;
    element.elementIds = {QStringLiteral("barcode")};

    core::PrintPreflightReport report;
    // 故意交错等级，验证面板只调整展示顺序，不更改输入或丢失定位信息。
    report.issues = {printer, field, copies, element};
    return report;
}

bool hasCount(const QString& text, int count, const QString& severity)
{
    return text.contains(QRegularExpression(QStringLiteral("(?<!\\d)%1\\s*(?:项|个)?\\s*%2")
        .arg(count).arg(QRegularExpression::escape(severity))));
}

void compareIssues(const core::PrintPreflightIssue& actual, const core::PrintPreflightIssue& expected)
{
    QCOMPARE(actual.severity, expected.severity);
    QCOMPARE(actual.code, expected.code);
    QCOMPARE(actual.message, expected.message);
    QCOMPARE(actual.target, expected.target);
    QCOMPARE(actual.fieldKey, expected.fieldKey);
    QCOMPARE(actual.elementIds, expected.elementIds);
    QCOMPARE(actual.line, expected.line);
    QCOMPARE(actual.column, expected.column);
}

} // 匿名命名空间

class PrintPreflightPanelTests final : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase() { app::applySleekprTheme(*qApp); }
    void startsPendingWithoutIssues();
    void reportGroupsErrorsBeforeWarningsAndSummarizes();
    void warningsRemainEnabledAndNavigable();
    void emptyReportPassesAndRemovesPreviousIssues_data();
    void emptyReportPassesAndRemovesPreviousIssues();
    void pendingClearsOldIssuesImmediatelyAndBlocksActivation();
    void mouseClickActivatesExactlyOnceWithOriginalIssue();
    void enterActivatesExactlyOnce_data();
    void enterActivatesExactlyOnce();
    void replacingReportCannotActivateOldIssue();
    void activationRemainsValidWhenFirstReceiverClearsReport();
    void manyIssuesDoNotCreatePerRowWidgets();
    void longMessagesWrapWithoutGrowingPanel_data();
    void longMessagesWrapWithoutGrowingPanel();
    void collapsingLeavesOnlyTheHeaderAndKeyboardCanExpand();
    void accessibleControlsSupportKeyboardNavigation();
};

void PrintPreflightPanelTests::startsPendingWithoutIssues()
{
    app::PrintPreflightPanel panel;
    QCOMPARE(panel.objectName(), QStringLiteral("printPreflightPanel"));
    auto* summary = panel.findChild<QToolButton*>(QStringLiteral("printPreflightSummary"));
    auto* issues = panel.findChild<QListWidget*>(QStringLiteral("printPreflightIssues"));
    QVERIFY(summary != nullptr && issues != nullptr);
    QVERIFY(summary->text().contains(QStringLiteral("待检查")));
    QCOMPARE(issues->count(), 0);
    QVERIFY(!issues->isEnabled());
}

void PrintPreflightPanelTests::reportGroupsErrorsBeforeWarningsAndSummarizes()
{
    app::PrintPreflightPanel panel;
    const auto report = mixedReport();
    panel.setReport(report);
    auto* summary = panel.findChild<QToolButton*>(QStringLiteral("printPreflightSummary"));
    auto* issues = panel.findChild<QListWidget*>(QStringLiteral("printPreflightIssues"));
    QVERIFY(summary != nullptr && issues != nullptr);
    QCOMPARE(issues->count(), 4);
    QVERIFY(hasCount(summary->text(), 2, QStringLiteral("错误")));
    QVERIFY(hasCount(summary->text(), 2, QStringLiteral("警告")));
    QVERIFY(!summary->text().contains(QStringLiteral("检查通过")));
    const int sourceRows[] = {1, 3, 0, 2};
    for (int row = 0; row < 4; ++row) {
        const auto text = issues->item(row)->text();
        QVERIFY(text.contains(row < 2 ? QStringLiteral("错误") : QStringLiteral("警告")));
        QVERIFY(text.contains(report.issues[sourceRows[row]].message));
    }
    QCOMPARE(report.issues.first().code, QStringLiteral("printer-capabilities"));
}

void PrintPreflightPanelTests::warningsRemainEnabledAndNavigable()
{
    app::PrintPreflightPanel panel;
    core::PrintPreflightReport report;
    report.issues = {mixedReport().issues[0]};
    panel.setReport(report);
    panel.resize(420, 140);
    panel.show();
    auto* summary = panel.findChild<QToolButton*>(QStringLiteral("printPreflightSummary"));
    auto* issues = panel.findChild<QListWidget*>(QStringLiteral("printPreflightIssues"));
    QVERIFY(summary != nullptr && issues != nullptr);
    QCOMPARE(issues->count(), 1);
    QVERIFY(hasCount(summary->text(), 0, QStringLiteral("错误")));
    QVERIFY(hasCount(summary->text(), 1, QStringLiteral("警告")));
    QVERIFY(!summary->text().contains(QStringLiteral("检查通过")));
    QVERIFY(panel.isEnabled() && issues->isEnabled());
    QVERIFY(issues->item(0)->flags().testFlag(Qt::ItemIsEnabled));
    QVERIFY(issues->item(0)->flags().testFlag(Qt::ItemIsSelectable));
    if (!summary->isChecked()) summary->click();
    QTRY_VERIFY(issues->isVisible());
    QList<core::PrintPreflightIssue> activated;
    connect(&panel, &app::PrintPreflightPanel::issueActivated, &panel,
        [&activated](const core::PrintPreflightIssue& issue) { activated.append(issue); });
    QTest::mouseClick(issues->viewport(), Qt::LeftButton, Qt::NoModifier,
        issues->visualItemRect(issues->item(0)).center());
    QCOMPARE(activated.size(), 1);
    compareIssues(activated.first(), report.issues.first());
}

void PrintPreflightPanelTests::emptyReportPassesAndRemovesPreviousIssues_data()
{
    QTest::addColumn<bool>("hadIssues");
    QTest::newRow("initially-empty") << false;
    QTest::newRow("clears-previous") << true;
}

void PrintPreflightPanelTests::emptyReportPassesAndRemovesPreviousIssues()
{
    QFETCH(bool, hadIssues);
    app::PrintPreflightPanel panel;
    auto* summary = panel.findChild<QToolButton*>(QStringLiteral("printPreflightSummary"));
    auto* issues = panel.findChild<QListWidget*>(QStringLiteral("printPreflightIssues"));
    QVERIFY(summary != nullptr && issues != nullptr);
    if (hadIssues) {
        panel.setReport(mixedReport());
        QCOMPARE(issues->count(), 4);
    }
    panel.setReport({});
    QCOMPARE(issues->count(), 0);
    QVERIFY(issues->currentItem() == nullptr);
    QVERIFY(summary->text().contains(QStringLiteral("检查通过")));
    QVERIFY(!summary->text().contains(QStringLiteral("待检查")));
    QVERIFY(panel.isEnabled());
}

void PrintPreflightPanelTests::pendingClearsOldIssuesImmediatelyAndBlocksActivation()
{
    app::PrintPreflightPanel panel;
    panel.setReport(mixedReport());
    panel.resize(480, 140);
    panel.show();
    auto* summary = panel.findChild<QToolButton*>(QStringLiteral("printPreflightSummary"));
    auto* issues = panel.findChild<QListWidget*>(QStringLiteral("printPreflightIssues"));
    QVERIFY(summary != nullptr && issues != nullptr);
    QCOMPARE(issues->count(), 4);
    if (!summary->isChecked()) summary->click();
    QTRY_VERIFY(issues->isVisible());
    issues->setCurrentRow(0);
    const auto oldPosition = issues->visualItemRect(issues->item(0)).center();
    QList<core::PrintPreflightIssue> activated;
    connect(&panel, &app::PrintPreflightPanel::issueActivated, &panel,
        [&activated](const core::PrintPreflightIssue& issue) { activated.append(issue); });

    panel.setPending();
    // 不等待事件循环：开始新检查时必须同步撤销旧问题的定位能力。
    QCOMPARE(issues->count(), 0);
    QVERIFY(!issues->isEnabled());
    QVERIFY(issues->currentItem() == nullptr);
    QVERIFY(summary->text().contains(QStringLiteral("待检查")));
    QVERIFY(!summary->text().contains(QStringLiteral("检查通过")));
    QTest::mouseClick(issues->viewport(), Qt::LeftButton, Qt::NoModifier, oldPosition);
    QTest::keyClick(issues, Qt::Key_Return);
    QCoreApplication::processEvents();
    QVERIFY(activated.isEmpty());

    core::PrintPreflightReport fresh;
    fresh.issues = {mixedReport().issues[2]};
    panel.setReport(fresh);
    QCOMPARE(issues->count(), 1);
    QVERIFY(issues->isEnabled());
    if (!summary->isChecked()) summary->click();
    issues->setCurrentRow(0);
    QTest::keyClick(issues, Qt::Key_Return);
    QCOMPARE(activated.size(), 1);
    compareIssues(activated.first(), fresh.issues.first());
}

void PrintPreflightPanelTests::mouseClickActivatesExactlyOnceWithOriginalIssue()
{
    app::PrintPreflightPanel panel;
    const auto report = mixedReport();
    panel.setReport(report);
    panel.resize(480, 140);
    panel.show();
    auto* summary = panel.findChild<QToolButton*>(QStringLiteral("printPreflightSummary"));
    auto* issues = panel.findChild<QListWidget*>(QStringLiteral("printPreflightIssues"));
    QVERIFY(summary != nullptr && issues != nullptr);
    QCOMPARE(issues->count(), 4);
    if (!summary->isChecked()) summary->click();
    QTRY_VERIFY(issues->isVisible());
    QList<core::PrintPreflightIssue> activated;
    connect(&panel, &app::PrintPreflightPanel::issueActivated, &panel,
        [&activated](const core::PrintPreflightIssue& issue) { activated.append(issue); });
    QCOMPARE(activated.size(), 0);
    const auto position = issues->visualItemRect(issues->item(0)).center();
    QTest::mouseClick(issues->viewport(), Qt::LeftButton, Qt::NoModifier, position);
    QCoreApplication::processEvents();
    QCOMPARE(activated.size(), 1);
    compareIssues(activated[0], report.issues[1]);
    // 对同一条目再次主动点击应再定位一次，不能把防重发误做成永久去重。
    QTest::mouseClick(issues->viewport(), Qt::LeftButton, Qt::NoModifier, position);
    QCoreApplication::processEvents();
    QCOMPARE(activated.size(), 2);
    compareIssues(activated[1], report.issues[1]);
}

void PrintPreflightPanelTests::enterActivatesExactlyOnce_data()
{
    QTest::addColumn<int>("key");
    QTest::newRow("return") << int(Qt::Key_Return);
    QTest::newRow("keypad-enter") << int(Qt::Key_Enter);
}

void PrintPreflightPanelTests::enterActivatesExactlyOnce()
{
    QFETCH(int, key);
    app::PrintPreflightPanel panel;
    const auto report = mixedReport();
    panel.setReport(report);
    panel.resize(480, 140);
    panel.show();
    auto* summary = panel.findChild<QToolButton*>(QStringLiteral("printPreflightSummary"));
    auto* issues = panel.findChild<QListWidget*>(QStringLiteral("printPreflightIssues"));
    QVERIFY(summary != nullptr && issues != nullptr);
    QCOMPARE(issues->count(), 4);
    if (!summary->isChecked()) summary->click();
    QTRY_VERIFY(issues->isVisible());
    QList<core::PrintPreflightIssue> activated;
    connect(&panel, &app::PrintPreflightPanel::issueActivated, &panel,
        [&activated](const core::PrintPreflightIssue& issue) { activated.append(issue); });
    issues->setCurrentRow(0);
    issues->setFocus();
    QTest::keyClick(issues, Qt::Key_Down);
    QCOMPARE(issues->currentRow(), 1);
    QVERIFY(activated.isEmpty());
    QTest::keyClick(issues, static_cast<Qt::Key>(key));
    QCoreApplication::processEvents();
    QCOMPARE(activated.size(), 1);
    compareIssues(activated.first(), report.issues[3]);
}

void PrintPreflightPanelTests::replacingReportCannotActivateOldIssue()
{
    app::PrintPreflightPanel panel;
    panel.setReport(mixedReport());
    panel.resize(480, 140);
    panel.show();
    auto* summary = panel.findChild<QToolButton*>(QStringLiteral("printPreflightSummary"));
    auto* issues = panel.findChild<QListWidget*>(QStringLiteral("printPreflightIssues"));
    QVERIFY(summary != nullptr && issues != nullptr);
    QCOMPARE(issues->count(), 4);
    if (!summary->isChecked()) summary->click();
    QTRY_VERIFY(issues->isVisible());
    issues->setCurrentRow(3);
    core::PrintPreflightReport replacement;
    auto json = mixedReport().issues[1];
    json.code = QStringLiteral("invalid-json");
    json.message = QStringLiteral("数据格式错误");
    json.target = core::PrintPreflightTarget::Json;
    json.fieldKey.clear();
    json.elementIds.clear();
    json.line = 20;
    json.column = 3;
    replacement.issues = {json};
    QList<core::PrintPreflightIssue> activated;
    connect(&panel, &app::PrintPreflightPanel::issueActivated, &panel,
        [&activated](const core::PrintPreflightIssue& issue) { activated.append(issue); });
    panel.setReport(replacement);
    QCOMPARE(issues->count(), 1);
    QVERIFY(activated.isEmpty());
    issues->setCurrentRow(0);
    QTest::keyClick(issues, Qt::Key_Return);
    QCOMPARE(activated.size(), 1);
    compareIssues(activated.first(), json);
}

void PrintPreflightPanelTests::activationRemainsValidWhenFirstReceiverClearsReport()
{
    app::PrintPreflightPanel panel;
    const auto report = mixedReport();
    panel.setReport(report);
    panel.resize(480, 140);
    panel.show();
    auto* issues = panel.findChild<QListWidget*>(QStringLiteral("printPreflightIssues"));
    QVERIFY(issues != nullptr);
    QCOMPARE(issues->count(), 4);
    QTRY_VERIFY(issues->isVisible());
    QList<core::PrintPreflightIssue> activated;
    // 首个接收者可能同步触发重检，后续接收者仍须拿到完整的同一问题。
    connect(&panel, &app::PrintPreflightPanel::issueActivated, &panel,
        [&panel](const core::PrintPreflightIssue&) { panel.setPending(); });
    connect(&panel, &app::PrintPreflightPanel::issueActivated, &panel,
        [&activated](const core::PrintPreflightIssue& issue) { activated.append(issue); });
    issues->setCurrentRow(0);
    QTest::keyClick(issues, Qt::Key_Return);
    QCOMPARE(issues->count(), 0);
    QCOMPARE(activated.size(), 1);
    compareIssues(activated.first(), report.issues[1]);
}

void PrintPreflightPanelTests::manyIssuesDoNotCreatePerRowWidgets()
{
    app::PrintPreflightPanel panel;
    core::PrintPreflightReport report;
    const auto issue = mixedReport().issues[1];
    for (int row = 0; row < 1000; ++row) report.issues.append(issue);
    panel.setReport(report);
    auto* issues = panel.findChild<QListWidget*>(QStringLiteral("printPreflightIssues"));
    QVERIFY(issues != nullptr);
    QCOMPARE(issues->count(), 1000);
    for (int row = 0; row < issues->count(); ++row) {
        QVERIFY(issues->itemWidget(issues->item(row)) == nullptr);
    }
}

void PrintPreflightPanelTests::longMessagesWrapWithoutGrowingPanel_data()
{
    QTest::addColumn<int>("width");
    QTest::addColumn<QString>("message");
    const auto chinese = QStringLiteral("字段内容过长，请检查商品名称与打印区域。").repeated(18);
    const auto unbroken = QString(480, QLatin1Char('X'));
    QTest::newRow("narrow-chinese") << 240 << chinese;
    QTest::newRow("narrow-unbroken") << 240 << unbroken;
    QTest::newRow("desktop-chinese") << 360 << chinese;
    QTest::newRow("desktop-unbroken") << 360 << unbroken;
}

void PrintPreflightPanelTests::longMessagesWrapWithoutGrowingPanel()
{
    QFETCH(int, width);
    QFETCH(QString, message);
    app::PrintPreflightPanel panel;
    core::PrintPreflightReport report;
    auto issue = mixedReport().issues[1];
    issue.message = message;
    report.issues = {issue};
    panel.setReport(report);
    panel.resize(width, 140);
    panel.show();
    auto* summary = panel.findChild<QToolButton*>(QStringLiteral("printPreflightSummary"));
    auto* issues = panel.findChild<QListWidget*>(QStringLiteral("printPreflightIssues"));
    QVERIFY(summary != nullptr && issues != nullptr);
    QCOMPARE(issues->count(), 1);
    if (!summary->isChecked()) summary->click();
    QTRY_VERIFY(issues->isVisible());
    QTRY_COMPARE(panel.width(), width);
    QVERIFY(panel.minimumSizeHint().width() <= width);
    QVERIFY(panel.sizeHint().height() >= 100 && panel.sizeHint().height() <= 180);
    QVERIFY(issues->wordWrap());
    QCOMPARE(issues->textElideMode(), Qt::ElideNone);
    QVERIFY(issues->item(0)->text().contains(message));
    QTRY_COMPARE(issues->horizontalScrollBar()->maximum(), 0);
    QTRY_VERIFY(issues->visualItemRect(issues->item(0)).height() > issues->fontMetrics().height() * 2);
    const auto narrowHeight = issues->visualItemRect(issues->item(0)).height();
    QVERIFY(issues->visualItemRect(issues->item(0)).width() <= issues->viewport()->width());
    QVERIFY(panel.rect().contains(issues->geometry()));
    QVERIFY(panel.rect().contains(summary->geometry()));

    // 拉宽后行高必须重算，避免长消息仍沿用窄视口的缓存尺寸。
    panel.resize(600, 140);
    QTRY_VERIFY(issues->visualItemRect(issues->item(0)).height() < narrowHeight);
    QCOMPARE(issues->horizontalScrollBar()->maximum(), 0);
}

void PrintPreflightPanelTests::collapsingLeavesOnlyTheHeaderAndKeyboardCanExpand()
{
    QWidget host;
    auto* layout = new QVBoxLayout(&host);
    auto* panel = new app::PrintPreflightPanel(&host);
    layout->addWidget(panel);
    layout->addStretch();
    panel->setReport(mixedReport());
    auto* summary = panel->findChild<QToolButton*>(QStringLiteral("printPreflightSummary"));
    auto* issues = panel->findChild<QListWidget*>(QStringLiteral("printPreflightIssues"));
    QVERIFY(summary != nullptr && issues != nullptr);
    QCOMPARE(issues->count(), 4);
    QVERIFY(summary->isCheckable());
    if (!summary->isChecked()) summary->click();
    host.resize(480, 360);
    host.show();
    QTRY_VERIFY(issues->isVisible());
    const auto expandedHeight = panel->sizeHint().height();
    QTest::mouseClick(summary, Qt::LeftButton);
    QTRY_VERIFY(!issues->isVisible());
    QVERIFY(!summary->isChecked());
    QVERIFY(panel->sizeHint().height() < expandedHeight);
    QTRY_VERIFY(panel->height() <= summary->height() + 16);
    QCOMPARE(issues->count(), 4);
    summary->setFocus();
    QTest::keyClick(summary, Qt::Key_Space);
    QTRY_VERIFY(issues->isVisible());
    QVERIFY(summary->isChecked());
    QCOMPARE(issues->count(), 4);
}

void PrintPreflightPanelTests::accessibleControlsSupportKeyboardNavigation()
{
    app::PrintPreflightPanel panel;
    panel.setReport(mixedReport());
    panel.resize(480, 140);
    panel.show();
    panel.activateWindow();
    auto* summary = panel.findChild<QToolButton*>(QStringLiteral("printPreflightSummary"));
    auto* issues = panel.findChild<QListWidget*>(QStringLiteral("printPreflightIssues"));
    QVERIFY(summary != nullptr && issues != nullptr);
    QCOMPARE(issues->count(), 4);
    for (auto* control : {static_cast<QWidget*>(&panel), static_cast<QWidget*>(summary), static_cast<QWidget*>(issues)}) {
        auto* accessible = QAccessible::queryAccessibleInterface(control);
        QVERIFY(accessible != nullptr);
        QVERIFY(!accessible->text(QAccessible::Name).isEmpty());
    }
    QVERIFY(summary->focusPolicy() != Qt::NoFocus);
    QVERIFY(issues->focusPolicy() != Qt::NoFocus);
    if (!summary->isChecked()) summary->click();
    summary->setFocus();
    QTRY_VERIFY(summary->hasFocus());
    QTest::keyClick(summary, Qt::Key_Tab);
    QTRY_VERIFY(issues->hasFocus());
    QTest::keyClick(issues, Qt::Key_Down);
    QVERIFY(issues->currentRow() >= 0);
}

QTEST_MAIN(PrintPreflightPanelTests)
#include "tst_print_preflight_panel.moc"

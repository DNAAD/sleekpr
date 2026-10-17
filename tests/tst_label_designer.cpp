#include "sleekpr/app/AppTheme.h"
#include "sleekpr/app/TemplateDesignerFactory.h"
#include "sleekpr/app/TemplateDesignerWindow.h"
#include "sleekpr/app/TemplatePreviewLabel.h"
#include "sleekpr/app/SettingsWindow.h"
#include "sleekpr/core/templates/TemplateDocumentJson.h"
#include "sleekpr/core/templates/TemplateLibraryStore.h"
#include "sleekpr/core/templates/PaperSpecStore.h"

#include <QFile>
#include <QCheckBox>
#include <QJsonArray>
#include <QJsonDocument>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QToolButton>
#include <QtTest>

#include <algorithm>

using namespace sleekpr::app;
using namespace sleekpr::core;

class LabelDesignerTests final : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void labelDesignerOmitsTableControls();
    void labelElementsCanStillBeEditedAndPrePrinted();
    void labelEditingPreservesLegacyTables();
    void editorSeparatesObjectsToolsAndProperties();
    void editorFitsCompactDesktop();
    void sampleDataDockExpandsForErrors();
    void propertySectionsFollowSelectedElement();
    void previewUsesDeviceIndependentCoordinates();
    void settingsSeparatesPrintingAndSecurity();
    void savingCommitsPendingPropertyEdits();
    void fitModeRefitsAfterPaperChanges();
};

void LabelDesignerTests::initTestCase()
{
    applySleekprTheme(*qApp);
    QVERIFY(!QPixmap(QStringLiteral(":/sleekpr/ui/caret-down.png")).isNull());
    QVERIFY(!QPixmap(QStringLiteral(":/sleekpr/ui/check.png")).isNull());
}

void LabelDesignerTests::labelDesignerOmitsTableControls()
{
    TemplateDesignerWindow window(PrintClientSettings{}, nullptr);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    QVERIFY(window.findChild<QPushButton*>(QStringLiteral("designerAddTableButton")) == nullptr);
    QVERIFY(window.findChild<QWidget*>(QStringLiteral("designerTableInspectorTab")) == nullptr);
    QVERIFY(window.findChild<QWidget*>(QStringLiteral("tableColumnEditorPanel")) == nullptr);
    QVERIFY(window.findChild<QWidget*>(QStringLiteral("tableAdvancedEditorPanel")) == nullptr);
    QVERIFY(window.findChild<QWidget*>(QStringLiteral("tableMergeRegionEditorGrid")) == nullptr);

    auto* tabs = window.findChild<QTabWidget*>(QStringLiteral("designerInspectorTabs"));
    QVERIFY(tabs != nullptr);
    QCOMPARE(tabs->count(), 3);
    for (const auto* name : {"designerAddFixedTextButton", "designerAddBoundFieldButton",
             "designerAddQrCodeButton", "designerAddRectangleButton", "designerAddArrayGridButton"}) {
        QVERIFY(window.findChild<QPushButton*>(QString::fromLatin1(name)) != nullptr);
    }
    const auto screenshotPath = qEnvironmentVariable("SLEEKPR_TEST_SCREENSHOT");
    if (!screenshotPath.isEmpty()) {
        QVERIFY(window.grab().save(screenshotPath));
        window.resize(1440, 900);
        QCoreApplication::processEvents();
        QVERIFY(window.grab().save(screenshotPath + QStringLiteral(".wide.png")));
    }
}

void LabelDesignerTests::labelElementsCanStillBeEditedAndPrePrinted()
{
    PrintClientSettings changedSettings;
    NativePrintDrawingPlan capturedPlan;
    int printCount = 0;
    TemplateDesignerWindow window(
        PrintClientSettings{},
        [&changedSettings](const PrintClientSettings& settings) { changedSettings = settings; },
        QString(),
        [&capturedPlan, &printCount](const NativePrintDrawingPlan& plan, const QString&) {
            capturedPlan = plan;
            ++printCount;
            return true;
        });
    auto* addLayer = window.findChild<QPushButton*>(QStringLiteral("addLayerButton"));
    QVERIFY(addLayer != nullptr);
    addLayer->click();
    for (const auto* name : {"designerAddFixedTextButton", "designerAddBoundFieldButton",
             "designerAddQrCodeButton", "designerAddRectangleButton", "designerAddArrayGridButton"}) {
        auto* button = window.findChild<QPushButton*>(QString::fromLatin1(name));
        QVERIFY(button != nullptr);
        button->click();
    }
    const auto& layer = changedSettings.templateDocuments[QStringLiteral("default")].layers.last();
    QCOMPARE(layer.elements.size(), 5);
    QVERIFY(layer.tables.isEmpty());

    auto* list = window.findChild<QListWidget*>(QStringLiteral("templateElementList"));
    auto* content = window.findChild<QPlainTextEdit*>(QStringLiteral("elementValueEdit"));
    QVERIFY(list != nullptr);
    QVERIFY(content != nullptr);
    list->setCurrentRow(0);
    content->setPlainText(QString::fromUtf8("标签测试 ${productName}"));
    QTRY_COMPARE(changedSettings.templateDocuments[QStringLiteral("default")].layers.last().elements.first().text,
        QString::fromUtf8("标签测试 ${productName}"));

    auto* prePrint = window.findChild<QPushButton*>(QStringLiteral("prePrintCurrentTemplateButton"));
    QVERIFY(prePrint != nullptr);
    prePrint->click();
    QCOMPARE(printCount, 1);
    QCOMPARE(capturedPlan.pages.size(), 1);
    QVERIFY(std::any_of(capturedPlan.pages.first().commands.cbegin(), capturedPlan.pages.first().commands.cend(),
        [](const NativeDrawCommand& command) { return command.text.startsWith(QString::fromUtf8("标签测试 ")); }));
}

void LabelDesignerTests::labelEditingPreservesLegacyTables()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    auto document = TemplateDesignerFactory::createBlankDocument(
        QStringLiteral("legacy-table-label"), QString::fromUtf8("旧标签"),
        QStringLiteral("default"), QStringLiteral("label-80x30"), {});
    TemplateLayer layer;
    layer.id = QStringLiteral("base");
    layer.name = QString::fromUtf8("标签图层");
    auto text = TemplateDesignerFactory::createElement(TemplateElementType::FixedText);
    text.layerId = layer.id;
    layer.elements.append(text);
    auto table = TemplateDesignerFactory::createTable();
    table.layerId = layer.id;
    layer.tables.append(table);
    document.layers = {layer};
    PrintClientSettings settings;
    settings.templateDocuments.insert(QStringLiteral("default"), document);
    PrintClientSettings changedSettings;
    TemplateDesignerWindow window(settings,
        [&changedSettings](const PrintClientSettings& next) { changedSettings = next; });
    auto* list = window.findChild<QListWidget*>(QStringLiteral("templateElementList"));
    auto* content = window.findChild<QPlainTextEdit*>(QStringLiteral("elementValueEdit"));
    auto* preview = dynamic_cast<TemplatePreviewLabel*>(window.findChild<QLabel*>(QStringLiteral("designerPreviewLabel")));
    QVERIFY(list != nullptr);
    QVERIFY(content != nullptr);
    QVERIFY(preview != nullptr);
    QCOMPARE(list->count(), 1);
    list->setCurrentRow(0);
    content->setPlainText(QString::fromUtf8("调整后的标签"));
    QTRY_VERIFY(!changedSettings.templateDocuments.isEmpty());
    QCOMPARE(TemplateDocumentJson::toJson(changedSettings.templateDocuments[QStringLiteral("default")])["layers"].toArray()
                 .first().toObject()["tables"].toArray(),
        TemplateDocumentJson::toJson(document)["layers"].toArray().first().toObject()["tables"].toArray());

    const auto origin = preview->printableImageOriginPx();
    const auto imageSize = preview->printableImageSizePx();
    const QPoint tablePoint(origin.x() + qRound((table.x + 2.0) / 80.0 * imageSize.width()),
        origin.y() + qRound((table.y + table.headerRowHeightMm + 1.0) / 30.0 * imageSize.height()));
    QTest::mouseClick(preview, Qt::LeftButton, Qt::NoModifier, tablePoint);
    QTest::keyClick(preview, Qt::Key_Space, Qt::ShiftModifier);
    QTest::keyClick(preview, Qt::Key_M, Qt::ControlModifier);
    QCOMPARE(preview->canvasFloatingActionCount(), 0);
    QCOMPARE(preview->tablePaginationOverlayCount(), 0);

    const auto path = directory.filePath(QStringLiteral("label.json"));
    QVERIFY(window.exportTemplateToFile(path));
    QFile exported(path);
    QVERIFY(exported.open(QIODevice::ReadOnly));
    const auto json = QJsonDocument::fromJson(exported.readAll()).object();
    QCOMPARE(json["layers"].toArray().first().toObject()["tables"].toArray(),
        TemplateDocumentJson::toJson(document)["layers"].toArray().first().toObject()["tables"].toArray());
}

void LabelDesignerTests::editorSeparatesObjectsToolsAndProperties()
{
    TemplateDesignerWindow window(PrintClientSettings{}, nullptr);
    auto* left = window.findChild<QWidget*>(QStringLiteral("designerLayerPanel"));
    auto* workspace = window.findChild<QWidget*>(QStringLiteral("designerWorkspacePanel"));
    auto* inspector = window.findChild<QWidget*>(QStringLiteral("designerInspectorPanel"));
    auto* list = window.findChild<QListWidget*>(QStringLiteral("templateElementList"));
    auto* tools = window.findChild<QWidget*>(QStringLiteral("designerInsertionTools"));
    auto* addText = window.findChild<QPushButton*>(QStringLiteral("designerAddFixedTextButton"));
    auto* toolbar = window.findChild<QWidget*>(QStringLiteral("designerTopToolbar"));
    QVERIFY(left != nullptr && workspace != nullptr && inspector != nullptr && list != nullptr);
    QVERIFY(left->isAncestorOf(list));
    QVERIFY(tools != nullptr && addText != nullptr);
    QVERIFY(workspace->isAncestorOf(tools));
    QVERIFY(tools->isAncestorOf(addText));
    QVERIFY(toolbar != nullptr);
    QVERIFY(!workspace->isAncestorOf(toolbar));
    QVERIFY(!inspector->isAncestorOf(list));
}

void LabelDesignerTests::editorFitsCompactDesktop()
{
    TemplateDesignerWindow window(PrintClientSettings{}, nullptr);
    window.resize(1024, 720);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    QVERIFY2(window.width() <= 1024, "标签编辑器不能被工具栏撑出桌面宽度");
    auto* toolbar = window.findChild<QWidget*>(QStringLiteral("designerTopToolbar"));
    QVERIFY(toolbar != nullptr);
    for (auto* button : toolbar->findChildren<QPushButton*>()) {
        if (button->isVisible()) {
            const QRect bounds(button->mapTo(&window, QPoint()), button->size());
            QVERIFY2(window.rect().contains(bounds), qPrintable(button->objectName()));
        }
    }
    const auto screenshotPath = qEnvironmentVariable("SLEEKPR_TEST_SCREENSHOT");
    if (!screenshotPath.isEmpty()) {
        QVERIFY(window.grab().save(screenshotPath + QStringLiteral(".compact.png")));
    }
}

void LabelDesignerTests::sampleDataDockExpandsForErrors()
{
    TemplateDesignerWindow window(PrintClientSettings{}, nullptr);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    auto* toggle = window.findChild<QToolButton*>(QStringLiteral("designerSampleDataToggle"));
    auto* editor = window.findChild<QPlainTextEdit*>(QStringLiteral("templateSampleDataEdit"));
    auto* error = window.findChild<QLabel*>(QStringLiteral("templateSampleDataErrorLabel"));
    QVERIFY(toggle != nullptr && editor != nullptr && error != nullptr);
    QVERIFY(!editor->isVisible());
    toggle->click();
    QVERIFY(editor->isVisible());
    toggle->click();
    QVERIFY(!editor->isVisible());
    editor->setPlainText(QStringLiteral("{\n\"productName\": }"));
    QTRY_VERIFY(editor->isVisible());
    QVERIFY(error->isVisible());
    QVERIFY(error->text().contains(QString::fromUtf8("行")));
    QVERIFY(error->text().contains(QString::fromUtf8("列")));
}

void LabelDesignerTests::propertySectionsFollowSelectedElement()
{
    TemplateDesignerWindow window(PrintClientSettings{}, nullptr);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    window.findChild<QPushButton*>(QStringLiteral("addLayerButton"))->click();
    window.findChild<QPushButton*>(QStringLiteral("designerAddFixedTextButton"))->click();
    auto* textStyle = window.findChild<QWidget*>(QStringLiteral("designerTextStyleSection"));
    auto* apply = window.findChild<QPushButton*>(QStringLiteral("applyElementPropertiesButton"));
    QVERIFY(textStyle != nullptr && apply != nullptr);
    QVERIFY(textStyle->isVisible());
    QVERIFY(!apply->isVisible());
    window.findChild<QPushButton*>(QStringLiteral("designerAddQrCodeButton"))->click();
    QVERIFY(!textStyle->isVisible());
    auto* aids = window.findChild<QCheckBox*>(QStringLiteral("designerDesignAidCheck"));
    QVERIFY(aids != nullptr);
    QVERIFY(!aids->isChecked());
}

void LabelDesignerTests::previewUsesDeviceIndependentCoordinates()
{
    TemplatePreviewLabel label;
    QPixmap image(400, 200);
    image.fill(Qt::white);
    image.setDevicePixelRatio(2.0);
    label.setPixmap(image);
    QCOMPARE(label.printableImageSizePx(), QSize(200, 100));
}

void LabelDesignerTests::settingsSeparatesPrintingAndSecurity()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    SettingsWindow window(directory.filePath(QStringLiteral("settings.json")), nullptr);
    window.resize(1100, 740);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    auto* security = window.findChild<QPushButton*>(QStringLiteral("settingsSecurityNavButton"));
    auto* printing = window.findChild<QPushButton*>(QStringLiteral("settingsPrintNavButton"));
    auto* securityPage = window.findChild<QWidget*>(QStringLiteral("settingsSecurityPage"));
    auto* printPage = window.findChild<QWidget*>(QStringLiteral("settingsPrintPage"));
    QVERIFY(security != nullptr && printing != nullptr && securityPage != nullptr && printPage != nullptr);
    QVERIFY(printPage->isVisible());
    QVERIFY(!securityPage->isVisible());
    security->click();
    QVERIFY(securityPage->isVisible());
    QVERIFY(!printPage->isVisible());
    QVERIFY(security->isChecked());
    printing->click();
    QVERIFY(printPage->isVisible());
    QVERIFY(!security->isChecked());
    const auto screenshotPath = qEnvironmentVariable("SLEEKPR_TEST_SCREENSHOT");
    if (!screenshotPath.isEmpty()) {
        QVERIFY(window.grab().save(screenshotPath + QStringLiteral(".settings.png")));
    }
}

void LabelDesignerTests::savingCommitsPendingPropertyEdits()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    auto document = TemplateDesignerFactory::createBlankDocument(QStringLiteral("fast-save"),
        QString::fromUtf8("快速保存"), QStringLiteral("default"), QStringLiteral("label-80x30"), {});
    TemplateLayer layer;
    layer.id = QStringLiteral("base");
    auto text = TemplateDesignerFactory::createElement(TemplateElementType::FixedText);
    text.layerId = layer.id;
    text.text = QString::fromUtf8("原始文本");
    layer.elements.append(text);
    document.layers = {layer};
    PrintClientSettings settings;
    settings.templateDocuments.insert(QStringLiteral("default"), document);
    TemplateDesignerWindow window(settings, nullptr, directory.path());
    auto* value = window.findChild<QPlainTextEdit*>(QStringLiteral("elementValueEdit"));
    auto* save = window.findChild<QPushButton*>(QStringLiteral("saveTemplateToLibraryButton"));
    QVERIFY(value != nullptr && save != nullptr);
    value->setPlainText(QString::fromUtf8("立即保存的内容"));
    save->click();
    const auto saved = TemplateLibraryStore(directory.path()).loadTemplate(document.id);
    QVERIFY(saved.has_value());
    QCOMPARE(saved->layers.first().elements.first().text, QString::fromUtf8("立即保存的内容"));
}

void LabelDesignerTests::fitModeRefitsAfterPaperChanges()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    PaperSpec large;
    large.id = QStringLiteral("label-80x30");
    large.name = QStringLiteral("80x30");
    large.widthMm = 80.0;
    large.heightMm = 30.0;
    auto small = large;
    small.id = QStringLiteral("label-40x15");
    small.name = QStringLiteral("40x15");
    small.widthMm = 40.0;
    small.heightMm = 15.0;
    PaperSpecStore papers(directory.filePath(QStringLiteral("paper-specs.json")));
    QVERIFY(papers.savePaperSpec(large));
    QVERIFY(papers.savePaperSpec(small));
    PrintClientSettings settings;
    settings.templateDocuments.insert(QStringLiteral("default"), TemplateDesignerFactory::createBlankDocument(
        QStringLiteral("fit-paper"), QString::fromUtf8("适配纸张"), QStringLiteral("default"), large.id, {}));
    TemplateDesignerWindow window(settings, nullptr, directory.filePath(QStringLiteral("templates")));
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    QTest::qWait(50);
    auto* preview = dynamic_cast<TemplatePreviewLabel*>(window.findChild<QLabel*>(QStringLiteral("designerPreviewLabel")));
    auto* paper = window.findChild<QComboBox*>(QStringLiteral("paperSpecCombo"));
    QVERIFY(preview != nullptr && paper != nullptr);
    const auto originalWidth = preview->printableImageSizePx().width();
    const auto smallIndex = paper->findData(small.id);
    QVERIFY(smallIndex >= 0);
    paper->setCurrentIndex(smallIndex);
    QTRY_VERIFY_WITH_TIMEOUT(qAbs(preview->printableImageSizePx().width() - originalWidth) <= 2, 1000);
}

QTEST_MAIN(LabelDesignerTests)
#include "tst_label_designer.moc"

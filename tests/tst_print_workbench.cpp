#include "sleekpr/app/AppTheme.h"
#include "sleekpr/app/PrintWorkbenchWindow.h"
#include "sleekpr/app/PrintFieldEditorPanel.h"
#include "sleekpr/app/PrintPreviewPanel.h"
#include "sleekpr/core/settings/FileSettingsStore.h"
#include "sleekpr/core/templates/FieldPresetStore.h"
#include "sleekpr/core/templates/TemplateLibraryStore.h"
#include "sleekpr/core/printing/PrintCopyPlanner.h"
#include "sleekpr/core/native/NativePlanJsonSerializer.h"
#include "sleekpr/infrastructure/printing/LabelPrintEngine.h"

#include <QComboBox>
#include <QCheckBox>
#include <QFile>
#include <QEvent>
#include <QFormLayout>
#include <QSignalSpy>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QTemporaryDir>
#include <QTabWidget>
#include <QtTest>

#include <algorithm>

using namespace sleekpr;

namespace {

class RecordingPrintEngine final : public infrastructure::LabelPrintEngine
{
public:
    bool print(const core::NativeLabelDrawingPlan& plan, const QString& printer) override
    {
        return print(core::NativePrintDrawingPlan::fromSinglePage(plan), printer);
    }
    bool print(const core::NativePrintDrawingPlan& plan, const QString& printer) override
    {
        ++calls;
        lastPlan = plan;
        lastPrinter = printer;
        return succeeds;
    }
    int calls = 0;
    bool succeeds = true;
    core::NativePrintDrawingPlan lastPlan;
    QString lastPrinter;
};

core::PrintClientSettings workbenchSettings(bool withSchema = true)
{
    core::TemplateDocument document;
    document.id = QStringLiteral("product-tag");
    document.name = QString::fromUtf8("商品标签");
    document.paperSpecId = QStringLiteral("label-80x30");
    document.sampleData.insert(QStringLiteral("productName"), QString::fromUtf8("不能默认打印的模拟商品"));
    if (withSchema) {
        core::FieldDefinition field;
        field.key = QStringLiteral("productName");
        field.displayName = QString::fromUtf8("品名");
        field.required = true;
        document.fieldSchema.append(field);
    }
    core::TemplateLayer layer;
    layer.id = QStringLiteral("base");
    core::TemplateElement element;
    element.id = QStringLiteral("name");
    element.layerId = layer.id;
    element.type = core::TemplateElementType::BoundField;
    element.fieldKey = QStringLiteral("productName");
    element.x = 2;
    element.y = 2;
    element.width = 30;
    element.height = 6;
    element.fontSizePt = 8;
    layer.elements.append(element);
    document.layers.append(layer);
    core::PrintClientSettings settings;
    settings.defaultPrinter = QStringLiteral("测试打印机");
    settings.templateDocuments.insert(QStringLiteral("default"), document);
    return settings;
}

QStringList testPrinters() { return {QStringLiteral("测试打印机")}; }

void useJsonInput(app::PrintWorkbenchWindow& window)
{
    auto* tabs = window.findChild<QTabWidget*>(QStringLiteral("printFieldModeTabs"));
    if (tabs == nullptr) return;
    for (int index = 0; index < tabs->count(); ++index) {
        if (tabs->tabText(index) == QStringLiteral("JSON")) tabs->setCurrentIndex(index);
    }
}

void fillLegacyProductName(app::PrintWorkbenchWindow& window)
{
    if (auto* name = window.findChild<QLineEdit*>(QStringLiteral("printField_productName"))) {
        name->setText(QString::fromUtf8("本次真实数据"));
    }
}

QLabel* fieldTitle(app::PrintWorkbenchWindow& window)
{
    auto* fields = window.findChild<app::PrintFieldEditorPanel*>();
    if (fields == nullptr) return nullptr;
    auto* input = fields->findChild<QWidget*>(QStringLiteral("printField_productName"));
    auto* form = fields->findChild<QFormLayout*>();
    return input == nullptr || form == nullptr ? nullptr
        : qobject_cast<QLabel*>(form->labelForField(input->parentWidget()));
}

} // 匿名命名空间

class PrintWorkbenchTests final : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase() { app::applySleekprTheme(*qApp); }
    void copiesPreservePageOrderAndPhysicalPlan();
    void copiesRejectInvalidAndOversizedJobs();
    void workbenchDoesNotPrintSampleData();
    void workbenchPrintsCurrentValuesAndCopiesOnce();
    void workbenchInvalidJsonDisablesStalePrintPlan();
    void workbenchHandlesMissingPrinterAndEngineFailure();
    void workbenchFitsDesktopAndPreservesDraftOnReload();
    void workbenchSearchCannotPrintHiddenTemplate();
    void workbenchRequiresReviewAfterExternalTemplateChange();
    void workbenchDoesNotApplyAnotherTemplatesPreset();
    void workbenchReportsCorruptPresetStore();
    void previewCanRenderBeforeWindowIsShown();
    void workbenchInfersMissingFieldsWithoutPreset();
    void workbenchOffersPresetActions();
    void workbenchSavesPresetForCurrentTemplate();
    void workbenchSavesPresetWithoutPrinter();
    void workbenchRejectsIncompletePreset();
    void workbenchAllowsExplicitEmptyLegacyField();
    void workbenchReportsAllReferencedFields();
    void workbenchShowsPreflightProblems();
    void workbenchPreflightLocatesFieldAndInvalidatesImmediately();
    void workbenchPreflightLocatesInvalidGeometry();
    void workbenchPreflightLocatesPrinterAndJson();
    void workbenchPreflightWarningsRemainPrintable();
    void workbenchCopyChangesInvalidateOldLocation();
    void workbenchPrintAfterHighlightKeepsNativeCommands();
    void workbenchRefreshesDisplayNameAndPrintsWithOriginalKey();
    void workbenchDisplayNameRefreshKeepsInvalidDraft();
    void workbenchActivationRefreshesLibraryFieldNames();
    void workbenchChecksUseDisplayNamesWithoutDuplicateStatus();
    void workbenchArrayProblemsUseDisplayNames();
};

void PrintWorkbenchTests::copiesPreservePageOrderAndPhysicalPlan()
{
    core::NativePrintDrawingPlan source;
    source.renderDpi = 203;
    source.paperSize = core::LabelPaperSize(40, 20);
    core::NativeDrawCommand first;
    first.text = QStringLiteral("first");
    auto second = first;
    second.text = QStringLiteral("second");
    source.pages = {{1, {first}}, {2, {second}}};
    QString error;
    const auto repeated = core::PrintCopyPlanner::repeat(source, 3, &error);
    QVERIFY(repeated.has_value());
    QCOMPARE(repeated->pages.size(), 6);
    QCOMPARE(repeated->renderDpi, 203.0);
    QCOMPARE(repeated->paperSize.widthMm(), 40.0);
    for (int index = 0; index < repeated->pages.size(); ++index) {
        QCOMPARE(repeated->pages[index].commands.first().text, index % 2 == 0 ? first.text : second.text);
        QCOMPARE(repeated->pages[index].pageNumber, index + 1);
    }
    QCOMPARE(source.pages.size(), 2);
}

void PrintWorkbenchTests::copiesRejectInvalidAndOversizedJobs()
{
    core::NativePrintDrawingPlan source;
    QString error;
    QVERIFY(!core::PrintCopyPlanner::repeat(source, 1, &error).has_value());
    source.pages.append(core::NativePrintPage{});
    QVERIFY(!core::PrintCopyPlanner::repeat(source, 0, &error).has_value());
    QVERIFY(!core::PrintCopyPlanner::repeat(source, 101, &error).has_value());
    source.pages = QList<core::NativePrintPage>(101);
    QVERIFY(!core::PrintCopyPlanner::repeat(source, 100, &error).has_value());
    QVERIFY(!error.isEmpty());
}

void PrintWorkbenchTests::workbenchDoesNotPrintSampleData()
{
    QTemporaryDir directory;
    const auto path = directory.filePath(QStringLiteral("settings.json"));
    QVERIFY(core::FileSettingsStore(path).save(workbenchSettings()));
    RecordingPrintEngine engine;
    app::PrintWorkbenchWindow window(path, &engine, testPrinters);
    auto* print = window.findChild<QPushButton*>(QStringLiteral("workbenchPrintButton"));
    auto* status = window.findChild<QLabel*>(QStringLiteral("workbenchStatusLabel"));
    auto* name = window.findChild<QLineEdit*>(QStringLiteral("printField_productName"));
    QVERIFY(print != nullptr && status != nullptr && name != nullptr);
    QVERIFY(name->text().isEmpty());
    QVERIFY(!print->isEnabled());
    auto* issues = window.findChild<QListWidget*>(QStringLiteral("printPreflightIssues"));
    QVERIFY(issues != nullptr && issues->count() > 0);
    QVERIFY(issues->item(0)->text().contains(QString::fromUtf8("品名")));
    QVERIFY(status->isHidden());
    print->click();
    QCOMPARE(engine.calls, 0);
}

void PrintWorkbenchTests::workbenchPrintsCurrentValuesAndCopiesOnce()
{
    QTemporaryDir directory;
    const auto path = directory.filePath(QStringLiteral("settings.json"));
    QVERIFY(core::FileSettingsStore(path).save(workbenchSettings()));
    RecordingPrintEngine engine;
    app::PrintWorkbenchWindow window(path, &engine, testPrinters);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    auto* name = window.findChild<QLineEdit*>(QStringLiteral("printField_productName"));
    auto* copies = window.findChild<QSpinBox*>(QStringLiteral("workbenchCopiesSpin"));
    auto* print = window.findChild<QPushButton*>(QStringLiteral("workbenchPrintButton"));
    auto* status = window.findChild<QLabel*>(QStringLiteral("workbenchStatusLabel"));
    QVERIFY(name != nullptr && copies != nullptr && print != nullptr && status != nullptr);
    name->setText(QString::fromUtf8("手动输入商品"));
    copies->setValue(3);
    QTRY_VERIFY(print->isEnabled());
    QCOMPARE(engine.calls, 0);
    auto* preview = window.findChild<QLabel*>(QStringLiteral("workbenchPreviewImage"));
    QVERIFY(preview != nullptr && !preview->pixmap().isNull());
    const auto image = preview->pixmap().toImage();
    int inkPixels = 0;
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            if (image.pixelColor(x, y).lightness() < 128) ++inkPixels;
        }
    }
    QVERIFY(inkPixels > 10);
    QVERIFY(qAbs(double(image.width()) / image.height() - 80.0 / 30.0) < 0.03);
    print->click();
    print->click();
    QTRY_COMPARE(engine.calls, 1);
    QCOMPARE(engine.lastPlan.pages.size(), 3);
    QCOMPARE(engine.lastPrinter, QStringLiteral("测试打印机"));
    QCOMPARE(engine.lastPlan.renderDpi, 203.0);
    for (const auto& page : engine.lastPlan.pages) {
        QCOMPARE(page.commands.first().text, QString::fromUtf8("手动输入商品"));
    }
    QVERIFY(status->text().contains(QString::fromUtf8("提交")));
    QVERIFY(!status->text().contains(QString::fromUtf8("打印完成")));
}

void PrintWorkbenchTests::workbenchInvalidJsonDisablesStalePrintPlan()
{
    QTemporaryDir directory;
    const auto path = directory.filePath(QStringLiteral("settings.json"));
    QVERIFY(core::FileSettingsStore(path).save(workbenchSettings(false)));
    RecordingPrintEngine engine;
    app::PrintWorkbenchWindow window(path, &engine, testPrinters);
    useJsonInput(window);
    auto* json = window.findChild<QPlainTextEdit*>(QStringLiteral("printValuesJsonEdit"));
    auto* print = window.findChild<QPushButton*>(QStringLiteral("workbenchPrintButton"));
    QVERIFY(json != nullptr && print != nullptr);
    json->setPlainText(QStringLiteral("{\"productName\":\"before\"}"));
    QTRY_VERIFY(print->isEnabled());
    json->setPlainText(QStringLiteral("{\n\"productName\": }"));
    QVERIFY(!print->isEnabled());
    print->click();
    QCOMPARE(engine.calls, 0);
    auto* error = window.findChild<QLabel*>(QStringLiteral("printValuesErrorLabel"));
    QVERIFY(error != nullptr);
    QTRY_VERIFY(error->text().contains(QString::fromUtf8("行")));
}

void PrintWorkbenchTests::workbenchHandlesMissingPrinterAndEngineFailure()
{
    QTemporaryDir directory;
    const auto path = directory.filePath(QStringLiteral("settings.json"));
    QVERIFY(core::FileSettingsStore(path).save(workbenchSettings(false)));
    RecordingPrintEngine engine;
    app::PrintWorkbenchWindow unavailable(path, &engine, [] { return QStringList{}; });
    auto* button = unavailable.findChild<QPushButton*>(QStringLiteral("workbenchPrintButton"));
    QVERIFY(button != nullptr && !button->isEnabled());
    button->click();
    QCOMPARE(engine.calls, 0);
    app::PrintWorkbenchWindow failing(path, &engine, testPrinters);
    fillLegacyProductName(failing);
    engine.succeeds = false;
    button = failing.findChild<QPushButton*>(QStringLiteral("workbenchPrintButton"));
    QTRY_VERIFY(button->isEnabled());
    button->click();
    QTRY_COMPARE(engine.calls, 1);
    auto* status = failing.findChild<QLabel*>(QStringLiteral("workbenchStatusLabel"));
    QVERIFY(status != nullptr);
    QVERIFY(status->text().contains(QString::fromUtf8("失败")));
}

void PrintWorkbenchTests::workbenchFitsDesktopAndPreservesDraftOnReload()
{
    QTemporaryDir directory;
    const auto path = directory.filePath(QStringLiteral("settings.json"));
    QVERIFY(core::FileSettingsStore(path).save(workbenchSettings()));
    RecordingPrintEngine engine;
    app::PrintWorkbenchWindow window(path, &engine, testPrinters);
    window.resize(1024, 720);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    QVERIFY(window.width() <= 1024);
    auto* name = window.findChild<QLineEdit*>(QStringLiteral("printField_productName"));
    QVERIFY(name != nullptr);
    name->setText(QString::fromUtf8("尚未提交的数据"));
    window.reloadFromDisk();
    name = window.findChild<QLineEdit*>(QStringLiteral("printField_productName"));
    QVERIFY(name != nullptr);
    QCOMPARE(name->text(), QString::fromUtf8("尚未提交的数据"));
    for (const auto* objectName : {"workbenchTemplateList", "workbenchPreviewScrollArea", "workbenchDataPanel", "workbenchPrintButton"}) {
        auto* child = window.findChild<QWidget*>(QString::fromLatin1(objectName));
        QVERIFY(child != nullptr && child->isVisible());
        QVERIFY(window.rect().contains(QRect(child->mapTo(&window, QPoint()), child->size())));
    }
    QTest::qWait(250);
    const auto screenshot = qEnvironmentVariable("SLEEKPR_WORKBENCH_SCREENSHOT");
    if (!screenshot.isEmpty()) {
        QVERIFY(window.grab().save(screenshot + QStringLiteral(".compact.png")));
        window.resize(1440, 900);
        QTest::qWait(100);
        QVERIFY(window.grab().save(screenshot));
    }
}

void PrintWorkbenchTests::workbenchSearchCannotPrintHiddenTemplate()
{
    QTemporaryDir directory;
    const auto path = directory.filePath(QStringLiteral("settings.json"));
    QVERIFY(core::FileSettingsStore(path).save(workbenchSettings(false)));
    RecordingPrintEngine engine;
    app::PrintWorkbenchWindow window(path, &engine, testPrinters);
    fillLegacyProductName(window);
    auto* search = window.findChild<QLineEdit*>(QStringLiteral("workbenchTemplateSearch"));
    auto* print = window.findChild<QPushButton*>(QStringLiteral("workbenchPrintButton"));
    QVERIFY(search != nullptr && print != nullptr);
    QTRY_VERIFY(print->isEnabled());
    search->setText(QStringLiteral("no-matching-template"));
    QVERIFY(!print->isEnabled());
    print->click();
    QCOMPARE(engine.calls, 0);
}

void PrintWorkbenchTests::workbenchRequiresReviewAfterExternalTemplateChange()
{
    QTemporaryDir directory;
    const auto path = directory.filePath(QStringLiteral("settings.json"));
    auto settings = workbenchSettings(false);
    QVERIFY(core::FileSettingsStore(path).save(settings));
    RecordingPrintEngine engine;
    app::PrintWorkbenchWindow window(path, &engine, testPrinters);
    fillLegacyProductName(window);
    auto* print = window.findChild<QPushButton*>(QStringLiteral("workbenchPrintButton"));
    auto* status = window.findChild<QLabel*>(QStringLiteral("workbenchStatusLabel"));
    QVERIFY(print != nullptr && status != nullptr);
    QTRY_VERIFY(print->isEnabled());
    settings.templateDocuments[QStringLiteral("default")].layers.first().elements.first().x = 8.0;
    QVERIFY(core::FileSettingsStore(path).save(settings));
    print->click();
    QTest::qWait(20);
    QCOMPARE(engine.calls, 0);
    QVERIFY(status->text().contains(QString::fromUtf8("更新")));
    print->click();
    QTRY_COMPARE(engine.calls, 1);
    QCOMPARE(engine.lastPlan.pages.first().commands.first().x, 8.0);
}

void PrintWorkbenchTests::workbenchDoesNotApplyAnotherTemplatesPreset()
{
    QTemporaryDir directory;
    auto settings = workbenchSettings();
    auto second = settings.templateDocuments[QStringLiteral("default")];
    second.id = QStringLiteral("other-tag");
    second.name = QStringLiteral("Other");
    settings.templateDocuments.insert(QStringLiteral("other"), second);
    const auto path = directory.filePath(QStringLiteral("settings.json"));
    QVERIFY(core::FileSettingsStore(path).save(settings));
    core::FieldPreset preset;
    preset.id = QStringLiteral("only-first");
    preset.name = QString::fromUtf8("仅供第一模板");
    preset.templateId = QStringLiteral("default");
    preset.values.insert(QStringLiteral("productName"), QStringLiteral("first-customer"));
    QVERIFY(core::FieldPresetStore(directory.filePath(QStringLiteral("field-presets.json"))).savePreset(preset));
    RecordingPrintEngine engine;
    app::PrintWorkbenchWindow window(path, &engine, testPrinters);
    auto* presets = window.findChild<QComboBox*>(QStringLiteral("workbenchPresetCombo"));
    auto* list = window.findChild<QListWidget*>(QStringLiteral("workbenchTemplateList"));
    QVERIFY(presets != nullptr && list != nullptr);
    const auto index = presets->findData(preset.id);
    QVERIFY(index >= 0);
    presets->setCurrentIndex(index);
    list->setCurrentRow(1);
    QCOMPARE(presets->findData(preset.id), -1);
    auto* name = window.findChild<QLineEdit*>(QStringLiteral("printField_productName"));
    QVERIFY(name != nullptr && name->text().isEmpty());
}

void PrintWorkbenchTests::workbenchReportsCorruptPresetStore()
{
    QTemporaryDir directory;
    const auto path = directory.filePath(QStringLiteral("settings.json"));
    QVERIFY(core::FileSettingsStore(path).save(workbenchSettings(false)));
    QFile file(directory.filePath(QStringLiteral("field-presets.json")));
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("{broken");
    file.close();
    RecordingPrintEngine engine;
    app::PrintWorkbenchWindow window(path, &engine, testPrinters);
    auto* print = window.findChild<QPushButton*>(QStringLiteral("workbenchPrintButton"));
    auto* status = window.findChild<QLabel*>(QStringLiteral("workbenchStatusLabel"));
    QVERIFY(print != nullptr && status != nullptr);
    QVERIFY(!print->isEnabled());
    auto* issues = window.findChild<QListWidget*>(QStringLiteral("printPreflightIssues"));
    QVERIFY(issues != nullptr && issues->count() > 0);
    QVERIFY(issues->item(0)->text().contains(QString::fromUtf8("预设")));
    QVERIFY(status->isHidden());
}

void PrintWorkbenchTests::previewCanRenderBeforeWindowIsShown()
{
    app::PrintPreviewPanel preview;
    preview.resize(300, 200);
    core::NativePrintDrawingPlan plan;
    plan.pages.append(core::NativePrintPage{});
    preview.setPlan(plan);
    auto* label = preview.findChild<QLabel*>(QStringLiteral("workbenchPreviewImage"));
    QVERIFY(label != nullptr);
    QVERIFY(!label->pixmap().isNull());
    QVERIFY(label->pixmap().height() >= 1);
}

void PrintWorkbenchTests::workbenchInfersMissingFieldsWithoutPreset()
{
    QTemporaryDir directory;
    const auto path = directory.filePath(QStringLiteral("settings.json"));
    QVERIFY(core::FileSettingsStore(path).save(workbenchSettings(false)));
    RecordingPrintEngine engine;
    app::PrintWorkbenchWindow window(path, &engine, testPrinters);
    auto* name = window.findChild<QLineEdit*>(QStringLiteral("printField_productName"));
    auto* print = window.findChild<QPushButton*>(QStringLiteral("workbenchPrintButton"));
    QVERIFY2(name != nullptr, "没有字段定义和预设的旧模板也必须展示实际引用的字段");
    QVERIFY(print != nullptr && !print->isEnabled());
    QVERIFY(name->text().isEmpty());
    name->setText(QString::fromUtf8("本次真实商品"));
    QTRY_VERIFY(print->isEnabled());
    print->click();
    QTRY_COMPARE(engine.calls, 1);
    QCOMPARE(engine.lastPlan.pages.first().commands.first().text, QString::fromUtf8("本次真实商品"));
}

void PrintWorkbenchTests::workbenchSavesPresetForCurrentTemplate()
{
    QTemporaryDir directory;
    const auto path = directory.filePath(QStringLiteral("settings.json"));
    QVERIFY(core::FileSettingsStore(path).save(workbenchSettings(false)));
    RecordingPrintEngine engine;
    app::PrintWorkbenchWindow window(path, &engine, testPrinters);
    fillLegacyProductName(window);
    QString error;
    QVERIFY2(window.saveCurrentValuesAsPreset(QString::fromUtf8("门店标签数据"), &error), qPrintable(error));
    const auto presets = core::FieldPresetStore(directory.filePath(QStringLiteral("field-presets.json"))).presets();
    QCOMPARE(presets.size(), 1);
    auto* list = window.findChild<QListWidget*>(QStringLiteral("workbenchTemplateList"));
    QVERIFY(list != nullptr && list->currentItem() != nullptr);
    QCOMPARE(presets.first().templateId, list->currentItem()->data(Qt::UserRole).toString());
    QCOMPARE(presets.first().values[QStringLiteral("productName")].toString(), QString::fromUtf8("本次真实数据"));
    QCOMPARE(engine.calls, 0);
    app::PrintWorkbenchWindow reloaded(path, &engine, testPrinters);
    auto* combo = reloaded.findChild<QComboBox*>(QStringLiteral("workbenchPresetCombo"));
    QVERIFY(combo != nullptr);
    const auto presetIndex = combo->findData(presets.first().id);
    QVERIFY(presetIndex > 0);
    combo->setCurrentIndex(presetIndex);
    auto* name = reloaded.findChild<QLineEdit*>(QStringLiteral("printField_productName"));
    QVERIFY(name != nullptr);
    QCOMPARE(name->text(), QString::fromUtf8("本次真实数据"));
    QCOMPARE(engine.calls, 0);
}

void PrintWorkbenchTests::workbenchRejectsIncompletePreset()
{
    QTemporaryDir directory;
    const auto path = directory.filePath(QStringLiteral("settings.json"));
    QVERIFY(core::FileSettingsStore(path).save(workbenchSettings(false)));
    RecordingPrintEngine engine;
    app::PrintWorkbenchWindow window(path, &engine, testPrinters);
    QString error;
    QVERIFY(!window.saveCurrentValuesAsPreset(QStringLiteral("incomplete"), &error));
    QVERIFY(error.contains(QString::fromUtf8("商品名称")));
    QVERIFY(!error.contains(QStringLiteral("productName")));
    QVERIFY(window.findChild<QLabel*>(QStringLiteral("workbenchStatusLabel"))->isHidden());
    QVERIFY(!QFile::exists(directory.filePath(QStringLiteral("field-presets.json"))));
    fillLegacyProductName(window);
    QVERIFY(!window.saveCurrentValuesAsPreset(QStringLiteral("  "), &error));
    QCOMPARE(engine.calls, 0);
}

void PrintWorkbenchTests::workbenchSavesPresetWithoutPrinter()
{
    QTemporaryDir directory;
    const auto path = directory.filePath(QStringLiteral("settings.json"));
    QVERIFY(core::FileSettingsStore(path).save(workbenchSettings(false)));
    RecordingPrintEngine engine;
    app::PrintWorkbenchWindow window(path, &engine, [] { return QStringList{}; });
    fillLegacyProductName(window);
    auto* save = window.findChild<QPushButton*>(QStringLiteral("workbenchSavePresetButton"));
    auto* print = window.findChild<QPushButton*>(QStringLiteral("workbenchPrintButton"));
    QVERIFY(save != nullptr && print != nullptr);
    QTRY_VERIFY(save->isEnabled());
    QVERIFY(!print->isEnabled());
    QString error;
    QVERIFY2(window.saveCurrentValuesAsPreset(QStringLiteral("offline"), &error), qPrintable(error));
    QCOMPARE(core::FieldPresetStore(directory.filePath(QStringLiteral("field-presets.json"))).presets().size(), 1);
    QCOMPARE(engine.calls, 0);
}

void PrintWorkbenchTests::workbenchAllowsExplicitEmptyLegacyField()
{
    QTemporaryDir directory;
    const auto path = directory.filePath(QStringLiteral("settings.json"));
    QVERIFY(core::FileSettingsStore(path).save(workbenchSettings(false)));
    RecordingPrintEngine engine;
    app::PrintWorkbenchWindow window(path, &engine, testPrinters);
    auto* allowEmpty = window.findChild<QCheckBox*>(QStringLiteral("printField_productName_allowEmpty"));
    auto* print = window.findChild<QPushButton*>(QStringLiteral("workbenchPrintButton"));
    QVERIFY(allowEmpty != nullptr && print != nullptr);
    QVERIFY(!print->isEnabled());
    allowEmpty->setChecked(true);
    QTRY_VERIFY(print->isEnabled());
    allowEmpty->setChecked(false);
    QVERIFY(!print->isEnabled());
    QCOMPARE(engine.calls, 0);
}

void PrintWorkbenchTests::workbenchReportsAllReferencedFields()
{
    QTemporaryDir directory;
    const auto path = directory.filePath(QStringLiteral("settings.json"));
    auto settings = workbenchSettings(false);
    auto& layer = settings.templateDocuments[QStringLiteral("default")].layers.first();
    auto code = layer.elements.first();
    code.id = QStringLiteral("sales-code");
    code.type = core::TemplateElementType::FixedText;
    code.text = QStringLiteral("${salesCode}");
    code.y = 10;
    layer.elements.append(code);
    auto grid = code;
    grid.id = QStringLiteral("parts-grid");
    grid.type = core::TemplateElementType::ArrayGrid;
    grid.fieldKey.clear();
    grid.x = 40;
    grid.y = 2;
    grid.width = 35;
    grid.height = 20;
    grid.arrayGridRows = 2;
    grid.arrayGridColumns = 2;
    grid.dataPath = QStringLiteral("parts");
    grid.arrayGridCellTemplate = QStringLiteral("${name}:${weight}");
    layer.elements.append(grid);
    QVERIFY(core::FileSettingsStore(path).save(settings));
    RecordingPrintEngine engine;
    app::PrintWorkbenchWindow window(path, &engine, testPrinters);
    auto* issues = window.findChild<QListWidget*>(QStringLiteral("printPreflightIssues"));
    QVERIFY(issues != nullptr);
    QStringList messages;
    for (int row = 0; row < issues->count(); ++row) messages.append(issues->item(row)->text());
    QVERIFY(messages.join(QLatin1Char('\n')).contains(QString::fromUtf8("商品名称")));
    QVERIFY(messages.join(QLatin1Char('\n')).contains(QString::fromUtf8("销售码")));
    QVERIFY(messages.join(QLatin1Char('\n')).contains(QStringLiteral("parts")));
    QVERIFY(window.findChild<QLineEdit*>(QStringLiteral("printField_name")) == nullptr);
    fillLegacyProductName(window);
    auto* salesCode = window.findChild<QLineEdit*>(QStringLiteral("printField_salesCode"));
    QVERIFY(salesCode != nullptr);
    salesCode->setText(QStringLiteral("code-1"));
    auto* parts = window.findChild<QPlainTextEdit*>(QStringLiteral("printField_parts"));
    QVERIFY(parts != nullptr);
    parts->setPlainText(QStringLiteral("[{\"name\":\"A\",\"weight\":\"1\"}]"));
    auto* print = window.findChild<QPushButton*>(QStringLiteral("workbenchPrintButton"));
    QVERIFY(print != nullptr);
    QTRY_VERIFY(print->isEnabled());
    const auto screenshot = qEnvironmentVariable("SLEEKPR_WORKBENCH_SCREENSHOT");
    if (!screenshot.isEmpty()) {
        window.resize(1024, 720);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        QVERIFY(window.width() <= 1024);
        QTest::qWait(100);
        QVERIFY(window.grab().save(screenshot + QStringLiteral(".inferred.png")));
    }
    print->click();
    QTRY_COMPARE(engine.calls, 1);
    QVERIFY(std::any_of(engine.lastPlan.pages.first().commands.cbegin(), engine.lastPlan.pages.first().commands.cend(),
        [](const auto& command) { return command.text == QStringLiteral("A:1"); }));
}

void PrintWorkbenchTests::workbenchOffersPresetActions()
{
    QTemporaryDir directory;
    const auto path = directory.filePath(QStringLiteral("settings.json"));
    QVERIFY(core::FileSettingsStore(path).save(workbenchSettings(false)));
    RecordingPrintEngine engine;
    app::PrintWorkbenchWindow window(path, &engine, testPrinters);
    QVERIFY(window.findChild<QPushButton*>(QStringLiteral("workbenchSavePresetButton")) != nullptr);
    auto* manage = window.findChild<QPushButton*>(QStringLiteral("workbenchManagePresetsButton"));
    QVERIFY(manage != nullptr && !manage->isEnabled());
    int opens = 0;
    window.setOpenFieldPresetManagerCallback([&opens] { ++opens; });
    QVERIFY(manage->isEnabled());
    manage->click();
    QCOMPARE(opens, 1);
    QCOMPARE(engine.calls, 0);
}

void PrintWorkbenchTests::workbenchShowsPreflightProblems()
{
    QTemporaryDir directory;
    const auto path = directory.filePath(QStringLiteral("settings.json"));
    QVERIFY(core::FileSettingsStore(path).save(workbenchSettings(false)));
    RecordingPrintEngine engine;
    app::PrintWorkbenchWindow window(path, &engine, testPrinters);
    auto* panel = window.findChild<QWidget*>(QStringLiteral("printPreflightPanel"));
    QVERIFY2(panel != nullptr, "工作台必须提供可定位的打印前检查面板");
    auto* issues = panel->findChild<QListWidget*>(QStringLiteral("printPreflightIssues"));
    QVERIFY(issues != nullptr);
    QVERIFY(issues->count() > 0);
    QVERIFY(issues->item(0)->text().contains(QString::fromUtf8("商品名称")));
    QCOMPARE(engine.calls, 0);
}

void PrintWorkbenchTests::workbenchPreflightLocatesFieldAndInvalidatesImmediately()
{
    QTemporaryDir directory;
    const auto path = directory.filePath(QStringLiteral("settings.json"));
    QVERIFY(core::FileSettingsStore(path).save(workbenchSettings(false)));
    RecordingPrintEngine engine;
    app::PrintWorkbenchWindow window(path, &engine, testPrinters);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    auto* issues = window.findChild<QListWidget*>(QStringLiteral("printPreflightIssues"));
    auto* name = window.findChild<QLineEdit*>(QStringLiteral("printField_productName"));
    QVERIFY(issues != nullptr && name != nullptr && issues->count() == 1);
    QTest::mouseClick(issues->viewport(), Qt::LeftButton, Qt::NoModifier, issues->visualItemRect(issues->item(0)).center());
    QTRY_VERIFY(name->hasFocus());
    auto* preview = window.findChild<QWidget*>(QStringLiteral("workbenchPreviewPanel"));
    QVERIFY(preview != nullptr);
    QCOMPARE(preview->property("printPreflightHighlightCount").toInt(), 1);
    name->setText(QStringLiteral("updated"));
    QCOMPARE(issues->count(), 0);
    QCOMPARE(preview->property("printPreflightHighlightCount").toInt(), 0);
    auto* print = window.findChild<QPushButton*>(QStringLiteral("workbenchPrintButton"));
    QVERIFY(print != nullptr && !print->isEnabled());
    QTRY_VERIFY(print->isEnabled());
    QCOMPARE(issues->count(), 0);
    QCOMPARE(engine.calls, 0);
}

void PrintWorkbenchTests::workbenchPreflightLocatesInvalidGeometry()
{
    QTemporaryDir directory;
    const auto path = directory.filePath(QStringLiteral("settings.json"));
    auto settings = workbenchSettings(false);
    settings.templateDocuments[QStringLiteral("default")].layers.first().elements.first().x = 70;
    QVERIFY(core::FileSettingsStore(path).save(settings));
    RecordingPrintEngine engine;
    app::PrintWorkbenchWindow window(path, &engine, testPrinters);
    fillLegacyProductName(window);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    auto* issues = window.findChild<QListWidget*>(QStringLiteral("printPreflightIssues"));
    QVERIFY(issues != nullptr);
    QTRY_COMPARE(issues->count(), 1);
    QVERIFY(issues->item(0)->text().contains(QString::fromUtf8("超出")));
    QTest::mouseClick(issues->viewport(), Qt::LeftButton, Qt::NoModifier, issues->visualItemRect(issues->item(0)).center());
    auto* preview = window.findChild<QWidget*>(QStringLiteral("workbenchPreviewPanel"));
    QVERIFY(preview != nullptr);
    QVERIFY(preview->property("printPreflightDiagnostic").toBool());
    QCOMPARE(preview->property("printPreflightHighlightCount").toInt(), 1);
    const auto screenshot = qEnvironmentVariable("SLEEKPR_WORKBENCH_SCREENSHOT");
    if (!screenshot.isEmpty()) QVERIFY(window.grab().save(screenshot + QStringLiteral(".preflight.png")));
    QCOMPARE(engine.calls, 0);
}

void PrintWorkbenchTests::workbenchPreflightLocatesPrinterAndJson()
{
    QTemporaryDir directory;
    const auto path = directory.filePath(QStringLiteral("settings.json"));
    QVERIFY(core::FileSettingsStore(path).save(workbenchSettings(false)));
    RecordingPrintEngine engine;
    app::PrintWorkbenchWindow window(path, &engine, [] { return QStringList{}; });
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    fillLegacyProductName(window);
    auto* issues = window.findChild<QListWidget*>(QStringLiteral("printPreflightIssues"));
    QVERIFY(issues != nullptr);
    QTRY_COMPARE(issues->count(), 1);
    QVERIFY(issues->item(0)->text().contains(QString::fromUtf8("打印机")));
    QTest::mouseClick(issues->viewport(), Qt::LeftButton, Qt::NoModifier, issues->visualItemRect(issues->item(0)).center());
    auto* printer = window.findChild<QComboBox*>(QStringLiteral("workbenchPrinterCombo"));
    QVERIFY(printer != nullptr);
    QTRY_VERIFY(printer->hasFocus());
    useJsonInput(window);
    auto* json = window.findChild<QPlainTextEdit*>(QStringLiteral("printValuesJsonEdit"));
    QVERIFY(json != nullptr);
    json->setPlainText(QStringLiteral("{\n\"productName\": }"));
    QTRY_VERIFY(issues->count() >= 2);
    QVERIFY(issues->item(0)->text().contains(QString::fromUtf8("行")));
    QTest::mouseClick(issues->viewport(), Qt::LeftButton, Qt::NoModifier, issues->visualItemRect(issues->item(0)).center());
    QTRY_VERIFY(json->hasFocus());
    QCOMPARE(json->textCursor().blockNumber(), 1);
    QVERIFY(json->textCursor().positionInBlock() > 0);
    QCOMPARE(engine.calls, 0);
}

void PrintWorkbenchTests::workbenchPreflightWarningsRemainPrintable()
{
    QTemporaryDir directory;
    const auto path = directory.filePath(QStringLiteral("settings.json"));
    QVERIFY(core::FileSettingsStore(path).save(workbenchSettings(false)));
    RecordingPrintEngine engine;
    app::PrintWorkbenchWindow window(path, &engine, testPrinters);
    auto* empty = window.findChild<QCheckBox*>(QStringLiteral("printField_productName_allowEmpty"));
    auto* print = window.findChild<QPushButton*>(QStringLiteral("workbenchPrintButton"));
    auto* issues = window.findChild<QListWidget*>(QStringLiteral("printPreflightIssues"));
    QVERIFY(empty != nullptr && print != nullptr && issues != nullptr);
    empty->setChecked(true);
    QTRY_VERIFY(print->isEnabled());
    QCOMPARE(issues->count(), 1);
    QVERIFY(issues->item(0)->text().contains(QString::fromUtf8("警告")));
    QCOMPARE(engine.calls, 0);
}

void PrintWorkbenchTests::workbenchCopyChangesInvalidateOldLocation()
{
    QTemporaryDir directory;
    const auto path = directory.filePath(QStringLiteral("settings.json"));
    QVERIFY(core::FileSettingsStore(path).save(workbenchSettings(false)));
    RecordingPrintEngine engine;
    app::PrintWorkbenchWindow window(path, &engine, testPrinters);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    auto* empty = window.findChild<QCheckBox*>(QStringLiteral("printField_productName_allowEmpty"));
    auto* issues = window.findChild<QListWidget*>(QStringLiteral("printPreflightIssues"));
    auto* copies = window.findChild<QSpinBox*>(QStringLiteral("workbenchCopiesSpin"));
    auto* print = window.findChild<QPushButton*>(QStringLiteral("workbenchPrintButton"));
    auto* preview = window.findChild<QWidget*>(QStringLiteral("workbenchPreviewPanel"));
    QVERIFY(empty != nullptr && issues != nullptr && copies != nullptr && print != nullptr && preview != nullptr);
    empty->setChecked(true);
    QTRY_VERIFY(print->isEnabled());
    QTest::mouseClick(issues->viewport(), Qt::LeftButton, Qt::NoModifier, issues->visualItemRect(issues->item(0)).center());
    QCOMPARE(preview->property("printPreflightHighlightCount").toInt(), 1);
    copies->setValue(2);
    QCOMPARE(preview->property("printPreflightHighlightCount").toInt(), 0);
    QCOMPARE(issues->count(), 0);
    QVERIFY(!print->isEnabled());
    QTRY_VERIFY(print->isEnabled());
    QCOMPARE(issues->count(), 1);
    QCOMPARE(engine.calls, 0);
}

void PrintWorkbenchTests::workbenchPrintAfterHighlightKeepsNativeCommands()
{
    QTemporaryDir directory;
    const auto path = directory.filePath(QStringLiteral("settings.json"));
    auto settings = workbenchSettings(false);
    auto fixed = settings.templateDocuments[QStringLiteral("default")].layers.first().elements.first();
    fixed.id = QStringLiteral("fixed");
    fixed.type = core::TemplateElementType::FixedText;
    fixed.text = QString::fromUtf8("固定内容");
    fixed.y = 10;
    settings.templateDocuments[QStringLiteral("default")].layers.first().elements.append(fixed);
    QVERIFY(core::FileSettingsStore(path).save(settings));
    RecordingPrintEngine engine;
    app::PrintWorkbenchWindow window(path, &engine, testPrinters);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    auto* empty = window.findChild<QCheckBox*>(QStringLiteral("printField_productName_allowEmpty"));
    auto* issues = window.findChild<QListWidget*>(QStringLiteral("printPreflightIssues"));
    auto* print = window.findChild<QPushButton*>(QStringLiteral("workbenchPrintButton"));
    auto* preview = window.findChild<QWidget*>(QStringLiteral("workbenchPreviewPanel"));
    QVERIFY(empty != nullptr && issues != nullptr && print != nullptr && preview != nullptr);
    empty->setChecked(true);
    QTRY_VERIFY(print->isEnabled());
    QTest::mouseClick(issues->viewport(), Qt::LeftButton, Qt::NoModifier, issues->visualItemRect(issues->item(0)).center());
    QCOMPARE(preview->property("printPreflightHighlightCount").toInt(), 1);
    infrastructure::TemplatePrintInput input;
    input.templateId = QStringLiteral("default");
    input.printerName = testPrinters().first();
    input.requireCompleteValues = true;
    input.values = QJsonObject{{"productName", ""}};
    const auto expected = infrastructure::TemplatePrintService(path).prepare(settings, input);
    QVERIFY(expected.has_value());
    print->click();
    QTRY_COMPARE(engine.calls, 1);
    QCOMPARE(engine.lastPlan.pages.size(), expected->printPlan.pages.size());
    QCOMPARE(core::NativePlanJsonSerializer().toJson(engine.lastPlan.firstPageAsLabelPlan()),
        core::NativePlanJsonSerializer().toJson(expected->printPlan.firstPageAsLabelPlan()));
}

void PrintWorkbenchTests::workbenchRefreshesDisplayNameAndPrintsWithOriginalKey()
{
    QTemporaryDir directory;
    const auto path = directory.filePath(QStringLiteral("settings.json"));
    auto settings = workbenchSettings();
    QVERIFY(core::FileSettingsStore(path).save(settings));
    RecordingPrintEngine engine;
    app::PrintWorkbenchWindow window(path, &engine, testPrinters);
    fillLegacyProductName(window);
    auto* input = window.findChild<QLineEdit*>(QStringLiteral("printField_productName"));
    auto* title = fieldTitle(window);
    auto* print = window.findChild<QPushButton*>(QStringLiteral("workbenchPrintButton"));
    QVERIFY(input != nullptr && title != nullptr && print != nullptr);
    QTRY_VERIFY(print->isEnabled());
    settings.templateDocuments[QStringLiteral("default")].fieldDisplayNames.insert(QStringLiteral("productName"), QString::fromUtf8("工作台品名"));
    QVERIFY(core::FileSettingsStore(path).save(settings));
    window.refreshFieldDisplayNames();
    QCOMPARE(fieldTitle(window), title);
    QCOMPARE(title->text(), QString::fromUtf8("工作台品名 *"));
    QCOMPARE(window.findChild<QLineEdit*>(QStringLiteral("printField_productName")), input);
    QCOMPARE(input->text(), QString::fromUtf8("本次真实数据"));
    QVERIFY(print->isEnabled());
    const auto screenshot = qEnvironmentVariable("SLEEKPR_WORKBENCH_SCREENSHOT");
    if (!screenshot.isEmpty()) {
        window.resize(1024, 720);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        QTest::qWait(50);
        QVERIFY(window.grab().save(screenshot + QStringLiteral(".display-name.png")));
    }
    print->click();
    QTRY_COMPARE(engine.calls, 1);
    QCOMPARE(engine.lastPlan.pages.first().commands.first().text, QString::fromUtf8("本次真实数据"));
}

void PrintWorkbenchTests::workbenchDisplayNameRefreshKeepsInvalidDraft()
{
    QTemporaryDir directory;
    const auto path = directory.filePath(QStringLiteral("settings.json"));
    auto settings = workbenchSettings();
    QVERIFY(core::FileSettingsStore(path).save(settings));
    RecordingPrintEngine engine;
    app::PrintWorkbenchWindow window(path, &engine, testPrinters);
    useJsonInput(window);
    auto* json = window.findChild<QPlainTextEdit*>(QStringLiteral("printValuesJsonEdit"));
    auto* fields = window.findChild<app::PrintFieldEditorPanel*>();
    auto* title = fieldTitle(window);
    QVERIFY(json != nullptr && fields != nullptr && title != nullptr);
    const auto draft = QStringLiteral("{\n\"productName\": }");
    json->setPlainText(draft);
    settings.templateDocuments[QStringLiteral("default")].fieldDisplayNames.insert(QStringLiteral("productName"), QString::fromUtf8("更新后的名称"));
    QVERIFY(core::FileSettingsStore(path).save(settings));
    QSignalSpy edits(fields, &app::PrintFieldEditorPanel::valuesEdited);
    window.refreshFieldDisplayNames();
    QCOMPARE(title->text(), QString::fromUtf8("更新后的名称 *"));
    QCOMPARE(json->toPlainText(), draft);
    QCOMPARE(edits.count(), 0);
    QVERIFY(!window.findChild<QPushButton*>(QStringLiteral("workbenchPrintButton"))->isEnabled());
    QCOMPARE(engine.calls, 0);
}

void PrintWorkbenchTests::workbenchActivationRefreshesLibraryFieldNames()
{
    QTemporaryDir directory;
    const auto path = directory.filePath(QStringLiteral("settings.json"));
    auto settings = workbenchSettings(false);
    QVERIFY(core::FileSettingsStore(path).save(settings));
    auto document = settings.templateDocuments.value(QStringLiteral("default"));
    document.id = QStringLiteral("library-document");
    document.fieldDisplayNames.insert(QStringLiteral("productName"), QString::fromUtf8("原库字段名"));
    core::TemplateLibraryStore library(directory.filePath(QStringLiteral("templates")));
    QVERIFY(library.saveTemplate(document));
    RecordingPrintEngine engine;
    app::PrintWorkbenchWindow window(path, &engine, testPrinters);
    auto* list = window.findChild<QListWidget*>(QStringLiteral("workbenchTemplateList"));
    QVERIFY(list != nullptr);
    for (int row = 0; row < list->count(); ++row) {
        if (list->item(row)->data(Qt::UserRole).toString().contains(QStringLiteral("library:"))) list->setCurrentRow(row);
    }
    auto* title = fieldTitle(window);
    QVERIFY(title != nullptr);
    QCOMPARE(title->text(), QString::fromUtf8("原库字段名 *"));
    fillLegacyProductName(window);
    document.fieldDisplayNames.insert(QStringLiteral("productName"), QString::fromUtf8("新库字段名"));
    QVERIFY(library.saveTemplate(document));
    QEvent activation(QEvent::WindowActivate);
    QCoreApplication::sendEvent(&window, &activation);
    QTest::qWait(50);
    QCOMPARE(title->text(), QString::fromUtf8("新库字段名 *"));
    QCOMPARE(window.findChild<QLineEdit*>(QStringLiteral("printField_productName"))->text(), QString::fromUtf8("本次真实数据"));
    QCOMPARE(engine.calls, 0);
}

void PrintWorkbenchTests::workbenchChecksUseDisplayNamesWithoutDuplicateStatus()
{
    QTemporaryDir directory;
    const auto path = directory.filePath(QStringLiteral("settings.json"));
    auto settings = workbenchSettings(false);
    auto& document = settings.templateDocuments[QStringLiteral("default")];
    document.fieldDisplayNames.insert(QStringLiteral("productName"), QString::fromUtf8("商品名称（%2）"));
    QVERIFY(core::FileSettingsStore(path).save(settings));
    RecordingPrintEngine engine;
    app::PrintWorkbenchWindow window(path, &engine, testPrinters);
    auto* issues = window.findChild<QListWidget*>(QStringLiteral("printPreflightIssues"));
    auto* status = window.findChild<QLabel*>(QStringLiteral("workbenchStatusLabel"));
    auto* print = window.findChild<QPushButton*>(QStringLiteral("workbenchPrintButton"));
    auto* empty = window.findChild<QCheckBox*>(QStringLiteral("printField_productName_allowEmpty"));
    QVERIFY(issues != nullptr && status != nullptr && print != nullptr && empty != nullptr);
    QCOMPARE(issues->count(), 1);
    QCOMPARE(issues->item(0)->text(), QString::fromUtf8("错误：缺少字段 商品名称（%2） 的输入值"));
    QVERIFY(status->isHidden());

    // 改名即时刷新提示，但定位和实际输入仍使用原始 key。
    document.fieldDisplayNames.insert(QStringLiteral("productName"), QString::fromUtf8("标签品名"));
    QVERIFY(core::FileSettingsStore(path).save(settings));
    window.refreshFieldDisplayNames();
    QCOMPARE(issues->item(0)->text(), QString::fromUtf8("错误：缺少字段 标签品名 的输入值"));
    empty->setChecked(true);
    QTRY_VERIFY(print->isEnabled());
    QCOMPARE(issues->count(), 1);
    QCOMPARE(issues->item(0)->text(), QString::fromUtf8("警告：字段 标签品名 已明确留空"));
    QVERIFY(status->isHidden());

    fillLegacyProductName(window);
    empty->setChecked(false);
    QTRY_VERIFY(print->isEnabled());
    QCOMPARE(issues->count(), 0);
    QVERIFY(status->isHidden());
    print->click();
    QTRY_COMPARE(engine.calls, 1);
    QVERIFY(!status->isHidden());
    QVERIFY(status->text().contains(QString::fromUtf8("提交")));
    QCOMPARE(engine.lastPlan.pages.first().commands.first().text, QString::fromUtf8("本次真实数据"));
}

void PrintWorkbenchTests::workbenchArrayProblemsUseDisplayNames()
{
    QTemporaryDir directory;
    const auto path = directory.filePath(QStringLiteral("settings.json"));
    auto settings = workbenchSettings(false);
    auto& document = settings.templateDocuments[QStringLiteral("default")];
    auto& grid = document.layers.first().elements.first();
    grid.type = core::TemplateElementType::ArrayGrid;
    grid.dataPath = QStringLiteral("items");
    grid.arrayGridRows = 1;
    grid.arrayGridColumns = 1;
    grid.arrayGridCellTemplate = QStringLiteral("${productName}:${weight}");
    document.fieldDisplayNames = {{QStringLiteral("items"), QString::fromUtf8("商品明细")},
        {QStringLiteral("productName"), QString::fromUtf8("明细品名")}};
    QVERIFY(core::FileSettingsStore(path).save(settings));
    RecordingPrintEngine engine;
    app::PrintWorkbenchWindow window(path, &engine, testPrinters);
    auto* items = window.findChild<QPlainTextEdit*>(QStringLiteral("printField_items"));
    auto* issues = window.findChild<QListWidget*>(QStringLiteral("printPreflightIssues"));
    QVERIFY(items != nullptr && issues != nullptr);
    items->setPlainText(QStringLiteral("[{}]"));
    QTRY_COMPARE(issues->count(), 2);
    QCOMPARE(issues->item(0)->text(), QString::fromUtf8("错误：数组 商品明细 第1项缺少子字段 明细品名"));
    QCOMPARE(issues->item(1)->text(), QString::fromUtf8("错误：数组 商品明细 第1项缺少子字段 weight"));
    QVERIFY(window.findChild<QLabel*>(QStringLiteral("workbenchStatusLabel"))->isHidden());
    QVERIFY(!window.findChild<QPushButton*>(QStringLiteral("workbenchPrintButton"))->isEnabled());
    QCOMPARE(engine.calls, 0);
}

QTEST_MAIN(PrintWorkbenchTests)
#include "tst_print_workbench.moc"

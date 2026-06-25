#include <QtTest/QtTest>

#include <QAbstractItemView>
#include <QAction>
#include <QApplication>
#include <QByteArray>
#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFile>
#include <QFocusEvent>
#include <QGridLayout>
#include <QImage>
#include <QLineEdit>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSplitter>
#include <QSpinBox>
#include <QTabWidget>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QTimer>

#include <algorithm>
#include <cmath>

#include "sleekpr/app/TemplateDragCoordinateMapper.h"
#include "sleekpr/app/TemplateDesignerCommand.h"
#include "sleekpr/app/TemplateDesignerFactory.h"
#include "sleekpr/app/TemplateDesignerPresenter.h"
#include "sleekpr/app/TableColumnEditorPanel.h"
#include "sleekpr/app/TableColumnTextCodec.h"
#include "sleekpr/app/TemplateInspectorPanel.h"
#include "sleekpr/app/TemplateElementHitTester.h"
#include "sleekpr/app/AppTheme.h"
#include "sleekpr/app/FieldPresetManagerWindow.h"
#include "sleekpr/app/PaperSpecManagerWindow.h"
#include "sleekpr/app/TemplatePreviewLabel.h"
#include "sleekpr/app/TemplateDesignerWindow.h"
#include "sleekpr/app/WindowActivation.h"
#include "sleekpr/app/SettingsWindow.h"
#include "sleekpr/core/native/NativeDrawCommand.h"
#include "sleekpr/core/native/NativePrintDrawingPlan.h"
#include "sleekpr/core/settings/FileSettingsStore.h"
#include "sleekpr/core/settings/TemplateElement.h"
#include "sleekpr/core/templates/FieldPresetStore.h"
#include "sleekpr/core/templates/PaperSpecStore.h"
#include "sleekpr/core/templates/TemplateLibraryStore.h"

using namespace sleekpr::app;
using namespace sleekpr::core;

namespace {

TemplateDocument createLibraryTestDocument(
    const QString& id,
    const QString& name,
    const QString& templateKey,
    const QString& layerName,
    const QString& elementText)
{
    TemplateLayer layer;
    layer.id = id + QStringLiteral("-layer");
    layer.name = layerName;

    TemplateElement element;
    element.id = id + QStringLiteral("-title");
    element.layerId = layer.id;
    element.text = elementText;
    layer.elements.append(element);

    TemplateDocument document;
    document.id = id;
    document.name = name;
    document.templateKey = templateKey;
    document.layers = {layer};
    return document;
}

QByteArray imageBytes(const QPixmap& pixmap)
{
    const auto image = pixmap.toImage().convertToFormat(QImage::Format_ARGB32);
    return QByteArray(reinterpret_cast<const char*>(image.constBits()), image.sizeInBytes());
}

} // 匿名命名空间

class AppTests final : public QObject
{
    Q_OBJECT

private slots:
    void openingAndSavingSettingsDoesNotCreateImplicitElementOverride();
    void startupWindowActivationShowsNormalWindow();
    void dragCoordinateMapperConvertsPixelDeltaToMillimeters();
    void templatePreviewLabelReportsDragDelta();
    void templatePreviewLabelRejectsDragStartOutsideEditableArea();
    void settingsWindowShowsPreviewOnlyWithoutElementEditor();
    void settingsWindowLoadsTemplateLibraryDocuments();
    void settingsWindowEditsAllowedOrigins();
    void settingsWindowSavesLocalHttpLimitSettings();
    void settingsWindowUsesCompactLocalHttpLimitGrid();
    void settingsWindowShowsWorkbenchNavigationAndOverview();
    void templateElementHitTesterSelectsSmallestTopmostElement();
    void templateElementHitTesterReturnsArrayGridElementIdFromCellCommand();
    void templateElementHitTesterReturnsTableElementIdFromRowCommand();
    void templateDesignerFactoryCreatesDefaultElementsAndDocuments();
    void templateDesignerWindowAddsLayer();
    void templateDesignerWindowPreservesLegacyDynamicElements();
    void templateDesignerWindowAddsFixedTextToCurrentLayer();
    void templateDesignerWindowEditsFixedTextPlaceholderContent();
    void templateDesignerWindowEditsQrPayloadAndRotation();
    void templateDesignerWindowAddsTableToCurrentLayer();
    void templateDesignerWindowAddsAndEditsArrayGridElement();
    void templateDesignerWindowUpdatesSelectedTableProperties();
    void templateDesignerWindowDeletesSelectedEmptyLayer();
    void templateDesignerWindowDoesNotEditLockedLayer();
    void templateDesignerWindowShowsDesignerPreview();
    void templateDesignerWindowConfiguresRulerPrecision();
    void templateDesignerWindowConfiguresDesignAids();
    void templateDesignerWindowPrePrintsCurrentTemplate();
    void templateDesignerWindowRendersPlaceholdersWithSampleData();
    void templateDesignerWindowSavesSampleDataWithDedicatedButton();
    void templateDesignerWindowSavesSampleDataWithTemplate();
    void templateDesignerWindowSavesAndRestoresVersion();
    void templateDesignerWindowExportsAndImportsTemplateFile();
    void templateDesignerWindowSavesAndLoadsTemplateLibraryDocument();
    void templateDesignerWindowListsAndLoadsSelectedLibraryTemplate();
    void templateDesignerWindowSelectsLibraryTemplateWhenCurrentRowChanges();
    void templateDesignerWindowCreatesBlankLibraryTemplate();
    void templateDesignerPreviewKeepsRenderedAspectRatio();
    void templateDesignerPreviewUsesInteractiveDpi();
    void templateDesignerWindowDeletesSelectedTemplateLibraryDocument();
    void templateDesignerWindowSearchesCopiesAndRenamesLibraryTemplates();
    void templateDesignerWindowSavesAndReplacesDeviceProfile();
    void templateDesignerWindowCalculatesDeviceScaleFromMeasuredSize();
    void templateDesignerWindowSelectsPaperSpecFromStore();
    void templateDesignerWindowShowsTemplateValidationIssues();
    void templateDesignerWindowUsesWorkbenchShell();
    void templateDesignerWindowUsesProfessionalEditorLayout();
    void templateDesignerWindowSwitchesInspectorTabWhenCanvasElementSelected();
    void templateDesignerWindowElementListSupportsRenameMenuAndDragSort();
    void templateDesignerWindowAutoAppliesElementAndTableProperties();
    void templateDesignerWindowAutoAppliesTextAutoFitFontProperties();
    void templateDesignerWindowAutoApplyKeepsElementListStable();
    void templateDesignerWindowCoalescesAutoApplyPreviewRefresh();
    void templateDesignerWindowAppliesNumericPropertyPromptly();
    void templateDesignerWindowCoalescesDragPreviewRefresh();
    void templateDesignerWindowSkipsNoopPropertyApply();
    void templateDesignerWindowAppliesPendingTextOnFocusOut();
    void templateDesignerWindowShowsSampleJsonErrorNearEditor();
    void templateDesignerWindowToolbarExposesEditingActionsAndZoom();
    void templateDesignerStateSkipsDuplicateDocumentHistory();
    void templateInspectorPanelEmitsSemanticPropertySignals();
    void templateDesignerPresenterAppliesElementPropertyCommand();
    void tableColumnTextCodecFormatsAndParsesLegacyColumns();
    void tableDesignerCommandUsesStructuredColumnsWhenPresent();
    void templateDesignerPresenterMapsTableColumns();
    void tableColumnEditorPanelEditsAndReordersColumns();
    void templateDesignerPresenterRejectsInvalidTableColumns();
    void paperSpecManagerWindowSavesAndDeletesSpecs();
    void fieldPresetManagerWindowSavesAndDeletesPresets();
    void settingsWindowHasPaperSpecManagerEntry();
    void settingsWindowHasFieldPresetManagerEntry();
    void templateDesignerWindowHasPaperSpecManagerEntry();
    void templateDesignerWindowHasFieldPresetManagerEntry();
    void applicationRoutesPreviewOnlyThroughSettingsWindow();
    void settingsWindowHasTemplateDesignerEntry();
    void appThemeDefinesWorkbenchStyle();
};

void AppTests::openingAndSavingSettingsDoesNotCreateImplicitElementOverride()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const auto settingsPath = dir.filePath("settings.json");
    FileSettingsStore store(settingsPath);
    QVERIFY(store.save(PrintClientSettings{}));

    int appliedCount = 0;
    SettingsWindow window(settingsPath, [&appliedCount](const PrintClientSettings&) {
        ++appliedCount;
    });

    QPushButton* saveButton = nullptr;
    for (auto* button : window.findChildren<QPushButton*>()) {
        if (button->text() == QString::fromUtf8("保存")) {
            saveButton = button;
            break;
        }
    }
    QVERIFY(saveButton != nullptr);

    saveButton->click();

    const auto actual = store.load();
    QVERIFY(!actual.templateOverrides.value("default").contains("identifier"));
    QVERIFY(actual.templateOverrides.value("default").isEmpty());
    QCOMPARE(appliedCount, 1);
}

void AppTests::startupWindowActivationShowsNormalWindow()
{
    QWidget window;
    window.setWindowTitle(QString::fromUtf8("启动显示测试"));
    window.showMinimized();
    QVERIFY(window.isMinimized());

    showAndActivateWindow(window);
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    QVERIFY(window.isVisible());
    QVERIFY(!window.isMinimized());
}

void AppTests::dragCoordinateMapperConvertsPixelDeltaToMillimeters()
{
    const TemplateDragCoordinateMapper mapper(QSizeF(80.0, 30.0), QSize(800, 300));

    const auto moved = mapper.movedTopLeftMm(QPointF(10.0, 5.0), QPoint(20, -10), QSizeF(12.0, 4.0));
    QCOMPARE(moved.x(), 12.0);
    QCOMPARE(moved.y(), 4.0);

    const auto clamped = mapper.movedTopLeftMm(QPointF(78.0, 29.0), QPoint(50, 50), QSizeF(4.0, 2.0));
    QCOMPARE(clamped.x(), 76.0);
    QCOMPARE(clamped.y(), 28.0);
}

void AppTests::templatePreviewLabelReportsDragDelta()
{
    TemplatePreviewLabel label;
    label.resize(120, 80);
    label.setDraggingEnabled(true);

    QPoint lastDelta;
    label.setDragDeltaCallback([&lastDelta](QPoint delta) {
        lastDelta = delta;
    });

    label.show();
    QVERIFY(QTest::qWaitForWindowExposed(&label));

    QTest::mousePress(&label, Qt::LeftButton, Qt::NoModifier, QPoint(10, 10));
    QTest::mouseMove(&label, QPoint(25, 18));
    QTest::mouseRelease(&label, Qt::LeftButton, Qt::NoModifier, QPoint(25, 18));

    QCOMPARE(lastDelta, QPoint(15, 8));
}

void AppTests::templatePreviewLabelRejectsDragStartOutsideEditableArea()
{
    TemplatePreviewLabel label;
    label.resize(120, 80);
    label.setDraggingEnabled(true);
    label.setDragStartCallback([](QPoint position) {
        return QRect(QPoint(0, 0), QSize(40, 40)).contains(position);
    });

    QPoint lastDelta;
    label.setDragDeltaCallback([&lastDelta](QPoint delta) {
        lastDelta = delta;
    });

    label.show();
    QVERIFY(QTest::qWaitForWindowExposed(&label));

    QTest::mousePress(&label, Qt::LeftButton, Qt::NoModifier, QPoint(80, 60));
    QTest::mouseMove(&label, QPoint(95, 70));
    QTest::mouseRelease(&label, Qt::LeftButton, Qt::NoModifier, QPoint(95, 70));
    QCOMPARE(lastDelta, QPoint());

    QTest::mousePress(&label, Qt::LeftButton, Qt::NoModifier, QPoint(10, 10));
    QTest::mouseMove(&label, QPoint(25, 20));
    QTest::mouseRelease(&label, Qt::LeftButton, Qt::NoModifier, QPoint(25, 20));
    QCOMPARE(lastDelta, QPoint(15, 10));
}

void AppTests::settingsWindowShowsPreviewOnlyWithoutElementEditor()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const auto settingsPath = dir.filePath("settings.json");
    FileSettingsStore store(settingsPath);
    QVERIFY(store.save(PrintClientSettings{}));

    SettingsWindow window(settingsPath, nullptr);
    TemplatePreviewLabel* previewLabel = nullptr;
    for (auto* label : window.findChildren<QLabel*>()) {
        previewLabel = dynamic_cast<TemplatePreviewLabel*>(label);
        if (previewLabel != nullptr) {
            break;
        }
    }
    QVERIFY(previewLabel != nullptr);
    QVERIFY(!previewLabel->pixmap().isNull());
    QVERIFY(window.findChild<QCheckBox*>(QStringLiteral("snapToGridCheck")) == nullptr);
    QVERIFY(window.findChild<QPushButton*>(QStringLiteral("addFixedTextButton")) == nullptr);
    QVERIFY(window.findChild<QPushButton*>(QStringLiteral("addBoundFieldButton")) == nullptr);
    QVERIFY(window.findChild<QPushButton*>(QStringLiteral("addQrCodeButton")) == nullptr);
    QVERIFY(window.findChild<QPushButton*>(QStringLiteral("addRectangleButton")) == nullptr);
    QVERIFY(window.findChild<QLineEdit*>(QStringLiteral("customElementTextEdit")) == nullptr);
    QVERIFY(window.findChild<QComboBox*>(QStringLiteral("customElementTypeCombo")) == nullptr);
    QVERIFY(window.findChild<QComboBox*>(QStringLiteral("customElementFieldCombo")) == nullptr);
    QVERIFY(window.findChild<QDoubleSpinBox*>(QStringLiteral("elementXSpin")) == nullptr);
    QVERIFY(window.findChild<QDoubleSpinBox*>(QStringLiteral("elementYSpin")) == nullptr);
    QVERIFY(window.findChild<QDoubleSpinBox*>(QStringLiteral("elementWidthSpin")) == nullptr);
    QVERIFY(window.findChild<QDoubleSpinBox*>(QStringLiteral("elementHeightSpin")) == nullptr);
    QVERIFY(window.findChild<QDoubleSpinBox*>(QStringLiteral("elementFontSizeSpin")) == nullptr);
    QVERIFY(window.findChild<QPushButton*>(QStringLiteral("deleteCustomElementButton")) == nullptr);
}

void AppTests::settingsWindowLoadsTemplateLibraryDocuments()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const auto settingsPath = dir.filePath("settings.json");
    const auto templateDirectoryPath = dir.filePath(QStringLiteral("templates"));
    FileSettingsStore settingsStore(settingsPath);
    QVERIFY(settingsStore.save(PrintClientSettings{}));

    TemplateDocument document = createLibraryTestDocument(
        QStringLiteral("template-shop-label"),
        QString::fromUtf8("门店标签模板"),
        QStringLiteral("default"),
        QString::fromUtf8("基础图层"),
        QStringLiteral("${product_name}"));
    document.layers.first().elements.first().x = 50.0;
    document.layers.first().elements.first().y = 20.0;
    document.layers.first().elements.first().width = 25.0;
    document.layers.first().elements.first().height = 8.0;
    document.layers.first().elements.first().fontSizePt = 8.0;
    document.sampleData.insert(QStringLiteral("product_name"), QString::fromUtf8("模板库商品"));

    TemplateLibraryStore templateStore(templateDirectoryPath);
    QVERIFY(templateStore.saveTemplate(document));

    SettingsWindow window(settingsPath, nullptr, templateDirectoryPath);
    auto* templateCombo = window.findChild<QComboBox*>(QStringLiteral("settingsTemplateCombo"));
    TemplatePreviewLabel* previewLabel = nullptr;
    for (auto* label : window.findChildren<QLabel*>()) {
        previewLabel = dynamic_cast<TemplatePreviewLabel*>(label);
        if (previewLabel != nullptr) {
            break;
        }
    }
    QVERIFY(templateCombo != nullptr);
    QVERIFY(previewLabel != nullptr);
    QVERIFY(!previewLabel->pixmap().isNull());

    const auto defaultBytes = imageBytes(previewLabel->pixmap());
    const auto libraryIndex = templateCombo->findData(QStringLiteral("template-shop-label"));
    QVERIFY(libraryIndex >= 0);
    QVERIFY(templateCombo->itemText(libraryIndex).contains(QString::fromUtf8("门店标签模板")));

    templateCombo->setCurrentIndex(libraryIndex);

    QVERIFY(!previewLabel->pixmap().isNull());
    QVERIFY(imageBytes(previewLabel->pixmap()) != defaultBytes);
}

void AppTests::settingsWindowEditsAllowedOrigins()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const auto settingsPath = dir.filePath("settings.json");
    PrintClientSettings settings;
    settings.allowedOrigins = {QStringLiteral("https://manager.example.com")};
    FileSettingsStore store(settingsPath);
    QVERIFY(store.save(settings));

    SettingsWindow window(settingsPath, nullptr);
    auto* originsEdit = window.findChild<QPlainTextEdit*>(QStringLiteral("allowedOriginsEdit"));
    auto* saveButton = window.findChild<QPushButton*>(QStringLiteral("saveSettingsButton"));
    QVERIFY(originsEdit != nullptr);
    QVERIFY(saveButton != nullptr);
    QVERIFY(originsEdit->toPlainText().contains(QStringLiteral("https://manager.example.com")));

    originsEdit->setPlainText(QStringLiteral(
        "https://manager.example.com\n"
        "\n"
        " https://branch.example.com \n"
        "https://manager.example.com"));
    saveButton->click();

    const auto actual = store.load();
    QCOMPARE(actual.allowedOrigins, QStringList({
        QStringLiteral("https://manager.example.com"),
        QStringLiteral("https://branch.example.com"),
    }));
}

void AppTests::settingsWindowSavesLocalHttpLimitSettings()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const auto settingsPath = dir.filePath("settings.json");
    PrintClientSettings settings;
    settings.localHttpLimits.maxHeaderBytes = 32 * 1024;
    settings.localHttpLimits.maxContentLengthBytes = 10 * 1024 * 1024;
    settings.localHttpLimits.maxPreviewBatchItems = 40;
    settings.localHttpLimits.maxPreviewPages = 80;
    settings.localHttpLimits.maxPreviewResponseBytes = 5 * 1024 * 1024;
    QVERIFY(FileSettingsStore(settingsPath).save(settings));

    PrintClientSettings appliedSettings;
    int appliedCount = 0;
    SettingsWindow window(settingsPath, [&appliedSettings, &appliedCount](const PrintClientSettings& nextSettings) {
        appliedSettings = nextSettings;
        ++appliedCount;
    });

    auto* headerSpin = window.findChild<QSpinBox*>(QStringLiteral("settingsMaxHeaderKbSpin"));
    auto* bodySpin = window.findChild<QSpinBox*>(QStringLiteral("settingsMaxBodyMbSpin"));
    auto* batchSpin = window.findChild<QSpinBox*>(QStringLiteral("settingsMaxPreviewBatchItemsSpin"));
    auto* pagesSpin = window.findChild<QSpinBox*>(QStringLiteral("settingsMaxPreviewPagesSpin"));
    auto* responseSpin = window.findChild<QSpinBox*>(QStringLiteral("settingsMaxPreviewResponseMbSpin"));
    auto* saveButton = window.findChild<QPushButton*>(QStringLiteral("saveSettingsButton"));
    QVERIFY(headerSpin != nullptr);
    QVERIFY(bodySpin != nullptr);
    QVERIFY(batchSpin != nullptr);
    QVERIFY(pagesSpin != nullptr);
    QVERIFY(responseSpin != nullptr);
    QVERIFY(saveButton != nullptr);

    QCOMPARE(headerSpin->value(), 32);
    QCOMPARE(bodySpin->value(), 10);
    QCOMPARE(batchSpin->value(), 40);
    QCOMPARE(pagesSpin->value(), 80);
    QCOMPARE(responseSpin->value(), 5);

    headerSpin->setValue(24);
    bodySpin->setValue(9);
    batchSpin->setValue(60);
    pagesSpin->setValue(120);
    responseSpin->setValue(6);
    saveButton->click();

    QCOMPARE(appliedCount, 1);
    QCOMPARE(appliedSettings.localHttpLimits.maxHeaderBytes.value(), qsizetype(24 * 1024));
    QCOMPARE(appliedSettings.localHttpLimits.maxContentLengthBytes.value(), qsizetype(9 * 1024 * 1024));
    QCOMPARE(appliedSettings.localHttpLimits.maxPreviewBatchItems.value(), 60);
    QCOMPARE(appliedSettings.localHttpLimits.maxPreviewPages.value(), 120);
    QCOMPARE(appliedSettings.localHttpLimits.maxPreviewResponseBytes.value(), qsizetype(6 * 1024 * 1024));

    const auto saved = FileSettingsStore(settingsPath).load();
    QCOMPARE(saved.localHttpLimits.maxHeaderBytes.value(), qsizetype(24 * 1024));
    QCOMPARE(saved.localHttpLimits.maxContentLengthBytes.value(), qsizetype(9 * 1024 * 1024));
    QCOMPARE(saved.localHttpLimits.maxPreviewBatchItems.value(), 60);
    QCOMPARE(saved.localHttpLimits.maxPreviewPages.value(), 120);
    QCOMPARE(saved.localHttpLimits.maxPreviewResponseBytes.value(), qsizetype(6 * 1024 * 1024));
}

void AppTests::settingsWindowUsesCompactLocalHttpLimitGrid()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const auto settingsPath = dir.filePath("settings.json");
    QVERIFY(FileSettingsStore(settingsPath).save(PrintClientSettings{}));

    SettingsWindow window(settingsPath, nullptr);

    auto* limitsGridPanel = window.findChild<QWidget*>(QStringLiteral("settingsHttpLimitsGrid"));
    QVERIFY(limitsGridPanel != nullptr);

    auto* limitsGrid = qobject_cast<QGridLayout*>(limitsGridPanel->layout());
    QVERIFY(limitsGrid != nullptr);
    QCOMPARE(limitsGrid->columnCount(), 2);
    QVERIFY(limitsGrid->rowCount() >= 3);

    const QStringList spinNames = {
        QStringLiteral("settingsMaxHeaderKbSpin"),
        QStringLiteral("settingsMaxBodyMbSpin"),
        QStringLiteral("settingsMaxPreviewBatchItemsSpin"),
        QStringLiteral("settingsMaxPreviewPagesSpin"),
        QStringLiteral("settingsMaxPreviewResponseMbSpin"),
    };
    for (const auto& spinName : spinNames) {
        auto* spinBox = window.findChild<QSpinBox*>(spinName);
        QVERIFY2(spinBox != nullptr, qPrintable(spinName));
        QVERIFY2(spinBox->minimumWidth() >= 96, qPrintable(spinName));
    }
}

void AppTests::settingsWindowShowsWorkbenchNavigationAndOverview()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const auto settingsPath = dir.filePath("settings.json");
    PrintClientSettings settings;
    settings.defaultPrinter = QStringLiteral("Office Label Printer");
    QVERIFY(FileSettingsStore(settingsPath).save(settings));

    SettingsWindow window(settingsPath, nullptr);

    auto* shell = window.findChild<QWidget*>(QStringLiteral("settingsWorkbenchShell"));
    auto* navigationRail = window.findChild<QWidget*>(QStringLiteral("settingsNavigationRail"));
    auto* overviewPanel = window.findChild<QWidget*>(QStringLiteral("settingsOverviewPanel"));
    auto* serviceStatusLabel = window.findChild<QLabel*>(QStringLiteral("settingsServiceStatusLabel"));
    auto* printerStatusLabel = window.findChild<QLabel*>(QStringLiteral("settingsPrinterStatusLabel"));
    auto* templateStatusLabel = window.findChild<QLabel*>(QStringLiteral("settingsTemplateStatusLabel"));
    auto* designerButton = window.findChild<QPushButton*>(QStringLiteral("openTemplateDesignerButton"));
    auto* paperSpecButton = window.findChild<QPushButton*>(QStringLiteral("openPaperSpecManagerButton"));
    auto* fieldPresetButton = window.findChild<QPushButton*>(QStringLiteral("openFieldPresetManagerButton"));

    QVERIFY(shell != nullptr);
    QVERIFY(navigationRail != nullptr);
    QVERIFY(overviewPanel != nullptr);
    QVERIFY(serviceStatusLabel != nullptr);
    QVERIFY(printerStatusLabel != nullptr);
    QVERIFY(templateStatusLabel != nullptr);
    QVERIFY(designerButton != nullptr);
    QVERIFY(paperSpecButton != nullptr);
    QVERIFY(fieldPresetButton != nullptr);
    QVERIFY(serviceStatusLabel->text().contains(QStringLiteral("37122")));
    QVERIFY(printerStatusLabel->text().contains(QStringLiteral("Office Label Printer")));
    QVERIFY(templateStatusLabel->text().contains(QString::fromUtf8("默认标签")));
}

void AppTests::templateElementHitTesterSelectsSmallestTopmostElement()
{
    QList<NativeDrawCommand> commands;
    commands.append(NativeDrawCommand{NativeDrawCommandType::Text, 0.0, 0.0, 20.0, 20.0, QString(), 0.0, false, 0.0, 0, false, "large"});
    commands.append(NativeDrawCommand{NativeDrawCommandType::Text, 5.0, 5.0, 4.0, 4.0, QString(), 0.0, false, 0.0, 0, false, "small"});
    commands.append(NativeDrawCommand{NativeDrawCommandType::Text, 5.0, 5.0, 4.0, 4.0, QString(), 0.0, false, 0.0, 0, false, "smallTop"});

    const auto hit = TemplateElementHitTester().hitTest(commands, QSizeF(80.0, 30.0), QSize(800, 300), QPoint(60, 60));

    QVERIFY(hit.has_value());
    QCOMPARE(hit.value(), QStringLiteral("smallTop"));
}

void AppTests::templateElementHitTesterReturnsArrayGridElementIdFromCellCommand()
{
    QList<NativeDrawCommand> commands;
    commands.append(NativeDrawCommand{
        NativeDrawCommandType::Rectangle,
        2.0,
        3.0,
        20.0,
        10.0,
        QString(),
        0.0,
        false,
        0.0,
        0,
        false,
        QStringLiteral("headerGrid.cell0.border"),
    });

    const auto hit = TemplateElementHitTester().hitTest(commands, QSizeF(80.0, 30.0), QSize(800, 300), QPoint(30, 40));

    QVERIFY(hit.has_value());
    QCOMPARE(hit.value(), QStringLiteral("headerGrid"));
}

void AppTests::templateElementHitTesterReturnsTableElementIdFromRowCommand()
{
    QList<NativeDrawCommand> commands;
    commands.append(NativeDrawCommand{
        NativeDrawCommandType::Rectangle,
        5.0,
        10.0,
        20.0,
        5.0,
        QString(),
        0.0,
        false,
        0.0,
        0,
        false,
        QStringLiteral("saleTable.row0.productName.border"),
    });

    const auto hit = TemplateElementHitTester().hitTest(commands, QSizeF(80.0, 30.0), QSize(800, 300), QPoint(80, 120));

    QVERIFY(hit.has_value());
    QCOMPARE(hit.value(), QStringLiteral("saleTable"));
}

void AppTests::templateDesignerFactoryCreatesDefaultElementsAndDocuments()
{
    const auto fixedText = TemplateDesignerFactory::createElement(TemplateElementType::FixedText);
    QVERIFY(fixedText.id.startsWith(QStringLiteral("fixedText-")));
    QCOMPARE(fixedText.displayName, QString::fromUtf8("固定文本"));
    QCOMPARE(fixedText.text, QString::fromUtf8("固定文本"));

    const auto arrayGrid = TemplateDesignerFactory::createElement(TemplateElementType::ArrayGrid);
    QCOMPARE(arrayGrid.dataPath, QStringLiteral("header_items"));
    QCOMPARE(arrayGrid.arrayGridRows, 2);
    QCOMPARE(arrayGrid.arrayGridColumns, 3);
    QVERIFY(arrayGrid.arrayGridDrawBorders);

    const auto boundField = TemplateDesignerFactory::createElement(TemplateElementType::BoundField);
    QCOMPARE(boundField.fieldKey, QStringLiteral("productName"));

    const auto qrCode = TemplateDesignerFactory::createElement(TemplateElementType::QrCode);
    QCOMPARE(qrCode.fieldKey, QStringLiteral("qrPayload"));

    const auto table = TemplateDesignerFactory::createTable();
    QVERIFY(table.id.startsWith(QStringLiteral("table-")));
    QCOMPARE(table.dataPath, QStringLiteral("items"));
    QCOMPARE(table.columns.size(), 2);
    QCOMPARE(table.columns.first().fieldKey, QStringLiteral("productName"));

    const auto document = TemplateDesignerFactory::createBlankDocument(
        QStringLiteral("template-test"),
        QString::fromUtf8("测试模板"),
        QStringLiteral("default"),
        QString(),
        {});
    QCOMPARE(document.id, QStringLiteral("template-test"));
    QCOMPARE(document.layers.size(), 1);
    QCOMPARE(document.layers.first().id, QStringLiteral("base"));
    QCOMPARE(document.paperSpecId, QStringLiteral("label-80x30"));
    QVERIFY(document.sampleData.contains(QStringLiteral("product_name")));
    QVERIFY(document.sampleData.contains(boundField.fieldKey));
    QVERIFY(document.sampleData.contains(qrCode.fieldKey));
    QVERIFY(document.sampleData.contains(QStringLiteral("weight")));
    QVERIFY(document.sampleData.contains(table.dataPath));
    QCOMPARE(document.sampleData.value(boundField.fieldKey).toString(), QString::fromUtf8("足金串搭项链"));
    QVERIFY(!document.sampleData.value(qrCode.fieldKey).toString().isEmpty());
    const auto defaultItems = document.sampleData.value(table.dataPath).toArray();
    QVERIFY(!defaultItems.isEmpty());
    const auto firstDefaultItem = defaultItems.first().toObject();
    QVERIFY(firstDefaultItem.contains(table.columns.first().fieldKey));
    QVERIFY(firstDefaultItem.contains(QStringLiteral("weight")));
}

void AppTests::templateDesignerWindowAddsLayer()
{
    PrintClientSettings settings;
    PrintClientSettings changedSettings;
    int changedCount = 0;
    TemplateDesignerWindow window(settings, [&changedSettings, &changedCount](const PrintClientSettings& nextSettings) {
        changedSettings = nextSettings;
        ++changedCount;
    });

    auto* addLayerButton = window.findChild<QPushButton*>(QStringLiteral("addLayerButton"));
    auto* layerList = window.findChild<QListWidget*>(QStringLiteral("templateLayerList"));
    QVERIFY(addLayerButton != nullptr);
    QVERIFY(layerList != nullptr);

    const auto initialCount = layerList->count();
    QVERIFY(initialCount >= 1);
    addLayerButton->click();

    QCOMPARE(layerList->count(), initialCount + 1);
    QCOMPARE(changedCount, 1);
    QVERIFY(changedSettings.templateDocuments.contains("default"));
    QCOMPARE(changedSettings.templateDocuments.value("default").layers.size(), initialCount + 1);
}

void AppTests::templateDesignerWindowPreservesLegacyDynamicElements()
{
    PrintClientSettings settings;
    TemplateElement boundField;
    boundField.id = QStringLiteral("legacyBoundField");
    boundField.type = TemplateElementType::BoundField;
    boundField.fieldKey = QStringLiteral("productName");
    boundField.x = 30.0;
    boundField.y = 2.0;
    boundField.width = 10.0;
    boundField.height = 3.0;

    TemplateElement dynamicQrCode;
    dynamicQrCode.id = QStringLiteral("legacyDynamicQr");
    dynamicQrCode.type = TemplateElementType::QrCode;
    dynamicQrCode.fieldKey = QStringLiteral("qrPayload");
    dynamicQrCode.payload = QString();
    dynamicQrCode.x = 42.0;
    dynamicQrCode.y = 2.0;
    dynamicQrCode.width = 6.0;
    dynamicQrCode.height = 6.0;

    settings.templateElements["default"] = {boundField, dynamicQrCode};

    PrintClientSettings changedSettings;
    TemplateDesignerWindow window(settings, [&changedSettings](const PrintClientSettings& nextSettings) {
        changedSettings = nextSettings;
    });

    auto* addLayerButton = window.findChild<QPushButton*>(QStringLiteral("addLayerButton"));
    QVERIFY(addLayerButton != nullptr);
    addLayerButton->click();

    const auto document = changedSettings.templateDocuments.value("default");
    QVERIFY(!document.layers.isEmpty());

    const auto& elements = document.layers.first().elements;
    auto findElement = [&elements](const QString& id) {
        for (const auto& element : elements) {
            if (element.id == id) {
                return element;
            }
        }
        return TemplateElement{};
    };

    const auto migratedBoundField = findElement(QStringLiteral("legacyBoundField"));
    QCOMPARE(migratedBoundField.type, TemplateElementType::BoundField);
    QCOMPARE(migratedBoundField.fieldKey, QString("productName"));

    const auto migratedQrCode = findElement(QStringLiteral("legacyDynamicQr"));
    QCOMPARE(migratedQrCode.type, TemplateElementType::QrCode);
    QCOMPARE(migratedQrCode.fieldKey, QString("qrPayload"));
    QVERIFY(migratedQrCode.payload.isEmpty());
}

void AppTests::templateDesignerWindowAddsFixedTextToCurrentLayer()
{
    PrintClientSettings settings;
    TemplateDesignerWindow window(settings, nullptr);

    auto* addLayerButton = window.findChild<QPushButton*>(QStringLiteral("addLayerButton"));
    auto* addFixedTextButton = window.findChild<QPushButton*>(QStringLiteral("designerAddFixedTextButton"));
    auto* elementList = window.findChild<QListWidget*>(QStringLiteral("templateElementList"));
    QVERIFY(addLayerButton != nullptr);
    QVERIFY(addFixedTextButton != nullptr);
    QVERIFY(elementList != nullptr);

    addLayerButton->click();
    addFixedTextButton->click();

    QCOMPARE(elementList->count(), 1);
    QCOMPARE(elementList->item(0)->data(Qt::UserRole + 1).toString(), QString("fixedText"));
}

void AppTests::templateDesignerWindowEditsFixedTextPlaceholderContent()
{
    PrintClientSettings changedSettings;
    TemplateDesignerWindow window(PrintClientSettings{}, [&changedSettings](const PrintClientSettings& nextSettings) {
        changedSettings = nextSettings;
    });

    auto* addLayerButton = window.findChild<QPushButton*>(QStringLiteral("addLayerButton"));
    auto* addFixedTextButton = window.findChild<QPushButton*>(QStringLiteral("designerAddFixedTextButton"));
    auto* valueEdit = window.findChild<QPlainTextEdit*>(QStringLiteral("elementValueEdit"));
    auto* xSpin = window.findChild<QDoubleSpinBox*>(QStringLiteral("elementXSpin"));
    auto* ySpin = window.findChild<QDoubleSpinBox*>(QStringLiteral("elementYSpin"));
    auto* widthSpin = window.findChild<QDoubleSpinBox*>(QStringLiteral("elementWidthSpin"));
    auto* heightSpin = window.findChild<QDoubleSpinBox*>(QStringLiteral("elementHeightSpin"));
    auto* fontSizeSpin = window.findChild<QDoubleSpinBox*>(QStringLiteral("elementFontSizeSpin"));
    auto* boldCheck = window.findChild<QCheckBox*>(QStringLiteral("elementBoldCheck"));
    auto* verticalTextCheck = window.findChild<QCheckBox*>(QStringLiteral("elementVerticalTextCheck"));
    auto* applyButton = window.findChild<QPushButton*>(QStringLiteral("applyElementPropertiesButton"));
    QVERIFY(addLayerButton != nullptr);
    QVERIFY(addFixedTextButton != nullptr);
    QVERIFY(valueEdit != nullptr);
    QVERIFY(xSpin != nullptr);
    QVERIFY(ySpin != nullptr);
    QVERIFY(widthSpin != nullptr);
    QVERIFY(heightSpin != nullptr);
    QVERIFY(fontSizeSpin != nullptr);
    QVERIFY(boldCheck != nullptr);
    QVERIFY(verticalTextCheck != nullptr);
    QVERIFY(applyButton != nullptr);

    addLayerButton->click();
    addFixedTextButton->click();
    valueEdit->setPlainText(QString::fromUtf8("地址:${address}"));
    xSpin->setValue(12.5);
    ySpin->setValue(3.5);
    widthSpin->setValue(34.0);
    heightSpin->setValue(6.5);
    fontSizeSpin->setValue(8.0);
    boldCheck->setChecked(true);
    verticalTextCheck->setChecked(true);
    applyButton->click();

    const auto document = changedSettings.templateDocuments.value("default");
    QVERIFY(!document.layers.isEmpty());
    const auto element = document.layers.last().elements.first();
    QCOMPARE(element.text, QString::fromUtf8("地址:${address}"));
    QCOMPARE(element.x, 12.5);
    QCOMPARE(element.y, 3.5);
    QCOMPARE(element.width, 34.0);
    QCOMPARE(element.height, 6.5);
    QCOMPARE(element.fontSizePt, 8.0);
    QVERIFY(element.bold);
    QVERIFY(element.verticalText);
}

void AppTests::templateDesignerWindowEditsQrPayloadAndRotation()
{
    PrintClientSettings changedSettings;
    TemplateDesignerWindow window(PrintClientSettings{}, [&changedSettings](const PrintClientSettings& nextSettings) {
        changedSettings = nextSettings;
    });

    auto* addLayerButton = window.findChild<QPushButton*>(QStringLiteral("addLayerButton"));
    auto* addQrCodeButton = window.findChild<QPushButton*>(QStringLiteral("designerAddQrCodeButton"));
    auto* valueEdit = window.findChild<QPlainTextEdit*>(QStringLiteral("elementValueEdit"));
    auto* rotationSpin = window.findChild<QDoubleSpinBox*>(QStringLiteral("elementRotationSpin"));
    auto* applyButton = window.findChild<QPushButton*>(QStringLiteral("applyElementPropertiesButton"));
    QVERIFY(addLayerButton != nullptr);
    QVERIFY(addQrCodeButton != nullptr);
    QVERIFY(valueEdit != nullptr);
    QVERIFY(rotationSpin != nullptr);
    QVERIFY(applyButton != nullptr);

    addLayerButton->click();
    addQrCodeButton->click();

    QVERIFY(valueEdit->isEnabled());
    valueEdit->setPlainText(QString::fromUtf8("地址:${address}"));
    rotationSpin->setValue(90.0);
    applyButton->click();

    const auto document = changedSettings.templateDocuments.value("default");
    QVERIFY(!document.layers.isEmpty());
    const auto element = document.layers.last().elements.first();
    QCOMPARE(element.type, TemplateElementType::QrCode);
    QCOMPARE(element.payload, QString::fromUtf8("地址:${address}"));
    QCOMPARE(element.rotationDegrees, 90.0);
}

void AppTests::templateDesignerWindowAddsTableToCurrentLayer()
{
    PrintClientSettings changedSettings;
    TemplateDesignerWindow window(PrintClientSettings{}, [&changedSettings](const PrintClientSettings& nextSettings) {
        changedSettings = nextSettings;
    });

    auto* addLayerButton = window.findChild<QPushButton*>(QStringLiteral("addLayerButton"));
    auto* addTableButton = window.findChild<QPushButton*>(QStringLiteral("designerAddTableButton"));
    auto* elementList = window.findChild<QListWidget*>(QStringLiteral("templateElementList"));
    auto* dataPathEdit = window.findChild<QLineEdit*>(QStringLiteral("tableDataPathEdit"));
    QVERIFY(addLayerButton != nullptr);
    QVERIFY(addTableButton != nullptr);
    QVERIFY(elementList != nullptr);
    QVERIFY(dataPathEdit != nullptr);

    addLayerButton->click();
    addTableButton->click();

    QCOMPARE(elementList->count(), 1);
    QCOMPARE(elementList->item(0)->data(Qt::UserRole + 1).toString(), QString("table"));
    QCOMPARE(dataPathEdit->text(), QString("items"));

    const auto document = changedSettings.templateDocuments.value("default");
    QVERIFY(!document.layers.isEmpty());
    QCOMPARE(document.layers.last().tables.size(), 1);
    const auto table = document.layers.last().tables.first();
    QVERIFY(table.id.startsWith(QStringLiteral("table-")));
    QCOMPARE(table.dataPath, QString("items"));
    QCOMPARE(table.columns.size(), 2);
    QCOMPARE(table.columns.first().fieldKey, QString("productName"));
}

void AppTests::templateDesignerWindowAddsAndEditsArrayGridElement()
{
    PrintClientSettings changedSettings;
    TemplateDesignerWindow window(PrintClientSettings{}, [&changedSettings](const PrintClientSettings& nextSettings) {
        changedSettings = nextSettings;
    });

    auto* addLayerButton = window.findChild<QPushButton*>(QStringLiteral("addLayerButton"));
    auto* addArrayGridButton = window.findChild<QPushButton*>(QStringLiteral("designerAddArrayGridButton"));
    auto* elementList = window.findChild<QListWidget*>(QStringLiteral("templateElementList"));
    auto* dataPathEdit = window.findChild<QLineEdit*>(QStringLiteral("arrayGridDataPathEdit"));
    auto* rowsSpin = window.findChild<QSpinBox*>(QStringLiteral("arrayGridRowsSpin"));
    auto* columnsSpin = window.findChild<QSpinBox*>(QStringLiteral("arrayGridColumnsSpin"));
    auto* rowHeightSpin = window.findChild<QDoubleSpinBox*>(QStringLiteral("arrayGridRowHeightSpin"));
    auto* cellTemplateEdit = window.findChild<QPlainTextEdit*>(QStringLiteral("arrayGridCellTemplateEdit"));
    auto* drawBordersCheck = window.findChild<QCheckBox*>(QStringLiteral("arrayGridDrawBordersCheck"));
    auto* applyButton = window.findChild<QPushButton*>(QStringLiteral("applyElementPropertiesButton"));
    QVERIFY(addLayerButton != nullptr);
    QVERIFY(addArrayGridButton != nullptr);
    QVERIFY(elementList != nullptr);
    QVERIFY(dataPathEdit != nullptr);
    QVERIFY(rowsSpin != nullptr);
    QVERIFY(columnsSpin != nullptr);
    QVERIFY(rowHeightSpin != nullptr);
    QVERIFY(cellTemplateEdit != nullptr);
    QVERIFY(drawBordersCheck != nullptr);
    QVERIFY(applyButton != nullptr);

    addLayerButton->click();
    addArrayGridButton->click();
    QCOMPARE(elementList->count(), 1);
    QCOMPARE(elementList->item(0)->data(Qt::UserRole + 1).toString(), QString("arrayGrid"));

    dataPathEdit->setText(QStringLiteral("header_items"));
    rowsSpin->setValue(2);
    columnsSpin->setValue(3);
    rowHeightSpin->setValue(3.2);
    cellTemplateEdit->setPlainText(QStringLiteral("${text}:${value}"));
    drawBordersCheck->setChecked(false);
    applyButton->click();

    const auto document = changedSettings.templateDocuments.value("default");
    QVERIFY(!document.layers.isEmpty());
    const auto element = document.layers.last().elements.first();
    QCOMPARE(element.type, TemplateElementType::ArrayGrid);
    QCOMPARE(element.dataPath, QString("header_items"));
    QCOMPARE(element.arrayGridRows, 2);
    QCOMPARE(element.arrayGridColumns, 3);
    QCOMPARE(element.arrayGridRowHeightMm, 3.2);
    QCOMPARE(element.arrayGridCellTemplate, QString("${text}:${value}"));
    QCOMPARE(element.arrayGridDrawBorders, false);
}

void AppTests::templateDesignerWindowUpdatesSelectedTableProperties()
{
    PrintClientSettings changedSettings;
    TemplateDesignerWindow window(PrintClientSettings{}, [&changedSettings](const PrintClientSettings& nextSettings) {
        changedSettings = nextSettings;
    });

    auto* addLayerButton = window.findChild<QPushButton*>(QStringLiteral("addLayerButton"));
    auto* addTableButton = window.findChild<QPushButton*>(QStringLiteral("designerAddTableButton"));
    auto* elementList = window.findChild<QListWidget*>(QStringLiteral("templateElementList"));
    auto* displayNameEdit = window.findChild<QLineEdit*>(QStringLiteral("tableDisplayNameEdit"));
    auto* dataPathEdit = window.findChild<QLineEdit*>(QStringLiteral("tableDataPathEdit"));
    auto* xSpin = window.findChild<QDoubleSpinBox*>(QStringLiteral("tableXSpin"));
    auto* ySpin = window.findChild<QDoubleSpinBox*>(QStringLiteral("tableYSpin"));
    auto* widthSpin = window.findChild<QDoubleSpinBox*>(QStringLiteral("tableWidthSpin"));
    auto* heightSpin = window.findChild<QDoubleSpinBox*>(QStringLiteral("tableHeightSpin"));
    auto* headerHeightSpin = window.findChild<QDoubleSpinBox*>(QStringLiteral("tableHeaderHeightSpin"));
    auto* detailHeightSpin = window.findChild<QDoubleSpinBox*>(QStringLiteral("tableDetailHeightSpin"));
    auto* repeatHeaderCheck = window.findChild<QCheckBox*>(QStringLiteral("tableRepeatHeaderCheck"));
    auto* drawBordersCheck = window.findChild<QCheckBox*>(QStringLiteral("tableDrawBordersCheck"));
    auto* columnsEdit = window.findChild<QLineEdit*>(QStringLiteral("tableColumnsEdit"));
    auto* applyButton = window.findChild<QPushButton*>(QStringLiteral("applyTablePropertiesButton"));
    QVERIFY(addLayerButton != nullptr);
    QVERIFY(addTableButton != nullptr);
    QVERIFY(elementList != nullptr);
    QVERIFY(displayNameEdit != nullptr);
    QVERIFY(dataPathEdit != nullptr);
    QVERIFY(xSpin != nullptr);
    QVERIFY(ySpin != nullptr);
    QVERIFY(widthSpin != nullptr);
    QVERIFY(heightSpin != nullptr);
    QVERIFY(headerHeightSpin != nullptr);
    QVERIFY(detailHeightSpin != nullptr);
    QVERIFY(repeatHeaderCheck != nullptr);
    QVERIFY(drawBordersCheck != nullptr);
    QVERIFY(columnsEdit != nullptr);
    QVERIFY(applyButton != nullptr);

    addLayerButton->click();
    addTableButton->click();
    displayNameEdit->setText(QString::fromUtf8("销售明细"));
    dataPathEdit->setText(QStringLiteral("saleItems"));
    xSpin->setValue(6.0);
    ySpin->setValue(7.0);
    widthSpin->setValue(60.0);
    heightSpin->setValue(18.0);
    headerHeightSpin->setValue(4.0);
    detailHeightSpin->setValue(3.5);
    repeatHeaderCheck->setChecked(false);
    drawBordersCheck->setChecked(false);
    columnsEdit->setText(QString::fromUtf8("品名=productName:35,重量=weight:20"));
    applyButton->click();

    const auto table = changedSettings.templateDocuments.value("default").layers.last().tables.first();
    QCOMPARE(table.displayName, QString::fromUtf8("销售明细"));
    QCOMPARE(table.dataPath, QString("saleItems"));
    QCOMPARE(table.x, 6.0);
    QCOMPARE(table.y, 7.0);
    QCOMPARE(table.width, 60.0);
    QCOMPARE(table.height, 18.0);
    QCOMPARE(table.headerRowHeightMm, 4.0);
    QCOMPARE(table.detailRowHeightMm, 3.5);
    QCOMPARE(table.repeatHeaderOnPage, false);
    QCOMPARE(table.drawBorders, false);
    QCOMPARE(table.columns.size(), 2);
    QCOMPARE(table.columns[0].title, QString::fromUtf8("品名"));
    QCOMPARE(table.columns[0].fieldKey, QString("productName"));
    QCOMPARE(table.columns[0].widthMm, 35.0);
    QCOMPARE(table.columns[1].title, QString::fromUtf8("重量"));
    QCOMPARE(table.columns[1].fieldKey, QString("weight"));
    QCOMPARE(table.columns[1].widthMm, 20.0);
    QVERIFY(elementList->currentItem()->text().contains(QString::fromUtf8("销售明细")));
}

void AppTests::templateDesignerWindowDeletesSelectedEmptyLayer()
{
    PrintClientSettings settings;
    TemplateDesignerWindow window(settings, nullptr);

    auto* addLayerButton = window.findChild<QPushButton*>(QStringLiteral("addLayerButton"));
    auto* deleteLayerButton = window.findChild<QPushButton*>(QStringLiteral("deleteLayerButton"));
    auto* layerList = window.findChild<QListWidget*>(QStringLiteral("templateLayerList"));
    QVERIFY(addLayerButton != nullptr);
    QVERIFY(deleteLayerButton != nullptr);
    QVERIFY(layerList != nullptr);

    const auto initialCount = layerList->count();
    addLayerButton->click();
    deleteLayerButton->click();

    QCOMPARE(layerList->count(), initialCount);
}

void AppTests::templateDesignerWindowDoesNotEditLockedLayer()
{
    PrintClientSettings settings;
    TemplateDesignerWindow window(settings, nullptr);

    auto* addLayerButton = window.findChild<QPushButton*>(QStringLiteral("addLayerButton"));
    auto* lockLayerButton = window.findChild<QPushButton*>(QStringLiteral("lockLayerButton"));
    auto* addFixedTextButton = window.findChild<QPushButton*>(QStringLiteral("designerAddFixedTextButton"));
    auto* elementList = window.findChild<QListWidget*>(QStringLiteral("templateElementList"));
    QVERIFY(addLayerButton != nullptr);
    QVERIFY(lockLayerButton != nullptr);
    QVERIFY(addFixedTextButton != nullptr);
    QVERIFY(elementList != nullptr);

    addLayerButton->click();
    lockLayerButton->click();
    addFixedTextButton->click();

    QCOMPARE(elementList->count(), 0);
}

void AppTests::templateDesignerWindowShowsDesignerPreview()
{
    PrintClientSettings settings;
    TemplateDesignerWindow window(settings, nullptr);

    TemplatePreviewLabel* previewLabel = nullptr;
    for (auto* label : window.findChildren<QLabel*>(QStringLiteral("designerPreviewLabel"))) {
        previewLabel = dynamic_cast<TemplatePreviewLabel*>(label);
        if (previewLabel != nullptr) {
            break;
        }
    }
    QVERIFY(previewLabel != nullptr);
    QVERIFY(!previewLabel->pixmap().isNull());
}

void AppTests::templateDesignerWindowConfiguresRulerPrecision()
{
    TemplateDesignerWindow window(PrintClientSettings{}, nullptr);

    auto* precisionCombo = window.findChild<QComboBox*>(QStringLiteral("designerRulerPrecisionCombo"));
    TemplatePreviewLabel* previewLabel = nullptr;
    for (auto* label : window.findChildren<QLabel*>(QStringLiteral("designerPreviewLabel"))) {
        previewLabel = dynamic_cast<TemplatePreviewLabel*>(label);
        if (previewLabel != nullptr) {
            break;
        }
    }
    QVERIFY(precisionCombo != nullptr);
    QVERIFY(previewLabel != nullptr);
    QVERIFY(previewLabel->isRulerVisible());
    QCOMPARE(previewLabel->rulerPrecisionMm(), 1.0);
    QVERIFY(!previewLabel->pixmap().isNull());
    QVERIFY(previewLabel->printableImageOriginPx().x() > 0);
    QVERIFY(previewLabel->printableImageOriginPx().y() > 0);
    QCOMPARE(previewLabel->printableImageSizePx(), previewLabel->pixmap().size());
    QCOMPARE(
        previewLabel->size(),
        QSize(
            previewLabel->pixmap().width() + previewLabel->printableImageOriginPx().x(),
            previewLabel->pixmap().height() + previewLabel->printableImageOriginPx().y()));

    const auto halfMillimeterIndex = precisionCombo->findData(0.5);
    QVERIFY(halfMillimeterIndex >= 0);
    precisionCombo->setCurrentIndex(halfMillimeterIndex);

    QCOMPARE(previewLabel->rulerPrecisionMm(), 0.5);
}

void AppTests::templateDesignerWindowConfiguresDesignAids()
{
    TemplateDesignerWindow window(PrintClientSettings{}, nullptr);

    auto* designAidCheck = window.findChild<QCheckBox*>(QStringLiteral("designerDesignAidCheck"));
    auto* addFixedTextButton = window.findChild<QPushButton*>(QStringLiteral("designerAddFixedTextButton"));
    TemplatePreviewLabel* previewLabel = nullptr;
    for (auto* label : window.findChildren<QLabel*>(QStringLiteral("designerPreviewLabel"))) {
        previewLabel = dynamic_cast<TemplatePreviewLabel*>(label);
        if (previewLabel != nullptr) {
            break;
        }
    }

    QVERIFY(designAidCheck != nullptr);
    QVERIFY(addFixedTextButton != nullptr);
    QVERIFY(previewLabel != nullptr);
    QVERIFY(designAidCheck->isChecked());
    QVERIFY(previewLabel->isDesignAidVisible());

    addFixedTextButton->click();
    QVERIFY(previewLabel->designAidCommandCount() > 0);

    designAidCheck->setChecked(false);
    QVERIFY(!previewLabel->isDesignAidVisible());
}

void AppTests::templateDesignerWindowPrePrintsCurrentTemplate()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    PaperSpec labelSpec;
    labelSpec.id = QStringLiteral("label-80x30");
    labelSpec.name = QString::fromUtf8("80x30 标签");
    labelSpec.widthMm = 80.0;
    labelSpec.heightMm = 30.0;
    labelSpec.marginLeftMm = 2.0;
    labelSpec.marginTopMm = 1.0;
    labelSpec.defaultDpi = 203.0;
    QVERIFY(PaperSpecStore(dir.filePath(QStringLiteral("paper-specs.json"))).savePaperSpec(labelSpec));

    TemplateElement productNameText;
    productNameText.id = QStringLiteral("sample-product-name");
    productNameText.layerId = QStringLiteral("base");
    productNameText.type = TemplateElementType::FixedText;
    productNameText.text = QStringLiteral("${product_name}");
    productNameText.x = 5.0;
    productNameText.y = 5.0;
    productNameText.width = 60.0;
    productNameText.height = 8.0;
    productNameText.fontSizePt = 12.0;

    TemplateLayer baseLayer;
    baseLayer.id = QStringLiteral("base");
    baseLayer.name = QString::fromUtf8("基础图层");
    baseLayer.elements.append(productNameText);

    TemplateDocument document;
    document.id = QStringLiteral("preprint-template");
    document.name = QString::fromUtf8("预打印模板");
    document.templateKey = QStringLiteral("default");
    document.paperSpecId = QStringLiteral("label-80x30");
    document.layers = {baseLayer};
    DeviceProfile genericProfile;
    genericProfile.id = QStringLiteral("profile-default");
    genericProfile.dpi = 300.0;
    document.deviceProfiles = {genericProfile};

    PrintClientSettings settings;
    settings.defaultPrinter = QStringLiteral("Test Printer");
    settings.templateDocuments.insert(QStringLiteral("default"), document);

    int printCount = 0;
    NativePrintDrawingPlan capturedPlan;
    QString capturedPrinterName;
    TemplateDesignerWindow window(
        settings,
        nullptr,
        dir.filePath(QStringLiteral("templates")),
        [&printCount, &capturedPlan, &capturedPrinterName](
            const NativePrintDrawingPlan& plan,
            const QString& printerName) {
            ++printCount;
            capturedPlan = plan;
            capturedPrinterName = printerName;
            return true;
        });

    auto* sampleDataEdit = window.findChild<QPlainTextEdit*>(QStringLiteral("templateSampleDataEdit"));
    auto* prePrintButton = window.findChild<QPushButton*>(QStringLiteral("prePrintCurrentTemplateButton"));
    auto* statusLabel = window.findChild<QLabel*>(QStringLiteral("designerStatusLabel"));
    QVERIFY(sampleDataEdit != nullptr);
    QVERIFY(prePrintButton != nullptr);
    QVERIFY(statusLabel != nullptr);

    sampleDataEdit->setPlainText(QString::fromUtf8("{\"product_name\":\"足金串搭项链\"}"));
    prePrintButton->click();

    QCOMPARE(printCount, 1);
    QCOMPARE(capturedPrinterName, QString("Test Printer"));
    QCOMPARE(capturedPlan.pages.size(), 1);
    QCOMPARE(capturedPlan.paperSize.widthMm(), 80.0);
    QCOMPARE(capturedPlan.paperSize.heightMm(), 30.0);
    QCOMPARE(capturedPlan.renderDpi, 203.0);
    const auto command = std::find_if(
        capturedPlan.pages.first().commands.cbegin(),
        capturedPlan.pages.first().commands.cend(),
        [](const NativeDrawCommand& command) {
            return command.text == QString::fromUtf8("足金串搭项链");
        });
    QVERIFY(command != capturedPlan.pages.first().commands.cend());
    QCOMPARE(command->x, 7.0);
    QCOMPARE(command->y, 6.0);
    QVERIFY(statusLabel->text().contains(QString::fromUtf8("预打印")));
}

void AppTests::templateDesignerWindowRendersPlaceholdersWithSampleData()
{
    TemplateElement productNameText;
    productNameText.id = QStringLiteral("sample-product-name");
    productNameText.layerId = QStringLiteral("base");
    productNameText.type = TemplateElementType::FixedText;
    productNameText.text = QStringLiteral("${product_name}");
    productNameText.x = 5.0;
    productNameText.y = 5.0;
    productNameText.width = 60.0;
    productNameText.height = 8.0;
    productNameText.fontSizePt = 12.0;

    TemplateLayer baseLayer;
    baseLayer.id = QStringLiteral("base");
    baseLayer.name = QString::fromUtf8("基础图层");
    baseLayer.elements.append(productNameText);

    TemplateDocument document;
    document.id = QStringLiteral("sample-data-template");
    document.name = QString::fromUtf8("模拟数据模板");
    document.templateKey = QStringLiteral("default");
    document.paperSpecId = QStringLiteral("label-80x30");
    document.layers = {baseLayer};

    PrintClientSettings settings;
    settings.templateDocuments.insert(QStringLiteral("default"), document);
    TemplateDesignerWindow window(settings, nullptr);

    auto* sampleDataEdit = window.findChild<QPlainTextEdit*>(QStringLiteral("templateSampleDataEdit"));
    auto* statusLabel = window.findChild<QLabel*>(QStringLiteral("designerStatusLabel"));
    TemplatePreviewLabel* previewLabel = nullptr;
    for (auto* label : window.findChildren<QLabel*>(QStringLiteral("designerPreviewLabel"))) {
        previewLabel = dynamic_cast<TemplatePreviewLabel*>(label);
        if (previewLabel != nullptr) {
            break;
        }
    }
    QVERIFY(sampleDataEdit != nullptr);
    QVERIFY(statusLabel != nullptr);
    QVERIFY(previewLabel != nullptr);
    QVERIFY(sampleDataEdit->toPlainText().contains(QStringLiteral("product_name")));

    sampleDataEdit->setPlainText(QString::fromUtf8("{\"product_name\":\"足金串搭项链\"}"));
    QCoreApplication::processEvents();
    const auto goldPreview = imageBytes(previewLabel->pixmap());

    sampleDataEdit->setPlainText(QString::fromUtf8("{\"product_name\":\"银手镯\"}"));
    QCoreApplication::processEvents();
    const auto silverPreview = imageBytes(previewLabel->pixmap());

    QVERIFY(!goldPreview.isEmpty());
    QVERIFY(goldPreview != silverPreview);

    sampleDataEdit->setPlainText(QStringLiteral("{"));
    QCoreApplication::processEvents();
    QVERIFY(statusLabel->text().contains(QString::fromUtf8("模拟数据 JSON 解析失败")));
}

void AppTests::templateDesignerWindowSavesSampleDataWithDedicatedButton()
{
    TemplateDocument document;
    document.id = QStringLiteral("sample-data-template");
    document.name = QString::fromUtf8("模拟数据模板");
    document.templateKey = QStringLiteral("default");
    document.paperSpecId = QStringLiteral("label-80x30");

    PrintClientSettings settings;
    settings.templateDocuments.insert(QStringLiteral("default"), document);

    PrintClientSettings changedSettings;
    int changedCount = 0;
    TemplateDesignerWindow window(settings, [&changedSettings, &changedCount](const PrintClientSettings& nextSettings) {
        changedSettings = nextSettings;
        ++changedCount;
    });

    auto* sampleDataEdit = window.findChild<QPlainTextEdit*>(QStringLiteral("templateSampleDataEdit"));
    auto* saveSampleDataButton = window.findChild<QPushButton*>(QStringLiteral("saveTemplateSampleDataButton"));
    auto* statusLabel = window.findChild<QLabel*>(QStringLiteral("designerStatusLabel"));
    QVERIFY(sampleDataEdit != nullptr);
    QVERIFY(saveSampleDataButton != nullptr);
    QVERIFY(statusLabel != nullptr);

    sampleDataEdit->setPlainText(QString::fromUtf8("{\"product_name\":\"足金串搭项链\"}"));
    QCoreApplication::processEvents();
    saveSampleDataButton->click();

    QVERIFY(changedCount > 0);
    QCOMPARE(
        changedSettings.templateDocuments.value(QStringLiteral("default")).sampleData.value(QStringLiteral("product_name")).toString(),
        QString::fromUtf8("足金串搭项链"));
    QVERIFY(statusLabel->text().contains(QString::fromUtf8("已保存模拟数据")));

    sampleDataEdit->setPlainText(QStringLiteral("{"));
    QCoreApplication::processEvents();
    saveSampleDataButton->click();

    QVERIFY(statusLabel->text().contains(QString::fromUtf8("模拟数据 JSON 解析失败")));
}

void AppTests::templateDesignerWindowSavesSampleDataWithTemplate()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    TemplateLayer baseLayer;
    baseLayer.id = QStringLiteral("base");
    baseLayer.name = QString::fromUtf8("基础图层");

    TemplateDocument document;
    document.id = QStringLiteral("sample-data-template");
    document.name = QString::fromUtf8("模拟数据模板");
    document.templateKey = QStringLiteral("default");
    document.paperSpecId = QStringLiteral("label-80x30");
    document.layers = {baseLayer};
    document.sampleData.insert(QStringLiteral("product_name"), QString::fromUtf8("旧商品"));

    PrintClientSettings settings;
    settings.templateDocuments.insert(QStringLiteral("default"), document);
    TemplateDesignerWindow window(settings, nullptr, dir.path());

    auto* sampleDataEdit = window.findChild<QPlainTextEdit*>(QStringLiteral("templateSampleDataEdit"));
    auto* saveLibraryButton = window.findChild<QPushButton*>(QStringLiteral("saveTemplateToLibraryButton"));
    QVERIFY(sampleDataEdit != nullptr);
    QVERIFY(saveLibraryButton != nullptr);
    QVERIFY(sampleDataEdit->toPlainText().contains(QString::fromUtf8("旧商品")));

    sampleDataEdit->setPlainText(QString::fromUtf8(R"json({
        "product_name": "足金串搭项链",
        "header_items": [
            { "text": "素金KA", "value": "2.99" },
            { "text": "素金PC", "value": "4.13" }
        ]
    })json"));
    QCoreApplication::processEvents();
    saveLibraryButton->click();

    QString errorMessage;
    const auto savedDocument = TemplateLibraryStore(dir.path()).loadTemplate(QStringLiteral("sample-data-template"), &errorMessage);
    QVERIFY2(savedDocument.has_value(), qPrintable(errorMessage));
    QCOMPARE(savedDocument->sampleData["product_name"].toString(), QString::fromUtf8("足金串搭项链"));
    QCOMPARE(savedDocument->sampleData["header_items"].toArray().size(), 2);
}

void AppTests::templateDesignerWindowSavesAndRestoresVersion()
{
    PrintClientSettings settings;
    TemplateDesignerWindow window(settings, nullptr);

    auto* addLayerButton = window.findChild<QPushButton*>(QStringLiteral("addLayerButton"));
    auto* saveVersionButton = window.findChild<QPushButton*>(QStringLiteral("saveTemplateVersionButton"));
    auto* restoreVersionButton = window.findChild<QPushButton*>(QStringLiteral("restoreTemplateVersionButton"));
    auto* layerList = window.findChild<QListWidget*>(QStringLiteral("templateLayerList"));
    QVERIFY(addLayerButton != nullptr);
    QVERIFY(saveVersionButton != nullptr);
    QVERIFY(restoreVersionButton != nullptr);
    QVERIFY(layerList != nullptr);

    const auto initialCount = layerList->count();
    QVERIFY(initialCount >= 1);
    addLayerButton->click();
    saveVersionButton->click();
    addLayerButton->click();
    restoreVersionButton->click();

    QCOMPARE(layerList->count(), initialCount + 1);
}

void AppTests::templateDesignerWindowExportsAndImportsTemplateFile()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    PrintClientSettings importedSettings;
    TemplateDesignerWindow source(PrintClientSettings{}, nullptr);
    TemplateDesignerWindow target(PrintClientSettings{}, [&importedSettings](const PrintClientSettings& nextSettings) {
        importedSettings = nextSettings;
    });

    auto* addLayerButton = source.findChild<QPushButton*>(QStringLiteral("addLayerButton"));
    auto* targetLayerList = target.findChild<QListWidget*>(QStringLiteral("templateLayerList"));
    QVERIFY(addLayerButton != nullptr);
    QVERIFY(targetLayerList != nullptr);

    addLayerButton->click();
    const auto exportPath = dir.filePath(QStringLiteral("default.sleekpr-template.json"));

    QVERIFY(source.exportTemplateToFile(exportPath));
    QVERIFY(QFile::exists(exportPath));
    QVERIFY(target.importTemplateFromFile(exportPath));

    QCOMPARE(targetLayerList->count(), 2);
    QCOMPARE(importedSettings.templateDocuments.value("default").layers.size(), 2);
}

void AppTests::templateDesignerWindowSavesAndLoadsTemplateLibraryDocument()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const auto templateDirectoryPath = dir.filePath(QStringLiteral("templates"));
    TemplateDesignerWindow source(PrintClientSettings{}, nullptr, templateDirectoryPath);

    auto* addLayerButton = source.findChild<QPushButton*>(QStringLiteral("addLayerButton"));
    auto* saveLibraryButton = source.findChild<QPushButton*>(QStringLiteral("saveTemplateToLibraryButton"));
    QVERIFY(addLayerButton != nullptr);
    QVERIFY(saveLibraryButton != nullptr);

    addLayerButton->click();
    saveLibraryButton->click();

    sleekpr::core::TemplateLibraryStore store(templateDirectoryPath);
    const auto savedTemplate = store.loadTemplate(QStringLiteral("template-default"));
    QVERIFY(savedTemplate.has_value());
    QCOMPARE(savedTemplate->layers.size(), 2);

    PrintClientSettings importedSettings;
    TemplateDesignerWindow target(PrintClientSettings{}, [&importedSettings](const PrintClientSettings& nextSettings) {
        importedSettings = nextSettings;
    }, templateDirectoryPath);

    auto* loadLibraryButton = target.findChild<QPushButton*>(QStringLiteral("loadTemplateFromLibraryButton"));
    auto* targetLayerList = target.findChild<QListWidget*>(QStringLiteral("templateLayerList"));
    QVERIFY(loadLibraryButton != nullptr);
    QVERIFY(targetLayerList != nullptr);

    loadLibraryButton->click();

    QCOMPARE(targetLayerList->count(), 2);
    QCOMPARE(importedSettings.templateDocuments.value("default").layers.size(), 2);
}

void AppTests::templateDesignerWindowListsAndLoadsSelectedLibraryTemplate()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const auto templateDirectoryPath = dir.filePath(QStringLiteral("templates"));
    TemplateLibraryStore store(templateDirectoryPath);
    QVERIFY(store.saveTemplate(createLibraryTestDocument(
        QStringLiteral("template-invoice"),
        QString::fromUtf8("发货单模板"),
        QStringLiteral("invoice"),
        QString::fromUtf8("发货单图层"),
        QString::fromUtf8("发货单"))));
    QVERIFY(store.saveTemplate(createLibraryTestDocument(
        QStringLiteral("template-table"),
        QString::fromUtf8("复杂表格模板"),
        QStringLiteral("table"),
        QString::fromUtf8("明细表图层"),
        QString::fromUtf8("复杂表格"))));

    PrintClientSettings importedSettings;
    TemplateDesignerWindow window(PrintClientSettings{}, [&importedSettings](const PrintClientSettings& nextSettings) {
        importedSettings = nextSettings;
    }, templateDirectoryPath);

    auto* libraryList = window.findChild<QListWidget*>(QStringLiteral("templateLibraryList"));
    auto* loadSelectedButton = window.findChild<QPushButton*>(QStringLiteral("loadSelectedTemplateFromLibraryButton"));
    auto* targetLayerList = window.findChild<QListWidget*>(QStringLiteral("templateLayerList"));
    QVERIFY(libraryList != nullptr);
    QVERIFY(loadSelectedButton != nullptr);
    QVERIFY(targetLayerList != nullptr);
    QCOMPARE(libraryList->count(), 2);
    QCOMPARE(libraryList->item(0)->data(Qt::UserRole).toString(), QString("template-invoice"));
    QCOMPARE(libraryList->item(1)->data(Qt::UserRole).toString(), QString("template-table"));

    libraryList->setCurrentRow(1);
    loadSelectedButton->click();

    QCOMPARE(targetLayerList->count(), 1);
    QCOMPARE(targetLayerList->item(0)->text(), QString::fromUtf8("明细表图层"));
    const auto importedDocument = importedSettings.templateDocuments.value("default");
    QCOMPARE(importedDocument.id, QString("template-table"));
    QCOMPARE(importedDocument.templateKey, QString("default"));
    QVERIFY(!importedDocument.layers.isEmpty());
    QVERIFY(!importedDocument.layers.first().elements.isEmpty());
    QCOMPARE(importedDocument.layers.first().elements.first().text, QString::fromUtf8("复杂表格"));
}

void AppTests::templateDesignerWindowSelectsLibraryTemplateWhenCurrentRowChanges()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const auto templateDirectoryPath = dir.filePath(QStringLiteral("templates"));
    TemplateLibraryStore store(templateDirectoryPath);
    QVERIFY(store.saveTemplate(createLibraryTestDocument(
        QStringLiteral("template-invoice"),
        QString::fromUtf8("发货单模板"),
        QStringLiteral("invoice"),
        QString::fromUtf8("发货单图层"),
        QString::fromUtf8("发货单"))));
    QVERIFY(store.saveTemplate(createLibraryTestDocument(
        QStringLiteral("template-table"),
        QString::fromUtf8("复杂表格模板"),
        QStringLiteral("table"),
        QString::fromUtf8("明细表图层"),
        QString::fromUtf8("复杂表格"))));

    PrintClientSettings importedSettings;
    TemplateDesignerWindow window(PrintClientSettings{}, [&importedSettings](const PrintClientSettings& nextSettings) {
        importedSettings = nextSettings;
    }, templateDirectoryPath);

    auto* libraryList = window.findChild<QListWidget*>(QStringLiteral("templateLibraryList"));
    auto* targetLayerList = window.findChild<QListWidget*>(QStringLiteral("templateLayerList"));
    QVERIFY(libraryList != nullptr);
    QVERIFY(targetLayerList != nullptr);
    QCOMPARE(libraryList->count(), 2);

    libraryList->setCurrentRow(1);

    QCOMPARE(targetLayerList->count(), 1);
    QCOMPARE(targetLayerList->item(0)->text(), QString::fromUtf8("明细表图层"));
    const auto importedDocument = importedSettings.templateDocuments.value("default");
    QCOMPARE(importedDocument.id, QString("template-table"));
    QVERIFY(!importedDocument.layers.isEmpty());
    QVERIFY(!importedDocument.layers.first().elements.isEmpty());
    QCOMPARE(importedDocument.layers.first().elements.first().text, QString::fromUtf8("复杂表格"));
}

void AppTests::templateDesignerWindowCreatesBlankLibraryTemplate()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const auto templateDirectoryPath = dir.filePath(QStringLiteral("templates"));
    PrintClientSettings changedSettings;
    TemplateDesignerWindow window(PrintClientSettings{}, [&changedSettings](const PrintClientSettings& nextSettings) {
        changedSettings = nextSettings;
    }, templateDirectoryPath);

    auto* addLayerButton = window.findChild<QPushButton*>(QStringLiteral("addLayerButton"));
    auto* nameEdit = window.findChild<QLineEdit*>(QStringLiteral("templateLibraryNameEdit"));
    auto* createButton = window.findChild<QPushButton*>(QStringLiteral("createTemplateInLibraryButton"));
    auto* libraryList = window.findChild<QListWidget*>(QStringLiteral("templateLibraryList"));
    auto* layerList = window.findChild<QListWidget*>(QStringLiteral("templateLayerList"));
    auto* elementList = window.findChild<QListWidget*>(QStringLiteral("templateElementList"));
    QVERIFY(addLayerButton != nullptr);
    QVERIFY(nameEdit != nullptr);
    QVERIFY(createButton != nullptr);
    QVERIFY(libraryList != nullptr);
    QVERIFY(layerList != nullptr);
    QVERIFY(elementList != nullptr);

    addLayerButton->click();
    nameEdit->setText(QString::fromUtf8("新标签模板"));
    createButton->click();

    QCOMPARE(libraryList->count(), 1);
    const auto createdTemplateId = libraryList->item(0)->data(Qt::UserRole).toString();
    QVERIFY(createdTemplateId.startsWith(QStringLiteral("template-")));
    QCOMPARE(changedSettings.templateDocuments.value("default").id, createdTemplateId);

    TemplateLibraryStore store(templateDirectoryPath);
    const auto createdDocument = store.loadTemplate(createdTemplateId);
    QVERIFY(createdDocument.has_value());
    QCOMPARE(createdDocument->name, QString::fromUtf8("新标签模板"));
    QCOMPARE(createdDocument->templateKey, QString("default"));
    QCOMPARE(createdDocument->layers.size(), 1);
    QVERIFY(createdDocument->layers.first().elements.isEmpty());
    QVERIFY(createdDocument->layers.first().tables.isEmpty());
    QCOMPARE(layerList->count(), 1);
    QCOMPARE(elementList->count(), 0);
    QCOMPARE(changedSettings.templateDocuments.value("default").layers.size(), 1);
    QVERIFY(changedSettings.templateDocuments.value("default").layers.first().elements.isEmpty());
}

void AppTests::templateDesignerPreviewKeepsRenderedAspectRatio()
{
    TemplateDesignerWindow window(PrintClientSettings{}, nullptr);

    TemplatePreviewLabel* previewLabel = nullptr;
    for (auto* labelWidget : window.findChildren<QLabel*>(QStringLiteral("designerPreviewLabel"))) {
        previewLabel = dynamic_cast<TemplatePreviewLabel*>(labelWidget);
        if (previewLabel != nullptr) {
            break;
        }
    }

    QVERIFY(previewLabel != nullptr);
    QVERIFY(!previewLabel->hasScaledContents());
    QVERIFY(!previewLabel->pixmap().isNull());
    const auto origin = previewLabel->printableImageOriginPx();
    QCOMPARE(
        previewLabel->size(),
        QSize(previewLabel->pixmap().width() + origin.x(), previewLabel->pixmap().height() + origin.y()));
    QVERIFY(previewLabel->pixmap().width() > previewLabel->pixmap().height());
}

void AppTests::templateDesignerPreviewUsesInteractiveDpi()
{
    TemplateDesignerWindow window(PrintClientSettings{}, nullptr);

    TemplatePreviewLabel* previewLabel = nullptr;
    for (auto* label : window.findChildren<QLabel*>(QStringLiteral("designerPreviewLabel"))) {
        previewLabel = dynamic_cast<TemplatePreviewLabel*>(label);
        if (previewLabel != nullptr) {
            break;
        }
    }
    QVERIFY(previewLabel != nullptr);
    QVERIFY(!previewLabel->pixmap().isNull());

    // 80x30 标签的设计器预览应使用交互 DPI，避免每次编辑都生成 300DPI 大图。
    QVERIFY(previewLabel->pixmap().width() <= 640);
    QVERIFY(previewLabel->pixmap().height() <= 260);
}

void AppTests::templateDesignerWindowDeletesSelectedTemplateLibraryDocument()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const auto templateDirectoryPath = dir.filePath(QStringLiteral("templates"));
    TemplateLibraryStore store(templateDirectoryPath);
    QVERIFY(store.saveTemplate(createLibraryTestDocument(
        QStringLiteral("template-invoice"),
        QString::fromUtf8("发货单模板"),
        QStringLiteral("invoice"),
        QString::fromUtf8("发货单图层"),
        QString::fromUtf8("发货单"))));
    QVERIFY(store.saveTemplate(createLibraryTestDocument(
        QStringLiteral("template-table"),
        QString::fromUtf8("复杂表格模板"),
        QStringLiteral("table"),
        QString::fromUtf8("明细表图层"),
        QString::fromUtf8("复杂表格"))));

    TemplateDesignerWindow window(PrintClientSettings{}, nullptr, templateDirectoryPath);

    auto* libraryList = window.findChild<QListWidget*>(QStringLiteral("templateLibraryList"));
    auto* deleteButton = window.findChild<QPushButton*>(QStringLiteral("deleteSelectedTemplateFromLibraryButton"));
    QVERIFY(libraryList != nullptr);
    QVERIFY(deleteButton != nullptr);
    QCOMPARE(libraryList->count(), 2);

    libraryList->setCurrentRow(0);
    deleteButton->click();

    QCOMPARE(libraryList->count(), 1);
    QCOMPARE(libraryList->item(0)->data(Qt::UserRole).toString(), QString("template-table"));
    QVERIFY(!store.loadTemplate(QStringLiteral("template-invoice")).has_value());
    QVERIFY(store.loadTemplate(QStringLiteral("template-table")).has_value());
}

void AppTests::templateDesignerWindowSearchesCopiesAndRenamesLibraryTemplates()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const auto templateDirectoryPath = dir.filePath(QStringLiteral("templates"));
    TemplateLibraryStore store(templateDirectoryPath);
    QVERIFY(store.saveTemplate(createLibraryTestDocument(
        QStringLiteral("template-invoice"),
        QString::fromUtf8("发货单模板"),
        QStringLiteral("invoice"),
        QString::fromUtf8("发货单图层"),
        QString::fromUtf8("发货单"))));
    QVERIFY(store.saveTemplate(createLibraryTestDocument(
        QStringLiteral("template-table"),
        QString::fromUtf8("复杂表格模板"),
        QStringLiteral("table"),
        QString::fromUtf8("明细表图层"),
        QString::fromUtf8("复杂表格"))));

    TemplateDesignerWindow window(PrintClientSettings{}, nullptr, templateDirectoryPath);

    auto* searchEdit = window.findChild<QLineEdit*>(QStringLiteral("templateLibrarySearchEdit"));
    auto* nameEdit = window.findChild<QLineEdit*>(QStringLiteral("templateLibraryNameEdit"));
    auto* libraryList = window.findChild<QListWidget*>(QStringLiteral("templateLibraryList"));
    auto* duplicateButton = window.findChild<QPushButton*>(QStringLiteral("duplicateSelectedTemplateFromLibraryButton"));
    auto* renameButton = window.findChild<QPushButton*>(QStringLiteral("renameSelectedTemplateFromLibraryButton"));
    QVERIFY(searchEdit != nullptr);
    QVERIFY(nameEdit != nullptr);
    QVERIFY(libraryList != nullptr);
    QVERIFY(duplicateButton != nullptr);
    QVERIFY(renameButton != nullptr);

    QCOMPARE(libraryList->count(), 2);
    searchEdit->setText(QStringLiteral("invoice"));
    QCOMPARE(libraryList->count(), 1);
    QCOMPARE(libraryList->item(0)->data(Qt::UserRole).toString(), QString("template-invoice"));

    searchEdit->clear();
    libraryList->setCurrentRow(0);
    duplicateButton->click();

    const auto templateIdsAfterCopy = store.templateIds();
    QCOMPARE(templateIdsAfterCopy.size(), 3);
    const auto copiedIt = std::find_if(templateIdsAfterCopy.cbegin(), templateIdsAfterCopy.cend(), [](const QString& id) {
        return id != QStringLiteral("template-invoice") && id != QStringLiteral("template-table");
    });
    QVERIFY(copiedIt != templateIdsAfterCopy.cend());
    const auto copiedDocument = store.loadTemplate(*copiedIt);
    QVERIFY(copiedDocument.has_value());
    QCOMPARE(copiedDocument->name, QString::fromUtf8("发货单模板 副本"));
    QCOMPARE(copiedDocument->layers.first().name, QString::fromUtf8("发货单图层"));

    nameEdit->setText(QString::fromUtf8("门店常用发货单"));
    renameButton->click();

    const auto renamedDocument = store.loadTemplate(*copiedIt);
    QVERIFY(renamedDocument.has_value());
    QCOMPARE(renamedDocument->name, QString::fromUtf8("门店常用发货单"));
    QVERIFY(libraryList->currentItem() != nullptr);
    QCOMPARE(libraryList->currentItem()->data(Qt::UserRole).toString(), *copiedIt);
}

void AppTests::templateDesignerWindowSavesAndReplacesDeviceProfile()
{
    PrintClientSettings changedSettings;
    TemplateDesignerWindow window(PrintClientSettings{}, [&changedSettings](const PrintClientSettings& nextSettings) {
        changedSettings = nextSettings;
    });

    auto* printerEdit = window.findChild<QLineEdit*>(QStringLiteral("deviceProfilePrinterEdit"));
    auto* dpiSpin = window.findChild<QDoubleSpinBox*>(QStringLiteral("deviceProfileDpiSpin"));
    auto* scaleXSpin = window.findChild<QDoubleSpinBox*>(QStringLiteral("deviceProfileScaleXSpin"));
    auto* scaleYSpin = window.findChild<QDoubleSpinBox*>(QStringLiteral("deviceProfileScaleYSpin"));
    auto* offsetXSpin = window.findChild<QDoubleSpinBox*>(QStringLiteral("deviceProfileOffsetXSpin"));
    auto* offsetYSpin = window.findChild<QDoubleSpinBox*>(QStringLiteral("deviceProfileOffsetYSpin"));
    auto* saveProfileButton = window.findChild<QPushButton*>(QStringLiteral("saveDeviceProfileButton"));
    QVERIFY(printerEdit != nullptr);
    QVERIFY(dpiSpin != nullptr);
    QVERIFY(scaleXSpin != nullptr);
    QVERIFY(scaleYSpin != nullptr);
    QVERIFY(offsetXSpin != nullptr);
    QVERIFY(offsetYSpin != nullptr);
    QVERIFY(saveProfileButton != nullptr);

    printerEdit->setText(QStringLiteral("Zebra ZD888"));
    dpiSpin->setValue(203.0);
    scaleXSpin->setValue(1.02);
    scaleYSpin->setValue(0.98);
    offsetXSpin->setValue(0.30);
    offsetYSpin->setValue(-0.20);
    saveProfileButton->click();

    auto profiles = changedSettings.templateDocuments.value("default").deviceProfiles;
    QCOMPARE(profiles.size(), 1);
    QCOMPARE(profiles.first().printerName, QString("Zebra ZD888"));
    QCOMPARE(profiles.first().dpi, 203.0);
    QCOMPARE(profiles.first().scaleX, 1.02);
    QCOMPARE(profiles.first().scaleY, 0.98);
    QCOMPARE(profiles.first().offsetX, 0.30);
    QCOMPARE(profiles.first().offsetY, -0.20);

    scaleXSpin->setValue(1.05);
    saveProfileButton->click();

    profiles = changedSettings.templateDocuments.value("default").deviceProfiles;
    QCOMPARE(profiles.size(), 1);
    QCOMPARE(profiles.first().scaleX, 1.05);
}

void AppTests::templateDesignerWindowCalculatesDeviceScaleFromMeasuredSize()
{
    PrintClientSettings changedSettings;
    TemplateDesignerWindow window(PrintClientSettings{}, [&changedSettings](const PrintClientSettings& nextSettings) {
        changedSettings = nextSettings;
    });

    auto* printerEdit = window.findChild<QLineEdit*>(QStringLiteral("deviceProfilePrinterEdit"));
    auto* scaleXSpin = window.findChild<QDoubleSpinBox*>(QStringLiteral("deviceProfileScaleXSpin"));
    auto* scaleYSpin = window.findChild<QDoubleSpinBox*>(QStringLiteral("deviceProfileScaleYSpin"));
    auto* expectedWidthSpin = window.findChild<QDoubleSpinBox*>(QStringLiteral("deviceCalibrationExpectedWidthSpin"));
    auto* actualWidthSpin = window.findChild<QDoubleSpinBox*>(QStringLiteral("deviceCalibrationActualWidthSpin"));
    auto* expectedHeightSpin = window.findChild<QDoubleSpinBox*>(QStringLiteral("deviceCalibrationExpectedHeightSpin"));
    auto* actualHeightSpin = window.findChild<QDoubleSpinBox*>(QStringLiteral("deviceCalibrationActualHeightSpin"));
    auto* calculateButton = window.findChild<QPushButton*>(QStringLiteral("calculateDeviceCalibrationScaleButton"));
    auto* saveProfileButton = window.findChild<QPushButton*>(QStringLiteral("saveDeviceProfileButton"));
    QVERIFY(printerEdit != nullptr);
    QVERIFY(scaleXSpin != nullptr);
    QVERIFY(scaleYSpin != nullptr);
    QVERIFY(expectedWidthSpin != nullptr);
    QVERIFY(actualWidthSpin != nullptr);
    QVERIFY(expectedHeightSpin != nullptr);
    QVERIFY(actualHeightSpin != nullptr);
    QVERIFY(calculateButton != nullptr);
    QVERIFY(saveProfileButton != nullptr);

    printerEdit->setText(QStringLiteral("TSC TE244"));
    expectedWidthSpin->setValue(80.0);
    actualWidthSpin->setValue(82.0);
    expectedHeightSpin->setValue(30.0);
    actualHeightSpin->setValue(29.4);
    calculateButton->click();

    QVERIFY(std::abs(scaleXSpin->value() - 0.976) < 0.0001);
    QVERIFY(std::abs(scaleYSpin->value() - 1.020) < 0.0001);

    saveProfileButton->click();

    const auto profiles = changedSettings.templateDocuments.value("default").deviceProfiles;
    QCOMPARE(profiles.size(), 1);
    QCOMPARE(profiles.first().printerName, QString("TSC TE244"));
    QVERIFY(std::abs(profiles.first().scaleX - 0.976) < 0.0001);
    QVERIFY(std::abs(profiles.first().scaleY - 1.020) < 0.0001);
    QVERIFY(profiles.first().notes.contains(QStringLiteral("80.000x30.000")));
    QVERIFY(profiles.first().notes.contains(QStringLiteral("82.000x29.400")));
}

void AppTests::templateDesignerWindowSelectsPaperSpecFromStore()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    PaperSpec label;
    label.id = QStringLiteral("label-80x30");
    label.name = QString::fromUtf8("80x30 标签");
    label.widthMm = 80.0;
    label.heightMm = 30.0;

    PaperSpec sheet;
    sheet.id = QStringLiteral("a4-sheet");
    sheet.name = QString::fromUtf8("A4 纸");
    sheet.kind = PaperSpecKind::Sheet;
    sheet.widthMm = 210.0;
    sheet.heightMm = 297.0;

    PaperSpecStore store(dir.filePath(QStringLiteral("paper-specs.json")));
    QVERIFY(store.savePaperSpec(label));
    QVERIFY(store.savePaperSpec(sheet));

    PrintClientSettings changedSettings;
    TemplateDesignerWindow window(PrintClientSettings{}, [&changedSettings](const PrintClientSettings& nextSettings) {
        changedSettings = nextSettings;
    }, dir.filePath(QStringLiteral("templates")));

    auto* paperSpecCombo = window.findChild<QComboBox*>(QStringLiteral("paperSpecCombo"));
    TemplatePreviewLabel* previewLabel = nullptr;
    for (auto* labelWidget : window.findChildren<QLabel*>(QStringLiteral("designerPreviewLabel"))) {
        previewLabel = dynamic_cast<TemplatePreviewLabel*>(labelWidget);
        if (previewLabel != nullptr) {
            break;
        }
    }
    QVERIFY(paperSpecCombo != nullptr);
    QVERIFY(previewLabel != nullptr);
    QCOMPARE(paperSpecCombo->count(), 2);

    const auto a4Index = paperSpecCombo->findData(QStringLiteral("a4-sheet"));
    QVERIFY(a4Index >= 0);
    paperSpecCombo->setCurrentIndex(a4Index);

    QCOMPARE(changedSettings.templateDocuments.value("default").paperSpecId, QString("a4-sheet"));
    QVERIFY(!previewLabel->pixmap().isNull());
    QVERIFY(previewLabel->pixmap().height() > previewLabel->pixmap().width());
}

void AppTests::templateDesignerWindowShowsTemplateValidationIssues()
{
    TemplateElement unknownText;
    unknownText.id = QStringLiteral("unknownText");
    unknownText.type = TemplateElementType::BoundField;
    unknownText.fieldKey = QStringLiteral("unknownField");
    unknownText.width = 20.0;
    unknownText.height = 5.0;

    TemplateLayer layer;
    layer.id = QStringLiteral("base");
    layer.name = QString::fromUtf8("基础图层");
    layer.elements = {unknownText};

    TemplateDocument document;
    document.id = QStringLiteral("template-invalid");
    document.templateKey = QStringLiteral("default");
    document.paperSpecId = QStringLiteral("label-80x30");
    document.layers = {layer};

    PrintClientSettings settings;
    settings.templateDocuments["default"] = document;

    TemplateDesignerWindow window(settings, nullptr);

    auto* statusLabel = window.findChild<QLabel*>(QStringLiteral("designerStatusLabel"));
    QVERIFY(statusLabel != nullptr);
    QVERIFY(statusLabel->text().contains(QString::fromUtf8("模板校验")));
    QVERIFY(statusLabel->text().contains(QStringLiteral("unknownField")));
}

void AppTests::templateDesignerWindowUsesWorkbenchShell()
{
    TemplateDesignerWindow window(PrintClientSettings{}, nullptr);

    auto* shell = window.findChild<QWidget*>(QStringLiteral("designerWorkbenchShell"));
    auto* topToolbar = window.findChild<QWidget*>(QStringLiteral("designerTopToolbar"));
    auto* layerPanel = window.findChild<QWidget*>(QStringLiteral("designerLayerPanel"));
    auto* workspacePanel = window.findChild<QWidget*>(QStringLiteral("designerWorkspacePanel"));
    auto* inspectorPanel = window.findChild<QWidget*>(QStringLiteral("designerInspectorPanel"));
    TemplatePreviewLabel* previewLabel = nullptr;
    for (auto* label : window.findChildren<QLabel*>(QStringLiteral("designerPreviewLabel"))) {
        previewLabel = dynamic_cast<TemplatePreviewLabel*>(label);
        if (previewLabel != nullptr) {
            break;
        }
    }
    auto* elementList = window.findChild<QListWidget*>(QStringLiteral("templateElementList"));
    auto* prePrintButton = window.findChild<QPushButton*>(QStringLiteral("prePrintCurrentTemplateButton"));

    QVERIFY(shell != nullptr);
    QVERIFY(topToolbar != nullptr);
    QVERIFY(layerPanel != nullptr);
    QVERIFY(workspacePanel != nullptr);
    QVERIFY(inspectorPanel != nullptr);
    QVERIFY(previewLabel != nullptr);
    QVERIFY(elementList != nullptr);
    QVERIFY(prePrintButton != nullptr);
    QVERIFY(workspacePanel->isAncestorOf(previewLabel));
    QVERIFY(inspectorPanel->isAncestorOf(elementList));
    QVERIFY(topToolbar->isAncestorOf(prePrintButton));
}

void AppTests::templateDesignerWindowUsesProfessionalEditorLayout()
{
    TemplateDesignerWindow window(PrintClientSettings{}, nullptr);

    auto* splitter = window.findChild<QSplitter*>(QStringLiteral("designerMainSplitter"));
    auto* leftResourceSplitter = window.findChild<QSplitter*>(QStringLiteral("designerLeftResourceSplitter"));
    auto* inspectorTabs = window.findChild<QTabWidget*>(QStringLiteral("designerInspectorTabs"));
    auto* layerSection = window.findChild<QWidget*>(QStringLiteral("designerLayerSection"));
    auto* librarySection = window.findChild<QWidget*>(QStringLiteral("designerLibrarySection"));
    auto* elementTab = window.findChild<QWidget*>(QStringLiteral("designerElementInspectorTab"));
    auto* tableTab = window.findChild<QWidget*>(QStringLiteral("designerTableInspectorTab"));
    auto* arrayGridTab = window.findChild<QWidget*>(QStringLiteral("designerArrayGridInspectorTab"));
    auto* deviceTab = window.findChild<QWidget*>(QStringLiteral("designerDeviceInspectorTab"));
    auto* layerList = window.findChild<QListWidget*>(QStringLiteral("templateLayerList"));
    auto* libraryList = window.findChild<QListWidget*>(QStringLiteral("templateLibraryList"));
    auto* elementList = window.findChild<QListWidget*>(QStringLiteral("templateElementList"));
    auto* tableColumnsEdit = window.findChild<QLineEdit*>(QStringLiteral("tableColumnsEdit"));
    auto* arrayGridTemplateEdit = window.findChild<QPlainTextEdit*>(QStringLiteral("arrayGridCellTemplateEdit"));
    auto* deviceProfileEdit = window.findChild<QLineEdit*>(QStringLiteral("deviceProfilePrinterEdit"));

    QVERIFY(splitter != nullptr);
    QVERIFY(leftResourceSplitter != nullptr);
    QVERIFY(inspectorTabs != nullptr);
    QVERIFY(layerSection != nullptr);
    QVERIFY(librarySection != nullptr);
    QVERIFY(elementTab != nullptr);
    QVERIFY(tableTab != nullptr);
    QVERIFY(arrayGridTab != nullptr);
    QVERIFY(deviceTab != nullptr);
    QCOMPARE(splitter->orientation(), Qt::Horizontal);
    QCOMPARE(leftResourceSplitter->orientation(), Qt::Vertical);
    QCOMPARE(inspectorTabs->count(), 4);
    QVERIFY(inspectorTabs->tabText(0).contains(QString::fromUtf8("元素")));
    QVERIFY(inspectorTabs->tabText(1).contains(QString::fromUtf8("表格")));
    QVERIFY(inspectorTabs->tabText(2).contains(QString::fromUtf8("数组")));
    QVERIFY(inspectorTabs->tabText(3).contains(QString::fromUtf8("设备")));
    QVERIFY(layerSection->isAncestorOf(layerList));
    QVERIFY(librarySection->isAncestorOf(libraryList));
    QVERIFY(elementTab->isAncestorOf(elementList));
    QVERIFY(tableTab->isAncestorOf(tableColumnsEdit));
    QVERIFY(arrayGridTab->isAncestorOf(arrayGridTemplateEdit));
    QVERIFY(deviceTab->isAncestorOf(deviceProfileEdit));
}

void AppTests::templateDesignerWindowSwitchesInspectorTabWhenCanvasElementSelected()
{
    TemplateDesignerWindow window(PrintClientSettings{}, nullptr);

    auto* addLayerButton = window.findChild<QPushButton*>(QStringLiteral("addLayerButton"));
    auto* addTableButton = window.findChild<QPushButton*>(QStringLiteral("designerAddTableButton"));
    auto* addArrayGridButton = window.findChild<QPushButton*>(QStringLiteral("designerAddArrayGridButton"));
    auto* inspectorTabs = window.findChild<QTabWidget*>(QStringLiteral("designerInspectorTabs"));
    auto* elementTab = window.findChild<QWidget*>(QStringLiteral("designerElementInspectorTab"));
    auto* tableTab = window.findChild<QWidget*>(QStringLiteral("designerTableInspectorTab"));
    auto* arrayGridTab = window.findChild<QWidget*>(QStringLiteral("designerArrayGridInspectorTab"));
    auto* sampleDataEdit = window.findChild<QPlainTextEdit*>(QStringLiteral("templateSampleDataEdit"));
    TemplatePreviewLabel* previewLabel = nullptr;
    for (auto* label : window.findChildren<QLabel*>(QStringLiteral("designerPreviewLabel"))) {
        previewLabel = dynamic_cast<TemplatePreviewLabel*>(label);
        if (previewLabel != nullptr) {
            break;
        }
    }
    QVERIFY(addLayerButton != nullptr);
    QVERIFY(addTableButton != nullptr);
    QVERIFY(addArrayGridButton != nullptr);
    QVERIFY(inspectorTabs != nullptr);
    QVERIFY(elementTab != nullptr);
    QVERIFY(tableTab != nullptr);
    QVERIFY(arrayGridTab != nullptr);
    QVERIFY(sampleDataEdit != nullptr);
    QVERIFY(previewLabel != nullptr);
    QVERIFY(!previewLabel->pixmap().isNull());

    addLayerButton->click();
    addArrayGridButton->click();
    addTableButton->click();
    sampleDataEdit->setPlainText(QString::fromUtf8(R"json({
  "header_items": [
    {"text": "A", "value": "1"},
    {"text": "B", "value": "2"},
    {"text": "C", "value": "3"}
  ],
  "items": [
    {"productName": "Ring", "weight": "1.2"}
  ]
})json"));

    const auto pointAtMm = [previewLabel](double xMm, double yMm) {
        const auto origin = previewLabel->printableImageOriginPx();
        const auto imageSize = previewLabel->printableImageSizePx();
        return QPoint(
            origin.x() + static_cast<int>(std::round(xMm / 80.0 * imageSize.width())),
            origin.y() + static_cast<int>(std::round(yMm / 30.0 * imageSize.height())));
    };

    inspectorTabs->setCurrentWidget(elementTab);
    QTest::mousePress(previewLabel, Qt::LeftButton, Qt::NoModifier, pointAtMm(10.0, 6.0));
    QTest::mouseRelease(previewLabel, Qt::LeftButton, Qt::NoModifier, pointAtMm(10.0, 6.0));
    QCOMPARE(inspectorTabs->currentWidget(), arrayGridTab);

    inspectorTabs->setCurrentWidget(elementTab);
    QTest::mousePress(previewLabel, Qt::LeftButton, Qt::NoModifier, pointAtMm(60.0, 12.0));
    QTest::mouseRelease(previewLabel, Qt::LeftButton, Qt::NoModifier, pointAtMm(60.0, 12.0));
    QCOMPARE(inspectorTabs->currentWidget(), tableTab);
}

void AppTests::templateDesignerWindowElementListSupportsRenameMenuAndDragSort()
{
    PrintClientSettings changedSettings;
    TemplateDesignerWindow window(PrintClientSettings{}, [&changedSettings](const PrintClientSettings& nextSettings) {
        changedSettings = nextSettings;
    });

    auto* addLayerButton = window.findChild<QPushButton*>(QStringLiteral("addLayerButton"));
    auto* addFixedTextButton = window.findChild<QPushButton*>(QStringLiteral("designerAddFixedTextButton"));
    auto* addQrCodeButton = window.findChild<QPushButton*>(QStringLiteral("designerAddQrCodeButton"));
    auto* elementList = window.findChild<QListWidget*>(QStringLiteral("templateElementList"));
    QVERIFY(addLayerButton != nullptr);
    QVERIFY(addFixedTextButton != nullptr);
    QVERIFY(addQrCodeButton != nullptr);
    QVERIFY(elementList != nullptr);

    addLayerButton->click();
    addFixedTextButton->click();
    addQrCodeButton->click();

    QCOMPARE(elementList->editTriggers() & QAbstractItemView::DoubleClicked, QAbstractItemView::DoubleClicked);
    QCOMPARE(elementList->dragDropMode(), QAbstractItemView::InternalMove);
    QCOMPARE(elementList->defaultDropAction(), Qt::MoveAction);
    QVERIFY(elementList->findChild<QAction*>(QStringLiteral("renameElementAction")) != nullptr);
    QVERIFY(elementList->findChild<QAction*>(QStringLiteral("deleteElementAction")) != nullptr);
    QVERIFY(elementList->findChild<QAction*>(QStringLiteral("moveElementUpAction")) != nullptr);
    QVERIFY(elementList->findChild<QAction*>(QStringLiteral("moveElementDownAction")) != nullptr);

    elementList->setCurrentRow(1);
    elementList->currentItem()->setText(QString::fromUtf8("二维码载荷"));
    QCoreApplication::processEvents();

    auto document = changedSettings.templateDocuments.value(QStringLiteral("default"));
    QVERIFY(!document.layers.isEmpty());
    QCOMPARE(document.layers.last().elements.size(), 2);
    QCOMPARE(document.layers.last().elements[1].displayName, QString::fromUtf8("二维码载荷"));

    QVERIFY(elementList->model()->moveRow(QModelIndex(), 1, QModelIndex(), 0));
    QCoreApplication::processEvents();

    document = changedSettings.templateDocuments.value(QStringLiteral("default"));
    QCOMPARE(document.layers.last().elements[0].displayName, QString::fromUtf8("二维码载荷"));
    QCOMPARE(document.layers.last().elements[0].zIndex, 0);
    QCOMPARE(document.layers.last().elements[1].zIndex, 1);
}

void AppTests::templateDesignerWindowAutoAppliesElementAndTableProperties()
{
    PrintClientSettings changedSettings;
    TemplateDesignerWindow window(PrintClientSettings{}, [&changedSettings](const PrintClientSettings& nextSettings) {
        changedSettings = nextSettings;
    });

    auto* addLayerButton = window.findChild<QPushButton*>(QStringLiteral("addLayerButton"));
    auto* addFixedTextButton = window.findChild<QPushButton*>(QStringLiteral("designerAddFixedTextButton"));
    auto* addTableButton = window.findChild<QPushButton*>(QStringLiteral("designerAddTableButton"));
    auto* valueEdit = window.findChild<QPlainTextEdit*>(QStringLiteral("elementValueEdit"));
    auto* xSpin = window.findChild<QDoubleSpinBox*>(QStringLiteral("elementXSpin"));
    auto* tableDisplayNameEdit = window.findChild<QLineEdit*>(QStringLiteral("tableDisplayNameEdit"));
    QVERIFY(addLayerButton != nullptr);
    QVERIFY(addFixedTextButton != nullptr);
    QVERIFY(addTableButton != nullptr);
    QVERIFY(valueEdit != nullptr);
    QVERIFY(xSpin != nullptr);
    QVERIFY(tableDisplayNameEdit != nullptr);

    addLayerButton->click();
    addFixedTextButton->click();
    valueEdit->setPlainText(QString::fromUtf8("自动应用文本"));
    xSpin->setValue(18.5);

    QTRY_COMPARE(
        changedSettings.templateDocuments.value(QStringLiteral("default")).layers.last().elements.first().text,
        QString::fromUtf8("自动应用文本"));
    QCOMPARE(changedSettings.templateDocuments.value(QStringLiteral("default")).layers.last().elements.first().x, 18.5);

    addTableButton->click();
    tableDisplayNameEdit->setText(QString::fromUtf8("自动应用表格"));

    QTRY_COMPARE(
        changedSettings.templateDocuments.value(QStringLiteral("default")).layers.last().tables.first().displayName,
        QString::fromUtf8("自动应用表格"));
}

void AppTests::templateDesignerWindowAutoAppliesTextAutoFitFontProperties()
{
    PrintClientSettings changedSettings;
    TemplateDesignerWindow window(PrintClientSettings{}, [&changedSettings](const PrintClientSettings& nextSettings) {
        changedSettings = nextSettings;
    });

    auto* addLayerButton = window.findChild<QPushButton*>(QStringLiteral("addLayerButton"));
    auto* addFixedTextButton = window.findChild<QPushButton*>(QStringLiteral("designerAddFixedTextButton"));
    auto* autoFitCheck = window.findChild<QCheckBox*>(QStringLiteral("elementAutoFitFontCheck"));
    auto* minSpin = window.findChild<QDoubleSpinBox*>(QStringLiteral("elementAutoFitMinFontSizeSpin"));
    auto* maxSpin = window.findChild<QDoubleSpinBox*>(QStringLiteral("elementAutoFitMaxFontSizeSpin"));
    QVERIFY(addLayerButton != nullptr);
    QVERIFY(addFixedTextButton != nullptr);
    QVERIFY(autoFitCheck != nullptr);
    QVERIFY(minSpin != nullptr);
    QVERIFY(maxSpin != nullptr);

    addLayerButton->click();
    addFixedTextButton->click();
    autoFitCheck->setChecked(true);
    minSpin->setValue(3.5);
    maxSpin->setValue(12.5);

    QTRY_VERIFY(changedSettings.templateDocuments.value(QStringLiteral("default")).layers.last().elements.first().autoFitFont);
    const auto element = changedSettings.templateDocuments.value(QStringLiteral("default")).layers.last().elements.first();
    QCOMPARE(element.autoFitMinFontSizePt, 3.5);
    QCOMPARE(element.autoFitMaxFontSizePt, 12.5);
}

void AppTests::templateDesignerWindowAutoApplyKeepsElementListStable()
{
    PrintClientSettings changedSettings;
    TemplateDesignerWindow window(PrintClientSettings{}, [&changedSettings](const PrintClientSettings& nextSettings) {
        changedSettings = nextSettings;
    });

    auto* addLayerButton = window.findChild<QPushButton*>(QStringLiteral("addLayerButton"));
    auto* addFixedTextButton = window.findChild<QPushButton*>(QStringLiteral("designerAddFixedTextButton"));
    auto* valueEdit = window.findChild<QPlainTextEdit*>(QStringLiteral("elementValueEdit"));
    auto* elementList = window.findChild<QListWidget*>(QStringLiteral("templateElementList"));
    QVERIFY(addLayerButton != nullptr);
    QVERIFY(addFixedTextButton != nullptr);
    QVERIFY(valueEdit != nullptr);
    QVERIFY(elementList != nullptr);

    addLayerButton->click();
    addFixedTextButton->click();
    QCOMPARE(elementList->count(), 1);

    QSignalSpy modelResetSpy(elementList->model(), &QAbstractItemModel::modelReset);
    QSignalSpy rowsInsertedSpy(elementList->model(), &QAbstractItemModel::rowsInserted);
    QSignalSpy rowsRemovedSpy(elementList->model(), &QAbstractItemModel::rowsRemoved);

    valueEdit->setPlainText(QString::fromUtf8("轻量自动应用文本"));

    QTRY_COMPARE(
        changedSettings.templateDocuments.value(QStringLiteral("default")).layers.last().elements.first().text,
        QString::fromUtf8("轻量自动应用文本"));
    QCOMPARE(modelResetSpy.count(), 0);
    QCOMPARE(rowsInsertedSpy.count(), 0);
    QCOMPARE(rowsRemovedSpy.count(), 0);
    QCOMPARE(elementList->count(), 1);
}

void AppTests::templateDesignerWindowCoalescesAutoApplyPreviewRefresh()
{
    PrintClientSettings changedSettings;
    TemplateDesignerWindow window(PrintClientSettings{}, [&changedSettings](const PrintClientSettings& nextSettings) {
        changedSettings = nextSettings;
    });

    auto* addLayerButton = window.findChild<QPushButton*>(QStringLiteral("addLayerButton"));
    auto* addFixedTextButton = window.findChild<QPushButton*>(QStringLiteral("designerAddFixedTextButton"));
    auto* valueEdit = window.findChild<QPlainTextEdit*>(QStringLiteral("elementValueEdit"));
    TemplatePreviewLabel* previewLabel = nullptr;
    for (auto* label : window.findChildren<QLabel*>(QStringLiteral("designerPreviewLabel"))) {
        previewLabel = dynamic_cast<TemplatePreviewLabel*>(label);
        if (previewLabel != nullptr) {
            break;
        }
    }
    auto* previewTimer = window.findChild<QTimer*>(QStringLiteral("designerPreviewRefreshTimer"));
    QVERIFY(addLayerButton != nullptr);
    QVERIFY(addFixedTextButton != nullptr);
    QVERIFY(valueEdit != nullptr);
    QVERIFY(previewLabel != nullptr);
    QVERIFY(previewTimer != nullptr);
    QVERIFY(previewTimer->isSingleShot());

    addLayerButton->click();
    addFixedTextButton->click();
    QVERIFY(!previewLabel->pixmap().isNull());
    const auto beforePreview = imageBytes(previewLabel->pixmap());

    valueEdit->setPlainText(QString::fromUtf8("自动应用预览合并刷新"));
    QCOMPARE(imageBytes(previewLabel->pixmap()), beforePreview);

    QTRY_COMPARE_WITH_TIMEOUT(
        changedSettings.templateDocuments.value(QStringLiteral("default")).layers.last().elements.first().text,
        QString::fromUtf8("自动应用预览合并刷新"),
        1500);

    QTRY_VERIFY_WITH_TIMEOUT(imageBytes(previewLabel->pixmap()) != beforePreview, 500);
}

void AppTests::templateDesignerWindowAppliesNumericPropertyPromptly()
{
    PrintClientSettings changedSettings;
    TemplateDesignerWindow window(PrintClientSettings{}, [&changedSettings](const PrintClientSettings& nextSettings) {
        changedSettings = nextSettings;
    });

    auto* addLayerButton = window.findChild<QPushButton*>(QStringLiteral("addLayerButton"));
    auto* addFixedTextButton = window.findChild<QPushButton*>(QStringLiteral("designerAddFixedTextButton"));
    auto* xSpin = window.findChild<QDoubleSpinBox*>(QStringLiteral("elementXSpin"));
    TemplatePreviewLabel* previewLabel = nullptr;
    for (auto* label : window.findChildren<QLabel*>(QStringLiteral("designerPreviewLabel"))) {
        previewLabel = dynamic_cast<TemplatePreviewLabel*>(label);
        if (previewLabel != nullptr) {
            break;
        }
    }
    QVERIFY(addLayerButton != nullptr);
    QVERIFY(addFixedTextButton != nullptr);
    QVERIFY(xSpin != nullptr);
    QVERIFY(previewLabel != nullptr);

    addLayerButton->click();
    addFixedTextButton->click();
    QVERIFY(!previewLabel->pixmap().isNull());
    const auto beforePreview = imageBytes(previewLabel->pixmap());

    const auto targetX = xSpin->value() + 8.0;
    xSpin->setValue(targetX);

    QTRY_VERIFY_WITH_TIMEOUT(
        std::abs(changedSettings.templateDocuments.value(QStringLiteral("default")).layers.last().elements.first().x - targetX) < 0.001,
        220);
    QTRY_VERIFY_WITH_TIMEOUT(imageBytes(previewLabel->pixmap()) != beforePreview, 260);
}

void AppTests::templateDesignerWindowCoalescesDragPreviewRefresh()
{
    PrintClientSettings changedSettings;
    int changedCount = 0;
    TemplateDesignerWindow window(PrintClientSettings{}, [&changedSettings, &changedCount](const PrintClientSettings& nextSettings) {
        changedSettings = nextSettings;
        ++changedCount;
    });

    auto* addLayerButton = window.findChild<QPushButton*>(QStringLiteral("addLayerButton"));
    auto* addFixedTextButton = window.findChild<QPushButton*>(QStringLiteral("designerAddFixedTextButton"));
    auto* elementList = window.findChild<QListWidget*>(QStringLiteral("templateElementList"));
    auto* previewTimer = window.findChild<QTimer*>(QStringLiteral("designerPreviewRefreshTimer"));
    auto* propertyTimer = window.findChild<QTimer*>(QStringLiteral("designerPropertyRefreshTimer"));
    auto* settingsTimer = window.findChild<QTimer*>(QStringLiteral("designerSettingsChangedTimer"));
    TemplatePreviewLabel* previewLabel = nullptr;
    for (auto* label : window.findChildren<QLabel*>(QStringLiteral("designerPreviewLabel"))) {
        previewLabel = dynamic_cast<TemplatePreviewLabel*>(label);
        if (previewLabel != nullptr) {
            break;
        }
    }
    QVERIFY(addLayerButton != nullptr);
    QVERIFY(addFixedTextButton != nullptr);
    QVERIFY(elementList != nullptr);
    QVERIFY(previewTimer != nullptr);
    QVERIFY(propertyTimer != nullptr);
    QVERIFY(settingsTimer != nullptr);
    QVERIFY(previewLabel != nullptr);

    addLayerButton->click();
    addFixedTextButton->click();
    QVERIFY(!previewLabel->pixmap().isNull());

    const auto pointAtMm = [previewLabel](double xMm, double yMm) {
        const auto origin = previewLabel->printableImageOriginPx();
        const auto imageSize = previewLabel->printableImageSizePx();
        return QPoint(
            origin.x() + static_cast<int>(std::round(xMm / 80.0 * imageSize.width())),
            origin.y() + static_cast<int>(std::round(yMm / 30.0 * imageSize.height())));
    };

    QTest::mousePress(previewLabel, Qt::LeftButton, Qt::NoModifier, pointAtMm(12.0, 7.0));
    QVERIFY(elementList->currentItem() != nullptr);
    const auto elementId = elementList->currentItem()->data(Qt::UserRole).toString();
    const auto elementX = [](const PrintClientSettings& settings, const QString& id, double* x) {
        const auto document = settings.templateDocuments.value(QStringLiteral("default"));
        for (const auto& layer : document.layers) {
            for (const auto& element : layer.elements) {
                if (element.id == id) {
                    *x = element.x;
                    return true;
                }
            }
        }
        return false;
    };

    const auto beforePreview = imageBytes(previewLabel->pixmap());
    const auto beforeChangedCount = changedCount;
    double beforeX = 0.0;
    QVERIFY(elementX(changedSettings, elementId, &beforeX));

    QSignalSpy modelResetSpy(elementList->model(), &QAbstractItemModel::modelReset);
    QSignalSpy rowsInsertedSpy(elementList->model(), &QAbstractItemModel::rowsInserted);
    QSignalSpy rowsRemovedSpy(elementList->model(), &QAbstractItemModel::rowsRemoved);

    QTest::mouseMove(previewLabel, pointAtMm(18.0, 7.0));

    QVERIFY(previewTimer->isActive());
    QVERIFY(propertyTimer->isActive());
    QVERIFY(settingsTimer->isActive());
    QCOMPARE(imageBytes(previewLabel->pixmap()), beforePreview);
    QCOMPARE(modelResetSpy.count(), 0);
    QCOMPARE(rowsInsertedSpy.count(), 0);
    QCOMPARE(rowsRemovedSpy.count(), 0);
    QTRY_VERIFY_WITH_TIMEOUT(imageBytes(previewLabel->pixmap()) != beforePreview, 90);

    QTest::mouseRelease(previewLabel, Qt::LeftButton, Qt::NoModifier, pointAtMm(18.0, 7.0));
    QTRY_VERIFY(!previewTimer->isActive());
    QVERIFY(imageBytes(previewLabel->pixmap()) != beforePreview);
    QTRY_VERIFY(changedCount > beforeChangedCount);
    double afterX = 0.0;
    QVERIFY(elementX(changedSettings, elementId, &afterX));
    QVERIFY(afterX > beforeX);
}

void AppTests::templateDesignerWindowSkipsNoopPropertyApply()
{
    int changeCount = 0;
    TemplateDesignerWindow window(PrintClientSettings{}, [&changeCount](const PrintClientSettings&) {
        ++changeCount;
    });

    auto* addLayerButton = window.findChild<QPushButton*>(QStringLiteral("addLayerButton"));
    auto* addFixedTextButton = window.findChild<QPushButton*>(QStringLiteral("designerAddFixedTextButton"));
    auto* applyButton = window.findChild<QPushButton*>(QStringLiteral("applyElementPropertiesButton"));
    QVERIFY(addLayerButton != nullptr);
    QVERIFY(addFixedTextButton != nullptr);
    QVERIFY(applyButton != nullptr);

    addLayerButton->click();
    addFixedTextButton->click();
    changeCount = 0;

    applyButton->click();
    QCoreApplication::processEvents();

    QCOMPARE(changeCount, 0);
}

void AppTests::templateDesignerWindowAppliesPendingTextOnFocusOut()
{
    PrintClientSettings changedSettings;
    TemplateDesignerWindow window(PrintClientSettings{}, [&changedSettings](const PrintClientSettings& nextSettings) {
        changedSettings = nextSettings;
    });

    auto* addLayerButton = window.findChild<QPushButton*>(QStringLiteral("addLayerButton"));
    auto* addFixedTextButton = window.findChild<QPushButton*>(QStringLiteral("designerAddFixedTextButton"));
    auto* valueEdit = window.findChild<QPlainTextEdit*>(QStringLiteral("elementValueEdit"));
    QVERIFY(addLayerButton != nullptr);
    QVERIFY(addFixedTextButton != nullptr);
    QVERIFY(valueEdit != nullptr);

    addLayerButton->click();
    addFixedTextButton->click();

    valueEdit->setFocus();
    QCoreApplication::processEvents();
    valueEdit->setPlainText(QString::fromUtf8("失焦立即提交文本"));
    QVERIFY(changedSettings.templateDocuments.value(QStringLiteral("default")).layers.last().elements.first().text
        != QString::fromUtf8("失焦立即提交文本"));

    QFocusEvent focusOut(QEvent::FocusOut);
    QCoreApplication::sendEvent(valueEdit, &focusOut);
    QCoreApplication::processEvents();

    QCOMPARE(
        changedSettings.templateDocuments.value(QStringLiteral("default")).layers.last().elements.first().text,
        QString::fromUtf8("失焦立即提交文本"));
}

void AppTests::templateDesignerWindowShowsSampleJsonErrorNearEditor()
{
    TemplateDesignerWindow window(PrintClientSettings{}, nullptr);

    auto* sampleDataEdit = window.findChild<QPlainTextEdit*>(QStringLiteral("templateSampleDataEdit"));
    auto* errorLabel = window.findChild<QLabel*>(QStringLiteral("templateSampleDataErrorLabel"));
    QVERIFY(sampleDataEdit != nullptr);
    QVERIFY(errorLabel != nullptr);

    sampleDataEdit->setPlainText(QString::fromUtf8("{\n  \"product_name\": \"A\",\n}"));
    QCoreApplication::processEvents();

    QVERIFY(!errorLabel->isHidden());
    QVERIFY(errorLabel->text().contains(QString::fromUtf8("模拟数据 JSON 解析失败")));
    QVERIFY(errorLabel->text().contains(QString::fromUtf8("第")));
    QVERIFY(errorLabel->text().contains(QString::fromUtf8("行")));
    QVERIFY(errorLabel->text().contains(QString::fromUtf8("列")));

    sampleDataEdit->setPlainText(QString::fromUtf8("{\"product_name\":\"A\"}"));
    QCoreApplication::processEvents();
    QVERIFY(errorLabel->isHidden());
}

void AppTests::templateDesignerWindowToolbarExposesEditingActionsAndZoom()
{
    TemplateDesignerWindow window(PrintClientSettings{}, nullptr);

    auto* topToolbar = window.findChild<QWidget*>(QStringLiteral("designerTopToolbar"));
    auto* saveButton = window.findChild<QPushButton*>(QStringLiteral("saveTemplateToLibraryButton"));
    auto* prePrintButton = window.findChild<QPushButton*>(QStringLiteral("prePrintCurrentTemplateButton"));
    auto* importButton = window.findChild<QPushButton*>(QStringLiteral("importTemplateButton"));
    auto* exportButton = window.findChild<QPushButton*>(QStringLiteral("exportTemplateButton"));
    auto* undoButton = window.findChild<QPushButton*>(QStringLiteral("designerUndoButton"));
    auto* redoButton = window.findChild<QPushButton*>(QStringLiteral("designerRedoButton"));
    auto* zoomCombo = window.findChild<QComboBox*>(QStringLiteral("designerZoomCombo"));
    auto* designAidCheck = window.findChild<QCheckBox*>(QStringLiteral("designerDesignAidCheck"));
    auto* layerSection = window.findChild<QWidget*>(QStringLiteral("designerLayerSection"));
    auto* librarySection = window.findChild<QWidget*>(QStringLiteral("designerLibrarySection"));
    auto* layerList = window.findChild<QListWidget*>(QStringLiteral("templateLayerList"));
    auto* libraryList = window.findChild<QListWidget*>(QStringLiteral("templateLibraryList"));
    QVERIFY(topToolbar != nullptr);
    QVERIFY(saveButton != nullptr);
    QVERIFY(prePrintButton != nullptr);
    QVERIFY(importButton != nullptr);
    QVERIFY(exportButton != nullptr);
    QVERIFY(undoButton != nullptr);
    QVERIFY(redoButton != nullptr);
    QVERIFY(zoomCombo != nullptr);
    QVERIFY(designAidCheck != nullptr);
    QVERIFY(layerSection != nullptr);
    QVERIFY(librarySection != nullptr);
    QVERIFY(layerList != nullptr);
    QVERIFY(libraryList != nullptr);

    QVERIFY(topToolbar->isAncestorOf(saveButton));
    QVERIFY(topToolbar->isAncestorOf(prePrintButton));
    QVERIFY(topToolbar->isAncestorOf(importButton));
    QVERIFY(topToolbar->isAncestorOf(exportButton));
    QVERIFY(topToolbar->isAncestorOf(undoButton));
    QVERIFY(topToolbar->isAncestorOf(redoButton));
    QVERIFY(topToolbar->isAncestorOf(zoomCombo));
    QVERIFY(topToolbar->isAncestorOf(designAidCheck));
    QVERIFY(layerSection->isAncestorOf(layerList));
    QVERIFY(librarySection->isAncestorOf(libraryList));
    QVERIFY(zoomCombo->count() >= 4);
    QVERIFY(zoomCombo->findData(1.0) >= 0);
}

void AppTests::templateDesignerStateSkipsDuplicateDocumentHistory()
{
    TemplateDocument document;
    document.id = QStringLiteral("history-template");
    document.name = QString::fromUtf8("历史模板");
    document.templateKey = QStringLiteral("default");

    TemplateLayer layer;
    layer.id = QStringLiteral("base");
    layer.name = QString::fromUtf8("基础图层");
    document.layers = {layer};

    TemplateDesignerState state;
    state.resetDocumentHistory(document);
    state.rememberDocument(document);

    QVERIFY(!state.canUndo());

    auto changed = document;
    changed.name = QString::fromUtf8("历史模板 2");
    state.rememberDocument(changed);

    QVERIFY(state.canUndo());
    const auto previous = state.undoDocument();
    QVERIFY(previous.has_value());
    QCOMPARE(previous->name, QString::fromUtf8("历史模板"));
}

void AppTests::templateInspectorPanelEmitsSemanticPropertySignals()
{
    TemplateInspectorPanel panel;
    DesignerElementPropertyModel model;
    model.visible = true;
    model.canEdit = true;
    model.canEditValue = true;
    model.supportsAutoFitFont = true;
    model.supportsVerticalText = true;
    model.value = QString::fromUtf8("旧文本");
    model.x = 1.0;
    model.y = 2.0;
    model.width = 20.0;
    model.height = 6.0;
    model.fontSizePt = 8.0;
    panel.setElementProperties(model);

    QSignalSpy spy(&panel, &TemplateInspectorPanel::elementPropertiesEdited);
    auto* valueEdit = panel.findChild<QPlainTextEdit*>(QStringLiteral("elementValueEdit"));
    QVERIFY(valueEdit != nullptr);

    valueEdit->setPlainText(QString::fromUtf8("新文本"));

    QCOMPARE(spy.count(), 1);
    QVERIFY(spy.first().first().toInt() > 0);
    QCOMPARE(panel.elementProperties().value, QString::fromUtf8("新文本"));
}

void AppTests::templateDesignerPresenterAppliesElementPropertyCommand()
{
    TemplateElement element;
    element.id = QStringLiteral("text-1");
    element.layerId = QStringLiteral("base");
    element.type = TemplateElementType::FixedText;
    element.text = QString::fromUtf8("旧内容");
    element.x = 1.0;
    element.y = 2.0;
    element.width = 20.0;
    element.height = 6.0;
    element.fontSizePt = 6.0;

    TemplateDesignerPresenter presenter;
    auto model = presenter.elementPropertyModel(element, true);
    model.value = QString::fromUtf8("门店:${shop}");
    model.x = 12.5;
    model.autoFitFont = true;
    model.autoFitMinFontSizePt = 3.5;
    model.autoFitMaxFontSizePt = 13.0;

    const auto result = presenter.applyElementProperties(element, model);

    QVERIFY(result.changed);
    QVERIFY(result.errorMessage.isEmpty());
    QCOMPARE(element.text, QString::fromUtf8("门店:${shop}"));
    QCOMPARE(element.x, 12.5);
    QVERIFY(element.autoFitFont);
    QCOMPARE(element.autoFitMinFontSizePt, 3.5);
    QCOMPARE(element.autoFitMaxFontSizePt, 13.0);
}

void AppTests::tableColumnTextCodecFormatsAndParsesLegacyColumns()
{
    TableColumn fixed;
    fixed.id = QStringLiteral("name");
    fixed.title = QString::fromUtf8("品名");
    fixed.fieldKey = QStringLiteral("productName");
    fixed.widthMm = 26.0;
    fixed.widthMode = TableColumnWidthMode::Fixed;
    fixed.alignment = TableCellAlignment::Left;

    TableColumn flex;
    flex.id = QStringLiteral("weight");
    flex.title = QString::fromUtf8("重量");
    flex.fieldKey = QStringLiteral("weight");
    flex.widthMm = 18.0;

    const auto text = TableColumnTextCodec::format({fixed, flex});
    QCOMPARE(text, QString::fromUtf8("品名=productName:26.00,重量=weight:18.00"));

    QString errorMessage;
    const auto parsed = TableColumnTextCodec::parse(text, &errorMessage);
    QVERIFY2(errorMessage.isEmpty(), qPrintable(errorMessage));
    QCOMPARE(parsed.size(), 2);
    QCOMPARE(parsed[0].id, QStringLiteral("productName"));
    QCOMPARE(parsed[0].title, QString::fromUtf8("品名"));
    QCOMPARE(parsed[0].fieldKey, QStringLiteral("productName"));
    QCOMPARE(parsed[0].widthMm, 26.0);
}

void AppTests::tableDesignerCommandUsesStructuredColumnsWhenPresent()
{
    TableElement table;
    table.id = QStringLiteral("table-1");
    table.layerId = QStringLiteral("main");
    table.displayName = QString::fromUtf8("明细");
    table.dataPath = QStringLiteral("items");
    table.columns = TableColumnTextCodec::parse(QString::fromUtf8("旧列=oldField:20.00"));

    TemplateLayer layer;
    layer.id = QStringLiteral("main");
    layer.tables.append(table);

    TemplateDocument document;
    document.layers.append(layer);

    DesignerTableColumnModel column;
    column.columnId = QStringLiteral("structured-name");
    column.title = QString::fromUtf8("新列");
    column.fieldKey = QStringLiteral("newField");
    column.widthMode = TableColumnWidthMode::Flex;
    column.widthMm = 22.0;
    column.flexWeight = 2.0;
    column.alignment = TableCellAlignment::Right;
    column.fontSizePt = 7.0;
    column.bold = true;
    column.ellipsis = true;

    DesignerTablePropertyModel model;
    model.tableId = table.id;
    model.visible = true;
    model.canEdit = true;
    model.displayName = table.displayName;
    model.dataPath = table.dataPath;
    model.width = table.width;
    model.height = table.height;
    model.headerRowHeightMm = table.headerRowHeightMm;
    model.detailRowHeightMm = table.detailRowHeightMm;
    model.repeatHeaderOnPage = table.repeatHeaderOnPage;
    model.drawBorders = table.drawBorders;
    model.columnsText = QString::fromUtf8("旧列=oldField:20.00");
    model.columns = {column};

    const auto result = TablePropertiesCommand(model).apply(document, table.id);
    QVERIFY2(result.errorMessage.isEmpty(), qPrintable(result.errorMessage));
    QVERIFY(result.changed);
    const auto actual = document.layers.first().tables.first().columns.first();
    QCOMPARE(actual.id, QStringLiteral("structured-name"));
    QCOMPARE(actual.fieldKey, QStringLiteral("newField"));
    QCOMPARE(actual.widthMode, TableColumnWidthMode::Flex);
    QCOMPARE(actual.alignment, TableCellAlignment::Right);
    QVERIFY(actual.bold);
    QVERIFY(actual.ellipsis);
}

void AppTests::templateDesignerPresenterMapsTableColumns()
{
    TableColumn column;
    column.id = QStringLiteral("price");
    column.title = QString::fromUtf8("单价");
    column.fieldKey = QStringLiteral("price");
    column.widthMode = TableColumnWidthMode::Flex;
    column.widthMm = 18.0;
    column.flexWeight = 3.0;
    column.alignment = TableCellAlignment::Right;
    column.fontSizePt = 7.0;
    column.bold = true;
    column.ellipsis = true;

    TableElement table;
    table.id = QStringLiteral("items");
    table.columns.append(column);

    TemplateDesignerPresenter presenter;
    const auto model = presenter.tablePropertyModel(table, true);
    QCOMPARE(model.columns.size(), 1);
    QCOMPARE(model.columns.first().columnId, QStringLiteral("price"));
    QCOMPARE(model.columns.first().fieldKey, QStringLiteral("price"));
    QCOMPARE(model.columns.first().widthMode, TableColumnWidthMode::Flex);
    QCOMPARE(model.columns.first().alignment, TableCellAlignment::Right);
    QVERIFY(model.columns.first().bold);
    QVERIFY(model.columns.first().ellipsis);

    const auto roundTrip = presenter.tableColumnsFromModels(model.columns);
    QCOMPARE(roundTrip.size(), 1);
    QCOMPARE(roundTrip.first().id, QStringLiteral("price"));
    QCOMPARE(roundTrip.first().flexWeight, 3.0);
}

void AppTests::tableColumnEditorPanelEditsAndReordersColumns()
{
    TableColumnEditorPanel panel;
    DesignerTableColumnModel first;
    first.columnId = QStringLiteral("name");
    first.title = QString::fromUtf8("品名");
    first.fieldKey = QStringLiteral("productName");
    first.widthMode = TableColumnWidthMode::Fixed;
    first.widthMm = 30.0;

    DesignerTableColumnModel second;
    second.columnId = QStringLiteral("weight");
    second.title = QString::fromUtf8("重量");
    second.fieldKey = QStringLiteral("weight");
    second.alignment = TableCellAlignment::Right;
    panel.setColumns({first, second});

    auto* table = panel.findChild<QTableWidget*>(QStringLiteral("tableColumnEditorGrid"));
    QVERIFY(table != nullptr);
    QCOMPARE(table->rowCount(), 2);

    QSignalSpy editedSpy(&panel, &TableColumnEditorPanel::columnsEdited);
    table->item(0, 1)->setText(QStringLiteral("sku"));
    QTRY_COMPARE(editedSpy.count(), 1);
    QCOMPARE(panel.columns().first().fieldKey, QStringLiteral("sku"));

    panel.selectColumn(1);
    QSignalSpy movedSpy(&panel, &TableColumnEditorPanel::columnsEdited);
    auto* moveUp = panel.findChild<QPushButton*>(QStringLiteral("tableColumnMoveUpButton"));
    QVERIFY(moveUp != nullptr);
    moveUp->click();
    QTRY_COMPARE(movedSpy.count(), 1);
    QCOMPARE(panel.columns().first().columnId, QStringLiteral("weight"));
}

void AppTests::templateDesignerPresenterRejectsInvalidTableColumns()
{
    TableColumn column;
    column.id = QStringLiteral("productName");
    column.title = QString::fromUtf8("品名");
    column.fieldKey = QStringLiteral("productName");
    column.widthMm = 30.0;

    TableElement table;
    table.id = QStringLiteral("table-1");
    table.layerId = QStringLiteral("base");
    table.columns = {column};

    TemplateLayer layer;
    layer.id = QStringLiteral("base");
    layer.tables = {table};

    TemplateDocument document;
    document.layers = {layer};

    TemplateDesignerPresenter presenter;
    auto model = presenter.tablePropertyModel(table, true);
    model.columnsText = QString::fromUtf8("坏列配置");

    const auto result = presenter.applyTableProperties(document, table.id, model);

    QVERIFY(!result.changed);
    QVERIFY(!result.errorMessage.isEmpty());
    QCOMPARE(document.layers.first().tables.first().columns.size(), 1);
    QCOMPARE(document.layers.first().tables.first().columns.first().fieldKey, QStringLiteral("productName"));
}

void AppTests::paperSpecManagerWindowSavesAndDeletesSpecs()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    int changedCount = 0;
    const auto paperSpecPath = dir.filePath(QStringLiteral("paper-specs.json"));
    PaperSpecManagerWindow window(paperSpecPath, [&changedCount] {
        ++changedCount;
    });

    auto* addButton = window.findChild<QPushButton*>(QStringLiteral("addPaperSpecButton"));
    auto* saveButton = window.findChild<QPushButton*>(QStringLiteral("savePaperSpecButton"));
    auto* deleteButton = window.findChild<QPushButton*>(QStringLiteral("deletePaperSpecButton"));
    auto* idEdit = window.findChild<QLineEdit*>(QStringLiteral("paperSpecIdEdit"));
    auto* nameEdit = window.findChild<QLineEdit*>(QStringLiteral("paperSpecNameEdit"));
    auto* kindCombo = window.findChild<QComboBox*>(QStringLiteral("paperSpecKindCombo"));
    auto* widthSpin = window.findChild<QDoubleSpinBox*>(QStringLiteral("paperSpecWidthSpin"));
    auto* heightSpin = window.findChild<QDoubleSpinBox*>(QStringLiteral("paperSpecHeightSpin"));
    auto* dpiSpin = window.findChild<QDoubleSpinBox*>(QStringLiteral("paperSpecDpiSpin"));
    auto* gridEnabledCheck = window.findChild<QCheckBox*>(QStringLiteral("paperSpecGridEnabledCheck"));
    auto* gridRowsSpin = window.findChild<QSpinBox*>(QStringLiteral("paperSpecGridRowsSpin"));
    auto* gridColumnsSpin = window.findChild<QSpinBox*>(QStringLiteral("paperSpecGridColumnsSpin"));
    auto* list = window.findChild<QListWidget*>(QStringLiteral("paperSpecList"));

    QVERIFY(addButton != nullptr);
    QVERIFY(saveButton != nullptr);
    QVERIFY(deleteButton != nullptr);
    QVERIFY(idEdit != nullptr);
    QVERIFY(nameEdit != nullptr);
    QVERIFY(kindCombo != nullptr);
    QVERIFY(widthSpin != nullptr);
    QVERIFY(heightSpin != nullptr);
    QVERIFY(dpiSpin != nullptr);
    QVERIFY(gridEnabledCheck != nullptr);
    QVERIFY(gridRowsSpin != nullptr);
    QVERIFY(gridColumnsSpin != nullptr);
    QVERIFY(list != nullptr);

    addButton->click();
    idEdit->setText(QStringLiteral("a4-sheet"));
    nameEdit->setText(QString::fromUtf8("A4 纸"));
    kindCombo->setCurrentIndex(kindCombo->findData(QStringLiteral("sheet")));
    widthSpin->setValue(210.0);
    heightSpin->setValue(297.0);
    dpiSpin->setValue(300.0);
    saveButton->click();

    auto saved = PaperSpecStore(paperSpecPath).loadPaperSpec(QStringLiteral("a4-sheet"));
    QVERIFY(saved.has_value());
    QCOMPARE(saved->name, QString::fromUtf8("A4 纸"));
    QCOMPARE(saved->kind, PaperSpecKind::Sheet);
    QCOMPARE(saved->widthMm, 210.0);
    QCOMPARE(saved->heightMm, 297.0);
    QCOMPARE(changedCount, 1);

    nameEdit->setText(QString::fromUtf8("A4 多列标签"));
    gridEnabledCheck->setChecked(true);
    gridRowsSpin->setValue(10);
    gridColumnsSpin->setValue(2);
    saveButton->click();

    saved = PaperSpecStore(paperSpecPath).loadPaperSpec(QStringLiteral("a4-sheet"));
    QVERIFY(saved.has_value());
    QCOMPARE(saved->name, QString::fromUtf8("A4 多列标签"));
    QVERIFY(saved->labelGrid.enabled);
    QCOMPARE(saved->labelGrid.rows, 10);
    QCOMPARE(saved->labelGrid.columns, 2);
    QCOMPARE(changedCount, 2);
    QCOMPARE(list->count(), 1);

    deleteButton->click();
    QVERIFY(!PaperSpecStore(paperSpecPath).loadPaperSpec(QStringLiteral("a4-sheet")).has_value());
    QCOMPARE(changedCount, 3);
    QCOMPARE(list->count(), 0);
}

void AppTests::fieldPresetManagerWindowSavesAndDeletesPresets()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    int changedCount = 0;
    const auto presetPath = dir.filePath(QStringLiteral("field-presets.json"));
    FieldPresetManagerWindow window(presetPath, [&changedCount] {
        ++changedCount;
    });

    auto* addButton = window.findChild<QPushButton*>(QStringLiteral("addFieldPresetButton"));
    auto* saveButton = window.findChild<QPushButton*>(QStringLiteral("saveFieldPresetButton"));
    auto* deleteButton = window.findChild<QPushButton*>(QStringLiteral("deleteFieldPresetButton"));
    auto* idEdit = window.findChild<QLineEdit*>(QStringLiteral("fieldPresetIdEdit"));
    auto* nameEdit = window.findChild<QLineEdit*>(QStringLiteral("fieldPresetNameEdit"));
    auto* templateIdEdit = window.findChild<QLineEdit*>(QStringLiteral("fieldPresetTemplateIdEdit"));
    auto* valuesEdit = window.findChild<QPlainTextEdit*>(QStringLiteral("fieldPresetValuesEdit"));
    auto* list = window.findChild<QListWidget*>(QStringLiteral("fieldPresetList"));

    QVERIFY(addButton != nullptr);
    QVERIFY(saveButton != nullptr);
    QVERIFY(deleteButton != nullptr);
    QVERIFY(idEdit != nullptr);
    QVERIFY(nameEdit != nullptr);
    QVERIFY(templateIdEdit != nullptr);
    QVERIFY(valuesEdit != nullptr);
    QVERIFY(list != nullptr);

    addButton->click();
    idEdit->setText(QStringLiteral("default-invoice"));
    nameEdit->setText(QString::fromUtf8("默认发货单字段"));
    templateIdEdit->setText(QStringLiteral("invoice"));
    valuesEdit->setPlainText(QString::fromUtf8(R"json({"customerName":"散客","footerText":"感谢惠顾"})json"));
    saveButton->click();

    auto saved = FieldPresetStore(presetPath).loadPreset(QStringLiteral("default-invoice"));
    QVERIFY(saved.has_value());
    QCOMPARE(saved->name, QString::fromUtf8("默认发货单字段"));
    QCOMPARE(saved->templateId, QString("invoice"));
    QCOMPARE(saved->values["customerName"].toString(), QString::fromUtf8("散客"));
    QCOMPARE(changedCount, 1);

    valuesEdit->setPlainText(QString::fromUtf8(R"json({"customerName":"会员客户"})json"));
    saveButton->click();

    saved = FieldPresetStore(presetPath).loadPreset(QStringLiteral("default-invoice"));
    QVERIFY(saved.has_value());
    QCOMPARE(saved->values["customerName"].toString(), QString::fromUtf8("会员客户"));
    QCOMPARE(changedCount, 2);
    QCOMPARE(list->count(), 1);

    deleteButton->click();
    QVERIFY(!FieldPresetStore(presetPath).loadPreset(QStringLiteral("default-invoice")).has_value());
    QCOMPARE(changedCount, 3);
    QCOMPARE(list->count(), 0);
}

void AppTests::settingsWindowHasPaperSpecManagerEntry()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const auto settingsPath = dir.filePath("settings.json");
    QVERIFY(FileSettingsStore(settingsPath).save(PrintClientSettings{}));

    int openCount = 0;
    SettingsWindow window(settingsPath, nullptr);
    window.setOpenPaperSpecManagerCallback([&openCount] {
        ++openCount;
    });

    auto* openButton = window.findChild<QPushButton*>(QStringLiteral("openPaperSpecManagerButton"));
    QVERIFY(openButton != nullptr);

    openButton->click();
    QCOMPARE(openCount, 1);
}

void AppTests::settingsWindowHasFieldPresetManagerEntry()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const auto settingsPath = dir.filePath("settings.json");
    QVERIFY(FileSettingsStore(settingsPath).save(PrintClientSettings{}));

    int openCount = 0;
    SettingsWindow window(settingsPath, nullptr);
    window.setOpenFieldPresetManagerCallback([&openCount] {
        ++openCount;
    });

    auto* openButton = window.findChild<QPushButton*>(QStringLiteral("openFieldPresetManagerButton"));
    QVERIFY(openButton != nullptr);

    openButton->click();
    QCOMPARE(openCount, 1);
}

void AppTests::templateDesignerWindowHasPaperSpecManagerEntry()
{
    int openCount = 0;
    TemplateDesignerWindow window(PrintClientSettings{}, nullptr);
    window.setOpenPaperSpecManagerCallback([&openCount] {
        ++openCount;
    });

    auto* openButton = window.findChild<QPushButton*>(QStringLiteral("openPaperSpecManagerFromDesignerButton"));
    QVERIFY(openButton != nullptr);

    openButton->click();
    QCOMPARE(openCount, 1);
}

void AppTests::templateDesignerWindowHasFieldPresetManagerEntry()
{
    int openCount = 0;
    TemplateDesignerWindow window(PrintClientSettings{}, nullptr);
    window.setOpenFieldPresetManagerCallback([&openCount] {
        ++openCount;
    });

    auto* openButton = window.findChild<QPushButton*>(QStringLiteral("openFieldPresetManagerFromDesignerButton"));
    QVERIFY(openButton != nullptr);

    openButton->click();
    QCOMPARE(openCount, 1);
}

void AppTests::applicationRoutesPreviewOnlyThroughSettingsWindow()
{
    auto readSourceFile = [](const QString& relativePath) -> QString {
        const QStringList candidates = {
            relativePath,
            QStringLiteral("../%1").arg(relativePath),
            QStringLiteral("../../%1").arg(relativePath),
        };
        for (const auto& candidate : candidates) {
            QFile file(candidate);
            if (!file.exists()) {
                continue;
            }
            if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
                return {};
            }
            return QString::fromUtf8(file.readAll());
        }
        return {};
    };

    const auto mainSource = readSourceFile(QStringLiteral("src/app/main.cpp"));
    const auto cmakeSource = readSourceFile(QStringLiteral("CMakeLists.txt"));

    QVERIFY2(!mainSource.isEmpty(), qPrintable(QStringLiteral("找不到源码文件：%1").arg(QStringLiteral("src/app/main.cpp"))));
    QVERIFY2(!cmakeSource.isEmpty(), qPrintable(QStringLiteral("找不到源码文件：%1").arg(QStringLiteral("CMakeLists.txt"))));
    QVERIFY(!mainSource.contains(QStringLiteral("PreviewWindow")));
    QVERIFY(!mainSource.contains(QString::fromUtf8("预览标签")));
    QVERIFY(!cmakeSource.contains(QStringLiteral("src/app/PreviewWindow.cpp")));
}

void AppTests::settingsWindowHasTemplateDesignerEntry()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const auto settingsPath = dir.filePath("settings.json");
    FileSettingsStore store(settingsPath);
    QVERIFY(store.save(PrintClientSettings{}));

    int openCount = 0;
    SettingsWindow window(settingsPath, nullptr);
    window.setOpenTemplateDesignerCallback([&openCount] {
        ++openCount;
    });

    auto* openDesignerButton = window.findChild<QPushButton*>(QStringLiteral("openTemplateDesignerButton"));
    QVERIFY(openDesignerButton != nullptr);

    openDesignerButton->click();
    QCOMPARE(openCount, 1);
}

void AppTests::appThemeDefinesWorkbenchStyle()
{
    const auto styleSheet = sleekpr::app::sleekprAppStyleSheet();

    QVERIFY(styleSheet.contains(QStringLiteral("settingsNavigationRail")));
    QVERIFY(styleSheet.contains(QStringLiteral("designerTopToolbar")));
    QVERIFY(styleSheet.contains(QStringLiteral("designerLibrarySection")));
    QVERIFY(styleSheet.contains(QStringLiteral("designerLayerSection")));
    QVERIFY(styleSheet.contains(QStringLiteral("designerZoomCombo")));
    QVERIFY(styleSheet.contains(QStringLiteral("designerInspectorTabs")));
    QVERIFY(styleSheet.contains(QStringLiteral("QSplitter::handle")));
    QVERIFY(styleSheet.contains(QStringLiteral("buttonRole=\"primary\"")));
    QVERIFY(styleSheet.contains(QStringLiteral("buttonRole=\"danger\"")));
    QVERIFY(styleSheet.contains(QStringLiteral("QListWidget")));
}

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    AppTests tests;
    return QTest::qExec(&tests, argc, argv);
}

#include "tst_app.moc"

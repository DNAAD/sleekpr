#include <QtTest/QtTest>

#include <QDir>
#include <QColor>
#include <QImage>
#include <QJsonArray>
#include <QJsonObject>
#include <QFile>
#include <QTemporaryDir>

#include <algorithm>
#include <stdexcept>

#include "sleekpr/core/labels/LabelItem.h"
#include "sleekpr/core/labels/LabelRenderPlanner.h"
#include "sleekpr/core/native/NativeLabelDrawingPlanner.h"
#include "sleekpr/core/native/NativePrintDrawingPlan.h"
#include "sleekpr/core/native/NativePlanJsonSerializer.h"
#include "sleekpr/core/printing/LabelPaperSize.h"
#include "sleekpr/core/printing/PrintUnitConverter.h"
#include "sleekpr/core/security/AllowedOriginParser.h"
#include "sleekpr/core/security/LocalCorsPolicy.h"
#include "sleekpr/core/security/PrivateNetworkAccessPolicy.h"
#include "sleekpr/core/settings/FileSettingsStore.h"
#include "sleekpr/core/settings/PrinterSelectionResolver.h"
#include "sleekpr/core/settings/SettingsEditModel.h"
#include "sleekpr/core/settings/TemplateElement.h"
#include "sleekpr/core/settings/TemplateElementCatalog.h"
#include "sleekpr/core/templates/DeviceProfileResolver.h"
#include "sleekpr/core/templates/FieldPresetJson.h"
#include "sleekpr/core/templates/FieldPresetStore.h"
#include "sleekpr/core/templates/FieldSchemaJson.h"
#include "sleekpr/core/templates/PaperSpecJson.h"
#include "sleekpr/core/templates/PaperSpecStore.h"
#include "sleekpr/core/templates/TemplateDocument.h"
#include "sleekpr/core/templates/TemplateDocumentEditModel.h"
#include "sleekpr/core/templates/TemplateDocumentFactory.h"
#include "sleekpr/core/templates/TemplateDocumentJson.h"
#include "sleekpr/core/templates/TemplateLibraryStore.h"
#include "sleekpr/core/templates/TemplateDocumentRenderer.h"
#include "sleekpr/core/templates/TemplateDocumentValidator.h"
#include "sleekpr/core/templates/TemplateRenderContextBuilder.h"
#include "sleekpr/core/templates/TableElementJson.h"
#include "sleekpr/core/templates/TableElementLayout.h"
#include "sleekpr/core/templates/TableElementRenderer.h"
#include "sleekpr/infrastructure/preview/LabelPreviewImageRenderer.h"
#include "sleekpr/infrastructure/preview/LabelPreviewService.h"
#include "sleekpr/infrastructure/preview/PreviewLabelFactory.h"
#include "sleekpr/infrastructure/preview/QrCodeMatrixRenderer.h"
#include "sleekpr/infrastructure/rendering/TextAutoFitSizer.h"
#include "sleekpr/infrastructure/printing/QrModulePixelLayout.h"

using namespace sleekpr::core;

class CoreTests final : public QObject
{
    Q_OBJECT

private slots:
    void labelPaperSizeCreates80x30();
    void printUnitConverterMatchesDotnetBehavior();
    void nativePrintDrawingPlanWrapsSinglePagePlan();
    void settingsStoreReturnsDefaultsAndPersistsValues();
    void settingsStorePersistsLocalHttpLimitSettings();
    void settingsStorePersistsTemplateElements();
    void templateDocumentJsonPersistsLayersVersionsAndProfiles();
    void templateDocumentJsonPersistsAutoFitTextFont();
    void templateDocumentJsonPersistsSampleData();
    void templateDocumentJsonRejectsInvalidImportDocument();
    void templateDocumentEditModelManagesLayers();
    void templateDocumentEditModelReordersElements();
    void templateDocumentEditModelManagesTables();
    void templateDocumentEditModelRestoresVersionsWithoutProfiles();
    void templateDocumentEditModelRejectsLockedLayerElementMove();
    void deviceProfileResolverUsesPrinterSpecificProfile();
    void templateDocumentFactoryBootstrapsExistingDrawingPlan();
    void templateDocumentFactoryKeepsDynamicFieldsWhenRendered();
    void templateDocumentFactoryNormalizesCustomElements();
    void templateDocumentRendererSkipsHiddenLayersAndSortsElements();
    void settingsStorePersistsTemplateDocuments();
    void settingsJsonKeepsOldTemplateElementsWhenTemplateDocumentsMissing();
    void paperSpecJsonPersistsLabelAndSheetGrid();
    void paperSpecJsonRejectsInvalidDimensions();
    void paperSpecStoreSavesListsLoadsAndRemovesSpecs();
    void paperSpecStoreReportsInvalidDocument();
    void fieldSchemaJsonPersistsCustomFieldDefinitions();
    void fieldSchemaJsonRejectsInvalidFieldDefinitions();
    void fieldPresetJsonPersistsReusableValues();
    void fieldPresetJsonRejectsInvalidPreset();
    void fieldPresetStoreSavesListsLoadsAndRemovesPresets();
    void fieldPresetStoreReportsInvalidDocument();
    void templateRenderContextBuilderMergesValuesByPriority();
    void templateRenderContextBuilderKeepsSchemaFreeRequestValues();
    void templateRenderContextBuilderReportsMissingRequiredFields();
    void tableElementJsonPersistsColumnsAndLayout();
    void tableElementJsonPersistsComplexTableModel();
    void tableElementJsonKeepsLegacyColumnsCompatible();
    void tableElementJsonRejectsInvalidTable();
    void tableElementLayoutDerivesLegacyHeaderAndDetailCells();
    void tableElementLayoutRejectsInvalidMergeRegion();
    void tableElementLayoutUsesAutoRowHeightForWrappedText();
    void tableElementLayoutAppliesCellTemplatesAndMergeRegions();
    void tableElementLayoutRendersSummaryAndFooterBands();
    void tableElementRendererBuildsSinglePageCommands();
    void tableElementRendererUsesComplexLayoutCells();
    void tableElementRendererDistributesFlexibleColumnWidths();
    void tableElementRendererRejectsNonArrayData();
    void tableElementRendererResolvesNestedDataPathsAndFields();
    void tableElementRendererSplitsRowsAcrossPagesAndRepeatsHeader();
    void tableElementRendererRejectsPageTooShortForOneRow();
    void templateDocumentJsonPersistsLayerTables();
    void templateDocumentImportRejectsInvalidLayerTable();
    void templateDocumentRendererAppendsTableCommands();
    void templateDocumentRendererInterpolatesFixedTextPlaceholders();
    void templateDocumentRendererWrapsFixedTextWithinElementWidth();
    void templateDocumentRendererRendersArrayGridElements();
    void templateDocumentRendererUsesArrayGridRowHeight();
    void templateDocumentRendererBuildsMultiPagePrintPlan();
    void templateDocumentJsonPersistsPaperMetadata();
    void templateDocumentImportRequiresPaperSpecIdForGenericTemplates();
    void templateDocumentJsonPersistsFieldSchema();
    void templateDocumentImportRejectsInvalidFieldSchema();
    void templateDocumentValidatorReportsDesignErrors();
    void templateDocumentValidatorAllowsFlexibleColumnsWithinTableWidth();
    void templateDocumentValidatorAllowsLegacyFieldsAndReportsQrCapacity();
    void templateLibraryStoreSavesListsAndLoadsTemplates();
    void templateLibraryStoreRemovesSavedTemplates();
    void templateLibraryStoreRejectsInvalidTemplateImport();
    void allowedOriginParserNormalizesValidOrigins();
    void corsAndPrivateNetworkPoliciesKeepLocalContract();
    void printerSelectionUsesConfiguredDefaultOnly();
    void templateElementCatalogReturnsEditableDefaults();
    void settingsEditModelResetsElementOverridesOnly();
    void repeatedElementOverridesKeepRelativeLayout();
    void previewLabelFactoryCreatesNativeDemoData();
    void labelPlannerMapsFieldsToRenderPlan();
    void nativeDrawingPlannerEmitsMillimeterCommandsAndOffsets();
    void nativeDrawingPlannerAppendsTemplateElements();
    void templateDocumentRendererMarksFixedAndBoundTextForAutoFit();
    void textAutoFitSizerShrinksAndExpandsText();
    void textAutoFitSizerCachesRepeatedMeasurements();
    void templateDocumentRendererRendersFixedTextVertically();
    void templateDocumentRendererInterpolatesQrCodePayloadTemplate();
    void nativeDrawingPlannerUsesSilverTemplateForFactory25003();
    void nativeDrawingPlannerUsesStableDefaultRenderDpi();
    void qrCodeMatrixRendererEncodesPayloadAsRealQrCode();
    void qrCodeMatrixRendererSupportsVersion2Payloads();
    void qrCodeMatrixRendererSupportsVersion3And4Payloads();
    void labelPreviewImageRendererRendersPngCanvas();
    void labelPreviewImageRendererRendersQtImage();
    void labelPreviewImageRendererRotatesNinetyDegreesFromTopToBottom();
    void labelPreviewImageRendererWrapsTextAndClipsToCommandHeight();
    void qrModulePixelLayoutKeepsDesignedQrPixelSize();
    void labelPreviewServiceUsesTemplateDocumentDeviceProfile();
    void labelPreviewServiceUsesTemplateDocumentSampleData();
    void labelPreviewServiceRenderPreviewUsesDefaultPrinterDeviceProfile();
    void nativePlanJsonSerializerExportsPaperAndCommands();
};

void CoreTests::labelPaperSizeCreates80x30()
{
    const auto size = LabelPaperSize::create80x30();

    QCOMPARE(size.widthMm(), 80.0);
    QCOMPARE(size.heightMm(), 30.0);
    QCOMPARE(size.widthHundredthsInch(), 315);
    QCOMPARE(size.heightHundredthsInch(), 118);
}

void CoreTests::printUnitConverterMatchesDotnetBehavior()
{
    const PrintUnitConverter converter(300.0);

    QCOMPARE(converter.millimetersToHundredthsInch(80.0), 315);
    QCOMPARE(converter.millimetersToHundredthsInch(30.0), 118);
    QCOMPARE(converter.millimetersToPixels(80.0), 945);
    QCOMPARE(converter.millimetersToPixels(30.0), 354);
    QCOMPARE(converter.millimetersToPixels(1.0), 12);
}

void CoreTests::nativePrintDrawingPlanWrapsSinglePagePlan()
{
    NativeLabelDrawingPlan labelPlan;
    labelPlan.paperSize = LabelPaperSize::create80x30();
    labelPlan.renderDpi = 203.0;
    NativeDrawCommand command;
    command.type = NativeDrawCommandType::Text;
    command.x = 1.0;
    command.y = 2.0;
    command.width = 10.0;
    command.height = 3.0;
    command.text = QStringLiteral("A");
    command.fontSizePt = 4.0;
    command.elementKey = QStringLiteral("title");
    labelPlan.commands.append(command);

    const auto printPlan = NativePrintDrawingPlan::fromSinglePage(labelPlan);
    QCOMPARE(printPlan.pages.size(), 1);
    QCOMPARE(printPlan.pages.first().pageNumber, 1);
    QCOMPARE(printPlan.pages.first().commands.first().elementKey, QString("title"));

    const auto firstPage = printPlan.firstPageAsLabelPlan();
    QCOMPARE(firstPage.renderDpi, 203.0);
    QCOMPARE(firstPage.commands.size(), 1);
    QCOMPARE(firstPage.commands.first().text, QString("A"));
}

void CoreTests::settingsStoreReturnsDefaultsAndPersistsValues()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    FileSettingsStore store(dir.filePath("settings.json"));
    const auto defaults = store.load();
    QCOMPARE(defaults.defaultPrinter, QString());
    QCOMPARE(defaults.labelOffset.x, 0.0);
    QCOMPARE(defaults.labelOffset.y, 0.0);
    QCOMPARE(defaults.allowedOrigins.size(), 0);
    QVERIFY(defaults.templateElements.isEmpty());

    PrintClientSettings expected;
    expected.defaultPrinter = "EPSON LQ-735KII ESC/P2";
    expected.labelOffset = LabelOffset{1.5, -2.0};
    expected.allowedOrigins = {"http://127.0.0.1:80", "http://localhost:5173"};
    expected.templateOverrides["default"]["productName"] = TemplateElementOverride{10.8, 0.4, 4.9, true};

    QVERIFY(store.save(expected));
    const auto actual = store.load();

    QCOMPARE(actual.defaultPrinter, expected.defaultPrinter);
    QCOMPARE(actual.labelOffset.x, 1.5);
    QCOMPARE(actual.labelOffset.y, -2.0);
    QCOMPARE(actual.allowedOrigins, expected.allowedOrigins);
    const auto productNameOverride = actual.templateOverrides.value("default").value("productName");
    QCOMPARE(productNameOverride.x.value(), 10.8);
    QCOMPARE(productNameOverride.bold.value(), true);
}

void CoreTests::settingsStorePersistsLocalHttpLimitSettings()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    FileSettingsStore store(dir.filePath("settings.json"));
    const auto defaults = store.load();
    QVERIFY(!defaults.localHttpLimits.maxHeaderBytes.has_value());
    QVERIFY(!defaults.localHttpLimits.maxContentLengthBytes.has_value());
    QVERIFY(!defaults.localHttpLimits.maxPreviewBatchItems.has_value());
    QVERIFY(!defaults.localHttpLimits.maxPreviewPages.has_value());
    QVERIFY(!defaults.localHttpLimits.maxPreviewResponseBytes.has_value());

    PrintClientSettings expected;
    expected.localHttpLimits.maxHeaderBytes = 24 * 1024;
    expected.localHttpLimits.maxContentLengthBytes = 9 * 1024 * 1024;
    expected.localHttpLimits.maxPreviewBatchItems = 60;
    expected.localHttpLimits.maxPreviewPages = 120;
    expected.localHttpLimits.maxPreviewResponseBytes = 6 * 1024 * 1024;

    QVERIFY(store.save(expected));
    const auto actual = store.load();

    QCOMPARE(actual.localHttpLimits.maxHeaderBytes.value(), qsizetype(24 * 1024));
    QCOMPARE(actual.localHttpLimits.maxContentLengthBytes.value(), qsizetype(9 * 1024 * 1024));
    QCOMPARE(actual.localHttpLimits.maxPreviewBatchItems.value(), 60);
    QCOMPARE(actual.localHttpLimits.maxPreviewPages.value(), 120);
    QCOMPARE(actual.localHttpLimits.maxPreviewResponseBytes.value(), qsizetype(6 * 1024 * 1024));
}

void CoreTests::settingsStorePersistsTemplateElements()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    TemplateElement fixedText;
    fixedText.id = "custom_fixed";
    fixedText.type = TemplateElementType::FixedText;
    fixedText.displayName = "Custom Fixed";
    fixedText.x = 3.5;
    fixedText.y = 4.5;
    fixedText.width = 12.0;
    fixedText.height = 2.5;
    fixedText.text = "STATIC";
    fixedText.fontSizePt = 4.8;
    fixedText.bold = true;

    TemplateElement boundField;
    boundField.id = "custom_bound";
    boundField.type = TemplateElementType::BoundField;
    boundField.displayName = "Custom Bound";
    boundField.x = 16.0;
    boundField.y = 5.0;
    boundField.width = 20.0;
    boundField.height = 3.0;
    boundField.fieldKey = "productName";
    boundField.fontSizePt = 4.2;

    TemplateElement qrCode;
    qrCode.id = "custom_qr";
    qrCode.type = TemplateElementType::QrCode;
    qrCode.displayName = "Custom QR";
    qrCode.x = 42.0;
    qrCode.y = 2.0;
    qrCode.width = 8.0;
    qrCode.height = 8.0;
    qrCode.payload = "STATIC-QR";

    TemplateElement rectangle;
    rectangle.id = "custom_rect";
    rectangle.type = TemplateElementType::Rectangle;
    rectangle.displayName = "Custom Rectangle";
    rectangle.x = 55.0;
    rectangle.y = 7.0;
    rectangle.width = 10.0;
    rectangle.height = 4.0;

    TemplateElement arrayGrid;
    arrayGrid.id = "legacy_array_grid";
    arrayGrid.type = TemplateElementType::ArrayGrid;
    arrayGrid.displayName = "Header Items";
    arrayGrid.x = 24.0;
    arrayGrid.y = 25.0;
    arrayGrid.width = 30.0;
    arrayGrid.height = 12.0;
    arrayGrid.dataPath = QStringLiteral("header_items");
    arrayGrid.arrayGridRows = 2;
    arrayGrid.arrayGridColumns = 3;
    arrayGrid.arrayGridCellTemplate = QStringLiteral("${text}:${value}");
    arrayGrid.arrayGridDrawBorders = false;

    PrintClientSettings expected;
    expected.templateElements["default"] = {fixedText, boundField, qrCode, rectangle, arrayGrid};

    FileSettingsStore store(dir.filePath("settings.json"));
    QVERIFY(store.save(expected));

    const auto actual = store.load();
    const auto elements = actual.templateElements.value("default");
    QCOMPARE(elements.size(), 5);
    QCOMPARE(elements[0].id, QString("custom_fixed"));
    QCOMPARE(elements[0].type, TemplateElementType::FixedText);
    QCOMPARE(elements[0].text, QString("STATIC"));
    QCOMPARE(elements[0].bold, true);
    QCOMPARE(elements[1].type, TemplateElementType::BoundField);
    QCOMPARE(elements[1].fieldKey, QString("productName"));
    QCOMPARE(elements[2].type, TemplateElementType::QrCode);
    QCOMPARE(elements[2].payload, QString("STATIC-QR"));
    QCOMPARE(elements[3].type, TemplateElementType::Rectangle);
    QCOMPARE(elements[3].width, 10.0);
    QCOMPARE(elements[4].type, TemplateElementType::ArrayGrid);
    QCOMPARE(elements[4].dataPath, QString("header_items"));
    QCOMPARE(elements[4].arrayGridRows, 2);
    QCOMPARE(elements[4].arrayGridColumns, 3);
    QCOMPARE(elements[4].arrayGridCellTemplate, QString("${text}:${value}"));
    QCOMPARE(elements[4].arrayGridDrawBorders, false);
}

void CoreTests::templateDocumentJsonPersistsLayersVersionsAndProfiles()
{
    TemplateElement fixedText;
    fixedText.id = "element-title";
    fixedText.type = TemplateElementType::FixedText;
    fixedText.text = "TITLE";
    fixedText.x = 3.0;
    fixedText.y = 4.0;
    fixedText.width = 20.0;
    fixedText.height = 3.0;
    fixedText.rotationDegrees = 90.0;
    fixedText.verticalText = true;
    fixedText.maxLines = 2;
    fixedText.ellipsis = true;

    TemplateLayer layer;
    layer.id = "layer-main";
    layer.name = "Main";
    layer.visible = true;
    layer.locked = false;
    layer.elements = {fixedText};

    TemplateVersion version;
    version.id = "version-1";
    version.name = "Initial";
    version.createdAt = "2026-06-15T10:00:00+08:00";
    version.note = "baseline";
    version.layersSnapshot = {layer};

    DeviceProfile profile;
    profile.id = "profile-default";
    profile.printerName = "";
    profile.dpi = 300.0;
    profile.scaleX = 1.01;
    profile.scaleY = 0.99;
    profile.offsetX = 0.2;
    profile.offsetY = -0.1;

    TemplateDocument expected;
    expected.id = "template-default";
    expected.name = "Default";
    expected.templateKey = "default";
    expected.schemaVersion = 1;
    expected.activeVersionId = "version-1";
    expected.layers = {layer};
    expected.versions = {version};
    expected.deviceProfiles = {profile};

    const auto json = TemplateDocumentJson::toJson(expected);
    const auto actual = TemplateDocumentJson::fromJson(json);

    QCOMPARE(actual.id, expected.id);
    QCOMPARE(actual.layers.size(), 1);
    QCOMPARE(actual.layers.first().elements.first().id, QString("element-title"));
    QCOMPARE(actual.layers.first().elements.first().rotationDegrees, 90.0);
    QCOMPARE(actual.layers.first().elements.first().verticalText, true);
    QCOMPARE(actual.layers.first().elements.first().maxLines, 2);
    QCOMPARE(actual.layers.first().elements.first().ellipsis, true);
    QCOMPARE(actual.layers.first().elements.first().layerId, QString("layer-main"));
    QCOMPARE(actual.versions.size(), 1);
    QCOMPARE(actual.versions.first().layersSnapshot.first().id, QString("layer-main"));
    QCOMPARE(actual.deviceProfiles.size(), 1);
    QCOMPARE(actual.deviceProfiles.first().scaleX, 1.01);
}

void CoreTests::templateDocumentJsonPersistsSampleData()
{
    TemplateDocument expected;
    expected.id = "template-sample-data";
    expected.name = QString::fromUtf8("样例数据模板");
    expected.templateKey = "default";
    expected.sampleData.insert(QStringLiteral("product_name"), QString::fromUtf8("足金串搭项链"));
    expected.sampleData.insert(QStringLiteral("sales_code"), QStringLiteral("606178PD35"));
    expected.sampleData.insert(QStringLiteral("header_items"), QJsonArray{
        QJsonObject{{QStringLiteral("text"), QString::fromUtf8("素金KA")}, {QStringLiteral("value"), QStringLiteral("2.99")}},
        QJsonObject{{QStringLiteral("text"), QString::fromUtf8("素金PC")}, {QStringLiteral("value"), QStringLiteral("4.13")}},
    });

    const auto json = TemplateDocumentJson::toJson(expected);
    const auto actual = TemplateDocumentJson::fromJson(json);

    QVERIFY(json["sampleData"].isObject());
    QCOMPARE(actual.sampleData["product_name"].toString(), QString::fromUtf8("足金串搭项链"));
    QCOMPARE(actual.sampleData["sales_code"].toString(), QStringLiteral("606178PD35"));
    QCOMPARE(actual.sampleData["header_items"].toArray().size(), 2);
}

void CoreTests::templateDocumentJsonPersistsAutoFitTextFont()
{
    TemplateElement element;
    element.id = QStringLiteral("auto-fit-title");
    element.type = TemplateElementType::FixedText;
    element.text = QString::fromUtf8("自动适配文本");
    element.fontSizePt = 5.0;
    element.autoFitFont = true;
    element.autoFitMinFontSizePt = 3.0;
    element.autoFitMaxFontSizePt = 12.0;

    TemplateLayer layer;
    layer.id = QStringLiteral("layer-main");
    element.layerId = layer.id;
    layer.elements = {element};

    TemplateDocument document;
    document.id = QStringLiteral("template-auto-fit");
    document.name = QString::fromUtf8("自动适配模板");
    document.templateKey = QStringLiteral("default");
    document.layers = {layer};

    const auto json = TemplateDocumentJson::toJson(document);
    const auto actual = TemplateDocumentJson::fromJson(json);
    const auto actualElement = actual.layers.first().elements.first();

    QVERIFY(json["layers"].toArray().first().toObject()["elements"].toArray().first().toObject()["autoFitFont"].toBool());
    QCOMPARE(actualElement.autoFitFont, true);
    QCOMPARE(actualElement.autoFitMinFontSizePt, 3.0);
    QCOMPARE(actualElement.autoFitMaxFontSizePt, 12.0);
}

void CoreTests::templateDocumentJsonRejectsInvalidImportDocument()
{
    QJsonObject missingLayers;
    missingLayers["schemaVersion"] = 1;
    missingLayers["id"] = "template-missing-layers";
    missingLayers["templateKey"] = "default";

    QString missingLayersError;
    QVERIFY(!TemplateDocumentJson::validateForImport(missingLayers, &missingLayersError));
    QVERIFY(missingLayersError.contains(QString::fromUtf8("图层")));

    QJsonObject duplicateLayer;
    duplicateLayer["schemaVersion"] = 1;
    duplicateLayer["id"] = "template-invalid";
    duplicateLayer["templateKey"] = "default";

    QJsonArray layers;
    layers.append(QJsonObject{
        {"id", "same"},
        {"name", "A"},
        {"elements", QJsonArray{
                         QJsonObject{{"id", "title-a"}, {"type", "fixedText"}},
                     }},
    });
    layers.append(QJsonObject{
        {"id", "same"},
        {"name", "B"},
        {"elements", QJsonArray{
                         QJsonObject{{"id", "title-b"}, {"type", "fixedText"}},
                     }},
    });
    duplicateLayer["layers"] = layers;

    QString errorMessage;
    QVERIFY(!TemplateDocumentJson::validateForImport(duplicateLayer, &errorMessage));
    QVERIFY(errorMessage.contains(QString::fromUtf8("图层")));

    QJsonObject whitespaceDuplicateLayer;
    whitespaceDuplicateLayer["schemaVersion"] = 1;
    whitespaceDuplicateLayer["id"] = "template-whitespace-layer";
    whitespaceDuplicateLayer["templateKey"] = "default";
    whitespaceDuplicateLayer["layers"] = QJsonArray{
        QJsonObject{
            {"id", "same"},
            {"elements", QJsonArray{
                             QJsonObject{{"id", "title-a"}, {"type", "fixedText"}},
                         }},
        },
        QJsonObject{
            {"id", " same "},
            {"elements", QJsonArray{
                             QJsonObject{{"id", "title-b"}, {"type", "fixedText"}},
                         }},
        },
    };

    QString whitespaceLayerError;
    QVERIFY(!TemplateDocumentJson::validateForImport(whitespaceDuplicateLayer, &whitespaceLayerError));
    QVERIFY(whitespaceLayerError.contains(QString::fromUtf8("图层")));

    QJsonObject emptyTemplate;
    emptyTemplate["schemaVersion"] = 1;
    emptyTemplate["id"] = "template-empty";
    emptyTemplate["templateKey"] = "default";
    emptyTemplate["layers"] = QJsonArray{
        QJsonObject{
            {"id", "layer-empty"},
            {"elements", QJsonArray{}},
        },
    };

    // 新建模板会先保存空白基础图层，后续再由用户逐步添加元素。
    QString emptyTemplateError;
    QVERIFY(TemplateDocumentJson::validateForImport(emptyTemplate, &emptyTemplateError));
    QVERIFY(emptyTemplateError.isEmpty());

    QJsonObject invalidElement;
    invalidElement["schemaVersion"] = 1;
    invalidElement["id"] = "template-invalid-element";
    invalidElement["templateKey"] = "default";
    invalidElement["layers"] = QJsonArray{
        QJsonObject{
            {"id", "layer-main"},
            {"elements", QJsonArray{
                             QJsonObject{{"id", "element-title"}, {"type", "badType"}},
                         }},
        },
    };

    QString invalidTypeError;
    QVERIFY(!TemplateDocumentJson::validateForImport(invalidElement, &invalidTypeError));
    QVERIFY(invalidTypeError.contains(QString::fromUtf8("未知元素类型")));

    QJsonObject duplicateElement;
    duplicateElement["schemaVersion"] = 1;
    duplicateElement["id"] = "template-duplicate-element";
    duplicateElement["templateKey"] = "default";
    duplicateElement["layers"] = QJsonArray{
        QJsonObject{
            {"id", "layer-main"},
            {"elements", QJsonArray{
                             QJsonObject{{"id", "same"}, {"type", "fixedText"}},
                             QJsonObject{{"id", "same"}, {"type", "rectangle"}},
                         }},
        },
    };

    QString duplicateElementError;
    QVERIFY(!TemplateDocumentJson::validateForImport(duplicateElement, &duplicateElementError));
    QVERIFY(duplicateElementError.contains(QString::fromUtf8("元素")));

    QJsonObject whitespaceDuplicateElement;
    whitespaceDuplicateElement["schemaVersion"] = 1;
    whitespaceDuplicateElement["id"] = "template-whitespace-element";
    whitespaceDuplicateElement["templateKey"] = "default";
    whitespaceDuplicateElement["layers"] = QJsonArray{
        QJsonObject{
            {"id", "layer-main"},
            {"elements", QJsonArray{
                             QJsonObject{{"id", "same"}, {"type", "fixedText"}},
                             QJsonObject{{"id", " same "}, {"type", "rectangle"}},
                         }},
        },
    };

    QString whitespaceElementError;
    QVERIFY(!TemplateDocumentJson::validateForImport(whitespaceDuplicateElement, &whitespaceElementError));
    QVERIFY(whitespaceElementError.contains(QString::fromUtf8("元素")));

    QJsonObject mismatchedLayerId;
    mismatchedLayerId["schemaVersion"] = 1;
    mismatchedLayerId["id"] = "template-mismatched-layer";
    mismatchedLayerId["templateKey"] = "default";
    mismatchedLayerId["layers"] = QJsonArray{
        QJsonObject{
            {"id", "layer-main"},
            {"elements", QJsonArray{
                             QJsonObject{{"id", "title"}, {"type", "fixedText"}, {"layerId", "other-layer"}},
                         }},
        },
    };

    QString layerIdError;
    QVERIFY(!TemplateDocumentJson::validateForImport(mismatchedLayerId, &layerIdError));
    QVERIFY(layerIdError.contains(QStringLiteral("layerId")));
}

void CoreTests::templateDocumentEditModelManagesLayers()
{
    TemplateDocument document;

    QVERIFY(TemplateDocumentEditModel::addLayer(document, "base", "Base"));
    QVERIFY(TemplateDocumentEditModel::addLayer(document, "foreground", "Foreground"));

    QCOMPARE(document.layers.size(), 2);
    QCOMPARE(document.layers[0].id, QString("base"));
    QCOMPARE(document.layers[1].id, QString("foreground"));

    QVERIFY(TemplateDocumentEditModel::moveLayerDown(document, "base"));
    QCOMPARE(document.layers[0].id, QString("foreground"));
    QCOMPARE(document.layers[1].id, QString("base"));

    QVERIFY(TemplateDocumentEditModel::setLayerVisible(document, "base", false));
    QCOMPARE(document.layers[1].visible, false);

    QVERIFY(TemplateDocumentEditModel::setLayerLocked(document, "base", true));
    QVERIFY(!TemplateDocumentEditModel::moveLayerUp(document, "base"));
    QVERIFY(!TemplateDocumentEditModel::moveLayerDown(document, "base"));
    QCOMPARE(document.layers[1].id, QString("base"));
}

void CoreTests::templateDocumentEditModelReordersElements()
{
    TemplateDocument document;
    QVERIFY(TemplateDocumentEditModel::addLayer(document, "base", "Base"));

    TemplateElement first;
    first.id = "first";
    first.x = 1.0;
    TemplateElement second;
    second.id = "second";
    second.x = 2.0;

    QVERIFY(TemplateDocumentEditModel::addElement(document, "base", first));
    QVERIFY(TemplateDocumentEditModel::addElement(document, "base", second));
    QCOMPARE(document.layers.first().elements[0].zIndex, 0);
    QCOMPARE(document.layers.first().elements[1].zIndex, 1);

    QVERIFY(TemplateDocumentEditModel::moveElementUp(document, "second"));

    const auto elements = document.layers.first().elements;
    QCOMPARE(elements[0].id, QString("second"));
    QCOMPARE(elements[0].zIndex, 0);
    QCOMPARE(elements[1].id, QString("first"));
    QCOMPARE(elements[1].zIndex, 1);
}

void CoreTests::templateDocumentEditModelManagesTables()
{
    TemplateDocument document;
    QVERIFY(TemplateDocumentEditModel::addLayer(document, "base", "Base"));

    TableElement table;
    table.id = "saleItems";
    table.displayName = QString::fromUtf8("明细表");
    table.dataPath = "items";
    table.x = 4.0;
    table.y = 5.0;

    TableColumn column;
    column.id = "productName";
    column.title = QString::fromUtf8("品名");
    column.fieldKey = "productName";
    column.widthMm = 35.0;
    table.columns.append(column);

    QVERIFY(TemplateDocumentEditModel::addTable(document, "base", table));
    QCOMPARE(document.layers.first().tables.size(), 1);
    QCOMPARE(document.layers.first().tables.first().layerId, QString("base"));
    QCOMPARE(document.layers.first().tables.first().zIndex, 0);

    auto updated = document.layers.first().tables.first();
    updated.displayName = QString::fromUtf8("销售明细");
    updated.dataPath = "saleItems";
    updated.width = 60.0;
    QVERIFY(TemplateDocumentEditModel::updateTable(document, "saleItems", updated));
    QCOMPARE(document.layers.first().tables.first().displayName, QString::fromUtf8("销售明细"));
    QCOMPARE(document.layers.first().tables.first().dataPath, QString("saleItems"));
    QCOMPARE(document.layers.first().tables.first().width, 60.0);

    QVERIFY(TemplateDocumentEditModel::setTableLocked(document, "saleItems", true));
    QVERIFY(!TemplateDocumentEditModel::moveTable(document, "saleItems", 10.0, 11.0));
    QCOMPARE(document.layers.first().tables.first().x, 4.0);
    QCOMPARE(document.layers.first().tables.first().y, 5.0);

    QVERIFY(TemplateDocumentEditModel::setTableLocked(document, "saleItems", false));
    QVERIFY(TemplateDocumentEditModel::moveTable(document, "saleItems", 10.0, 11.0));
    QCOMPARE(document.layers.first().tables.first().x, 10.0);
    QCOMPARE(document.layers.first().tables.first().y, 11.0);

    QVERIFY(TemplateDocumentEditModel::deleteTable(document, "saleItems"));
    QVERIFY(document.layers.first().tables.isEmpty());
}

void CoreTests::templateDocumentEditModelRestoresVersionsWithoutProfiles()
{
    TemplateLayer snapshotLayer;
    snapshotLayer.id = "snapshot";
    snapshotLayer.name = "Snapshot";

    TemplateVersion version;
    version.id = "version-1";
    version.name = "Initial";
    version.layersSnapshot = {snapshotLayer};

    TemplateLayer currentLayer;
    currentLayer.id = "current";
    currentLayer.name = "Current";

    DeviceProfile profile;
    profile.id = "profile-default";
    profile.printerName = "Printer A";
    profile.scaleX = 1.05;

    TemplateDocument document;
    document.layers = {currentLayer};
    document.versions = {version};
    document.deviceProfiles = {profile};

    QVERIFY(TemplateDocumentEditModel::restoreVersion(document, "version-1"));

    QCOMPARE(document.activeVersionId, QString("version-1"));
    QCOMPARE(document.layers.size(), 1);
    QCOMPARE(document.layers.first().id, QString("snapshot"));
    QCOMPARE(document.deviceProfiles.size(), 1);
    QCOMPARE(document.deviceProfiles.first().id, QString("profile-default"));
    QCOMPARE(document.deviceProfiles.first().scaleX, 1.05);

    QVERIFY(!TemplateDocumentEditModel::saveVersion(document, " version-2 ", "Bad", "2026-06-16T10:00:00+08:00", "bad"));
    QVERIFY(TemplateDocumentEditModel::saveVersion(document, "version-2", "Second", "2026-06-16T10:01:00+08:00", "second"));
    QVERIFY(!TemplateDocumentEditModel::saveVersion(document, "version-2", "Duplicate", "2026-06-16T10:02:00+08:00", "duplicate"));
    QCOMPARE(document.activeVersionId, QString("version-2"));
}

void CoreTests::templateDocumentEditModelRejectsLockedLayerElementMove()
{
    TemplateElement element;
    element.id = "locked-element";
    element.x = 3.0;
    element.y = 4.0;

    TemplateLayer layer;
    layer.id = "base";
    layer.locked = true;
    layer.elements = {element};

    TemplateDocument document;
    document.layers = {layer};

    QVERIFY(!TemplateDocumentEditModel::moveElement(document, "locked-element", 9.0, 10.0));
    QCOMPARE(document.layers.first().elements.first().x, 3.0);
    QCOMPARE(document.layers.first().elements.first().y, 4.0);
}

void CoreTests::deviceProfileResolverUsesPrinterSpecificProfile()
{
    DeviceProfile defaultProfile;
    defaultProfile.id = "profile-default";
    defaultProfile.printerName = "";
    defaultProfile.offsetX = 0.2;

    DeviceProfile printerProfile;
    printerProfile.id = "profile-printer-a";
    printerProfile.printerName = "Printer A";
    printerProfile.offsetX = 1.5;

    const QList<DeviceProfile> profiles{defaultProfile, printerProfile};

    QCOMPARE(DeviceProfileResolver::resolve(profiles, "printer a").id, QString("profile-printer-a"));
    QCOMPARE(DeviceProfileResolver::resolve(profiles, "Printer B").id, QString("profile-default"));
}

void CoreTests::templateDocumentFactoryBootstrapsExistingDrawingPlan()
{
    const auto labelPlan = LabelRenderPlanner().createPlan(sleekpr::infrastructure::PreviewLabelFactory::createDemoLabel());
    const auto baseDrawingPlan = NativeLabelDrawingPlanner().createPlan(labelPlan);
    const auto document = TemplateDocumentFactory::fromDrawingPlan(
        "default",
        QString::fromUtf8("默认标签"),
        baseDrawingPlan,
        QList<TemplateElement>{});

    QCOMPARE(document.templateKey, QString("default"));
    QCOMPARE(document.layers.size(), 1);
    QVERIFY(document.layers.first().elements.size() >= baseDrawingPlan.commands.size());
    QCOMPARE(document.layers.first().elements.first().zIndex, 0);

    const auto elements = document.layers.first().elements;
    const auto verticalIdentifier = std::find_if(elements.cbegin(), elements.cend(), [](const TemplateElement& element) {
        return element.id == QString("verticalIdentifier");
    });

    QVERIFY(verticalIdentifier != elements.cend());
    QCOMPARE(verticalIdentifier->rotationDegrees, 90.0);
}

void CoreTests::templateDocumentFactoryKeepsDynamicFieldsWhenRendered()
{
    const auto demoLabelPlan = LabelRenderPlanner().createPlan(sleekpr::infrastructure::PreviewLabelFactory::createDemoLabel());
    const auto document = TemplateDocumentFactory::fromDrawingPlan(
        "default",
        QString::fromUtf8("默认标签"),
        NativeLabelDrawingPlanner().createPlan(demoLabelPlan),
        QList<TemplateElement>{});

    LabelItem liveItem;
    liveItem.identifierCode = "LIVE-001";
    liveItem.productName = "Live Ring";
    liveItem.weightCategory = "Finished";
    liveItem.finishedProductWeight = 3.2;
    liveItem.roughWeight = 3.56;
    liveItem.salesCode = "XS-LIVE";
    liveItem.goldPurity = "Au999";
    liveItem.address = "Store Live";
    liveItem.tagWeight = 0.99;
    liveItem.finishedProductParts = {
        LabelPartItem{"Ring", 1.23},
        LabelPartItem{"Chain", 2.34},
        LabelPartItem{"Pendant", 3.45},
        LabelPartItem{"Bead", 4.56},
        LabelPartItem{"Hook", 5.67},
        LabelPartItem{"Clasp", 6.78},
        LabelPartItem{"Seal", 7.89},
        LabelPartItem{"Tag", 8.9},
        LabelPartItem{"Charm", 9.99},
    };
    const auto liveLabelPlan = LabelRenderPlanner().createPlan(liveItem);
    const auto drawingPlan = TemplateDocumentRenderer().render(document, liveLabelPlan, LabelOffset{}, DeviceProfile{});

    auto findCommand = [&drawingPlan](const QString& key) -> std::optional<NativeDrawCommand> {
        for (const auto& command : drawingPlan.commands) {
            if (command.elementKey == key) {
                return command;
            }
        }
        return std::nullopt;
    };

    const auto finishedWeightLabel = findCommand("finishedWeightLabel");
    QVERIFY(finishedWeightLabel.has_value());
    QCOMPARE(finishedWeightLabel->text, QString("Finished(g): "));

    const auto finishedWeightValue = findCommand("finishedWeightValue");
    QVERIFY(finishedWeightValue.has_value());
    QCOMPARE(finishedWeightValue->text, QString("3.20"));

    const auto roughWeightLabel = findCommand("roughWeightLabel");
    QVERIFY(roughWeightLabel.has_value());
    QCOMPARE(roughWeightLabel->text, QString::fromUtf8("总件重(g): "));

    const auto roughWeightValue = findCommand("roughWeightValue");
    QVERIFY(roughWeightValue.has_value());
    QCOMPARE(roughWeightValue->text, QString("3.56"));

    const auto qrNote = findCommand("qrNote");
    QVERIFY(qrNote.has_value());
    QVERIFY(qrNote->text.contains(QString("0.99")));

    QList<NativeDrawCommand> partRows;
    for (const auto& command : drawingPlan.commands) {
        if (command.elementKey.startsWith(QStringLiteral("partRow"))) {
            partRows.append(command);
        }
    }
    QCOMPARE(partRows.size(), 9);
    QCOMPARE(partRows[0].text, QString("Ring:1.23g"));
    QCOMPARE(partRows[1].text, QString("Chain:2.34g"));
    QCOMPARE(partRows[8].text, QString("Charm:9.99g"));
}

void CoreTests::templateDocumentFactoryNormalizesCustomElements()
{
    TemplateElement top;
    top.id = "custom-top";
    top.layerId = "legacy-layer";
    top.zIndex = 5;

    TemplateElement bottom;
    bottom.id = "custom-bottom";
    bottom.zIndex = 1;

    NativeLabelDrawingPlan emptyPlan;
    const auto document = TemplateDocumentFactory::fromDrawingPlan(
        "default",
        QString::fromUtf8("默认标签"),
        emptyPlan,
        QList<TemplateElement>{top, bottom});

    QCOMPARE(document.layers.size(), 1);
    const auto layer = document.layers.first();
    QCOMPARE(layer.elements.size(), 2);
    QCOMPARE(layer.elements[0].id, QString("custom-bottom"));
    QCOMPARE(layer.elements[0].layerId, layer.id);
    QCOMPARE(layer.elements[0].zIndex, 0);
    QCOMPARE(layer.elements[1].id, QString("custom-top"));
    QCOMPARE(layer.elements[1].layerId, layer.id);
    QCOMPARE(layer.elements[1].zIndex, 1);
}

void CoreTests::templateDocumentRendererSkipsHiddenLayersAndSortsElements()
{
    TemplateElement back;
    back.id = "back";
    back.type = TemplateElementType::Rectangle;
    back.x = 1.0;
    back.y = 2.0;
    back.width = 12.0;
    back.height = 5.0;
    back.zIndex = 2;

    TemplateElement front;
    front.id = "front";
    front.type = TemplateElementType::FixedText;
    front.text = "FRONT";
    front.x = 3.0;
    front.y = 4.0;
    front.width = 10.0;
    front.height = 2.0;
    front.zIndex = 1;

    TemplateElement hidden;
    hidden.id = "hidden";
    hidden.type = TemplateElementType::FixedText;
    hidden.text = "HIDDEN";

    TemplateLayer visibleLayer;
    visibleLayer.id = "visible";
    visibleLayer.elements = {back, front};

    TemplateLayer hiddenLayer;
    hiddenLayer.id = "hidden-layer";
    hiddenLayer.visible = false;
    hiddenLayer.elements = {hidden};

    TemplateDocument document;
    document.layers = {hiddenLayer, visibleLayer};

    const auto labelPlan = LabelRenderPlanner().createPlan(sleekpr::infrastructure::PreviewLabelFactory::createDemoLabel());
    const auto drawingPlan = TemplateDocumentRenderer().render(document, labelPlan, LabelOffset{}, DeviceProfile{});

    QCOMPARE(drawingPlan.commands.size(), 2);
    QCOMPARE(drawingPlan.commands[0].elementKey, QString("front"));
    QCOMPARE(drawingPlan.commands[1].elementKey, QString("back"));
}

void CoreTests::settingsStorePersistsTemplateDocuments()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    TemplateElement element;
    element.id = "title";
    element.type = TemplateElementType::FixedText;
    element.text = "TITLE";

    TemplateLayer layer;
    layer.id = "layer-main";
    layer.elements = {element};

    TemplateDocument document;
    document.id = "template-default";
    document.templateKey = "default";
    document.layers = {layer};

    PrintClientSettings expected;
    expected.templateDocuments["default"] = document;

    FileSettingsStore store(dir.filePath("settings.json"));
    QVERIFY(store.save(expected));

    const auto actual = store.load();
    QVERIFY(actual.templateDocuments.contains("default"));
    QCOMPARE(actual.templateDocuments["default"].layers.first().elements.first().id, QString("title"));
}

void CoreTests::settingsJsonKeepsOldTemplateElementsWhenTemplateDocumentsMissing()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    QFile file(dir.filePath(QStringLiteral("settings.json")));
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
    file.write(R"json({
        "templateElements": {
            "default": [{
                "id": "legacy-fixed",
                "type": "fixedText",
                "text": "LEGACY",
                "x": 3.0,
                "y": 4.0,
                "width": 12.0,
                "height": 3.0
            }]
        }
    })json");
    file.close();

    const auto settings = FileSettingsStore(dir.filePath(QStringLiteral("settings.json"))).load();

    QVERIFY(settings.templateDocuments.isEmpty());
    QCOMPARE(settings.templateElements.value(QStringLiteral("default")).size(), 1);
    QCOMPARE(settings.templateElements.value(QStringLiteral("default")).first().id, QString("legacy-fixed"));
    QCOMPARE(settings.templateElements.value(QStringLiteral("default")).first().text, QString("LEGACY"));
}

void CoreTests::paperSpecJsonPersistsLabelAndSheetGrid()
{
    PaperSpec spec;
    spec.id = QStringLiteral("a4-2x5");
    spec.name = QString::fromUtf8("A4 双列标签");
    spec.kind = PaperSpecKind::Sheet;
    spec.orientation = PaperOrientation::Portrait;
    spec.widthMm = 210.0;
    spec.heightMm = 297.0;
    spec.marginLeftMm = 5.0;
    spec.marginTopMm = 6.0;
    spec.marginRightMm = 5.0;
    spec.marginBottomMm = 6.0;
    spec.defaultDpi = 300.0;
    spec.labelGrid.enabled = true;
    spec.labelGrid.rows = 5;
    spec.labelGrid.columns = 2;
    spec.labelGrid.horizontalGapMm = 3.0;
    spec.labelGrid.verticalGapMm = 4.0;

    const auto json = PaperSpecJson::toJson(spec);
    QString errorMessage;
    QVERIFY(PaperSpecJson::validate(json, &errorMessage));
    const auto actual = PaperSpecJson::fromJson(json);

    QCOMPARE(actual.id, QString("a4-2x5"));
    QCOMPARE(actual.kind, PaperSpecKind::Sheet);
    QCOMPARE(actual.orientation, PaperOrientation::Portrait);
    QCOMPARE(actual.widthMm, 210.0);
    QCOMPARE(actual.heightMm, 297.0);
    QVERIFY(actual.labelGrid.enabled);
    QCOMPARE(actual.labelGrid.rows, 5);
    QCOMPARE(actual.labelGrid.columns, 2);
    QCOMPARE(actual.labelGrid.horizontalGapMm, 3.0);
    QCOMPARE(actual.labelGrid.verticalGapMm, 4.0);
}

void CoreTests::paperSpecJsonRejectsInvalidDimensions()
{
    QJsonObject json;
    json["schemaVersion"] = 1;
    json["id"] = QStringLiteral("bad-paper");
    json["name"] = QString::fromUtf8("错误纸张");
    json["kind"] = QStringLiteral("label");
    json["orientation"] = QStringLiteral("portrait");
    json["widthMm"] = 0.0;
    json["heightMm"] = 30.0;

    QString errorMessage;
    QVERIFY(!PaperSpecJson::validate(json, &errorMessage));
    QVERIFY(errorMessage.contains(QString::fromUtf8("宽高")));
}

void CoreTests::paperSpecStoreSavesListsLoadsAndRemovesSpecs()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    PaperSpec label;
    label.id = QStringLiteral("label-80x30");
    label.name = QString::fromUtf8("80x30 标签");
    label.widthMm = 80.0;
    label.heightMm = 30.0;
    label.defaultDpi = 203.0;

    PaperSpec sheet;
    sheet.id = QStringLiteral("a4-sheet");
    sheet.name = QString::fromUtf8("A4 多列标签纸");
    sheet.kind = PaperSpecKind::Sheet;
    sheet.widthMm = 210.0;
    sheet.heightMm = 297.0;
    sheet.labelGrid.enabled = true;
    sheet.labelGrid.rows = 10;
    sheet.labelGrid.columns = 3;
    sheet.labelGrid.horizontalGapMm = 2.0;
    sheet.labelGrid.verticalGapMm = 3.0;

    PaperSpecStore store(dir.filePath(QStringLiteral("paper-specs.json")));
    QString errorMessage;
    QVERIFY2(store.savePaperSpec(label, &errorMessage), qPrintable(errorMessage));
    QVERIFY2(store.savePaperSpec(sheet, &errorMessage), qPrintable(errorMessage));

    QCOMPARE(store.paperSpecIds(), QStringList({"a4-sheet", "label-80x30"}));

    const auto loadedSheet = store.loadPaperSpec(QStringLiteral("a4-sheet"), &errorMessage);
    QVERIFY2(loadedSheet.has_value(), qPrintable(errorMessage));
    QCOMPARE(loadedSheet->kind, PaperSpecKind::Sheet);
    QCOMPARE(loadedSheet->labelGrid.columns, 3);

    label.name = QString::fromUtf8("80x30 标签更新");
    QVERIFY2(store.savePaperSpec(label, &errorMessage), qPrintable(errorMessage));
    const auto loadedLabel = PaperSpecStore(dir.filePath(QStringLiteral("paper-specs.json")))
                                 .loadPaperSpec(QStringLiteral("label-80x30"), &errorMessage);
    QVERIFY2(loadedLabel.has_value(), qPrintable(errorMessage));
    QCOMPARE(loadedLabel->name, QString::fromUtf8("80x30 标签更新"));

    QVERIFY2(store.removePaperSpec(QStringLiteral("a4-sheet"), &errorMessage), qPrintable(errorMessage));
    QVERIFY(!store.loadPaperSpec(QStringLiteral("a4-sheet"), &errorMessage).has_value());
    QCOMPARE(store.paperSpecIds(), QStringList({"label-80x30"}));
}

void CoreTests::paperSpecStoreReportsInvalidDocument()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    QFile file(dir.filePath(QStringLiteral("paper-specs.json")));
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
    file.write(R"json({"schemaVersion":99,"paperSpecs":[]})json");
    file.close();

    QString errorMessage;
    const auto specs = PaperSpecStore(dir.filePath(QStringLiteral("paper-specs.json"))).paperSpecs(&errorMessage);

    QVERIFY(specs.isEmpty());
    QVERIFY(errorMessage.contains(QStringLiteral("schemaVersion")));
}

void CoreTests::fieldSchemaJsonPersistsCustomFieldDefinitions()
{
    QList<FieldDefinition> fields;

    FieldDefinition customerName;
    customerName.key = QStringLiteral("customerName");
    customerName.displayName = QString::fromUtf8("客户名称");
    customerName.type = FieldValueType::Text;
    customerName.required = true;
    customerName.defaultValue = QString::fromUtf8("散客");
    customerName.sampleValue = QString::fromUtf8("张三");
    customerName.format = QStringLiteral("plain");
    customerName.source = FieldSource::Custom;
    fields.append(customerName);

    FieldDefinition totalAmount;
    totalAmount.key = QStringLiteral("totalAmount");
    totalAmount.displayName = QString::fromUtf8("合计金额");
    totalAmount.type = FieldValueType::Money;
    totalAmount.required = false;
    totalAmount.defaultValue = QStringLiteral("0.00");
    totalAmount.sampleValue = QStringLiteral("1280.00");
    totalAmount.format = QStringLiteral("0.00");
    totalAmount.source = FieldSource::BuiltIn;
    fields.append(totalAmount);

    const auto json = FieldSchemaJson::toJson(fields);
    QString errorMessage;
    QVERIFY(FieldSchemaJson::validate(json, &errorMessage));

    const auto actual = FieldSchemaJson::fromJson(json);
    QCOMPARE(actual.size(), 2);
    QCOMPARE(actual[0].key, QString("customerName"));
    QCOMPARE(actual[0].displayName, QString::fromUtf8("客户名称"));
    QCOMPARE(actual[0].type, FieldValueType::Text);
    QVERIFY(actual[0].required);
    QCOMPARE(actual[0].defaultValue, QString::fromUtf8("散客"));
    QCOMPARE(actual[0].sampleValue, QString::fromUtf8("张三"));
    QCOMPARE(actual[0].format, QString("plain"));
    QCOMPARE(actual[0].source, FieldSource::Custom);

    QCOMPARE(actual[1].key, QString("totalAmount"));
    QCOMPARE(actual[1].type, FieldValueType::Money);
    QCOMPARE(actual[1].source, FieldSource::BuiltIn);
}

void CoreTests::fieldSchemaJsonRejectsInvalidFieldDefinitions()
{
    QJsonObject missingKeyField;
    missingKeyField["key"] = QStringLiteral(" ");
    missingKeyField["displayName"] = QString::fromUtf8("空字段");
    missingKeyField["type"] = QStringLiteral("text");
    missingKeyField["source"] = QStringLiteral("custom");

    QJsonObject missingKeySchema;
    missingKeySchema["fields"] = QJsonArray{missingKeyField};

    QString errorMessage;
    QVERIFY(!FieldSchemaJson::validate(missingKeySchema, &errorMessage));
    QVERIFY(errorMessage.contains(QString::fromUtf8("字段")));

    QJsonObject unknownTypeField;
    unknownTypeField["key"] = QStringLiteral("badField");
    unknownTypeField["displayName"] = QString::fromUtf8("错误字段");
    unknownTypeField["type"] = QStringLiteral("unknown");
    unknownTypeField["source"] = QStringLiteral("custom");

    QJsonObject unknownTypeSchema;
    unknownTypeSchema["fields"] = QJsonArray{unknownTypeField};

    QVERIFY(!FieldSchemaJson::validate(unknownTypeSchema, &errorMessage));
    QVERIFY(errorMessage.contains(QString::fromUtf8("类型")));
}

void CoreTests::fieldPresetJsonPersistsReusableValues()
{
    FieldPreset preset;
    preset.schemaVersion = 1;
    preset.id = QStringLiteral("default-sale-order");
    preset.name = QString::fromUtf8("默认销售单字段");
    preset.templateId = QStringLiteral("sale-order");
    preset.updatedAt = QStringLiteral("2026-06-16T10:30:00+08:00");
    preset.values["customerName"] = QString::fromUtf8("散客");
    preset.values["footerText"] = QString::fromUtf8("感谢惠顾");

    const auto json = FieldPresetJson::toJson(preset);
    QString errorMessage;
    QVERIFY(FieldPresetJson::validate(json, &errorMessage));

    const auto actual = FieldPresetJson::fromJson(json);
    QCOMPARE(actual.id, QString("default-sale-order"));
    QCOMPARE(actual.templateId, QString("sale-order"));
    QCOMPARE(actual.values["customerName"].toString(), QString::fromUtf8("散客"));
    QCOMPARE(actual.values["footerText"].toString(), QString::fromUtf8("感谢惠顾"));
}

void CoreTests::fieldPresetJsonRejectsInvalidPreset()
{
    QJsonObject missingTemplate;
    missingTemplate["schemaVersion"] = 1;
    missingTemplate["id"] = QStringLiteral("bad-preset");
    missingTemplate["values"] = QJsonObject{{QStringLiteral("customerName"), QString::fromUtf8("散客")}};

    QString errorMessage;
    QVERIFY(!FieldPresetJson::validate(missingTemplate, &errorMessage));
    QVERIFY(errorMessage.contains(QStringLiteral("templateId")));

    QJsonObject invalidValues;
    invalidValues["schemaVersion"] = 1;
    invalidValues["id"] = QStringLiteral("bad-preset");
    invalidValues["templateId"] = QStringLiteral("sale-order");
    invalidValues["values"] = QStringLiteral("not-object");

    QVERIFY(!FieldPresetJson::validate(invalidValues, &errorMessage));
    QVERIFY(errorMessage.contains(QString::fromUtf8("字段值")));
}

void CoreTests::fieldPresetStoreSavesListsLoadsAndRemovesPresets()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    FieldPreset defaultPreset;
    defaultPreset.id = QStringLiteral("default-sale-order");
    defaultPreset.name = QString::fromUtf8("默认销售单字段");
    defaultPreset.templateId = QStringLiteral("sale-order");
    defaultPreset.values["customerName"] = QString::fromUtf8("散客");
    defaultPreset.values["footerText"] = QString::fromUtf8("感谢惠顾");

    FieldPreset vipPreset;
    vipPreset.id = QStringLiteral("vip-sale-order");
    vipPreset.name = QString::fromUtf8("会员销售单字段");
    vipPreset.templateId = QStringLiteral("sale-order");
    vipPreset.values["customerName"] = QString::fromUtf8("会员客户");

    FieldPresetStore store(dir.filePath(QStringLiteral("field-presets.json")));
    QString errorMessage;
    QVERIFY2(store.savePreset(defaultPreset, &errorMessage), qPrintable(errorMessage));
    QVERIFY2(store.savePreset(vipPreset, &errorMessage), qPrintable(errorMessage));

    QCOMPARE(store.presetIds(), QStringList({"default-sale-order", "vip-sale-order"}));
    const auto loadedVip = store.loadPreset(QStringLiteral("vip-sale-order"), &errorMessage);
    QVERIFY2(loadedVip.has_value(), qPrintable(errorMessage));
    QCOMPARE(loadedVip->values["customerName"].toString(), QString::fromUtf8("会员客户"));

    defaultPreset.values["footerText"] = QString::fromUtf8("以实物为准");
    QVERIFY2(store.savePreset(defaultPreset, &errorMessage), qPrintable(errorMessage));
    const auto loadedDefault = FieldPresetStore(dir.filePath(QStringLiteral("field-presets.json")))
                                   .loadPreset(QStringLiteral("default-sale-order"), &errorMessage);
    QVERIFY2(loadedDefault.has_value(), qPrintable(errorMessage));
    QCOMPARE(loadedDefault->values["footerText"].toString(), QString::fromUtf8("以实物为准"));

    QVERIFY2(store.removePreset(QStringLiteral("vip-sale-order"), &errorMessage), qPrintable(errorMessage));
    QVERIFY(!store.loadPreset(QStringLiteral("vip-sale-order"), &errorMessage).has_value());
    QCOMPARE(store.presetIds(), QStringList({"default-sale-order"}));
}

void CoreTests::fieldPresetStoreReportsInvalidDocument()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    QFile file(dir.filePath(QStringLiteral("field-presets.json")));
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
    file.write(R"json({"schemaVersion":99,"fieldPresets":[]})json");
    file.close();

    QString errorMessage;
    const auto presets = FieldPresetStore(dir.filePath(QStringLiteral("field-presets.json"))).presets(&errorMessage);

    QVERIFY(presets.isEmpty());
    QVERIFY(errorMessage.contains(QStringLiteral("schemaVersion")));
}

void CoreTests::templateRenderContextBuilderMergesValuesByPriority()
{
    QList<FieldDefinition> fields;

    FieldDefinition customerName;
    customerName.key = QStringLiteral("customerName");
    customerName.displayName = QString::fromUtf8("客户名称");
    customerName.defaultValue = QString::fromUtf8("默认客户");
    fields.append(customerName);

    FieldDefinition footerText;
    footerText.key = QStringLiteral("footerText");
    footerText.displayName = QString::fromUtf8("底部备注");
    footerText.defaultValue = QString::fromUtf8("默认备注");
    fields.append(footerText);

    FieldDefinition operatorName;
    operatorName.key = QStringLiteral("operatorName");
    operatorName.displayName = QString::fromUtf8("打印员");
    operatorName.defaultValue = QString::fromUtf8("系统");
    fields.append(operatorName);

    QJsonObject presetValues;
    presetValues["customerName"] = QString::fromUtf8("方案客户");
    presetValues["footerText"] = QString::fromUtf8("方案备注");

    QJsonObject requestValues;
    requestValues["customerName"] = QString::fromUtf8("本次客户");

    const auto context = TemplateRenderContextBuilder::build(fields, presetValues, requestValues);
    QCOMPARE(context.values["customerName"].toString(), QString::fromUtf8("本次客户"));
    QCOMPARE(context.values["footerText"].toString(), QString::fromUtf8("方案备注"));
    QCOMPARE(context.values["operatorName"].toString(), QString::fromUtf8("系统"));
    QVERIFY(!context.hasMissingRequiredFields());
}

void CoreTests::templateRenderContextBuilderKeepsSchemaFreeRequestValues()
{
    QJsonObject requestValues;
    requestValues["productName"] = QString::fromUtf8("足金串搭项链");
    requestValues["finishedProductPartVO"] = QJsonArray{
        QJsonObject{
            {QStringLiteral("categoryName"), QString::fromUtf8("素金KA")},
            {QStringLiteral("partWeight"), QStringLiteral("2.99")}
        }
    };

    const auto context = TemplateRenderContextBuilder::build(QList<FieldDefinition>{}, QJsonObject{}, requestValues);

    QCOMPARE(context.values["productName"].toString(), QString::fromUtf8("足金串搭项链"));
    QCOMPARE(context.values["finishedProductPartVO"].toArray().size(), 1);
    QVERIFY(!context.hasMissingRequiredFields());
}

void CoreTests::templateRenderContextBuilderReportsMissingRequiredFields()
{
    QList<FieldDefinition> fields;
    FieldDefinition customerName;
    customerName.key = QStringLiteral("customerName");
    customerName.displayName = QString::fromUtf8("客户名称");
    customerName.required = true;
    fields.append(customerName);

    const auto context = TemplateRenderContextBuilder::build(fields, QJsonObject{}, QJsonObject{});
    QVERIFY(context.hasMissingRequiredFields());
    QCOMPARE(context.missingRequiredFieldNames, QStringList{QString::fromUtf8("客户名称")});
}

void CoreTests::tableElementJsonPersistsColumnsAndLayout()
{
    TableElement table;
    table.id = QStringLiteral("saleItems");
    table.layerId = QStringLiteral("main");
    table.displayName = QString::fromUtf8("销售明细");
    table.x = 5.0;
    table.y = 10.0;
    table.width = 90.0;
    table.height = 40.0;
    table.dataPath = QStringLiteral("items");
    table.headerRowHeightMm = 6.0;
    table.detailRowHeightMm = 5.0;

    TableColumn nameColumn;
    nameColumn.id = QStringLiteral("name");
    nameColumn.title = QString::fromUtf8("品名");
    nameColumn.fieldKey = QStringLiteral("productName");
    nameColumn.widthMm = 50.0;
    table.columns.append(nameColumn);

    TableColumn weightColumn;
    weightColumn.id = QStringLiteral("weight");
    weightColumn.title = QString::fromUtf8("重量");
    weightColumn.fieldKey = QStringLiteral("weight");
    weightColumn.widthMm = 20.0;
    weightColumn.alignment = TableCellAlignment::Right;
    weightColumn.widthMode = TableColumnWidthMode::Flex;
    weightColumn.flexWeight = 2.0;
    table.columns.append(weightColumn);

    const auto json = TableElementJson::toJson(table);
    QString errorMessage;
    QVERIFY(TableElementJson::validate(json, QStringLiteral("main"), &errorMessage));

    const auto actual = TableElementJson::fromJson(json, QStringLiteral("main"));
    QCOMPARE(actual.id, QString("saleItems"));
    QCOMPARE(actual.dataPath, QString("items"));
    QCOMPARE(actual.columns.size(), 2);
    QCOMPARE(actual.columns[1].alignment, TableCellAlignment::Right);
    QCOMPARE(actual.columns[1].widthMode, TableColumnWidthMode::Flex);
    QCOMPARE(actual.columns[1].flexWeight, 2.0);
}

void CoreTests::tableElementJsonPersistsComplexTableModel()
{
    TableElement table;
    table.id = QStringLiteral("complex-table");
    table.layerId = QStringLiteral("main");
    table.displayName = QString::fromUtf8("复杂表格");
    table.dataPath = QStringLiteral("items");

    TableColumn nameColumn;
    nameColumn.id = QStringLiteral("name");
    nameColumn.title = QString::fromUtf8("品名");
    nameColumn.fieldKey = QStringLiteral("productName");
    nameColumn.widthMm = 24.0;
    nameColumn.wrapText = true;
    nameColumn.minWidthMm = 12.0;
    nameColumn.defaultCellStyleId = QStringLiteral("body");
    table.columns.append(nameColumn);

    TableColumn amountColumn;
    amountColumn.id = QStringLiteral("amount");
    amountColumn.title = QString::fromUtf8("金额");
    amountColumn.fieldKey = QStringLiteral("amount");
    amountColumn.alignment = TableCellAlignment::Right;
    table.columns.append(amountColumn);

    TableCellStyle bodyStyle;
    bodyStyle.id = QStringLiteral("body");
    bodyStyle.fontSizePt = 7.5;
    bodyStyle.wrapText = true;
    bodyStyle.paddingLeftMm = 0.8;
    bodyStyle.paddingRightMm = 0.8;
    bodyStyle.backgroundColor = QStringLiteral("#FFFFFF");
    bodyStyle.textColor = QStringLiteral("#111111");
    table.cellStyles.append(bodyStyle);

    TableRowBand headerBand;
    headerBand.id = QStringLiteral("header");
    headerBand.kind = TableRowBandKind::Header;
    headerBand.heightMode = TableRowHeightMode::Fixed;
    headerBand.heightMm = 5.0;
    headerBand.repeatOnPage = true;
    table.rowBands.append(headerBand);

    TableRowBand detailBand;
    detailBand.id = QStringLiteral("detail");
    detailBand.kind = TableRowBandKind::Detail;
    detailBand.heightMode = TableRowHeightMode::Auto;
    detailBand.minHeightMm = 4.0;
    table.rowBands.append(detailBand);

    TableCellTemplate detailName;
    detailName.id = QStringLiteral("detail-name");
    detailName.rowBandId = QStringLiteral("detail");
    detailName.columnId = QStringLiteral("name");
    detailName.textTemplate = QStringLiteral("${productName}\n${spec}");
    detailName.styleId = QStringLiteral("body");
    detailName.overflowPolicy = TableCellOverflowPolicy::Wrap;
    detailName.maxLines = 3;
    table.cellTemplates.append(detailName);

    TableMergeRegion merge;
    merge.id = QStringLiteral("header-title");
    merge.rowBandId = QStringLiteral("header");
    merge.startRowOffset = 0;
    merge.startColumnId = QStringLiteral("name");
    merge.rowSpan = 1;
    merge.colSpan = 2;
    table.mergeRegions.append(merge);

    table.pagination.repeatHeaderOnPage = true;
    table.pagination.maxPages = 5;
    table.pagination.overflowPolicy = TableTableOverflowPolicy::Error;

    const auto json = TableElementJson::toJson(table);

    QVERIFY(json.contains(QStringLiteral("rowBands")));
    QVERIFY(json.contains(QStringLiteral("cellTemplates")));
    QVERIFY(json.contains(QStringLiteral("cellStyles")));
    QVERIFY(json.contains(QStringLiteral("mergeRegions")));
    QVERIFY(json.contains(QStringLiteral("pagination")));

    QString errorMessage;
    QVERIFY2(TableElementJson::validate(json, QStringLiteral("main"), &errorMessage), qPrintable(errorMessage));

    const auto actual = TableElementJson::fromJson(json, QStringLiteral("main"));
    QCOMPARE(actual.columns.first().wrapText, true);
    QCOMPARE(actual.columns.first().minWidthMm, 12.0);
    QCOMPARE(actual.columns.first().defaultCellStyleId, QStringLiteral("body"));
    QCOMPARE(actual.cellStyles.size(), 1);
    QCOMPARE(actual.cellStyles.first().textColor, QStringLiteral("#111111"));
    QCOMPARE(actual.rowBands.size(), 2);
    QCOMPARE(actual.rowBands[1].heightMode, TableRowHeightMode::Auto);
    QCOMPARE(actual.cellTemplates.size(), 1);
    QCOMPARE(actual.cellTemplates.first().overflowPolicy, TableCellOverflowPolicy::Wrap);
    QCOMPARE(actual.mergeRegions.size(), 1);
    QCOMPARE(actual.pagination.maxPages, 5);
}

void CoreTests::tableElementJsonKeepsLegacyColumnsCompatible()
{
    TableElement table;
    table.id = QStringLiteral("legacy-table");
    table.layerId = QStringLiteral("main");
    table.dataPath = QStringLiteral("items");

    TableColumn column;
    column.id = QStringLiteral("name");
    column.title = QString::fromUtf8("品名");
    column.fieldKey = QStringLiteral("productName");
    table.columns.append(column);

    const auto json = TableElementJson::toJson(table);
    QVERIFY(json.contains(QStringLiteral("columns")));
    QVERIFY(!json.contains(QStringLiteral("rowBands")));
    QVERIFY(!json.contains(QStringLiteral("cellTemplates")));

    const auto actual = TableElementJson::fromJson(json, QStringLiteral("main"));
    QCOMPARE(actual.columns.size(), 1);
    QVERIFY(actual.rowBands.isEmpty());
    QVERIFY(actual.cellTemplates.isEmpty());
}

void CoreTests::tableElementJsonRejectsInvalidTable()
{
    QJsonObject invalidTable;
    invalidTable["id"] = QStringLiteral("saleItems");
    invalidTable["dataPath"] = QStringLiteral("items");
    invalidTable["width"] = 80.0;
    invalidTable["height"] = 30.0;
    invalidTable["columns"] = QJsonArray{};

    QString errorMessage;
    QVERIFY(!TableElementJson::validate(invalidTable, QStringLiteral("main"), &errorMessage));
    QVERIFY(errorMessage.contains(QString::fromUtf8("列")));

    QJsonObject invalidColumn;
    invalidColumn["id"] = QStringLiteral("name");
    invalidColumn["title"] = QString::fromUtf8("品名");
    invalidColumn["fieldKey"] = QStringLiteral("productName");
    invalidColumn["widthMm"] = 0.0;
    invalidTable["columns"] = QJsonArray{invalidColumn};

    QVERIFY(!TableElementJson::validate(invalidTable, QStringLiteral("main"), &errorMessage));
    QVERIFY(errorMessage.contains(QString::fromUtf8("列宽")));
}

void CoreTests::tableElementLayoutDerivesLegacyHeaderAndDetailCells()
{
    TableElement table;
    table.id = QStringLiteral("legacy-table");
    table.dataPath = QStringLiteral("items");
    table.width = 50.0;
    table.height = 20.0;
    table.headerRowHeightMm = 5.0;
    table.detailRowHeightMm = 5.0;

    TableColumn nameColumn;
    nameColumn.id = QStringLiteral("name");
    nameColumn.title = QString::fromUtf8("品名");
    nameColumn.fieldKey = QStringLiteral("productName");
    nameColumn.widthMm = 25.0;
    table.columns.append(nameColumn);

    TableColumn amountColumn;
    amountColumn.id = QStringLiteral("amount");
    amountColumn.title = QString::fromUtf8("金额");
    amountColumn.fieldKey = QStringLiteral("amount");
    amountColumn.widthMm = 25.0;
    amountColumn.alignment = TableCellAlignment::Right;
    table.columns.append(amountColumn);

    TemplateRenderContext context;
    context.values = QJsonObject{
        {QStringLiteral("items"),
         QJsonArray{
             QJsonObject{{QStringLiteral("productName"), QString::fromUtf8("戒指")}, {QStringLiteral("amount"), QStringLiteral("1880")}},
             QJsonObject{{QStringLiteral("productName"), QString::fromUtf8("项链")}, {QStringLiteral("amount"), QStringLiteral("2999")}},
         }},
    };

    const auto result = TableElementLayout::layout(table, context);
    QVERIFY2(result.success(), qPrintable(result.errorMessage));
    QCOMPARE(result.pages.size(), 1);
    QCOMPARE(result.pages.first().firstRowIndex, 0);
    QCOMPARE(result.pages.first().rowCount, 2);
    QCOMPARE(result.pages.first().cells.size(), 6);

    const auto headerName = result.pages.first().cells.first();
    QCOMPARE(headerName.rowBandId, QStringLiteral("header"));
    QCOMPARE(headerName.columnId, QStringLiteral("name"));
    QCOMPARE(headerName.text, QString::fromUtf8("品名"));
    QCOMPARE(headerName.rect.x(), 0.0);
    QCOMPARE(headerName.rect.y(), 0.0);
    QCOMPARE(headerName.rect.width(), 25.0);
    QCOMPARE(headerName.rect.height(), 5.0);

    const auto firstDetailName = result.pages.first().cells[2];
    QCOMPARE(firstDetailName.rowBandId, QStringLiteral("detail"));
    QCOMPARE(firstDetailName.columnId, QStringLiteral("name"));
    QCOMPARE(firstDetailName.text, QString::fromUtf8("戒指"));
    QCOMPARE(firstDetailName.rect.y(), 5.0);

    const auto secondDetailAmount = result.pages.first().cells[5];
    QCOMPARE(secondDetailAmount.rowBandId, QStringLiteral("detail"));
    QCOMPARE(secondDetailAmount.columnId, QStringLiteral("amount"));
    QCOMPARE(secondDetailAmount.text, QStringLiteral("2999"));
    QCOMPARE(secondDetailAmount.style.alignment, TableCellAlignment::Right);
}

void CoreTests::tableElementLayoutRejectsInvalidMergeRegion()
{
    TableElement table;
    table.id = QStringLiteral("invalid-merge-table");
    table.dataPath = QStringLiteral("items");
    table.width = 40.0;
    table.height = 20.0;

    TableColumn column;
    column.id = QStringLiteral("name");
    column.title = QString::fromUtf8("品名");
    column.fieldKey = QStringLiteral("productName");
    column.widthMm = 40.0;
    table.columns.append(column);

    TableRowBand headerBand;
    headerBand.id = QStringLiteral("header");
    headerBand.kind = TableRowBandKind::Header;
    table.rowBands.append(headerBand);

    TableMergeRegion merge;
    merge.id = QStringLiteral("bad-merge");
    merge.rowBandId = QStringLiteral("header");
    merge.startColumnId = QStringLiteral("missing-column");
    merge.colSpan = 2;
    table.mergeRegions.append(merge);

    TemplateRenderContext context;
    context.values = QJsonObject{{QStringLiteral("items"), QJsonArray{}}};

    const auto result = TableElementLayout::layout(table, context);
    QVERIFY(!result.success());
    QVERIFY(result.errorMessage.contains(QStringLiteral("invalid-merge-table")));
    QVERIFY(result.errorMessage.contains(QStringLiteral("bad-merge")));
    QVERIFY(result.errorMessage.contains(QStringLiteral("missing-column")));
}

void CoreTests::tableElementLayoutUsesAutoRowHeightForWrappedText()
{
    TableElement table;
    table.id = QStringLiteral("auto-row-height");
    table.dataPath = QStringLiteral("items");
    table.width = 20.0;
    table.height = 40.0;
    table.detailRowHeightMm = 5.0;

    TableColumn column;
    column.id = QStringLiteral("name");
    column.title = QString::fromUtf8("品名");
    column.fieldKey = QStringLiteral("productName");
    column.widthMm = 20.0;
    column.wrapText = true;
    table.columns.append(column);

    TableRowBand detailBand;
    detailBand.id = QStringLiteral("detail");
    detailBand.kind = TableRowBandKind::Detail;
    detailBand.heightMode = TableRowHeightMode::Auto;
    detailBand.minHeightMm = 4.0;
    table.rowBands.append(detailBand);

    TemplateRenderContext context;
    context.values = QJsonObject{
        {QStringLiteral("items"),
         QJsonArray{
             QJsonObject{{QStringLiteral("productName"), QString::fromUtf8("很长很长的商品名称需要自动换行显示")}},
         }},
    };

    const auto result = TableElementLayout::layout(table, context);
    QVERIFY2(result.success(), qPrintable(result.errorMessage));

    const auto detailCell = std::find_if(result.pages.first().cells.cbegin(), result.pages.first().cells.cend(), [](const TableLayoutCell& cell) {
        return cell.rowBandId == QStringLiteral("detail");
    });
    QVERIFY(detailCell != result.pages.first().cells.cend());
    QVERIFY(detailCell->rect.height() > table.detailRowHeightMm);
    QCOMPARE(detailCell->overflowPolicy, TableCellOverflowPolicy::Wrap);
}

void CoreTests::tableElementLayoutAppliesCellTemplatesAndMergeRegions()
{
    TableElement table;
    table.id = QStringLiteral("merge-layout");
    table.dataPath = QStringLiteral("items");
    table.width = 60.0;
    table.height = 30.0;

    TableColumn nameColumn;
    nameColumn.id = QStringLiteral("name");
    nameColumn.title = QString::fromUtf8("品名");
    nameColumn.fieldKey = QStringLiteral("productName");
    nameColumn.widthMm = 30.0;
    table.columns.append(nameColumn);

    TableColumn specColumn;
    specColumn.id = QStringLiteral("spec");
    specColumn.title = QString::fromUtf8("规格");
    specColumn.fieldKey = QStringLiteral("spec");
    specColumn.widthMm = 30.0;
    table.columns.append(specColumn);

    TableCellTemplate mergedHeader;
    mergedHeader.id = QStringLiteral("merged-header");
    mergedHeader.rowBandId = QStringLiteral("header");
    mergedHeader.columnId = QStringLiteral("name");
    mergedHeader.textTemplate = QString::fromUtf8("商品信息");
    mergedHeader.colSpan = 2;
    table.cellTemplates.append(mergedHeader);

    TableMergeRegion merge;
    merge.id = QStringLiteral("header-merge");
    merge.rowBandId = QStringLiteral("header");
    merge.startColumnId = QStringLiteral("name");
    merge.colSpan = 2;
    table.mergeRegions.append(merge);

    TemplateRenderContext context;
    context.values = QJsonObject{
        {QStringLiteral("items"),
         QJsonArray{
             QJsonObject{{QStringLiteral("productName"), QString::fromUtf8("戒指")}, {QStringLiteral("spec"), QStringLiteral("16#")}},
         }},
    };

    const auto result = TableElementLayout::layout(table, context);
    QVERIFY2(result.success(), qPrintable(result.errorMessage));

    const auto headerCell = std::find_if(result.pages.first().cells.cbegin(), result.pages.first().cells.cend(), [](const TableLayoutCell& cell) {
        return cell.rowBandId == QStringLiteral("header") && !cell.coveredByMerge;
    });
    QVERIFY(headerCell != result.pages.first().cells.cend());
    QCOMPARE(headerCell->text, QString::fromUtf8("商品信息"));
    QCOMPARE(headerCell->rect.width(), 60.0);
}

void CoreTests::tableElementLayoutRendersSummaryAndFooterBands()
{
    TableElement table;
    table.id = QStringLiteral("summary-table");
    table.dataPath = QStringLiteral("items");
    table.width = 40.0;
    table.height = 40.0;

    TableColumn labelColumn;
    labelColumn.id = QStringLiteral("label");
    labelColumn.title = QString::fromUtf8("项目");
    labelColumn.fieldKey = QStringLiteral("name");
    labelColumn.widthMm = 20.0;
    table.columns.append(labelColumn);

    TableColumn amountColumn;
    amountColumn.id = QStringLiteral("amount");
    amountColumn.title = QString::fromUtf8("金额");
    amountColumn.fieldKey = QStringLiteral("amount");
    amountColumn.widthMm = 20.0;
    table.columns.append(amountColumn);

    TableRowBand summaryBand;
    summaryBand.id = QStringLiteral("summary");
    summaryBand.kind = TableRowBandKind::Summary;
    summaryBand.heightMm = 5.0;
    table.rowBands.append(summaryBand);

    TableCellTemplate summaryLabel;
    summaryLabel.id = QStringLiteral("summary-label");
    summaryLabel.rowBandId = QStringLiteral("summary");
    summaryLabel.columnId = QStringLiteral("label");
    summaryLabel.textTemplate = QString::fromUtf8("合计");
    table.cellTemplates.append(summaryLabel);

    TableCellTemplate summaryAmount;
    summaryAmount.id = QStringLiteral("summary-amount");
    summaryAmount.rowBandId = QStringLiteral("summary");
    summaryAmount.columnId = QStringLiteral("amount");
    summaryAmount.textTemplate = QStringLiteral("${totalAmount}");
    table.cellTemplates.append(summaryAmount);

    TemplateRenderContext context;
    context.values = QJsonObject{
        {QStringLiteral("totalAmount"), QStringLiteral("1880.00")},
        {QStringLiteral("items"),
         QJsonArray{
             QJsonObject{{QStringLiteral("name"), QString::fromUtf8("戒指")}, {QStringLiteral("amount"), 1880}},
         }},
    };

    const auto result = TableElementLayout::layout(table, context);
    QVERIFY2(result.success(), qPrintable(result.errorMessage));

    const auto summaryCell = std::find_if(result.pages.first().cells.cbegin(), result.pages.first().cells.cend(), [](const TableLayoutCell& cell) {
        return cell.rowBandId == QStringLiteral("summary") && cell.columnId == QStringLiteral("amount");
    });
    QVERIFY(summaryCell != result.pages.first().cells.cend());
    QCOMPARE(summaryCell->text, QStringLiteral("1880.00"));
    QVERIFY(summaryCell->rect.y() > table.headerRowHeightMm);
}

void CoreTests::tableElementRendererBuildsSinglePageCommands()
{
    TableElement table;
    table.id = QStringLiteral("itemsTable");
    table.x = 5.0;
    table.y = 10.0;
    table.width = 70.0;
    table.height = 25.0;
    table.dataPath = QStringLiteral("items");
    table.headerRowHeightMm = 5.0;
    table.detailRowHeightMm = 5.0;

    TableColumn nameColumn;
    nameColumn.id = QStringLiteral("name");
    nameColumn.title = QString::fromUtf8("品名");
    nameColumn.fieldKey = QStringLiteral("productName");
    nameColumn.widthMm = 45.0;
    table.columns.append(nameColumn);

    TableColumn weightColumn;
    weightColumn.id = QStringLiteral("weight");
    weightColumn.title = QString::fromUtf8("重量");
    weightColumn.fieldKey = QStringLiteral("weight");
    weightColumn.widthMm = 25.0;
    table.columns.append(weightColumn);

    TemplateRenderContext context;
    context.values["items"] = QJsonArray{
        QJsonObject{{QStringLiteral("productName"), QString::fromUtf8("足金戒指")}, {QStringLiteral("weight"), QStringLiteral("3.21g")}},
        QJsonObject{{QStringLiteral("productName"), QString::fromUtf8("足金项链")}, {QStringLiteral("weight"), QStringLiteral("5.88g")}},
    };

    const auto result = TableElementRenderer::renderSinglePage(table, context, 1.0, 2.0);
    QVERIFY(result.success());
    QCOMPARE(result.pages.size(), 1);
    QCOMPARE(result.pages.first().rowCount, 2);
    QCOMPARE(result.commands.size(), 12);
    QCOMPARE(result.commands[0].type, NativeDrawCommandType::Rectangle);
    QCOMPARE(result.commands[1].text, QString::fromUtf8("品名"));
    QCOMPARE(result.commands[5].text, QString::fromUtf8("足金戒指"));
    QCOMPARE(result.commands[9].text, QString::fromUtf8("足金项链"));
    QCOMPARE(result.commands[0].x, 6.0);
    QCOMPARE(result.commands[0].y, 12.0);
}

void CoreTests::tableElementRendererUsesComplexLayoutCells()
{
    TableElement table;
    table.id = QStringLiteral("render-complex");
    table.dataPath = QStringLiteral("items");
    table.width = 40.0;
    table.height = 30.0;

    TableColumn column;
    column.id = QStringLiteral("name");
    column.title = QString::fromUtf8("品名");
    column.fieldKey = QStringLiteral("productName");
    column.widthMm = 40.0;
    column.wrapText = true;
    table.columns.append(column);

    TableCellStyle style;
    style.id = QStringLiteral("highlight");
    style.fontSizePt = 9.0;
    style.bold = true;
    style.wrapText = true;
    style.backgroundColor = QStringLiteral("#FFFFCC");
    style.textColor = QStringLiteral("#222222");
    table.cellStyles.append(style);

    TableCellTemplate detail;
    detail.id = QStringLiteral("detail-name");
    detail.rowBandId = QStringLiteral("detail");
    detail.columnId = QStringLiteral("name");
    detail.textTemplate = QStringLiteral("${productName}\n${spec}");
    detail.styleId = QStringLiteral("highlight");
    detail.overflowPolicy = TableCellOverflowPolicy::Wrap;
    detail.maxLines = 2;
    table.cellTemplates.append(detail);

    TemplateRenderContext context;
    context.values = QJsonObject{
        {QStringLiteral("items"),
         QJsonArray{
             QJsonObject{{QStringLiteral("productName"), QString::fromUtf8("戒指")}, {QStringLiteral("spec"), QStringLiteral("16#")}},
         }},
    };

    const auto result = TableElementRenderer::renderSinglePage(table, context);
    QVERIFY2(result.success(), qPrintable(result.errorMessage));

    const auto textCommand = std::find_if(result.commands.cbegin(), result.commands.cend(), [](const NativeDrawCommand& command) {
        return command.elementKey == QStringLiteral("render-complex.row0.name");
    });
    QVERIFY(textCommand != result.commands.cend());
    QCOMPARE(textCommand->text, QString::fromUtf8("戒指\n16#"));
    QVERIFY(textCommand->wrapText);
    QCOMPARE(textCommand->maxLines, 2);
    QCOMPARE(textCommand->fontSizePt, 9.0);
    QVERIFY(textCommand->bold);
    QCOMPARE(textCommand->backgroundColor, QStringLiteral("#FFFFCC"));
    QCOMPARE(textCommand->textColor, QStringLiteral("#222222"));
}

void CoreTests::tableElementRendererDistributesFlexibleColumnWidths()
{
    TableElement table;
    table.id = QStringLiteral("itemsTable");
    table.width = 80.0;
    table.height = 20.0;
    table.dataPath = QStringLiteral("items");

    TableColumn fixedLeft;
    fixedLeft.id = QStringLiteral("fixedLeft");
    fixedLeft.title = QStringLiteral("Fixed");
    fixedLeft.fieldKey = QStringLiteral("fixedLeft");
    fixedLeft.widthMm = 20.0;
    table.columns.append(fixedLeft);

    TableColumn flexOne;
    flexOne.id = QStringLiteral("flexOne");
    flexOne.title = QStringLiteral("Flex 1");
    flexOne.fieldKey = QStringLiteral("flexOne");
    flexOne.widthMode = TableColumnWidthMode::Flex;
    flexOne.flexWeight = 1.0;
    table.columns.append(flexOne);

    TableColumn flexTwo;
    flexTwo.id = QStringLiteral("flexTwo");
    flexTwo.title = QStringLiteral("Flex 2");
    flexTwo.fieldKey = QStringLiteral("flexTwo");
    flexTwo.widthMode = TableColumnWidthMode::Flex;
    flexTwo.flexWeight = 2.0;
    table.columns.append(flexTwo);

    TemplateRenderContext context;
    context.values["items"] = QJsonArray{
        QJsonObject{
            {QStringLiteral("fixedLeft"), QStringLiteral("A")},
            {QStringLiteral("flexOne"), QStringLiteral("B")},
            {QStringLiteral("flexTwo"), QStringLiteral("C")},
        },
    };

    const auto result = TableElementRenderer::renderSinglePage(table, context);

    QVERIFY(result.success());
    QCOMPARE(result.commands[0].width, 20.0);
    QCOMPARE(result.commands[2].x, 20.0);
    QCOMPARE(result.commands[2].width, 20.0);
    QCOMPARE(result.commands[4].x, 40.0);
    QCOMPARE(result.commands[4].width, 40.0);
    QCOMPARE(result.commands[6].width, 20.0);
    QCOMPARE(result.commands[8].width, 20.0);
    QCOMPARE(result.commands[10].width, 40.0);
}

void CoreTests::tableElementRendererRejectsNonArrayData()
{
    TableElement table;
    table.id = QStringLiteral("itemsTable");
    table.dataPath = QStringLiteral("items");

    TableColumn column;
    column.id = QStringLiteral("name");
    column.title = QString::fromUtf8("品名");
    column.fieldKey = QStringLiteral("productName");
    table.columns.append(column);

    TemplateRenderContext context;
    context.values["items"] = QJsonObject{{QStringLiteral("productName"), QString::fromUtf8("足金戒指")}};

    const auto result = TableElementRenderer::renderSinglePage(table, context);
    QVERIFY(!result.success());
    QVERIFY(result.errorMessage.contains(QStringLiteral("items")));
    QVERIFY(result.errorMessage.contains(QString::fromUtf8("数组")));
}

void CoreTests::tableElementRendererResolvesNestedDataPathsAndFields()
{
    TableElement table;
    table.id = QStringLiteral("itemsTable");
    table.dataPath = QStringLiteral("order.items");
    table.headerRowHeightMm = 5.0;
    table.detailRowHeightMm = 5.0;

    TableColumn nameColumn;
    nameColumn.id = QStringLiteral("name");
    nameColumn.title = QStringLiteral("Name");
    nameColumn.fieldKey = QStringLiteral("product.name");
    nameColumn.widthMm = 25.0;
    table.columns.append(nameColumn);

    TableColumn quantityColumn;
    quantityColumn.id = QStringLiteral("quantity");
    quantityColumn.title = QStringLiteral("Qty");
    quantityColumn.fieldKey = QStringLiteral("quantity");
    quantityColumn.widthMm = 15.0;
    table.columns.append(quantityColumn);

    TemplateRenderContext context;
    context.values["order"] = QJsonObject{
        {QStringLiteral("items"),
         QJsonArray{
             QJsonObject{
                 {QStringLiteral("product"), QJsonObject{{QStringLiteral("name"), QStringLiteral("Ring")}}},
                 {QStringLiteral("quantity"), 2},
             },
         }},
    };

    const auto result = TableElementRenderer::renderSinglePage(table, context);
    QVERIFY(result.success());
    QCOMPARE(result.pages.size(), 1);
    QCOMPARE(result.pages.first().rowCount, 1);
    QCOMPARE(result.commands[5].text, QStringLiteral("Ring"));
    QCOMPARE(result.commands[7].text, QStringLiteral("2"));
}

void CoreTests::tableElementRendererSplitsRowsAcrossPagesAndRepeatsHeader()
{
    TableElement table;
    table.id = QStringLiteral("itemsTable");
    table.x = 5.0;
    table.y = 8.0;
    table.width = 45.0;
    table.height = 15.0;
    table.dataPath = QStringLiteral("items");
    table.headerRowHeightMm = 5.0;
    table.detailRowHeightMm = 5.0;

    TableColumn nameColumn;
    nameColumn.id = QStringLiteral("name");
    nameColumn.title = QString::fromUtf8("品名");
    nameColumn.fieldKey = QStringLiteral("productName");
    nameColumn.widthMm = 45.0;
    table.columns.append(nameColumn);

    TemplateRenderContext context;
    context.values["items"] = QJsonArray{
        QJsonObject{{QStringLiteral("productName"), QString::fromUtf8("产品1")}},
        QJsonObject{{QStringLiteral("productName"), QString::fromUtf8("产品2")}},
        QJsonObject{{QStringLiteral("productName"), QString::fromUtf8("产品3")}},
        QJsonObject{{QStringLiteral("productName"), QString::fromUtf8("产品4")}},
        QJsonObject{{QStringLiteral("productName"), QString::fromUtf8("产品5")}},
    };

    const auto result = TableElementRenderer::renderPages(table, context);
    QVERIFY(result.success());
    QCOMPARE(result.pages.size(), 3);
    QCOMPARE(result.pages[0].firstRowIndex, 0);
    QCOMPARE(result.pages[0].rowCount, 2);
    QCOMPARE(result.pages[1].firstRowIndex, 2);
    QCOMPARE(result.pages[1].rowCount, 2);
    QCOMPARE(result.pages[2].firstRowIndex, 4);
    QCOMPARE(result.pages[2].rowCount, 1);
    QCOMPARE(result.pages[1].commands[1].text, QString::fromUtf8("品名"));
    QCOMPARE(result.pages[1].commands[3].text, QString::fromUtf8("产品3"));
    QCOMPARE(result.pages[2].commands[3].text, QString::fromUtf8("产品5"));
}

void CoreTests::tableElementRendererRejectsPageTooShortForOneRow()
{
    TableElement table;
    table.id = QStringLiteral("itemsTable");
    table.height = 5.0;
    table.dataPath = QStringLiteral("items");
    table.headerRowHeightMm = 5.0;
    table.detailRowHeightMm = 5.0;

    TableColumn column;
    column.id = QStringLiteral("name");
    column.title = QString::fromUtf8("品名");
    column.fieldKey = QStringLiteral("productName");
    table.columns.append(column);

    TemplateRenderContext context;
    context.values["items"] = QJsonArray{QJsonObject{{QStringLiteral("productName"), QString::fromUtf8("产品1")}}};

    const auto result = TableElementRenderer::renderPages(table, context);
    QVERIFY(!result.success());
    QVERIFY(result.errorMessage.contains(QString::fromUtf8("单行")));
}

void CoreTests::templateDocumentJsonPersistsLayerTables()
{
    TemplateElement title;
    title.id = QStringLiteral("title");
    title.layerId = QStringLiteral("main");
    title.text = QString::fromUtf8("销售明细");

    TableColumn nameColumn;
    nameColumn.id = QStringLiteral("name");
    nameColumn.title = QString::fromUtf8("品名");
    nameColumn.fieldKey = QStringLiteral("productName");
    nameColumn.widthMm = 45.0;

    TableElement table;
    table.id = QStringLiteral("saleItems");
    table.layerId = QStringLiteral("main");
    table.displayName = QString::fromUtf8("销售明细表");
    table.x = 5.0;
    table.y = 12.0;
    table.width = 70.0;
    table.height = 30.0;
    table.dataPath = QStringLiteral("items");
    table.columns.append(nameColumn);

    TemplateLayer layer;
    layer.id = QStringLiteral("main");
    layer.name = QString::fromUtf8("主图层");
    layer.elements.append(title);
    layer.tables.append(table);

    TemplateVersion version;
    version.id = QStringLiteral("v1");
    version.name = QString::fromUtf8("初版");
    version.layersSnapshot.append(layer);

    TemplateDocument document;
    document.schemaVersion = 1;
    document.id = QStringLiteral("sale-order");
    document.name = QString::fromUtf8("销售单");
    document.templateKey = QStringLiteral("saleOrder");
    document.category = QStringLiteral("document");
    document.paperSpecId = QStringLiteral("a4");
    document.layers.append(layer);
    document.versions.append(version);

    const auto json = TemplateDocumentJson::toJson(document);
    const auto tables = json["layers"].toArray().first().toObject()["tables"].toArray();
    QCOMPARE(tables.size(), 1);
    QCOMPARE(tables.first().toObject()["dataPath"].toString(), QString("items"));

    const auto actual = TemplateDocumentJson::fromJson(json);
    QCOMPARE(actual.layers.first().tables.size(), 1);
    QCOMPARE(actual.layers.first().tables.first().displayName, QString::fromUtf8("销售明细表"));
    QCOMPARE(actual.layers.first().tables.first().columns.first().title, QString::fromUtf8("品名"));
    QCOMPARE(actual.versions.first().layersSnapshot.first().tables.size(), 1);
}

void CoreTests::templateDocumentImportRejectsInvalidLayerTable()
{
    TemplateElement title;
    title.id = QStringLiteral("title");
    title.layerId = QStringLiteral("main");

    TemplateLayer layer;
    layer.id = QStringLiteral("main");
    layer.elements.append(title);

    TemplateDocument document;
    document.schemaVersion = 1;
    document.id = QStringLiteral("sale-order");
    document.templateKey = QStringLiteral("saleOrder");
    document.category = QStringLiteral("document");
    document.paperSpecId = QStringLiteral("a4");
    document.layers.append(layer);

    QJsonObject invalidTable;
    invalidTable["id"] = QStringLiteral("saleItems");
    invalidTable["layerId"] = QStringLiteral("main");
    invalidTable["dataPath"] = QStringLiteral("items");
    invalidTable["width"] = 80.0;
    invalidTable["height"] = 30.0;
    invalidTable["columns"] = QJsonArray{};

    auto json = TemplateDocumentJson::toJson(document);
    auto layers = json["layers"].toArray();
    auto layerJson = layers.first().toObject();
    layerJson["tables"] = QJsonArray{invalidTable};
    layers.replace(0, layerJson);
    json["layers"] = layers;

    QString errorMessage;
    QVERIFY(!TemplateDocumentJson::validateForImport(json, &errorMessage));
    QVERIFY(errorMessage.contains(QString::fromUtf8("表格")));
    QVERIFY(errorMessage.contains(QString::fromUtf8("列")));
}

void CoreTests::templateDocumentRendererAppendsTableCommands()
{
    TemplateElement title;
    title.id = QStringLiteral("title");
    title.layerId = QStringLiteral("main");
    title.text = QString::fromUtf8("销售单");
    title.x = 1.0;
    title.y = 2.0;
    title.width = 20.0;
    title.height = 4.0;

    TableColumn nameColumn;
    nameColumn.id = QStringLiteral("name");
    nameColumn.title = QString::fromUtf8("品名");
    nameColumn.fieldKey = QStringLiteral("productName");
    nameColumn.widthMm = 45.0;

    TableElement table;
    table.id = QStringLiteral("saleItems");
    table.layerId = QStringLiteral("main");
    table.zIndex = 1;
    table.x = 5.0;
    table.y = 8.0;
    table.width = 60.0;
    table.height = 18.0;
    table.dataPath = QStringLiteral("items");
    table.headerRowHeightMm = 5.0;
    table.detailRowHeightMm = 5.0;
    table.columns.append(nameColumn);

    TemplateLayer layer;
    layer.id = QStringLiteral("main");
    layer.elements.append(title);
    layer.tables.append(table);

    TemplateDocument document;
    document.layers.append(layer);

    TemplateRenderContext context;
    context.values["items"] = QJsonArray{
        QJsonObject{{QStringLiteral("productName"), QString::fromUtf8("足金戒指")}},
    };

    const auto labelPlan = LabelRenderPlanner().createPlan(sleekpr::infrastructure::PreviewLabelFactory::createDemoLabel());
    const auto drawingPlan = TemplateDocumentRenderer().render(document, labelPlan, LabelOffset{}, DeviceProfile{}, context);

    QCOMPARE(drawingPlan.commands.size(), 5);
    QCOMPARE(drawingPlan.commands[0].elementKey, QString("title"));
    QCOMPARE(drawingPlan.commands[1].elementKey, QString("saleItems.header.name.border"));
    QCOMPARE(drawingPlan.commands[2].text, QString::fromUtf8("品名"));
    QCOMPARE(drawingPlan.commands[4].text, QString::fromUtf8("足金戒指"));
}

void CoreTests::templateDocumentRendererInterpolatesFixedTextPlaceholders()
{
    TemplateElement summaryText;
    summaryText.id = QStringLiteral("summaryText");
    summaryText.type = TemplateElementType::FixedText;
    summaryText.width = 60.0;
    summaryText.height = 5.0;
    summaryText.text = QString::fromUtf8("商品：${productName}  重量：${weight}  缺失：${missingValue}");

    TemplateLayer layer;
    layer.id = QStringLiteral("base");
    layer.elements = {summaryText};

    TemplateDocument document;
    document.id = QStringLiteral("placeholder-template");
    document.layers = {layer};

    TemplateRenderContext context;
    context.values["productName"] = QString::fromUtf8("足金戒指");
    context.values["weight"] = 12.5;

    const auto labelPlan = LabelRenderPlanner().createPlan(LabelItem{});
    const auto drawingPlan = TemplateDocumentRenderer().render(document, labelPlan, LabelOffset{}, DeviceProfile{}, context);

    QCOMPARE(drawingPlan.commands.size(), 1);
    QCOMPARE(drawingPlan.commands.first().text, QString::fromUtf8("商品：足金戒指  重量：12.5  缺失："));
}

void CoreTests::templateDocumentRendererWrapsFixedTextWithinElementWidth()
{
    TemplateElement fixedText;
    fixedText.id = QStringLiteral("longAddress");
    fixedText.type = TemplateElementType::FixedText;
    fixedText.x = 2.0;
    fixedText.y = 3.0;
    fixedText.width = 12.0;
    fixedText.height = 6.0;
    fixedText.text = QString::fromUtf8("地址:${address}");
    fixedText.fontSizePt = 8.0;

    TemplateLayer layer;
    layer.id = QStringLiteral("base");
    layer.elements = {fixedText};

    TemplateDocument document;
    document.layers = {layer};

    TemplateRenderContext context;
    context.values.insert(QStringLiteral("address"), QString::fromUtf8("民族工匠一层111号"));

    const auto drawingPlan = TemplateDocumentRenderer().render(document, LabelRenderPlan{}, LabelOffset{}, DeviceProfile{}, context);

    QCOMPARE(drawingPlan.commands.size(), 1);
    QCOMPARE(drawingPlan.commands.first().type, NativeDrawCommandType::Text);
    QCOMPARE(drawingPlan.commands.first().width, 12.0);
    QCOMPARE(drawingPlan.commands.first().height, 6.0);
    QVERIFY(drawingPlan.commands.first().wrapText);
}

void CoreTests::templateDocumentRendererRendersArrayGridElements()
{
    TemplateElement grid;
    grid.id = QStringLiteral("headerGrid");
    grid.type = TemplateElementType::ArrayGrid;
    grid.x = 2.0;
    grid.y = 3.0;
    grid.width = 60.0;
    grid.height = 20.0;
    grid.fontSizePt = 6.0;
    grid.dataPath = QStringLiteral("header_items");
    grid.arrayGridRows = 2;
    grid.arrayGridColumns = 3;
    grid.arrayGridCellTemplate = QStringLiteral("${text}:${value}");
    grid.arrayGridDrawBorders = true;

    TemplateLayer layer;
    layer.id = QStringLiteral("base");
    layer.elements = {grid};

    TemplateDocument document;
    document.layers = {layer};

    TemplateRenderContext context;
    context.values["header_items"] = QJsonArray{
        QJsonObject{{QStringLiteral("text"), QString::fromUtf8("素金KA")}, {QStringLiteral("value"), QStringLiteral("2.99")}},
        QJsonObject{{QStringLiteral("text"), QString::fromUtf8("素金PC")}, {QStringLiteral("value"), QStringLiteral("4.13")}},
        QJsonObject{{QStringLiteral("text"), QString::fromUtf8("素金PA")}, {QStringLiteral("value"), QStringLiteral("3.91")}},
        QJsonObject{{QStringLiteral("text"), QString::fromUtf8("素金PD")}, {QStringLiteral("value"), QStringLiteral("11.49")}},
    };

    const auto labelPlan = LabelRenderPlanner().createPlan(LabelItem{});
    const auto drawingPlan = TemplateDocumentRenderer().render(document, labelPlan, LabelOffset{}, DeviceProfile{}, context);

    QCOMPARE(drawingPlan.commands.size(), 10);
    QCOMPARE(drawingPlan.commands[0].type, NativeDrawCommandType::Rectangle);
    QCOMPARE(drawingPlan.commands[0].elementKey, QString("headerGrid.cell0.border"));
    QCOMPARE(drawingPlan.commands[1].type, NativeDrawCommandType::Text);
    QCOMPARE(drawingPlan.commands[1].text, QString::fromUtf8("素金KA:2.99"));
    QCOMPARE(drawingPlan.commands[1].x, 2.6);
    QCOMPARE(drawingPlan.commands[1].y, 3.4);
    QCOMPARE(drawingPlan.commands[1].width, 18.8);
    QCOMPARE(drawingPlan.commands[1].height, 9.2);
    QCOMPARE(drawingPlan.commands[2].elementKey, QString("headerGrid.cell1.border"));
    QCOMPARE(drawingPlan.commands[3].text, QString::fromUtf8("素金PC:4.13"));
    QCOMPARE(drawingPlan.commands[7].text, QString::fromUtf8("素金PD:11.49"));
    QCOMPARE(drawingPlan.commands[8].elementKey, QString("headerGrid.cell4.border"));
    QCOMPARE(drawingPlan.commands[9].elementKey, QString("headerGrid.cell5.border"));
}

void CoreTests::templateDocumentRendererUsesArrayGridRowHeight()
{
    TemplateElement grid;
    grid.id = QStringLiteral("headerGrid");
    grid.type = TemplateElementType::ArrayGrid;
    grid.x = 2.0;
    grid.y = 3.0;
    grid.width = 30.0;
    grid.height = 6.0;
    grid.fontSizePt = 6.0;
    grid.dataPath = QStringLiteral("header_items");
    grid.arrayGridRows = 3;
    grid.arrayGridColumns = 1;
    grid.arrayGridRowHeightMm = 1.6;
    grid.arrayGridCellTemplate = QStringLiteral("${text}:${value}");
    grid.arrayGridDrawBorders = true;

    TemplateLayer layer;
    layer.id = QStringLiteral("base");
    layer.elements = {grid};

    TemplateDocument document;
    document.layers = {layer};

    TemplateRenderContext context;
    context.values["header_items"] = QJsonArray{
        QJsonObject{{QStringLiteral("text"), QString::fromUtf8("素金KA")}, {QStringLiteral("value"), QStringLiteral("2.99")}},
        QJsonObject{{QStringLiteral("text"), QString::fromUtf8("素金PC")}, {QStringLiteral("value"), QStringLiteral("4.13")}},
        QJsonObject{{QStringLiteral("text"), QString::fromUtf8("素金PA")}, {QStringLiteral("value"), QStringLiteral("3.91")}},
    };

    const auto drawingPlan = TemplateDocumentRenderer().render(document, LabelRenderPlan{}, LabelOffset{}, DeviceProfile{}, context);

    QCOMPARE(drawingPlan.commands.size(), 6);
    QCOMPARE(drawingPlan.commands[0].height, 2.0);
    QCOMPARE(drawingPlan.commands[1].height, 1.2);
    QCOMPARE(drawingPlan.commands[2].y, 4.6);
    QCOMPARE(drawingPlan.commands[3].y, 5.0);
    QCOMPARE(drawingPlan.commands[4].y, 6.2);
    QCOMPARE(drawingPlan.commands[5].y, 6.6);
    QCOMPARE(drawingPlan.commands[2].y - drawingPlan.commands[0].y, 1.6);
    QCOMPARE(drawingPlan.commands[4].y - drawingPlan.commands[2].y, 1.6);
}

void CoreTests::templateDocumentRendererBuildsMultiPagePrintPlan()
{
    TemplateElement title;
    title.id = QStringLiteral("title");
    title.layerId = QStringLiteral("main");
    title.text = QString::fromUtf8("销售单");
    title.x = 1.0;
    title.y = 2.0;
    title.width = 20.0;
    title.height = 4.0;

    TableColumn nameColumn;
    nameColumn.id = QStringLiteral("name");
    nameColumn.title = QString::fromUtf8("品名");
    nameColumn.fieldKey = QStringLiteral("productName");
    nameColumn.widthMm = 45.0;

    TableElement table;
    table.id = QStringLiteral("saleItems");
    table.layerId = QStringLiteral("main");
    table.zIndex = 1;
    table.x = 5.0;
    table.y = 8.0;
    table.width = 45.0;
    table.height = 15.0;
    table.dataPath = QStringLiteral("items");
    table.headerRowHeightMm = 5.0;
    table.detailRowHeightMm = 5.0;
    table.columns.append(nameColumn);

    TemplateLayer layer;
    layer.id = QStringLiteral("main");
    layer.elements.append(title);
    layer.tables.append(table);

    TemplateDocument document;
    document.layers.append(layer);

    TemplateRenderContext context;
    context.values["items"] = QJsonArray{
        QJsonObject{{QStringLiteral("productName"), QString::fromUtf8("产品1")}},
        QJsonObject{{QStringLiteral("productName"), QString::fromUtf8("产品2")}},
        QJsonObject{{QStringLiteral("productName"), QString::fromUtf8("产品3")}},
    };

    const auto labelPlan = LabelRenderPlanner().createPlan(sleekpr::infrastructure::PreviewLabelFactory::createDemoLabel());
    const auto printPlan = TemplateDocumentRenderer().renderPrint(document, labelPlan, LabelOffset{}, DeviceProfile{}, context);

    QCOMPARE(printPlan.pages.size(), 2);
    QCOMPARE(printPlan.pages[0].commands[0].elementKey, QString("title"));
    QCOMPARE(printPlan.pages[0].commands[6].text, QString::fromUtf8("产品2"));
    QCOMPARE(printPlan.pages[1].commands[0].elementKey, QString("title"));
    QCOMPARE(printPlan.pages[1].commands[2].text, QString::fromUtf8("品名"));
    QCOMPARE(printPlan.pages[1].commands[4].text, QString::fromUtf8("产品3"));

    const auto firstPage = TemplateDocumentRenderer().render(document, labelPlan, LabelOffset{}, DeviceProfile{}, context);
    QCOMPARE(firstPage.commands.size(), printPlan.pages.first().commands.size());
    QCOMPARE(firstPage.commands.last().text, QString::fromUtf8("产品2"));
}

void CoreTests::templateDocumentJsonPersistsPaperMetadata()
{
    TemplateDocument document;
    document.schemaVersion = 1;
    document.id = QStringLiteral("sale-order");
    document.name = QString::fromUtf8("销售单");
    document.templateKey = QStringLiteral("saleOrder");
    document.category = QStringLiteral("document");
    document.paperSpecId = QStringLiteral("a4");

    TemplateLayer layer;
    layer.id = QStringLiteral("main");
    layer.name = QString::fromUtf8("主图层");
    TemplateElement element;
    element.id = QStringLiteral("title");
    element.layerId = layer.id;
    element.text = QString::fromUtf8("销售单");
    layer.elements.append(element);
    document.layers.append(layer);

    const auto json = TemplateDocumentJson::toJson(document);
    QCOMPARE(json["category"].toString(), QString("document"));
    QCOMPARE(json["paperSpecId"].toString(), QString("a4"));

    const auto actual = TemplateDocumentJson::fromJson(json);
    QCOMPARE(actual.category, QString("document"));
    QCOMPARE(actual.paperSpecId, QString("a4"));
}

void CoreTests::templateDocumentImportRequiresPaperSpecIdForGenericTemplates()
{
    TemplateDocument document;
    document.schemaVersion = 1;
    document.id = QStringLiteral("sale-order");
    document.name = QString::fromUtf8("销售单");
    document.templateKey = QStringLiteral("saleOrder");
    document.category = QStringLiteral("document");

    TemplateLayer layer;
    layer.id = QStringLiteral("main");
    TemplateElement element;
    element.id = QStringLiteral("title");
    element.layerId = layer.id;
    layer.elements.append(element);
    document.layers.append(layer);

    QString errorMessage;
    QVERIFY(!TemplateDocumentJson::validateForImport(TemplateDocumentJson::toJson(document), &errorMessage));
    QVERIFY(errorMessage.contains(QStringLiteral("paperSpecId")));
}

void CoreTests::templateDocumentJsonPersistsFieldSchema()
{
    TemplateDocument document;
    document.schemaVersion = 1;
    document.id = QStringLiteral("sale-order");
    document.name = QString::fromUtf8("销售单");
    document.templateKey = QStringLiteral("saleOrder");
    document.category = QStringLiteral("document");
    document.paperSpecId = QStringLiteral("a4");

    FieldDefinition customerName;
    customerName.key = QStringLiteral("customerName");
    customerName.displayName = QString::fromUtf8("客户名称");
    customerName.required = true;
    customerName.defaultValue = QString::fromUtf8("散客");
    document.fieldSchema.append(customerName);

    TemplateLayer layer;
    layer.id = QStringLiteral("main");
    TemplateElement element;
    element.id = QStringLiteral("title");
    element.layerId = layer.id;
    layer.elements.append(element);
    document.layers.append(layer);

    const auto json = TemplateDocumentJson::toJson(document);
    const auto fields = json["fieldSchema"].toObject()["fields"].toArray();
    QCOMPARE(fields.size(), 1);
    QCOMPARE(fields.first().toObject()["key"].toString(), QString("customerName"));

    const auto actual = TemplateDocumentJson::fromJson(json);
    QCOMPARE(actual.fieldSchema.size(), 1);
    QCOMPARE(actual.fieldSchema.first().displayName, QString::fromUtf8("客户名称"));
    QVERIFY(actual.fieldSchema.first().required);
}

void CoreTests::templateDocumentImportRejectsInvalidFieldSchema()
{
    TemplateDocument document;
    document.schemaVersion = 1;
    document.id = QStringLiteral("sale-order");
    document.name = QString::fromUtf8("销售单");
    document.templateKey = QStringLiteral("saleOrder");
    document.category = QStringLiteral("document");
    document.paperSpecId = QStringLiteral("a4");

    TemplateLayer layer;
    layer.id = QStringLiteral("main");
    TemplateElement element;
    element.id = QStringLiteral("title");
    element.layerId = layer.id;
    layer.elements.append(element);
    document.layers.append(layer);

    QJsonObject invalidField;
    invalidField["key"] = QStringLiteral(" ");
    invalidField["type"] = QStringLiteral("text");
    invalidField["source"] = QStringLiteral("custom");

    auto json = TemplateDocumentJson::toJson(document);
    json["fieldSchema"] = QJsonObject{{QStringLiteral("fields"), QJsonArray{invalidField}}};

    QString errorMessage;
    QVERIFY(!TemplateDocumentJson::validateForImport(json, &errorMessage));
    QVERIFY(errorMessage.contains(QString::fromUtf8("字段")));
}

void CoreTests::templateDocumentValidatorReportsDesignErrors()
{
    FieldDefinition knownField;
    knownField.key = QStringLiteral("knownField");
    knownField.displayName = QString::fromUtf8("已知字段");

    TemplateElement missingFieldText;
    missingFieldText.id = QStringLiteral("missingFieldText");
    missingFieldText.type = TemplateElementType::BoundField;
    missingFieldText.fieldKey = QStringLiteral("unknownField");
    missingFieldText.x = 75.0;
    missingFieldText.y = 2.0;
    missingFieldText.width = 10.0;
    missingFieldText.height = 5.0;

    TableColumn oversizedColumn;
    oversizedColumn.id = QStringLiteral("name");
    oversizedColumn.title = QString::fromUtf8("名称");
    oversizedColumn.fieldKey = QStringLiteral("name");
    oversizedColumn.widthMm = 60.0;

    TableElement table;
    table.id = QStringLiteral("saleItems");
    table.layerId = QStringLiteral("base");
    table.x = 1.0;
    table.y = 10.0;
    table.width = 50.0;
    table.height = 12.0;
    table.dataPath = QStringLiteral("items");
    table.columns = {oversizedColumn};

    TemplateLayer layer;
    layer.id = QStringLiteral("base");
    layer.elements = {missingFieldText};
    layer.tables = {table};

    TemplateDocument document;
    document.id = QStringLiteral("template-validation");
    document.fieldSchema = {knownField};
    document.layers = {layer};

    const auto result = TemplateDocumentValidator().validate(document, QSizeF(80.0, 30.0));

    QVERIFY(result.hasErrors());
    QVERIFY(result.containsCode(QStringLiteral("UNKNOWN_FIELD")));
    QVERIFY(result.containsCode(QStringLiteral("ELEMENT_OUT_OF_PAPER")));
    QVERIFY(result.containsCode(QStringLiteral("TABLE_COLUMNS_TOO_WIDE")));
}

void CoreTests::templateDocumentValidatorAllowsFlexibleColumnsWithinTableWidth()
{
    TableColumn fixedColumn;
    fixedColumn.id = QStringLiteral("fixed");
    fixedColumn.title = QStringLiteral("Fixed");
    fixedColumn.fieldKey = QStringLiteral("fixed");
    fixedColumn.widthMm = 40.0;

    TableColumn flexColumn;
    flexColumn.id = QStringLiteral("flex");
    flexColumn.title = QStringLiteral("Flex");
    flexColumn.fieldKey = QStringLiteral("flex");
    flexColumn.widthMm = 20.0;
    flexColumn.widthMode = TableColumnWidthMode::Flex;
    flexColumn.flexWeight = 1.0;

    TableElement table;
    table.id = QStringLiteral("saleItems");
    table.layerId = QStringLiteral("base");
    table.width = 50.0;
    table.height = 12.0;
    table.dataPath = QStringLiteral("items");
    table.columns = {fixedColumn, flexColumn};

    TemplateLayer layer;
    layer.id = QStringLiteral("base");
    layer.tables = {table};

    TemplateDocument document;
    document.id = QStringLiteral("template-flex-table");
    document.layers = {layer};

    const auto result = TemplateDocumentValidator().validate(document, QSizeF(80.0, 30.0));

    QVERIFY(!result.containsCode(QStringLiteral("TABLE_COLUMNS_TOO_WIDE")));
}

void CoreTests::templateDocumentValidatorAllowsLegacyFieldsAndReportsQrCapacity()
{
    TemplateElement legacyText;
    legacyText.id = QStringLiteral("productNameText");
    legacyText.type = TemplateElementType::BoundField;
    legacyText.fieldKey = QStringLiteral("productName");
    legacyText.width = 20.0;
    legacyText.height = 5.0;

    TemplateElement qrCode;
    qrCode.id = QStringLiteral("validLongQr");
    qrCode.type = TemplateElementType::QrCode;
    qrCode.width = 8.0;
    qrCode.height = 8.0;
    qrCode.payload = QStringLiteral("1234567890123456789012345678901234567890123456");

    TemplateElement tooLongQrCode;
    tooLongQrCode.id = QStringLiteral("tooLongQr");
    tooLongQrCode.type = TemplateElementType::QrCode;
    tooLongQrCode.y = 10.0;
    tooLongQrCode.width = 8.0;
    tooLongQrCode.height = 8.0;
    tooLongQrCode.payload = QStringLiteral("12345678901234567890123456789012345678901234567");

    TemplateLayer layer;
    layer.id = QStringLiteral("base");
    layer.elements = {legacyText, qrCode, tooLongQrCode};

    TemplateDocument document;
    document.id = QStringLiteral("template-qr");
    document.layers = {layer};

    const auto result = TemplateDocumentValidator().validate(document, QSizeF(80.0, 30.0));

    QVERIFY(result.hasErrors());
    QVERIFY(result.containsCode(QStringLiteral("QR_PAYLOAD_TOO_LONG")));
    QVERIFY(!result.containsCode(QStringLiteral("UNKNOWN_FIELD")));
}

void CoreTests::templateLibraryStoreSavesListsAndLoadsTemplates()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    TemplateDocument document;
    document.schemaVersion = 1;
    document.id = QStringLiteral("sale-order");
    document.name = QString::fromUtf8("销售单");
    document.templateKey = QStringLiteral("saleOrder");
    document.category = QStringLiteral("document");
    document.paperSpecId = QStringLiteral("a4");

    TemplateLayer layer;
    layer.id = QStringLiteral("main");
    TemplateElement element;
    element.id = QStringLiteral("title");
    element.layerId = layer.id;
    layer.elements.append(element);
    document.layers.append(layer);

    TemplateLibraryStore store(dir.path());
    QVERIFY(store.saveTemplate(document));

    const auto ids = store.templateIds();
    QCOMPARE(ids, QStringList{QStringLiteral("sale-order")});

    const auto loaded = store.loadTemplate(QStringLiteral("sale-order"));
    QVERIFY(loaded.has_value());
    QCOMPARE(loaded->id, QString("sale-order"));
    QCOMPARE(loaded->paperSpecId, QString("a4"));
}

void CoreTests::templateLibraryStoreRemovesSavedTemplates()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    TemplateDocument document;
    document.schemaVersion = 1;
    document.id = QStringLiteral("sale-order");
    document.name = QString::fromUtf8("销售单");
    document.templateKey = QStringLiteral("saleOrder");
    document.category = QStringLiteral("document");
    document.paperSpecId = QStringLiteral("a4");

    TemplateLayer layer;
    layer.id = QStringLiteral("main");
    TemplateElement element;
    element.id = QStringLiteral("title");
    element.layerId = layer.id;
    layer.elements.append(element);
    document.layers.append(layer);

    TemplateLibraryStore store(dir.path());
    QVERIFY(store.saveTemplate(document));
    QVERIFY(store.removeTemplate(QStringLiteral("sale-order")));

    QVERIFY(store.templateIds().isEmpty());
    QVERIFY(!store.loadTemplate(QStringLiteral("sale-order")).has_value());
}

void CoreTests::templateLibraryStoreRejectsInvalidTemplateImport()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    QFile file(QDir(dir.path()).filePath(QStringLiteral("broken.sleekpr-template.json")));
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
    file.write(R"json({"schemaVersion":1,"id":"broken","templateKey":"broken","layers":[]})json");
    file.close();

    QString errorMessage;
    TemplateLibraryStore store(dir.path());
    QVERIFY(!store.loadTemplate(QStringLiteral("broken"), &errorMessage).has_value());
    QVERIFY(errorMessage.contains(QString::fromUtf8("图层")));
}

void CoreTests::allowedOriginParserNormalizesValidOrigins()
{
    const auto origins = AllowedOriginParser::parse(
        "https://www.zy-taoxiaoti.com/\n"
        "http://114.132.160.27\n"
        "https://www.zy-taoxiaoti.com\n"
        "ftp://bad.example.com\n"
        "https://bad.example.com/path\n");

    QCOMPARE(origins.size(), 2);
    QCOMPARE(origins[0], QString("https://www.zy-taoxiaoti.com"));
    QCOMPARE(origins[1], QString("http://114.132.160.27"));
}

void CoreTests::corsAndPrivateNetworkPoliciesKeepLocalContract()
{
    const QStringList configured{"https://manager.example.com"};

    QVERIFY(LocalCorsPolicy::isAllowedOrigin("http://localhost", configured));
    QVERIFY(LocalCorsPolicy::isAllowedOrigin("http://127.0.0.1:5173", configured));
    QVERIFY(LocalCorsPolicy::isAllowedOrigin("http://[::1]:5173", configured));
    QVERIFY(LocalCorsPolicy::isAllowedOrigin("http://114.132.160.27", configured));
    QVERIFY(LocalCorsPolicy::isAllowedOrigin("https://manager.example.com", configured));
    QVERIFY(!LocalCorsPolicy::isAllowedOrigin("http://example.com", configured));
    QVERIFY(!LocalCorsPolicy::isAllowedOrigin("ftp://localhost", configured));

    QVERIFY(PrivateNetworkAccessPolicy::shouldAllow(
        "true",
        "https://manager.example.com",
        configured));
    QVERIFY(!PrivateNetworkAccessPolicy::shouldAllow(
        "false",
        "https://manager.example.com",
        configured));
}

void CoreTests::printerSelectionUsesConfiguredDefaultOnly()
{
    PrintClientSettings settings;
    settings.defaultPrinter = "Configured Printer";

    PrinterSelectionResolver resolver;
    QCOMPARE(resolver.resolve(settings, "Ignored Request Printer"), QString("Configured Printer"));
    QCOMPARE(resolver.resolve(PrintClientSettings{}, "Ignored Request Printer"), QString());
}

void CoreTests::templateElementCatalogReturnsEditableDefaults()
{
    const auto labelPlan = LabelRenderPlanner().createPlan(sleekpr::infrastructure::PreviewLabelFactory::createDemoLabel());
    const auto drawingPlan = NativeLabelDrawingPlanner().createPlan(labelPlan);
    const auto elements = TemplateElementCatalog::fromDrawingPlan(drawingPlan);

    QVERIFY(elements.size() >= 10);
    QCOMPARE(elements.first().key, QString("identifier"));
    QCOMPARE(elements.first().displayName, QString::fromUtf8("编号"));

    const auto qualityMark = TemplateElementCatalog::find(elements, "qualityMark");
    QVERIFY(qualityMark.has_value());
    QCOMPARE(qualityMark->displayName, QString::fromUtf8("合格证"));
    QCOMPARE(qualityMark->defaultValue.x.value(), drawingPlan.commands[1].x);
    QCOMPARE(qualityMark->defaultValue.fontSizePt.value(), drawingPlan.commands[1].fontSizePt);
}

void CoreTests::settingsEditModelResetsElementOverridesOnly()
{
    PrintClientSettings settings;
    settings.defaultPrinter = "Printer A";
    settings.labelOffset = LabelOffset{1.0, 2.0};
    settings.templateOverrides["default"]["productName"] = TemplateElementOverride{10.0, 1.0, 5.0, true};
    settings.templateOverrides["default"]["qrCode"] = TemplateElementOverride{2.0, 1.0, std::nullopt, std::nullopt};

    SettingsEditModel::resetElement(settings, "default", "productName");
    QVERIFY(!settings.templateOverrides["default"].contains("productName"));
    QVERIFY(settings.templateOverrides["default"].contains("qrCode"));

    SettingsEditModel::resetAllElements(settings, "default");
    QCOMPARE(settings.defaultPrinter, QString("Printer A"));
    QCOMPARE(settings.labelOffset.x, 1.0);
    QCOMPARE(settings.labelOffset.y, 2.0);
    QVERIFY(settings.templateOverrides["default"].isEmpty());
}

void CoreTests::repeatedElementOverridesKeepRelativeLayout()
{
    const auto labelPlan = LabelRenderPlanner().createPlan(sleekpr::infrastructure::PreviewLabelFactory::createDemoLabel());

    QHash<QString, TemplateElementOverride> overrides;
    overrides["partRow"] = TemplateElementOverride{5.0, 17.0, std::nullopt, std::nullopt};

    const auto drawingPlan = NativeLabelDrawingPlanner().createPlan(labelPlan, LabelOffset{}, overrides);

    QList<NativeDrawCommand> partRows;
    for (const auto& command : drawingPlan.commands) {
        if (command.elementKey == "partRow") {
            partRows.append(command);
        }
    }

    QCOMPARE(partRows.size(), 3);
    QCOMPARE(partRows[0].x, 5.0);
    QCOMPARE(partRows[0].y, 17.0);
    QCOMPARE(partRows[1].x, 16.5);
    QCOMPARE(partRows[1].y, 17.0);
    QCOMPARE(partRows[2].x, 5.0);
    QCOMPARE(partRows[2].y, 19.05);
}

void CoreTests::previewLabelFactoryCreatesNativeDemoData()
{
    const auto item = sleekpr::infrastructure::PreviewLabelFactory::createDemoLabel();

    QCOMPARE(item.identifierCode, QString("1000035933"));
    QVERIFY(item.productName.contains(QString::fromUtf8("足银镶金串珠")));
    QCOMPARE(item.finishedProductParts.size(), 3);
    QCOMPARE(item.finishedProductParts.first().categoryName, QString::fromUtf8("镶嵌ZB"));
}

void CoreTests::labelPlannerMapsFieldsToRenderPlan()
{
    LabelItem item;
    item.identifierCode = "10000000789";
    item.productName = "Gold Ring";
    item.weightCategory = "Finished";
    item.finishedProductWeight = 3.2;
    item.roughWeight = 3.56;
    item.salesCode = "XS-001";
    item.goldPurity = "Au999";
    item.address = "Store 1";
    item.categoryName = "Ring";
    item.additionalRemark = "Remark";
    item.inlayWeight = 0.12;
    item.ropeWeight = 0.34;
    item.finishedProductNote = "Note";

    const auto plan = LabelRenderPlanner().createPlan(item);

    QCOMPARE(plan.paperSize.widthMm(), 80.0);
    QCOMPARE(plan.templateKey, LabelTemplateKey::Default80x30);
    QCOMPARE(plan.identifierText, QString("10000000789"));
    QCOMPARE(plan.productName, QString("Gold Ring"));
    QCOMPARE(plan.finishedWeightText, QString("Finished(g): 3.20"));
    QCOMPARE(plan.roughWeightText, QString::fromUtf8("总件重(g): 3.56"));
    QCOMPARE(plan.salesCode, QString("XS-001"));
    QCOMPARE(plan.standardText, QString::fromUtf8("执行标准 QB/T2062 GB11887  Au999"));
    QCOMPARE(plan.addressText, QString::fromUtf8("地址:Store 1"));
    QCOMPARE(plan.parts.size(), 1);
    QCOMPARE(plan.parts[0].categoryName, QString("Ring"));
    QCOMPARE(plan.footerText, QString::fromUtf8("附加:Remark 附加重:0.12g 绳重:0.34g Note"));
}

void CoreTests::nativeDrawingPlannerEmitsMillimeterCommandsAndOffsets()
{
    LabelItem item;
    item.identifierCode = "10000000789";
    item.productName = "Gold Ring";
    item.weightCategory = "Finished";
    item.finishedProductWeight = 3.2;
    item.roughWeight = 3.56;
    item.salesCode = "XS-001";
    item.goldPurity = "Au999";
    item.address = "Store 1";

    const auto labelPlan = LabelRenderPlanner().createPlan(item);
    const auto drawingPlan = NativeLabelDrawingPlanner().createPlan(labelPlan, LabelOffset{1.2, -0.5});

    QCOMPARE(drawingPlan.paperSize.widthMm(), 80.0);
    QVERIFY(drawingPlan.commands.size() >= 10);
    QCOMPARE(drawingPlan.commands[0].type, NativeDrawCommandType::Text);
    QCOMPARE(drawingPlan.commands[0].text, QString("10000000789"));
    QCOMPARE(drawingPlan.commands[0].x, 1.2);
    QCOMPARE(drawingPlan.commands[0].y, -0.5);
    QCOMPARE(drawingPlan.commands[1].type, NativeDrawCommandType::Text);
    QCOMPARE(drawingPlan.commands[1].text, QString::fromUtf8("合\n格\n证"));
    QCOMPARE(drawingPlan.commands[2].type, NativeDrawCommandType::QrCode);
    QCOMPARE(drawingPlan.commands[2].width, 7.8);
    QCOMPARE(drawingPlan.commands[2].x, 3.0);
    QCOMPARE(drawingPlan.commands[2].y, 0.5);
    QCOMPARE(drawingPlan.commands[3].type, NativeDrawCommandType::Text);
    QCOMPARE(drawingPlan.commands[3].text, QString::fromUtf8("标签约0.20g"));
    QVERIFY(drawingPlan.commands[3].y >= drawingPlan.commands[2].y + drawingPlan.commands[2].height);
    QCOMPARE(drawingPlan.commands.last().rotationDegrees, 90.0);
}

void CoreTests::nativeDrawingPlannerAppendsTemplateElements()
{
    LabelItem item;
    item.identifierCode = "10000000789";
    item.productName = "Gold Ring";
    item.weightCategory = "Finished";
    item.finishedProductWeight = 3.2;
    item.roughWeight = 3.56;
    item.salesCode = "XS-001";
    item.goldPurity = "Au999";
    item.address = "Store 1";

    TemplateElement fixedText;
    fixedText.id = "custom_fixed";
    fixedText.type = TemplateElementType::FixedText;
    fixedText.x = 3.0;
    fixedText.y = 2.0;
    fixedText.width = 10.0;
    fixedText.height = 2.0;
    fixedText.text = "HELLO";
    fixedText.fontSizePt = 5.0;
    fixedText.bold = true;
    fixedText.zIndex = 2;
    fixedText.rotationDegrees = 90.0;
    fixedText.maxLines = 1;
    fixedText.ellipsis = true;

    TemplateElement boundField;
    boundField.id = "custom_bound";
    boundField.type = TemplateElementType::BoundField;
    boundField.x = 15.0;
    boundField.y = 2.0;
    boundField.width = 18.0;
    boundField.height = 3.0;
    boundField.fieldKey = "productName";
    boundField.fontSizePt = 4.0;
    boundField.zIndex = 1;

    TemplateElement qrCode;
    qrCode.id = "custom_qr";
    qrCode.type = TemplateElementType::QrCode;
    qrCode.x = 35.0;
    qrCode.y = 2.0;
    qrCode.width = 6.0;
    qrCode.height = 6.0;
    qrCode.fieldKey = "identifierText";
    qrCode.zIndex = 0;

    TemplateElement rectangle;
    rectangle.id = "custom_rect";
    rectangle.type = TemplateElementType::Rectangle;
    rectangle.x = 45.0;
    rectangle.y = 4.0;
    rectangle.width = 12.0;
    rectangle.height = 5.0;
    rectangle.visible = false;

    const auto labelPlan = LabelRenderPlanner().createPlan(item);
    const QList<TemplateElement> customElements{fixedText, boundField, qrCode, rectangle};
    const auto drawingPlan = NativeLabelDrawingPlanner().createPlan(
        labelPlan,
        LabelOffset{1.0, 2.0},
        {},
        customElements);

    auto findCommand = [&drawingPlan](const QString& key) -> std::optional<NativeDrawCommand> {
        for (const auto& command : drawingPlan.commands) {
            if (command.elementKey == key) {
                return command;
            }
        }
        return std::nullopt;
    };

    const auto fixedCommand = findCommand("custom_fixed");
    QVERIFY(fixedCommand.has_value());
    QCOMPARE(fixedCommand->type, NativeDrawCommandType::Text);
    QCOMPARE(fixedCommand->x, 4.0);
    QCOMPARE(fixedCommand->y, 4.0);
    QCOMPARE(fixedCommand->text, QString("HELLO"));
    QCOMPARE(fixedCommand->fontSizePt, 5.0);
    QCOMPARE(fixedCommand->bold, true);
    QCOMPARE(fixedCommand->rotationDegrees, 90.0);
    QCOMPARE(fixedCommand->maxLines, 1);
    QCOMPARE(fixedCommand->ellipsis, true);

    const auto boundCommand = findCommand("custom_bound");
    QVERIFY(boundCommand.has_value());
    QCOMPARE(boundCommand->type, NativeDrawCommandType::Text);
    QCOMPARE(boundCommand->text, QString("Gold Ring"));

    const auto qrCommand = findCommand("custom_qr");
    QVERIFY(qrCommand.has_value());
    QCOMPARE(qrCommand->type, NativeDrawCommandType::QrCode);
    QCOMPARE(qrCommand->text, QString("10000000789"));
    QCOMPARE(qrCommand->width, 6.0);

    const auto rectangleCommand = findCommand("custom_rect");
    QVERIFY(!rectangleCommand.has_value());

    QCOMPARE(drawingPlan.commands[drawingPlan.commands.size() - 3].elementKey, QString("custom_qr"));
    QCOMPARE(drawingPlan.commands[drawingPlan.commands.size() - 2].elementKey, QString("custom_bound"));
    QCOMPARE(drawingPlan.commands[drawingPlan.commands.size() - 1].elementKey, QString("custom_fixed"));
}

void CoreTests::templateDocumentRendererMarksFixedAndBoundTextForAutoFit()
{
    TemplateElement fixedText;
    fixedText.id = QStringLiteral("fixed-title");
    fixedText.type = TemplateElementType::FixedText;
    fixedText.text = QString::fromUtf8("门店：${storeName}");
    fixedText.width = 22.0;
    fixedText.height = 5.0;
    fixedText.autoFitFont = true;
    fixedText.autoFitMinFontSizePt = 3.0;
    fixedText.autoFitMaxFontSizePt = 11.0;

    TemplateElement boundField;
    boundField.id = QStringLiteral("bound-name");
    boundField.type = TemplateElementType::BoundField;
    boundField.fieldKey = QStringLiteral("productName");
    boundField.y = 6.0;
    boundField.width = 22.0;
    boundField.height = 5.0;
    boundField.autoFitFont = true;
    boundField.autoFitMinFontSizePt = 4.0;
    boundField.autoFitMaxFontSizePt = 13.0;

    TemplateElement arrayGrid;
    arrayGrid.id = QStringLiteral("grid");
    arrayGrid.type = TemplateElementType::ArrayGrid;
    arrayGrid.y = 12.0;
    arrayGrid.width = 22.0;
    arrayGrid.height = 8.0;
    arrayGrid.dataPath = QStringLiteral("items");
    arrayGrid.arrayGridRows = 1;
    arrayGrid.arrayGridColumns = 1;
    arrayGrid.autoFitFont = true;

    TemplateLayer layer;
    layer.id = QStringLiteral("layer-main");
    fixedText.layerId = layer.id;
    boundField.layerId = layer.id;
    arrayGrid.layerId = layer.id;
    layer.elements = {fixedText, boundField, arrayGrid};

    TemplateDocument document;
    document.id = QStringLiteral("template-auto-fit");
    document.name = QStringLiteral("auto-fit");
    document.templateKey = QStringLiteral("default");
    document.layers = {layer};

    TemplateRenderContext context;
    context.values.insert(QStringLiteral("storeName"), QString::fromUtf8("南京东路旗舰店"));
    context.values.insert(QStringLiteral("productName"), QString::fromUtf8("足金串搭项链"));
    context.values.insert(QStringLiteral("items"), QJsonArray{
        QJsonObject{{QStringLiteral("text"), QString::fromUtf8("素金KA")}},
    });

    const auto drawingPlan = TemplateDocumentRenderer().render(document, LabelRenderPlan{}, LabelOffset{}, DeviceProfile{}, context);
    auto findCommand = [&drawingPlan](const QString& key) -> std::optional<NativeDrawCommand> {
        for (const auto& command : drawingPlan.commands) {
            if (command.elementKey == key) {
                return command;
            }
        }
        return std::nullopt;
    };

    const auto fixedCommand = findCommand(QStringLiteral("fixed-title"));
    const auto boundCommand = findCommand(QStringLiteral("bound-name"));
    const auto gridCommand = findCommand(QStringLiteral("grid.cell0"));
    QVERIFY(fixedCommand.has_value());
    QVERIFY(boundCommand.has_value());
    QVERIFY(gridCommand.has_value());

    QCOMPARE(fixedCommand->autoFitFont, true);
    QCOMPARE(fixedCommand->autoFitMinFontSizePt, 3.0);
    QCOMPARE(fixedCommand->autoFitMaxFontSizePt, 11.0);
    QCOMPARE(boundCommand->autoFitFont, true);
    QCOMPARE(boundCommand->autoFitMinFontSizePt, 4.0);
    QCOMPARE(boundCommand->autoFitMaxFontSizePt, 13.0);
    QCOMPARE(gridCommand->autoFitFont, false);
}

void CoreTests::textAutoFitSizerShrinksAndExpandsText()
{
    NativeDrawCommand command;
    command.type = NativeDrawCommandType::Text;
    command.width = 18.0;
    command.height = 4.0;
    command.fontSizePt = 5.0;
    command.wrapText = true;
    command.autoFitFont = true;
    command.autoFitMinFontSizePt = 3.0;
    command.autoFitMaxFontSizePt = 12.0;

    command.text = QStringLiteral("SKU");
    const auto expandedSize = sleekpr::infrastructure::TextAutoFitSizer::fitPointSize(
        command,
        QSizeF(180.0, 40.0),
        300.0,
        300.0);
    QVERIFY(expandedSize > command.fontSizePt);
    QVERIFY(expandedSize <= command.autoFitMaxFontSizePt);

    command.text = QString::fromUtf8("足金串搭项链南京东路旗舰店限时活动价");
    const auto shrunkenSize = sleekpr::infrastructure::TextAutoFitSizer::fitPointSize(
        command,
        QSizeF(80.0, 18.0),
        300.0,
        300.0);
    QVERIFY(shrunkenSize < expandedSize);
    QVERIFY(shrunkenSize >= command.autoFitMinFontSizePt);
}

void CoreTests::textAutoFitSizerCachesRepeatedMeasurements()
{
    NativeDrawCommand command;
    command.type = NativeDrawCommandType::Text;
    command.text = QString::fromUtf8("足金串搭项链南京东路旗舰店");
    command.fontSizePt = 5.0;
    command.wrapText = true;
    command.autoFitFont = true;
    command.autoFitMinFontSizePt = 3.0;
    command.autoFitMaxFontSizePt = 12.0;

    sleekpr::infrastructure::TextAutoFitSizer::clearCache();
    QCOMPARE(sleekpr::infrastructure::TextAutoFitSizer::cacheEntryCount(), 0);

    const auto firstSize = sleekpr::infrastructure::TextAutoFitSizer::fitPointSize(
        command,
        QSizeF(120.0, 32.0),
        152.4,
        152.4);
    QCOMPARE(sleekpr::infrastructure::TextAutoFitSizer::cacheEntryCount(), 1);

    const auto repeatedSize = sleekpr::infrastructure::TextAutoFitSizer::fitPointSize(
        command,
        QSizeF(120.0, 32.0),
        152.4,
        152.4);
    QCOMPARE(repeatedSize, firstSize);
    QCOMPARE(sleekpr::infrastructure::TextAutoFitSizer::cacheEntryCount(), 1);

    command.text.append(QStringLiteral("A"));
    sleekpr::infrastructure::TextAutoFitSizer::fitPointSize(
        command,
        QSizeF(120.0, 32.0),
        152.4,
        152.4);
    QCOMPARE(sleekpr::infrastructure::TextAutoFitSizer::cacheEntryCount(), 2);
}

void CoreTests::templateDocumentRendererRendersFixedTextVertically()
{
    LabelItem item;
    item.identifierCode = "10000000789";
    item.productName = "Gold Ring";

    TemplateElement fixedText;
    fixedText.id = "quality";
    fixedText.type = TemplateElementType::FixedText;
    fixedText.width = 4.0;
    fixedText.height = 12.0;
    fixedText.text = QString::fromUtf8("合格${suffix}");
    fixedText.verticalText = true;

    TemplateLayer layer;
    layer.id = QStringLiteral("base");
    layer.elements = {fixedText};

    TemplateDocument document;
    document.layers = {layer};

    TemplateRenderContext context;
    context.values.insert(QStringLiteral("suffix"), QString::fromUtf8("证"));

    const auto labelPlan = LabelRenderPlanner().createPlan(item);
    const auto drawingPlan = TemplateDocumentRenderer().render(document, labelPlan, LabelOffset{}, DeviceProfile{}, context);

    QCOMPARE(drawingPlan.commands.size(), 1);
    QCOMPARE(drawingPlan.commands.first().type, NativeDrawCommandType::Text);
    QCOMPARE(drawingPlan.commands.first().text, QString::fromUtf8("合\n格\n证"));
}

void CoreTests::templateDocumentRendererInterpolatesQrCodePayloadTemplate()
{
    LabelItem item;
    item.identifierCode = "10000000789";
    item.productName = "Gold Ring";
    item.address = "Store 1";

    TemplateElement qrCode;
    qrCode.id = "custom_qr_payload";
    qrCode.type = TemplateElementType::QrCode;
    qrCode.x = 35.0;
    qrCode.y = 2.0;
    qrCode.width = 6.0;
    qrCode.height = 6.0;
    qrCode.payload = QString::fromUtf8("地址:${address}");
    qrCode.rotationDegrees = 90.0;

    const auto labelPlan = LabelRenderPlanner().createPlan(item);
    TemplateRenderContext context;
    context.values.insert(QStringLiteral("address"), QString::fromUtf8("民族工匠"));

    TemplateDocument document;
    TemplateLayer layer;
    layer.id = QStringLiteral("base");
    layer.elements = {qrCode};
    document.layers = {layer};

    const auto drawingPlan = TemplateDocumentRenderer().render(document, labelPlan, LabelOffset{}, DeviceProfile{}, context);

    QCOMPARE(drawingPlan.commands.size(), 1);
    QCOMPARE(drawingPlan.commands.first().type, NativeDrawCommandType::QrCode);
    QCOMPARE(drawingPlan.commands.first().text, QString::fromUtf8("地址:民族工匠"));
    QCOMPARE(drawingPlan.commands.first().rotationDegrees, 90.0);
}

void CoreTests::nativeDrawingPlannerUsesSilverTemplateForFactory25003()
{
    LabelItem item;
    item.factoryNo = 25003;
    item.identifierCode = "10000000789";
    item.productName = QString::fromUtf8("银标商品");
    item.weightCategory = QString::fromUtf8("银重");
    item.finishedProductWeight = 3.2;
    item.roughWeight = 3.56;
    item.salesCode = "XS-001";
    item.price = 128.5;

    const auto labelPlan = LabelRenderPlanner().createPlan(item);
    QCOMPARE(labelPlan.templateKey, LabelTemplateKey::Silver80x30);

    QHash<QString, TemplateElementOverride> overrides;
    overrides["priceText"] = TemplateElementOverride{17.5, 12.6, 5.5, true};
    const auto drawingPlan = NativeLabelDrawingPlanner().createPlan(labelPlan, LabelOffset{}, overrides);

    auto findCommand = [&drawingPlan](const QString& key) -> std::optional<NativeDrawCommand> {
        for (const auto& command : drawingPlan.commands) {
            if (command.elementKey == key) {
                return command;
            }
        }
        return std::nullopt;
    };

    const auto qrCode = findCommand("qrCode");
    QVERIFY(qrCode.has_value());
    QCOMPARE(qrCode->width, 8.8);

    const auto standardText = findCommand("standardText");
    QVERIFY(standardText.has_value());
    QVERIFY(standardText->text.contains(QString::fromUtf8("标签约0.20g")));

    const auto priceText = findCommand("priceText");
    QVERIFY(priceText.has_value());
    QCOMPARE(priceText->text, QString("CNY 128.50"));
    QCOMPARE(priceText->x, 17.5);
    QCOMPARE(priceText->y, 12.6);
    QCOMPARE(priceText->fontSizePt, 5.5);
    QCOMPARE(priceText->bold, true);
}

void CoreTests::nativeDrawingPlannerUsesStableDefaultRenderDpi()
{
    const auto defaultPlan = LabelRenderPlanner().createPlan(sleekpr::infrastructure::PreviewLabelFactory::createDemoLabel());
    QCOMPARE(NativeLabelDrawingPlanner().createPlan(defaultPlan).renderDpi, 300.0);

    auto silverItem = sleekpr::infrastructure::PreviewLabelFactory::createDemoLabel(LabelTemplateKey::Silver80x30);
    silverItem.factoryNo = 25003;
    const auto silverPlan = LabelRenderPlanner().createPlan(silverItem);
    QCOMPARE(NativeLabelDrawingPlanner().createPlan(silverPlan).renderDpi, 300.0);
}

void CoreTests::qrCodeMatrixRendererEncodesPayloadAsRealQrCode()
{
    const sleekpr::infrastructure::QrCodeMatrixRenderer renderer;
    const auto matrix = renderer.render("10000000789");
    const auto sameLengthDifferentPayload = renderer.render("10000000788");

    QCOMPARE(matrix.size(), 21);
    QCOMPARE(matrix.darkModuleCount(), 236);
    QVERIFY(!matrix.hasDarkModule(-1, 0));
    QVERIFY(matrix.hasDarkModule(0, 0));
    QVERIFY(matrix.hasDarkModule(6, 0));
    QVERIFY(matrix.hasDarkModule(14, 0));
    QVERIFY(matrix.hasDarkModule(20, 6));
    QVERIFY(matrix.hasDarkModule(0, 20));
    QVERIFY(matrix.hasDarkModule(8, 13));
    QVERIFY(matrix != sameLengthDifferentPayload);
    QVERIFY_THROWS_EXCEPTION(std::invalid_argument, renderer.render("   "));
}

void CoreTests::qrCodeMatrixRendererSupportsVersion2Payloads()
{
    const sleekpr::infrastructure::QrCodeMatrixRenderer renderer;
    const auto matrix = renderer.render("ABCDEFGHIJKLMNOPQRST");

    QCOMPARE(matrix.size(), 25);
    QVERIFY(matrix.darkModuleCount() > 0);
    QVERIFY(matrix.hasDarkModule(0, 0));
    QVERIFY(matrix.hasDarkModule(24, 6));
    QVERIFY(matrix.hasDarkModule(18, 18));
}

void CoreTests::qrCodeMatrixRendererSupportsVersion3And4Payloads()
{
    const sleekpr::infrastructure::QrCodeMatrixRenderer renderer;
    const auto version3Matrix = renderer.render(QStringLiteral("ABCDEFGHIJKLMNOPQRSTU"));
    const auto version4Matrix = renderer.render(QStringLiteral("1234567890123456789012345678901234567890123456"));

    QCOMPARE(version3Matrix.size(), 29);
    QVERIFY(version3Matrix.darkModuleCount() > 0);
    QVERIFY(version3Matrix.hasDarkModule(28, 6));
    QVERIFY(version3Matrix.hasDarkModule(22, 22));

    QCOMPARE(version4Matrix.size(), 33);
    QVERIFY(version4Matrix.darkModuleCount() > 0);
    QVERIFY(version4Matrix.hasDarkModule(32, 6));
    QVERIFY(version4Matrix.hasDarkModule(26, 26));
    QVERIFY_THROWS_EXCEPTION(std::length_error, renderer.render(QStringLiteral("12345678901234567890123456789012345678901234567")));
}

void CoreTests::labelPreviewImageRendererRendersPngCanvas()
{
    LabelItem item;
    item.identifierCode = "10000000789";
    item.productName = "Gold Ring";
    item.weightCategory = "Finished";
    item.finishedProductWeight = 3.2;
    item.roughWeight = 3.56;
    item.salesCode = "XS-001";
    item.goldPurity = "Au999";
    item.address = "Store 1";

    const auto labelPlan = LabelRenderPlanner().createPlan(item);
    const auto drawingPlan = NativeLabelDrawingPlanner().createPlan(labelPlan);
    const auto png = sleekpr::infrastructure::LabelPreviewImageRenderer().renderPng(drawingPlan);
    const auto image = QImage::fromData(png, "PNG");

    QVERIFY(!image.isNull());
    QCOMPARE(image.width(), 945);
    QCOMPARE(image.height(), 354);
}

void CoreTests::labelPreviewImageRendererRendersQtImage()
{
    const auto labelPlan = LabelRenderPlanner().createPlan(sleekpr::infrastructure::PreviewLabelFactory::createDemoLabel());
    const auto drawingPlan = NativeLabelDrawingPlanner().createPlan(labelPlan);
    const auto image = sleekpr::infrastructure::LabelPreviewImageRenderer().renderImage(drawingPlan);

    QVERIFY(!image.isNull());
    QCOMPARE(image.width(), 945);
    QCOMPARE(image.height(), 354);
}

void CoreTests::labelPreviewImageRendererRotatesNinetyDegreesFromTopToBottom()
{
    NativeDrawCommand command;
    command.type = NativeDrawCommandType::Text;
    command.x = 3.0;
    command.y = 3.0;
    command.width = 8.0;
    command.height = 22.0;
    command.text = QStringLiteral("1111WWWW");
    command.fontSizePt = 10.0;
    command.rotationDegrees = 90.0;
    command.elementKey = QStringLiteral("verticalCode");

    NativeLabelDrawingPlan drawingPlan;
    drawingPlan.paperSize = LabelPaperSize(24.0, 30.0);
    drawingPlan.renderDpi = 300.0;
    drawingPlan.commands = {command};

    const auto image = sleekpr::infrastructure::LabelPreviewImageRenderer().renderImage(drawingPlan);
    QVERIFY(!image.isNull());

    const auto mmToPx = [](double mm) {
        return static_cast<int>((mm * 300.0 / 25.4) + 0.5);
    };
    const auto inkCount = [&image](int left, int top, int right, int bottom) {
        int count = 0;
        for (int y = std::max(0, top); y < std::min(image.height(), bottom); ++y) {
            for (int x = std::max(0, left); x < std::min(image.width(), right); ++x) {
                if (image.pixelColor(x, y).value() < 220) {
                    ++count;
                }
            }
        }
        return count;
    };

    const auto left = mmToPx(command.x);
    const auto right = mmToPx(command.x + command.width);
    const auto top = mmToPx(command.y);
    const auto middle = mmToPx(command.y + command.height / 2.0);
    const auto bottom = mmToPx(command.y + command.height);
    const auto topInk = inkCount(left, top, right, middle);
    const auto bottomInk = inkCount(left, middle, right, bottom);

    QVERIFY(topInk > 0);
    QVERIFY(bottomInk > 0);
    QVERIFY(bottomInk > topInk);
}

void CoreTests::labelPreviewImageRendererWrapsTextAndClipsToCommandHeight()
{
    NativeDrawCommand command;
    command.type = NativeDrawCommandType::Text;
    command.x = 2.0;
    command.y = 2.0;
    command.width = 8.0;
    command.height = 8.0;
    command.text = QStringLiteral("AAAA AAAA AAAA AAAA");
    command.fontSizePt = 9.0;
    command.wrapText = true;
    command.elementKey = QStringLiteral("longText");

    NativeLabelDrawingPlan drawingPlan;
    drawingPlan.paperSize = LabelPaperSize(30.0, 18.0);
    drawingPlan.renderDpi = 300.0;
    drawingPlan.commands = {command};

    const auto image = sleekpr::infrastructure::LabelPreviewImageRenderer().renderImage(drawingPlan);
    QVERIFY(!image.isNull());

    const auto mmToPx = [](double mm) {
        return static_cast<int>((mm * 300.0 / 25.4) + 0.5);
    };
    const auto hasInk = [&image](int left, int top, int right, int bottom) {
        for (int y = std::max(0, top); y < std::min(image.height(), bottom); ++y) {
            for (int x = std::max(0, left); x < std::min(image.width(), right); ++x) {
                if (image.pixelColor(x, y).value() < 220) {
                    return true;
                }
            }
        }
        return false;
    };

    QVERIFY(hasInk(mmToPx(2.0), mmToPx(5.5), mmToPx(10.0), mmToPx(9.8)));
    QVERIFY(!hasInk(mmToPx(2.0), mmToPx(10.6), mmToPx(10.0), mmToPx(13.0)));
}

void CoreTests::qrModulePixelLayoutKeepsDesignedQrPixelSize()
{
    const auto bounds = sleekpr::infrastructure::QrModulePixelLayout::moduleBounds(21, 70);

    QCOMPARE(bounds.size(), 22);
    QCOMPARE(bounds.first(), 0);
    QCOMPARE(bounds.last(), 70);

    for (qsizetype index = 1; index < bounds.size(); ++index) {
        QVERIFY(bounds.at(index) > bounds.at(index - 1));
    }
}

void CoreTests::labelPreviewServiceUsesTemplateDocumentDeviceProfile()
{
    const auto demoLabelPlan = LabelRenderPlanner().createPlan(sleekpr::infrastructure::PreviewLabelFactory::createDemoLabel());
    auto document = TemplateDocumentFactory::fromDrawingPlan(
        "default",
        QString::fromUtf8("默认标签"),
        NativeLabelDrawingPlanner().createPlan(demoLabelPlan),
        QList<TemplateElement>{});

    DeviceProfile profile;
    profile.dpi = 203.0;
    document.deviceProfiles = {profile};

    PrintClientSettings settings;
    settings.templateDocuments["default"] = document;

    const auto image = sleekpr::infrastructure::LabelPreviewService().renderDemoPreview(settings);

    QVERIFY(!image.isNull());
    QVERIFY(image.width() < 945);
}

void CoreTests::labelPreviewServiceUsesTemplateDocumentSampleData()
{
    TemplateElement productElement;
    productElement.id = QStringLiteral("product-name");
    productElement.layerId = QStringLiteral("base");
    productElement.type = TemplateElementType::FixedText;
    productElement.text = QStringLiteral("${product_name}");
    productElement.x = 5.0;
    productElement.y = 5.0;
    productElement.width = 45.0;
    productElement.height = 8.0;
    productElement.fontSizePt = 11.0;

    TemplateLayer baseLayer;
    baseLayer.id = QStringLiteral("base");
    baseLayer.name = QString::fromUtf8("基础图层");
    baseLayer.elements = {productElement};

    TemplateDocument document;
    document.id = QStringLiteral("preview-sample-template");
    document.name = QString::fromUtf8("预览样例模板");
    document.templateKey = QStringLiteral("default");
    document.layers = {baseLayer};
    document.sampleData.insert(QStringLiteral("product_name"), QStringLiteral("SAMPLE-PRODUCT"));

    PrintClientSettings settings;
    settings.templateDocuments["default"] = document;

    const auto image = sleekpr::infrastructure::LabelPreviewService().renderDemoPreview(settings);
    QVERIFY(!image.isNull());

    const auto mmToPx = [](double value) {
        return static_cast<int>(std::round(value * 300.0 / 25.4));
    };
    bool hasInk = false;
    for (int y = mmToPx(5.0); y < mmToPx(12.0) && y < image.height(); ++y) {
        for (int x = mmToPx(5.0); x < mmToPx(50.0) && x < image.width(); ++x) {
            if (image.pixelColor(x, y).value() < 220) {
                hasInk = true;
                break;
            }
        }
        if (hasInk) {
            break;
        }
    }

    QVERIFY(hasInk);
}

void CoreTests::labelPreviewServiceRenderPreviewUsesDefaultPrinterDeviceProfile()
{
    const auto item = sleekpr::infrastructure::PreviewLabelFactory::createDemoLabel();
    const auto demoLabelPlan = LabelRenderPlanner().createPlan(item);
    auto document = TemplateDocumentFactory::fromDrawingPlan(
        "default",
        QString::fromUtf8("默认标签"),
        NativeLabelDrawingPlanner().createPlan(demoLabelPlan),
        QList<TemplateElement>{});

    DeviceProfile defaultProfile;
    defaultProfile.dpi = 300.0;

    DeviceProfile printerProfile;
    printerProfile.printerName = "Printer A";
    printerProfile.dpi = 203.0;
    document.deviceProfiles = {defaultProfile, printerProfile};

    PrintClientSettings settings;
    settings.defaultPrinter = "Printer A";
    settings.templateDocuments["default"] = document;

    const auto image = sleekpr::infrastructure::LabelPreviewService().renderPreview(item, settings);

    QVERIFY(!image.isNull());
    QVERIFY(image.width() < 945);
}

void CoreTests::nativePlanJsonSerializerExportsPaperAndCommands()
{
    LabelItem item;
    item.identifierCode = "10000000789";
    item.productName = "Gold Ring";
    item.weightCategory = "Finished";
    item.finishedProductWeight = 3.2;
    item.roughWeight = 3.56;
    item.salesCode = "XS-001";
    item.goldPurity = "Au999";
    item.address = "Store 1";

    const auto labelPlan = LabelRenderPlanner().createPlan(item);
    const auto drawingPlan = NativeLabelDrawingPlanner().createPlan(labelPlan);
    const auto json = NativePlanJsonSerializer().toJson(drawingPlan);

    QCOMPARE(json["paperSize"].toObject()["widthMm"].toDouble(), 80.0);
    QCOMPARE(json["paperSize"].toObject()["heightMm"].toDouble(), 30.0);
    QVERIFY(json["commands"].toArray().size() >= 10);
    QCOMPARE(json["commands"].toArray().first().toObject()["type"].toString(), QString("text"));
}

QTEST_MAIN(CoreTests)
#include "tst_core.moc"

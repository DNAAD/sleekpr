#include <QtTest/QtTest>

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
#include "sleekpr/core/templates/PaperSpecJson.h"
#include "sleekpr/core/templates/TemplateDocument.h"
#include "sleekpr/core/templates/TemplateDocumentEditModel.h"
#include "sleekpr/core/templates/TemplateDocumentFactory.h"
#include "sleekpr/core/templates/TemplateDocumentJson.h"
#include "sleekpr/core/templates/TemplateDocumentRenderer.h"
#include "sleekpr/infrastructure/preview/LabelPreviewImageRenderer.h"
#include "sleekpr/infrastructure/preview/LabelPreviewService.h"
#include "sleekpr/infrastructure/preview/PreviewLabelFactory.h"
#include "sleekpr/infrastructure/preview/QrCodeMatrixRenderer.h"

using namespace sleekpr::core;

class CoreTests final : public QObject
{
    Q_OBJECT

private slots:
    void labelPaperSizeCreates80x30();
    void printUnitConverterMatchesDotnetBehavior();
    void settingsStoreReturnsDefaultsAndPersistsValues();
    void settingsStorePersistsTemplateElements();
    void templateDocumentJsonPersistsLayersVersionsAndProfiles();
    void templateDocumentJsonRejectsInvalidImportDocument();
    void templateDocumentEditModelManagesLayers();
    void templateDocumentEditModelReordersElements();
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
    void nativeDrawingPlannerUsesSilverTemplateForFactory25003();
    void nativeDrawingPlannerUsesStableDefaultRenderDpi();
    void qrCodeMatrixRendererEncodesPayloadAsRealQrCode();
    void qrCodeMatrixRendererSupportsVersion2Payloads();
    void labelPreviewImageRendererRendersPngCanvas();
    void labelPreviewImageRendererRendersQtImage();
    void labelPreviewServiceUsesTemplateDocumentDeviceProfile();
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

    PrintClientSettings expected;
    expected.templateElements["default"] = {fixedText, boundField, qrCode, rectangle};

    FileSettingsStore store(dir.filePath("settings.json"));
    QVERIFY(store.save(expected));

    const auto actual = store.load();
    const auto elements = actual.templateElements.value("default");
    QCOMPARE(elements.size(), 4);
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
    QCOMPARE(actual.layers.first().elements.first().maxLines, 2);
    QCOMPARE(actual.layers.first().elements.first().ellipsis, true);
    QCOMPARE(actual.layers.first().elements.first().layerId, QString("layer-main"));
    QCOMPARE(actual.versions.size(), 1);
    QCOMPARE(actual.versions.first().layersSnapshot.first().id, QString("layer-main"));
    QCOMPARE(actual.deviceProfiles.size(), 1);
    QCOMPARE(actual.deviceProfiles.first().scaleX, 1.01);
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

    QString emptyTemplateError;
    QVERIFY(!TemplateDocumentJson::validateForImport(emptyTemplate, &emptyTemplateError));
    QVERIFY(emptyTemplateError.contains(QString::fromUtf8("元素")));

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

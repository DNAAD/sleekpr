#include "sleekpr/app/AppTheme.h"
#include "sleekpr/app/TemplateDesignerFactory.h"
#include "sleekpr/app/TemplateDesignerWindow.h"
#include "sleekpr/app/TemplatePreviewLabel.h"
#include "sleekpr/app/FieldBindingSelector.h"
#include "sleekpr/app/TemplateDesignerPresenter.h"
#include "sleekpr/app/TemplateInspectorPanel.h"
#include "sleekpr/app/PrintFieldEditorPanel.h"
#include "sleekpr/app/SettingsWindow.h"
#include "sleekpr/app/SettingsPersistenceService.h"
#include "support/SettingsPersistenceTestSupport.h"
#include "sleekpr/app/BuildInfoDialog.h"
#include "sleekpr/app/BuildInfo.h"
#include "sleekpr/app/PrintWorkbenchWindow.h"
#include "sleekpr/infrastructure/settings/SettingsRepository.h"
#include "sleekpr/infrastructure/preview/PreviewExecutor.h"
#include "sleekpr/infrastructure/diagnostics/ClientPerformanceMetrics.h"
#include "sleekpr/core/templates/TemplateDocumentJson.h"
#include "sleekpr/core/templates/TemplateLibraryStore.h"
#include "sleekpr/core/templates/PaperSpecStore.h"
#include "sleekpr/core/templates/FieldSchemaJson.h"
#include "sleekpr/core/templates/TemplateInputSchema.h"

#include <QFile>
#include <QElapsedTimer>
#include <QSaveFile>
#include <QAction>
#include <QClipboard>
#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QMessageBox>
#include <QInputDialog>
#include <QFormLayout>
#include <QLineEdit>
#include <QJsonArray>
#include <QJsonDocument>
#include <QListWidget>
#include <QMimeData>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSignalSpy>
#include <QTabWidget>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QToolButton>
#include <QTimer>
#include <QtTest>

#include <algorithm>
#include <memory>

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
    void canvasSupportsMultiSelectionAndDuplicate();
    void canvasGestureCommitsOneUndoStep();
    void canvasMarqueeResizeClipboardAndAlignment();
    void canvasSlowDragAndCancelAreTransactional();
    void canvasMapsDeviceCalibrationBackToMillimeters();
    void canvasRejectsUnsafeClipboardElements();
    void canvasAltDragCancelRestoresOriginalAndNoopHasNoHistory();
    void canvasWindowCanBeDestroyedDuringDrag();
    void canvasSmallEdgeElementHasVisibleHandlesAndMoveArea();
    void settingsOffersPrintWorkbenchEntry();
    void boundFieldPickerShowsChineseLabelAndSavesKey();
    void fieldPickerKeepsKeysSeparateFromLabels();
    void fieldPickerUsesSchemaNamesAndOnlyCurrentDocument();
    void fieldPickerAcceptsAndCancelsCustomInput();
    void fieldPickerRefreshIsSilentAndFitsNarrowPanel();
    void bindingKeyAndDisplayNameCanBeEditedSeparately();
    void bindingNameAutoAppliesAndStaysWithinTemplate();
    void bindingNameCommandPreservesRulesAndBlocksLockedEdits();
    void bindingKeyChangeDoesNotRenameDataOrOtherElements();
    void bindingKeySupportsDirectInputWithoutCarryingOldName();
    void bindingFastKeyChangeKeepsPreviousName();
    void bindingNameKeepsExplicitSchemaNameEqualToKey();
    void allDataReferencesExposeKeyAndDisplayName_data();
    void allDataReferencesExposeKeyAndDisplayName();
    void referenceEditsOnlyRewriteCurrentElement();
    void arrayReferencesKeepSourceAndRowKeysSeparate();
    void legacyQrBindingKeepsNativeMeaning();
    void unifiedReferenceNamesAutoApplySaveAndUndo();
    void unifiedReferenceValidationIsAtomic();
    void unifiedReferencesFollowContentWithoutMovingCursor();
    void arrayReferenceRefreshKeepsTemplateCursor();
    void partialKeyCollisionDoesNotRemoveEditor();
    void clearedLegacyQrStillAcceptsBindingKeyChange();
    void designerConflictKeepsDraftAndHistory();
    void designerOldSaveDoesNotReplaceNewDraft();
    void designerUndoWhileSavingPersistsOriginal();
    void designerHideKeepsQueuedSession();
    void designerInvalidJsonSurvivesOtherSaveCompletion();
    void designerSaveFailureDoesNotAdvanceBaseline();
    void failedDesignerCloseKeepsDraft();
    void sampleSaveFailureNeverReportsSaved();
    void restoreAfterExternalCorruptionUsesObservedDigest();
    void defaultDesignerIsNotDirtyUntilEdited();
    void invalidSampleDraftRequiresCloseConfirmation();
    void pendingSettingsTimerIsUnsaved();
    void corruptSettingsCannotBeAutoRecreatedByEditing();
    void reloadConflictRequiresConfirmation();
    void settingsSavePreservesExternalTemplateAndUneditedLimits();
    void settingsLateSaveDoesNotRollbackNewRuntimeApply();
    void settingsFailedTicketCannotDisplaceLaterRunningContext();
    void settingsEditAfterSaveIsNotImplicitlyPersisted();
    void settingsRevertDuringSaveIsNotTreatedAsUntouched();
    void settingsUntouchedFallbackDoesNotBecomeExplicit();
    void settingsPreviewRefreshDoesNotCreateManualDraft();
    void resumePreservesExplicitDisabledChild();
    void exitPauseDuringInitialLoadResumesUsableWindows();
    void hiddenSettingsDoesNotRenderOnTemplateChange();
    void visibleSettingsDraftSurvivesExternalChange();
    void settingsPreviewIsAsynchronous();
    void versionEntriesAndRuntimeDiagnostics();
    void designerEditDispatchMetrics();
};

namespace {

PrintClientSettings selectionTestSettings()
{
    auto document = TemplateDesignerFactory::createBlankDocument(QStringLiteral("selection-test"),
        QString::fromUtf8("选区测试"), QStringLiteral("default"), QStringLiteral("label-80x30"), {});
    TemplateLayer layer;
    layer.id = QStringLiteral("base");
    layer.name = QString::fromUtf8("标签图层");
    for (int index = 0; index < 3; ++index) {
        auto element = TemplateDesignerFactory::createElement(TemplateElementType::Rectangle);
        element.id = QStringLiteral("rect-%1").arg(index);
        element.layerId = layer.id;
        element.x = 5.0 + index * 20.0;
        element.y = 5.0;
        element.width = 10.0;
        element.height = 6.0;
        layer.elements.append(element);
    }
    document.layers = {layer};
    PrintClientSettings settings;
    settings.templateDocuments.insert(QStringLiteral("default"), document);
    return settings;
}

QPoint canvasPoint(TemplatePreviewLabel* preview, double x, double y)
{
    const auto size = preview->printableImageSizePx();
    return preview->printableImageOriginPx() + QPoint(qRound(x * size.width() / 80.0), qRound(y * size.height() / 30.0));
}

void dragCanvas(TemplatePreviewLabel* preview, QPoint start, QPoint finish)
{
    QTest::mousePress(preview, Qt::LeftButton, Qt::NoModifier, start);
    for (int step = 1; step <= 4; ++step) {
        const auto point = start + (finish - start) * step / 4;
        QMouseEvent move(QEvent::MouseMove, point, preview->mapToGlobal(point), Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(preview, &move);
    }
    QTest::mouseRelease(preview, Qt::LeftButton, Qt::NoModifier, finish);
}

} // 命名空间

void LabelDesignerTests::allDataReferencesExposeKeyAndDisplayName_data()
{
    QTest::addColumn<int>("type");
    QTest::addColumn<QString>("content");
    QTest::addColumn<int>("rows");
    QTest::newRow("text-placeholders") << int(TemplateElementType::FixedText) << QStringLiteral("${productName}/${salesCode}/${productName}") << 2;
    QTest::newRow("qr-expression") << int(TemplateElementType::QrCode) << QStringLiteral("https://test/${productName}/${salesCode}") << 2;
    QTest::newRow("array-grid") << int(TemplateElementType::ArrayGrid) << QStringLiteral("${productName}:${weight}") << 3;
    QTest::newRow("literal-text") << int(TemplateElementType::FixedText) << QString::fromUtf8("固定文字") << 0;
    QTest::newRow("literal-qr") << int(TemplateElementType::QrCode) << QStringLiteral("fixed-payload") << 0;
    QTest::newRow("rectangle") << int(TemplateElementType::Rectangle) << QString() << 0;
}

void LabelDesignerTests::allDataReferencesExposeKeyAndDisplayName()
{
    QFETCH(int, type);
    QFETCH(QString, content);
    QFETCH(int, rows);
    auto document = selectionTestSettings().templateDocuments.value(QStringLiteral("default"));
    auto element = document.layers.first().elements.first();
    element.type = TemplateElementType(type);
    element.text = content;
    element.payload = content;
    element.dataPath = QStringLiteral("items");
    element.arrayGridCellTemplate = content;
    document.fieldDisplayNames.insert(QStringLiteral("items"), QString::fromUtf8("商品明细"));
    TemplateInspectorPanel panel;
    TemplateDesignerPresenter presenter;
    panel.setElementProperties(presenter.elementPropertyModel(element, true, &document));
    auto* table = panel.findChild<QTableWidget*>(QStringLiteral("elementBindingsTable"));
    QVERIFY2(table != nullptr, "所有数据引用都应提供独立的 key 与 displayName 编辑入口");
    QCOMPARE(table->rowCount(), rows);
    QCOMPARE(table->isHidden(), rows == 0);
    if (rows > 0) {
        auto* key = qobject_cast<QLineEdit*>(table->cellWidget(0, 0));
        auto* name = qobject_cast<QLineEdit*>(table->cellWidget(0, 1));
        QVERIFY(key != nullptr && name != nullptr);
        QCOMPARE(key->text(), type == int(TemplateElementType::ArrayGrid) ? QStringLiteral("items") : QStringLiteral("productName"));
        QCOMPARE(name->text(), type == int(TemplateElementType::ArrayGrid) ? QString::fromUtf8("商品明细") : QString::fromUtf8("商品名称"));
        panel.setElementProperties(presenter.elementPropertyModel(element, false, &document));
        QVERIFY(!table->isEnabled());
    }
}

void LabelDesignerTests::referenceEditsOnlyRewriteCurrentElement()
{
    auto document = selectionTestSettings().templateDocuments.value(QStringLiteral("default"));
    auto& element = document.layers.first().elements.first();
    element.type = TemplateElementType::FixedText;
    element.text = QStringLiteral("productName: ${ productName } / ${productName} / ${salesCode}");
    auto other = element;
    other.id = QStringLiteral("other");
    document.layers.first().elements.append(other);
    const auto samples = document.sampleData;
    TemplateDesignerPresenter presenter;
    TemplateInspectorPanel panel;
    panel.setElementProperties(presenter.elementPropertyModel(document.layers.first().elements.first(), true, &document));
    auto* table = panel.findChild<QTableWidget*>(QStringLiteral("elementBindingsTable"));
    QVERIFY(table != nullptr && table->rowCount() == 2);
    qobject_cast<QLineEdit*>(table->cellWidget(0, 0))->setText(QStringLiteral("salesCode"));
    qobject_cast<QLineEdit*>(table->cellWidget(1, 0))->setText(QStringLiteral("productName"));
    qobject_cast<QLineEdit*>(table->cellWidget(0, 1))->setText(QString::fromUtf8("新编号"));
    const auto id = document.layers.first().elements.first().id;
    const auto result = presenter.applyElementProperties(document, id, panel.elementProperties());
    QVERIFY2(result.errorMessage.isEmpty(), qPrintable(result.errorMessage));
    QVERIFY(result.changed);
    QCOMPARE(document.layers.first().elements.first().text, QStringLiteral("productName: ${salesCode} / ${salesCode} / ${productName}"));
    QCOMPARE(document.layers.first().elements.last().text, other.text);
    QCOMPARE(document.fieldDisplayNames.value(QStringLiteral("salesCode")), QString::fromUtf8("新编号"));
    QVERIFY(!document.fieldDisplayNames.contains(QStringLiteral("productName")));
    QCOMPARE(document.sampleData, samples);
    QVERIFY(document.fieldSchema.isEmpty());
}

void LabelDesignerTests::arrayReferencesKeepSourceAndRowKeysSeparate()
{
    auto document = selectionTestSettings().templateDocuments.value(QStringLiteral("default"));
    auto& element = document.layers.first().elements.first();
    element.type = TemplateElementType::ArrayGrid;
    element.dataPath = QStringLiteral("order.items");
    element.arrayGridCellTemplate = QStringLiteral("${name}:${weight}");
    TemplateDesignerPresenter presenter;
    TemplateInspectorPanel panel;
    panel.setElementProperties(presenter.elementPropertyModel(element, true, &document));
    auto* table = panel.findChild<QTableWidget*>(QStringLiteral("elementBindingsTable"));
    QVERIFY(table != nullptr && table->rowCount() == 3);
    qobject_cast<QLineEdit*>(table->cellWidget(0, 0))->setText(QStringLiteral("order.parts"));
    qobject_cast<QLineEdit*>(table->cellWidget(0, 1))->setText(QString::fromUtf8("配件列表"));
    qobject_cast<QLineEdit*>(table->cellWidget(1, 0))->setText(QStringLiteral("title"));
    qobject_cast<QLineEdit*>(table->cellWidget(1, 1))->setText(QString::fromUtf8("配件名称"));
    const auto result = presenter.applyElementProperties(document, element.id, panel.elementProperties());
    QVERIFY2(result.errorMessage.isEmpty(), qPrintable(result.errorMessage));
    QCOMPARE(element.dataPath, QStringLiteral("order.parts"));
    QCOMPARE(element.arrayGridCellTemplate, QStringLiteral("${title}:${weight}"));
    QCOMPARE(document.fieldDisplayNames.value(QStringLiteral("order.parts")), QString::fromUtf8("配件列表"));
    QCOMPARE(document.fieldDisplayNames.value(QStringLiteral("title")), QString::fromUtf8("配件名称"));
    const auto fields = TemplateInputSchema::fields(document);
    QCOMPARE(fields.size(), 1);
    QCOMPARE(fields.first().definition.key, QStringLiteral("order.parts"));
}

void LabelDesignerTests::legacyQrBindingKeepsNativeMeaning()
{
    auto document = selectionTestSettings().templateDocuments.value(QStringLiteral("default"));
    auto& element = document.layers.first().elements.first();
    element.type = TemplateElementType::QrCode;
    element.fieldKey = QStringLiteral("qrPayload");
    element.payload.clear();
    TemplateDesignerPresenter presenter;
    TemplateInspectorPanel panel;
    panel.setElementProperties(presenter.elementPropertyModel(element, true, &document));
    auto* table = panel.findChild<QTableWidget*>(QStringLiteral("elementBindingsTable"));
    QVERIFY(table != nullptr && table->rowCount() == 1);
    qobject_cast<QLineEdit*>(table->cellWidget(0, 0))->setText(QStringLiteral("serial"));
    qobject_cast<QLineEdit*>(table->cellWidget(0, 1))->setText(QString::fromUtf8("追踪码"));
    const auto result = presenter.applyElementProperties(document, element.id, panel.elementProperties());
    QVERIFY2(result.errorMessage.isEmpty(), qPrintable(result.errorMessage));
    QCOMPARE(element.fieldKey, QStringLiteral("serial"));
    QVERIFY(element.payload.isEmpty());
    QCOMPARE(document.fieldDisplayNames.value(QStringLiteral("serial")), QString::fromUtf8("追踪码"));
}

void LabelDesignerTests::unifiedReferenceNamesAutoApplySaveAndUndo()
{
    QTemporaryDir directory;
    auto settings = selectionTestSettings();
    auto& document = settings.templateDocuments[QStringLiteral("default")];
    document.layers.first().elements.resize(1);
    auto& element = document.layers.first().elements.first();
    element.type = TemplateElementType::FixedText;
    element.text = QStringLiteral("${productName}");
    PrintClientSettings changed;
    TemplateDesignerWindow window(settings, [&changed](const auto& next) { changed = next; }, directory.path());
    auto* table = window.findChild<QTableWidget*>(QStringLiteral("elementBindingsTable"));
    QVERIFY(table != nullptr && table->rowCount() == 1);
    qobject_cast<QLineEdit*>(table->cellWidget(0, 1))->setText(QString::fromUtf8("标签品名"));
    QTRY_COMPARE(changed.templateDocuments.value(QStringLiteral("default")).fieldDisplayNames.value(QStringLiteral("productName")), QString::fromUtf8("标签品名"));
    qobject_cast<QLineEdit*>(table->cellWidget(0, 1))->setText(QString::fromUtf8("保存前品名"));
    window.findChild<QPushButton*>(QStringLiteral("saveTemplateToLibraryButton"))->click();
    const auto stored = TemplateLibraryStore(directory.path()).loadTemplate(document.id);
    QVERIFY(stored.has_value());
    QCOMPARE(stored->fieldDisplayNames.value(QStringLiteral("productName")), QString::fromUtf8("保存前品名"));
    QCOMPARE(stored->layers.first().elements.first().text, QStringLiteral("${productName}"));
    window.findChild<QPushButton*>(QStringLiteral("designerUndoButton"))->click();
    QCOMPARE(qobject_cast<QLineEdit*>(table->cellWidget(0, 1))->text(), QString::fromUtf8("标签品名"));
    window.findChild<QPushButton*>(QStringLiteral("designerRedoButton"))->click();
    QCOMPARE(qobject_cast<QLineEdit*>(table->cellWidget(0, 1))->text(), QString::fromUtf8("保存前品名"));
}

void LabelDesignerTests::unifiedReferenceValidationIsAtomic()
{
    auto document = selectionTestSettings().templateDocuments.value(QStringLiteral("default"));
    auto& element = document.layers.first().elements.first();
    element.type = TemplateElementType::FixedText;
    element.text = QStringLiteral("${name}/${weight}");
    TemplateDesignerPresenter presenter;
    const auto before = TemplateDocumentJson::toJson(document);
    auto model = presenter.elementPropertyModel(element, true, &document);
    QVERIFY(model.bindings.size() == 2);
    model.bindings[0].key.clear();
    model.x += 1;
    auto result = presenter.applyElementProperties(document, element.id, model);
    QVERIFY(!result.errorMessage.isEmpty());
    QVERIFY(!result.changed);
    QCOMPARE(TemplateDocumentJson::toJson(document), before);

    model = presenter.elementPropertyModel(element, true, &document);
    model.bindings[0].key = QStringLiteral("weight");
    model.bindings[0].displayName = QStringLiteral("one");
    model.bindings[0].displayNameEdited = true;
    model.bindings[1].displayName = QStringLiteral("two");
    model.bindings[1].displayNameEdited = true;
    result = presenter.applyElementProperties(document, element.id, model);
    QVERIFY(!result.errorMessage.isEmpty());
    QCOMPARE(TemplateDocumentJson::toJson(document), before);

    model = presenter.elementPropertyModel(element, true, &document);
    model.value = QStringLiteral("literal");
    model.bindings[0].displayName = QStringLiteral("orphan-name");
    model.bindings[0].displayNameEdited = true;
    result = presenter.applyElementProperties(document, element.id, model);
    QVERIFY(result.errorMessage.isEmpty());
    QCOMPARE(element.text, QStringLiteral("literal"));
    QVERIFY(document.fieldDisplayNames.isEmpty());

    element.type = TemplateElementType::ArrayGrid;
    element.dataPath = QStringLiteral("items");
    element.arrayGridCellTemplate.clear();
    model = presenter.elementPropertyModel(element, true, &document);
    QCOMPARE(model.bindings.size(), 2);
    QCOMPARE(model.bindings.last().key, QStringLiteral("value"));
    model.bindings.last().key = QStringLiteral("title");
    result = presenter.applyElementProperties(document, element.id, model);
    QVERIFY(result.errorMessage.isEmpty());
    QCOMPARE(element.arrayGridCellTemplate, QStringLiteral("${title}"));
}

void LabelDesignerTests::unifiedReferencesFollowContentWithoutMovingCursor()
{
    auto settings = selectionTestSettings();
    auto& element = settings.templateDocuments[QStringLiteral("default")].layers.first().elements.first();
    element.type = TemplateElementType::FixedText;
    element.text = QStringLiteral("literal");
    PrintClientSettings changed;
    TemplateDesignerWindow window(settings, [&changed](const auto& next) { changed = next; });
    window.resize(1120, 800);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    auto* table = window.findChild<QTableWidget*>(QStringLiteral("elementBindingsTable"));
    auto* content = window.findChild<QPlainTextEdit*>(QStringLiteral("elementValueEdit"));
    QVERIFY(table != nullptr && content != nullptr);
    QVERIFY(table->isHidden());
    content->setFocus();
    content->setPlainText(QStringLiteral("${productName}/${priceText}"));
    auto cursor = content->textCursor();
    cursor.setPosition(5);
    content->setTextCursor(cursor);
    QTRY_COMPARE(table->rowCount(), 2);
    QCOMPARE(content->textCursor().position(), 5);
    QVERIFY(content->hasFocus());
    auto* key = qobject_cast<QLineEdit*>(table->cellWidget(0, 0));
    auto* name = qobject_cast<QLineEdit*>(table->cellWidget(0, 1));
    QVERIFY(key != nullptr && name != nullptr);
    key->setFocus();
    key->setText(QStringLiteral("customerName"));
    key->setCursorPosition(4);
    QTRY_COMPARE(changed.templateDocuments.value(QStringLiteral("default")).layers.first().elements.first().text, QStringLiteral("${customerName}/${priceText}"));
    QCOMPARE(table->cellWidget(0, 0), key);
    QCOMPARE(key->cursorPosition(), 4);
    QVERIFY(key->hasFocus());
    QVERIFY(name->text().isEmpty() || name->text() == QStringLiteral("customerName"));
    QVERIFY(!changed.templateDocuments.value(QStringLiteral("default")).fieldDisplayNames.contains(QStringLiteral("customerName")));
    name->setText(QString::fromUtf8("客户名称"));
    QTRY_COMPARE(changed.templateDocuments.value(QStringLiteral("default")).fieldDisplayNames.value(QStringLiteral("customerName")), QString::fromUtf8("客户名称"));
    const auto screenshot = qEnvironmentVariable("SLEEKPR_BINDINGS_SCREENSHOT");
    if (!screenshot.isEmpty()) QVERIFY(window.grab().save(screenshot));
    content->setPlainText(QStringLiteral("literal"));
    QTRY_COMPARE(table->rowCount(), 0);
    QVERIFY(table->isHidden());
}

void LabelDesignerTests::arrayReferenceRefreshKeepsTemplateCursor()
{
    auto settings = selectionTestSettings();
    auto& element = settings.templateDocuments[QStringLiteral("default")].layers.first().elements.first();
    element.type = TemplateElementType::ArrayGrid;
    element.dataPath = QStringLiteral("items");
    element.arrayGridCellTemplate = QStringLiteral("${name}");
    TemplateDesignerWindow window(settings, {});
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    auto* content = window.findChild<QPlainTextEdit*>(QStringLiteral("arrayGridCellTemplateEdit"));
    auto* table = window.findChild<QTableWidget*>(QStringLiteral("elementBindingsTable"));
    QVERIFY(content != nullptr && table != nullptr);
    content->setFocus();
    content->setPlainText(QStringLiteral("${name}/${weight}"));
    auto cursor = content->textCursor();
    cursor.setPosition(4);
    content->setTextCursor(cursor);
    QTRY_COMPARE(table->rowCount(), 3);
    QCOMPARE(content->textCursor().position(), 4);
    QVERIFY(content->hasFocus());
}

void LabelDesignerTests::partialKeyCollisionDoesNotRemoveEditor()
{
    auto settings = selectionTestSettings();
    auto& element = settings.templateDocuments[QStringLiteral("default")].layers.first().elements.first();
    element.type = TemplateElementType::FixedText;
    element.text = QStringLiteral("${a}/${b}");
    PrintClientSettings changed = settings;
    TemplateDesignerWindow window(settings, [&changed](const auto& next) { changed = next; });
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    auto* table = window.findChild<QTableWidget*>(QStringLiteral("elementBindingsTable"));
    QVERIFY(table != nullptr && table->rowCount() == 2);
    auto* key = qobject_cast<QLineEdit*>(table->cellWidget(1, 0));
    QVERIFY(key != nullptr);
    key->setFocus();
    key->setText(QStringLiteral("a"));
    QTest::qWait(TemplateInspectorPanel::TextAutoApplyDelayMs + 250);
    QCOMPARE(table->rowCount(), 2);
    QCOMPARE(table->cellWidget(1, 0), key);
    QVERIFY(key->hasFocus());
    key->setText(QStringLiteral("aLong"));
    QTRY_COMPARE(changed.templateDocuments.value(QStringLiteral("default")).layers.first().elements.first().text, QStringLiteral("${a}/${aLong}"));
    key->setText(QStringLiteral("a"));
    window.findChild<QPlainTextEdit*>(QStringLiteral("elementValueEdit"))->setFocus();
    QTRY_COMPARE(table->rowCount(), 1);
    QCOMPARE(changed.templateDocuments.value(QStringLiteral("default")).layers.first().elements.first().text, QStringLiteral("${a}/${a}"));
}

void LabelDesignerTests::clearedLegacyQrStillAcceptsBindingKeyChange()
{
    auto document = selectionTestSettings().templateDocuments.value(QStringLiteral("default"));
    auto& element = document.layers.first().elements.first();
    element.type = TemplateElementType::QrCode;
    element.fieldKey = QStringLiteral("qrPayload");
    element.payload.clear();
    TemplateDesignerPresenter presenter;
    TemplateInspectorPanel panel;
    panel.setElementProperties(presenter.elementPropertyModel(element, true, &document));
    panel.elementValueEdit()->clear();
    auto* table = panel.findChild<QTableWidget*>(QStringLiteral("elementBindingsTable"));
    QVERIFY(table != nullptr && table->rowCount() == 1);
    qobject_cast<QLineEdit*>(table->cellWidget(0, 0))->setText(QStringLiteral("serial"));
    const auto result = presenter.applyElementProperties(document, element.id, panel.elementProperties());
    QVERIFY2(result.errorMessage.isEmpty(), qPrintable(result.errorMessage));
    QCOMPARE(element.fieldKey, QStringLiteral("serial"));
    QVERIFY(element.payload.isEmpty());
}

void LabelDesignerTests::designerOldSaveDoesNotReplaceNewDraft()
{
    QTemporaryDir dir; const auto path = dir.filePath("settings.json"); const auto settings = selectionTestSettings();
    sleekpr::infrastructure::SettingsRepository repo(path); QVERIFY(repo.replace(settings).ok());
    sleekpr::test::SettingsWriteGate gate{path}; SettingsPersistenceService service(path, gate.writer()); sleekpr::test::ReleaseSettingsGate unblock{gate};
    const auto id = service.openSession(QStringLiteral("designer")); QVERIFY(id); QTRY_VERIFY(service.isIdle());
    TemplateDesignerWindow window(settings, {}, dir.path()); window.attachPersistence(service, *id);
    auto* x = window.findChild<QDoubleSpinBox*>(QStringLiteral("elementXSpin"));
    auto* apply = window.findChild<QPushButton*>(QStringLiteral("applyElementPropertiesButton")); QVERIFY(x && apply);
    gate.armed = true; x->setValue(7); apply->click(); QTRY_VERIFY(gate.entered.available() > 0);
    x->setValue(8); apply->click(); gate.release.release(); QTRY_VERIFY(service.isIdle());
    QCOMPARE(x->value(), 8.0); QTRY_VERIFY(!window.hasUnsavedPersistenceChanges());
    QCOMPARE(repo.read().settings.templateDocuments.value(QStringLiteral("default")).layers[0].elements[0].x, 8.0);
    window.findChild<QPushButton*>(QStringLiteral("designerUndoButton"))->click(); QCOMPARE(x->value(), 7.0);
    QTRY_VERIFY(service.isIdle());
}

void LabelDesignerTests::designerUndoWhileSavingPersistsOriginal()
{
    QTemporaryDir dir; const auto path = dir.filePath("settings.json"); const auto settings = selectionTestSettings();
    sleekpr::infrastructure::SettingsRepository repo(path); QVERIFY(repo.replace(settings).ok());
    sleekpr::test::SettingsWriteGate gate{path}; SettingsPersistenceService service(path, gate.writer()); sleekpr::test::ReleaseSettingsGate unblock{gate};
    const auto id = service.openSession(QStringLiteral("designer")); QVERIFY(id); QTRY_VERIFY(service.isIdle());
    TemplateDesignerWindow window(settings, {}, dir.path()); window.attachPersistence(service, *id);
    auto* x = window.findChild<QDoubleSpinBox*>(QStringLiteral("elementXSpin")); gate.armed = true; x->setValue(7);
    window.findChild<QPushButton*>(QStringLiteral("applyElementPropertiesButton"))->click(); QTRY_VERIFY(gate.entered.available() > 0);
    window.findChild<QPushButton*>(QStringLiteral("designerUndoButton"))->click(); QCOMPARE(x->value(), 5.0);
    gate.release.release(); QTRY_VERIFY(service.isIdle());
    QCOMPARE(repo.read().settings.templateDocuments.value(QStringLiteral("default")).layers[0].elements[0].x, 5.0);
}

void LabelDesignerTests::designerHideKeepsQueuedSession()
{
    QTemporaryDir dir; const auto path = dir.filePath("settings.json"); const auto settings = selectionTestSettings();
    sleekpr::infrastructure::SettingsRepository repo(path); QVERIFY(repo.replace(settings).ok());
    sleekpr::test::SettingsWriteGate gate{path}; SettingsPersistenceService service(path, gate.writer()); sleekpr::test::ReleaseSettingsGate unblock{gate};
    const auto id = service.openSession(QStringLiteral("designer")); QVERIFY(id); QTRY_VERIFY(service.isIdle());
    TemplateDesignerWindow window(settings, {}, dir.path()); window.attachPersistence(service, *id); window.show();
    gate.armed = true; auto* x = window.findChild<QDoubleSpinBox*>(QStringLiteral("elementXSpin")); x->setValue(7);
    window.findChild<QPushButton*>(QStringLiteral("applyElementPropertiesButton"))->click(); QTRY_VERIFY(gate.entered.available() > 0);
    window.close(); QVERIFY(!window.isVisible()); QVERIFY(window.hasUnsavedPersistenceChanges());
    window.show(); QCOMPARE(x->value(), 7.0); gate.release.release(); QTRY_VERIFY(service.isIdle());
}

void LabelDesignerTests::designerInvalidJsonSurvivesOtherSaveCompletion()
{
    QTemporaryDir dir; const auto path = dir.filePath("settings.json"); const auto settings = selectionTestSettings();
    sleekpr::infrastructure::SettingsRepository repo(path); QVERIFY(repo.replace(settings).ok());
    SettingsPersistenceService service(path); const auto id = service.openSession(QStringLiteral("designer")); QVERIFY(id); QTRY_VERIFY(service.isIdle());
    TemplateDesignerWindow window(settings, {}, dir.path()); window.attachPersistence(service, *id);
    auto* json = window.findChild<QPlainTextEdit*>(QStringLiteral("templateSampleDataEdit")); json->setPlainText(QStringLiteral("{broken"));
    window.findChild<QDoubleSpinBox*>(QStringLiteral("elementXSpin"))->setValue(7);
    window.findChild<QPushButton*>(QStringLiteral("applyElementPropertiesButton"))->click(); QTRY_VERIFY(service.isIdle());
    QCOMPARE(json->toPlainText(), QStringLiteral("{broken")); QVERIFY(window.hasInvalidPersistenceDraft()); QVERIFY(window.hasUnsavedPersistenceChanges());
}

namespace {
struct DesignerStoreFixture
{
    QTemporaryDir directory;
    QString path = directory.filePath(QStringLiteral("settings.json"));
    sleekpr::infrastructure::SettingsRepository repository{path};
    sleekpr::test::SettingsWriteGate gate{path};
    SettingsPersistenceService service{path, gate.writer()};
    explicit DesignerStoreFixture(const PrintClientSettings& settings = selectionTestSettings()) { repository.replace(settings); }
    ~DesignerStoreFixture() { gate.release.release(32); }
    bool attach(TemplateDesignerWindow& window)
    {
        const auto id = service.openSession(QStringLiteral("标签设计器"));
        if (!id) return false;
        window.attachPersistence(service, *id); return true;
    }
};
}

void LabelDesignerTests::designerConflictKeepsDraftAndHistory()
{
    DesignerStoreFixture f; int notifications = 0;
    TemplateDesignerWindow window(selectionTestSettings(), [&](const auto&) { ++notifications; }, f.directory.path());
    QVERIFY(f.attach(window)); QTRY_VERIFY(window.property("persistenceReady").toBool());
    auto external = selectionTestSettings(); external.templateDocuments[QStringLiteral("default")].name = QStringLiteral("external");
    QVERIFY(f.repository.replace(external).ok());
    auto* x = window.findChild<QDoubleSpinBox*>(QStringLiteral("elementXSpin"));
    auto* apply = window.findChild<QPushButton*>(QStringLiteral("applyElementPropertiesButton"));
    x->setValue(7); apply->click();
    QTRY_COMPARE(window.property("settingsSaveStatus").toInt(), int(SettingsWriteStatus::Conflict)); QCOMPARE(notifications, 0);
    x->setValue(8); apply->click(); QVERIFY(!f.service.hasRunningWork());
    window.findChild<QPushButton*>(QStringLiteral("designerUndoButton"))->click(); QCOMPARE(x->value(), 7.0);
    window.findChild<QPushButton*>(QStringLiteral("designerRedoButton"))->click(); QCOMPARE(x->value(), 8.0);
    QCOMPARE(f.repository.read().settings.templateDocuments.value(QStringLiteral("default")).name, QStringLiteral("external"));
    QCOMPARE(f.gate.writes.load(), 0);
}

void LabelDesignerTests::designerSaveFailureDoesNotAdvanceBaseline()
{
    DesignerStoreFixture f; int notifications = 0; f.gate.failMain = true;
    TemplateDesignerWindow window(selectionTestSettings(), [&](const auto&) { ++notifications; }, f.directory.path());
    QVERIFY(f.attach(window)); QTRY_VERIFY(window.property("persistenceReady").toBool());
    auto* x = window.findChild<QDoubleSpinBox*>(QStringLiteral("elementXSpin"));
    auto* apply = window.findChild<QPushButton*>(QStringLiteral("applyElementPropertiesButton"));
    x->setValue(7); apply->click(); QTRY_COMPARE(window.property("settingsSaveStatus").toInt(), int(SettingsWriteStatus::WriteFailed));
    QCOMPARE(notifications, 0); QCOMPARE(f.gate.writes.load(), 1);
    QCOMPARE(f.repository.read().settings.templateDocuments.value(QStringLiteral("default")).layers[0].elements[0].x, 5.0);
    f.gate.failMain = false; x->setValue(8); apply->click(); QCOMPARE(f.gate.writes.load(), 1);
    window.findChild<QPushButton*>(QStringLiteral("persistenceRetryButton"))->click(); QTRY_VERIFY(f.service.isIdle());
    QCOMPARE(notifications, 1); QCOMPARE(f.gate.writes.load(), 2);
    QCOMPARE(f.repository.read().settings.templateDocuments.value(QStringLiteral("default")).layers[0].elements[0].x, 8.0);
}

void LabelDesignerTests::failedDesignerCloseKeepsDraft()
{
    DesignerStoreFixture f; f.gate.failMain = true;
    TemplateDesignerWindow window(selectionTestSettings(), {}, f.directory.path());
    QVERIFY(f.attach(window)); QTRY_VERIFY(window.property("persistenceReady").toBool()); window.show();
    auto* x = window.findChild<QDoubleSpinBox*>(QStringLiteral("elementXSpin")); x->setValue(7);
    window.findChild<QPushButton*>(QStringLiteral("applyElementPropertiesButton"))->click();
    QTRY_COMPARE(window.property("settingsSaveStatus").toInt(), int(SettingsWriteStatus::WriteFailed));
    bool asked = false;
    QTimer::singleShot(0, [&] { if (auto* box = qobject_cast<QMessageBox*>(QApplication::activeModalWidget())) {
        asked = true; box->button(QMessageBox::No)->click();
    }});
    window.close(); QVERIFY(asked); QVERIFY(window.isVisible()); QCOMPARE(x->value(), 7.0);
}

void LabelDesignerTests::sampleSaveFailureNeverReportsSaved()
{
    DesignerStoreFixture f; f.gate.failMain = true;
    TemplateDesignerWindow window(selectionTestSettings(), {}, f.directory.path());
    QVERIFY(f.attach(window)); QTRY_VERIFY(window.property("persistenceReady").toBool());
    window.findChild<QPlainTextEdit*>(QStringLiteral("templateSampleDataEdit"))->setPlainText(QStringLiteral("{\"value\":1}"));
    window.findChild<QPushButton*>(QStringLiteral("saveTemplateSampleDataButton"))->click();
    QTRY_COMPARE(window.property("settingsSaveStatus").toInt(), int(SettingsWriteStatus::WriteFailed));
    QVERIFY(window.findChild<QLabel*>(QStringLiteral("designerStatusLabel"))->text().contains(QString::fromUtf8("未保存")));
}

void LabelDesignerTests::restoreAfterExternalCorruptionUsesObservedDigest()
{
    DesignerStoreFixture f; auto settings = selectionTestSettings(); settings.labelOffset.x = 1; QVERIFY(f.repository.replace(settings).ok());
    TemplateDesignerWindow window(settings, {}, f.directory.path()); QVERIFY(f.attach(window)); QTRY_VERIFY(window.property("persistenceReady").toBool());
    QVERIFY(sleekpr::test::writeSettingsBytes(f.path, "{broken"));
    window.findChild<QDoubleSpinBox*>(QStringLiteral("elementXSpin"))->setValue(7);
    window.findChild<QPushButton*>(QStringLiteral("applyElementPropertiesButton"))->click();
    QTRY_COMPARE(window.property("settingsSaveStatus").toInt(), int(SettingsWriteStatus::ReadFailed));
    QTimer::singleShot(0, [] { if (auto* box = qobject_cast<QMessageBox*>(QApplication::activeModalWidget())) box->button(QMessageBox::Yes)->click(); });
    window.findChild<QPushButton*>(QStringLiteral("persistenceRestoreButton"))->click();
    QTRY_VERIFY(f.service.isIdle()); QCOMPARE(f.repository.read().status, SettingsReadStatus::Ready);
    QCOMPARE(window.findChild<QDoubleSpinBox*>(QStringLiteral("elementXSpin"))->value(), 5.0);
}

void LabelDesignerTests::defaultDesignerIsNotDirtyUntilEdited()
{
    QTemporaryDir dir; SettingsPersistenceService service(dir.filePath("settings.json"));
    auto id = service.openSession(QStringLiteral("designer")); QVERIFY(id);
    TemplateDesignerWindow window({}, {}, dir.path()); window.attachPersistence(service, *id);
    QTRY_VERIFY(window.property("persistenceReady").toBool()); QVERIFY(!window.hasUnsavedPersistenceChanges());
}

void LabelDesignerTests::invalidSampleDraftRequiresCloseConfirmation()
{
    DesignerStoreFixture f; TemplateDesignerWindow window(selectionTestSettings(), {}, f.directory.path());
    QVERIFY(f.attach(window)); QTRY_VERIFY(window.property("persistenceReady").toBool()); window.show();
    auto* editor = window.findChild<QPlainTextEdit*>(QStringLiteral("templateSampleDataEdit"));
    const QString draft = QString::fromUtf8("{\n  \"商品名称\":"); editor->setPlainText(draft); QVERIFY(window.hasUnsavedPersistenceChanges());
    window.findChild<QDoubleSpinBox*>(QStringLiteral("elementXSpin"))->setValue(7);
    window.findChild<QPushButton*>(QStringLiteral("applyElementPropertiesButton"))->click(); QTRY_VERIFY(f.service.isIdle());
    QVERIFY(window.hasUnsavedPersistenceChanges()); QCOMPARE(editor->toPlainText(), draft);
    bool asked = false; QTimer::singleShot(0, [&] { if (auto* box = qobject_cast<QMessageBox*>(QApplication::activeModalWidget())) {
        asked = true; box->button(QMessageBox::No)->click();
    }});
    window.close(); QVERIFY(asked); QVERIFY(window.isVisible()); QCOMPARE(editor->toPlainText(), draft);
    window.hide(); window.show(); QCOMPARE(editor->toPlainText(), draft);
}

void LabelDesignerTests::pendingSettingsTimerIsUnsaved()
{
    DesignerStoreFixture f; TemplateDesignerWindow window(selectionTestSettings(), {}, f.directory.path());
    QVERIFY(f.attach(window)); QTRY_VERIFY(window.property("persistenceReady").toBool());
    auto* timer = window.findChild<QTimer*>(QStringLiteral("designerSettingsChangedTimer")); QVERIFY(timer);
    timer->start(10000); QVERIFY(window.hasUnsavedPersistenceChanges()); timer->stop();
}

void LabelDesignerTests::corruptSettingsCannotBeAutoRecreatedByEditing()
{
    DesignerStoreFixture f; QVERIFY(sleekpr::test::writeSettingsBytes(f.path, "{broken"));
    TemplateDesignerWindow window(selectionTestSettings(), {}, f.directory.path());
    QVERIFY(f.attach(window)); QTRY_VERIFY(window.property("persistenceReady").toBool());
    window.findChild<QDoubleSpinBox*>(QStringLiteral("elementXSpin"))->setValue(7);
    window.findChild<QPushButton*>(QStringLiteral("applyElementPropertiesButton"))->click();
    QCOMPARE(f.gate.writes.load(), 0); QCOMPARE(window.property("settingsSaveStatus").toInt(), int(SettingsWriteStatus::ReadFailed));
    QFile file(f.path); QVERIFY(file.open(QIODevice::ReadOnly)); QCOMPARE(file.readAll(), QByteArray("{broken"));
}

void LabelDesignerTests::reloadConflictRequiresConfirmation()
{
    DesignerStoreFixture f; TemplateDesignerWindow window(selectionTestSettings(), {}, f.directory.path());
    QVERIFY(f.attach(window)); QTRY_VERIFY(window.property("persistenceReady").toBool());
    auto external = selectionTestSettings(); external.templateDocuments[QStringLiteral("default")].name = QStringLiteral("external"); QVERIFY(f.repository.replace(external).ok());
    auto* x = window.findChild<QDoubleSpinBox*>(QStringLiteral("elementXSpin")); x->setValue(7);
    auto* apply = window.findChild<QPushButton*>(QStringLiteral("applyElementPropertiesButton")); apply->click();
    QTRY_COMPARE(window.property("settingsSaveStatus").toInt(), int(SettingsWriteStatus::Conflict));
    auto* reload = window.findChild<QPushButton*>(QStringLiteral("persistenceReloadButton")); bool asked = false;
    QTimer::singleShot(0, [&] { if (auto* box = qobject_cast<QMessageBox*>(QApplication::activeModalWidget())) {
        asked = true; box->button(QMessageBox::No)->click();
    }});
    reload->click(); QVERIFY(asked); QCOMPARE(x->value(), 7.0);
    QTimer::singleShot(0, [] { if (auto* box = qobject_cast<QMessageBox*>(QApplication::activeModalWidget())) box->button(QMessageBox::Yes)->click(); });
    reload->click(); QTRY_COMPARE(x->value(), 5.0);
    x->setValue(6); apply->click(); QTRY_VERIFY(f.service.isIdle());
    QCOMPARE(f.repository.read().settings.templateDocuments.value(QStringLiteral("default")).name, QStringLiteral("external"));
    QCOMPARE(f.repository.read().settings.templateDocuments.value(QStringLiteral("default")).layers[0].elements[0].x, 6.0);
}

void LabelDesignerTests::settingsLateSaveDoesNotRollbackNewRuntimeApply()
{
    QTemporaryDir dir; const auto path = dir.filePath("settings.json"); auto initial = selectionTestSettings(); initial.localHttpLimits.maxHeaderBytes = 8 * 1024;
    sleekpr::infrastructure::SettingsRepository repo(path); QVERIFY(repo.replace(initial).ok());
    sleekpr::test::SettingsWriteGate gate{path}; SettingsPersistenceService service(path, gate.writer()); sleekpr::test::ReleaseSettingsGate unblock{gate};
    PrintClientSettings applied;
    SettingsWindow window(path, service, [&](const auto& value) { applied = value; }); QTRY_VERIFY(window.property("persistenceReady").toBool());
    auto* spin = window.findChild<QSpinBox*>(QStringLiteral("settingsMaxHeaderKbSpin")); QVERIFY(spin);
    gate.armed = true; spin->setValue(16); window.findChild<QPushButton*>(QStringLiteral("saveSettingsButton"))->click(); QTRY_VERIFY(gate.entered.available() > 0);
    spin->setValue(24); window.findChild<QPushButton*>(QStringLiteral("applySettingsButton"))->click();
    gate.release.release(); QTRY_VERIFY(service.isIdle());
    QCOMPARE(repo.read().settings.localHttpLimits.maxHeaderBytes.value(), qsizetype(16 * 1024));
    QCOMPARE(applied.localHttpLimits.maxHeaderBytes.value(), qsizetype(24 * 1024)); QCOMPARE(spin->value(), 24);
    QVERIFY(window.hasUnsavedPersistenceChanges());
}

void LabelDesignerTests::settingsFailedTicketCannotDisplaceLaterRunningContext()
{
    QTemporaryDir dir; const auto path = dir.filePath("settings.json"); auto initial = selectionTestSettings(); initial.localHttpLimits.maxHeaderBytes = 8 * 1024;
    sleekpr::infrastructure::SettingsRepository repo(path); QVERIFY(repo.replace(initial).ok());
    sleekpr::test::SettingsWriteGate gate{path}; std::atomic<bool> fail24{false};
    SettingsPersistenceService service(path, [&](const QString& target, const QByteArray& bytes, QString* error) {
        if (target == path) {
            if (gate.armed.exchange(false)) { gate.entered.release(); gate.release.acquire(); }
            const auto header = QJsonDocument::fromJson(bytes).object().value("localHttpLimits").toObject().value("maxHeaderBytes").toInt();
            if (header == 16 * 1024 || (header == 24 * 1024 && fail24)) return false;
        }
        return sleekpr::test::writeSettingsBytes(target, bytes, error);
    }); sleekpr::test::ReleaseSettingsGate unblock{gate};
    SettingsWindow window(path, service, {}); QTRY_VERIFY(window.property("persistenceReady").toBool());
    auto* spin = window.findChild<QSpinBox*>(QStringLiteral("settingsMaxHeaderKbSpin"));
    auto* save = window.findChild<QPushButton*>(QStringLiteral("saveSettingsButton"));
    auto* retry = window.findChild<QPushButton*>(QStringLiteral("persistenceRetryButton")); QVERIFY(spin && save && retry);
    gate.armed = true; spin->setValue(16); save->click(); QTRY_VERIFY(gate.entered.available() > 0); QVERIFY(gate.entered.tryAcquire());
    spin->setValue(24); save->click(); gate.release.release(); QTRY_COMPARE(window.property("settingsSaveStatus").toInt(), int(SettingsWriteStatus::WriteFailed));
    retry->click(); QTRY_VERIFY(service.isIdle()); QCOMPARE(repo.read().settings.localHttpLimits.maxHeaderBytes.value(), qsizetype(24 * 1024));
    gate.armed = true; spin->setValue(32); save->click(); QTRY_VERIFY(gate.entered.available() > 0);
    fail24 = true; spin->setValue(24); save->click(); gate.release.release();
    QTRY_COMPARE(window.property("settingsSaveStatus").toInt(), int(SettingsWriteStatus::WriteFailed));
    QCOMPARE(repo.read().settings.localHttpLimits.maxHeaderBytes.value(), qsizetype(32 * 1024));
    save->click(); fail24 = false; retry->click(); QTRY_VERIFY(service.isIdle());
    QCOMPARE(spin->value(), 24); QVERIFY(!window.hasUnsavedPersistenceChanges());
    QCOMPARE(repo.read().settings.localHttpLimits.maxHeaderBytes.value(), qsizetype(24 * 1024));
}

void LabelDesignerTests::settingsEditAfterSaveIsNotImplicitlyPersisted()
{
    QTemporaryDir dir; const auto path = dir.filePath("settings.json"); auto initial = selectionTestSettings(); initial.localHttpLimits.maxHeaderBytes = 8 * 1024;
    sleekpr::infrastructure::SettingsRepository repo(path); QVERIFY(repo.replace(initial).ok());
    sleekpr::test::SettingsWriteGate gate{path}; SettingsPersistenceService service(path, gate.writer()); sleekpr::test::ReleaseSettingsGate unblock{gate};
    SettingsWindow window(path, service, {}); QTRY_VERIFY(window.property("persistenceReady").toBool());
    auto* spin = window.findChild<QSpinBox*>(QStringLiteral("settingsMaxHeaderKbSpin")); gate.armed = true; spin->setValue(16);
    window.findChild<QPushButton*>(QStringLiteral("saveSettingsButton"))->click(); QTRY_VERIFY(gate.entered.available() > 0);
    spin->setValue(24); gate.release.release(); QTRY_VERIFY(service.isIdle());
    QCOMPARE(repo.read().settings.localHttpLimits.maxHeaderBytes.value(), qsizetype(16 * 1024)); QCOMPARE(spin->value(), 24);
    QVERIFY(window.hasUnsavedPersistenceChanges()); window.hide(); window.show(); QCOMPARE(spin->value(), 24);
}

void LabelDesignerTests::settingsRevertDuringSaveIsNotTreatedAsUntouched()
{
    QTemporaryDir dir; const auto path = dir.filePath("settings.json"); auto initial = selectionTestSettings(); initial.localHttpLimits.maxHeaderBytes = 8 * 1024;
    sleekpr::infrastructure::SettingsRepository repo(path); QVERIFY(repo.replace(initial).ok());
    sleekpr::test::SettingsWriteGate gate{path}; SettingsPersistenceService service(path, gate.writer()); sleekpr::test::ReleaseSettingsGate unblock{gate};
    SettingsWindow window(path, service, {}); QTRY_VERIFY(window.property("persistenceReady").toBool());
    auto* spin = window.findChild<QSpinBox*>(QStringLiteral("settingsMaxHeaderKbSpin")); auto* save = window.findChild<QPushButton*>(QStringLiteral("saveSettingsButton"));
    gate.armed = true; spin->setValue(16); save->click(); QTRY_VERIFY(gate.entered.available() > 0);
    spin->setValue(8); save->click(); gate.release.release(); QTRY_VERIFY(service.isIdle());
    QCOMPARE(repo.read().settings.localHttpLimits.maxHeaderBytes.value(), qsizetype(8 * 1024)); QVERIFY(!window.hasUnsavedPersistenceChanges());
}

void LabelDesignerTests::settingsUntouchedFallbackDoesNotBecomeExplicit()
{
    QTemporaryDir dir; const auto path = dir.filePath("settings.json");
    sleekpr::infrastructure::SettingsRepository repo(path); QVERIFY(repo.replace(selectionTestSettings()).ok()); SettingsPersistenceService service(path);
    SettingsWindow window(path, service, {}); QTRY_VERIFY(window.property("persistenceReady").toBool());
    window.findChild<QDoubleSpinBox*>(QStringLiteral("settingsOffsetXSpin"))->setValue(2);
    window.findChild<QPushButton*>(QStringLiteral("saveSettingsButton"))->click(); QTRY_VERIFY(service.isIdle());
    QVERIFY(!repo.read().settings.localHttpLimits.maxHeaderBytes.has_value());
}

void LabelDesignerTests::settingsPreviewRefreshDoesNotCreateManualDraft()
{
    QTemporaryDir dir; const auto path = dir.filePath("settings.json"); SettingsPersistenceService service(path);
    SettingsWindow window(path, service, {}); QTRY_VERIFY(window.property("persistenceReady").toBool());
    QVERIFY(!window.exitDraftState().manualDraft);
    window.findChild<QPushButton*>(QStringLiteral("refreshSettingsPreviewButton"))->click();
    QVERIFY(!window.exitDraftState().manualDraft); QVERIFY(!window.hasUnsavedPersistenceChanges());
}

void LabelDesignerTests::resumePreservesExplicitDisabledChild()
{
    QTemporaryDir dir; const auto path = dir.filePath("settings.json"); SettingsPersistenceService service(path);
    SettingsWindow window(path, service, {}); QTRY_VERIFY(window.property("persistenceReady").toBool());
    auto* spin = window.findChild<QSpinBox*>(QStringLiteral("settingsMaxHeaderKbSpin")); spin->setEnabled(false);
    window.setExitPaused(true); QVERIFY(!window.isEnabled()); window.setExitPaused(false);
    QVERIFY(window.isEnabled()); QVERIFY(!spin->isEnabled());
}

void LabelDesignerTests::exitPauseDuringInitialLoadResumesUsableWindows()
{
    QTemporaryDir dir; const auto path = dir.filePath("settings.json"); auto initial = selectionTestSettings();
    sleekpr::infrastructure::SettingsRepository repo(path); QVERIFY(repo.replace(initial).ok());
    sleekpr::test::SettingsWriteGate gate{path}; SettingsPersistenceService service(path, gate.writer()); sleekpr::test::ReleaseSettingsGate unblock{gate};
    auto writer = service.openSession(QStringLiteral("writer")); QVERIFY(writer); QTRY_VERIFY(service.isIdle());
    initial.defaultPrinter = QStringLiteral("A"); gate.armed = true;
    QVERIFY(service.requestSave(*writer, initial, SettingsEditScope::AllChangedFields)); QTRY_VERIFY(gate.entered.available() > 0);
    SettingsWindow settings(path, service, {}); settings.setExitPaused(true);
    auto reader = service.openSession(QStringLiteral("designer")); QVERIFY(reader);
    TemplateDesignerWindow designer({}, {}, dir.path()); designer.attachPersistence(service, *reader); designer.setExitPaused(true);
    gate.release.release(); QTRY_VERIFY(settings.property("persistenceReady").toBool() && designer.property("persistenceReady").toBool());
    settings.setExitPaused(false); designer.setExitPaused(false);
    QVERIFY(settings.isEnabled()); QVERIFY(designer.isEnabled());
}

void LabelDesignerTests::settingsSavePreservesExternalTemplateAndUneditedLimits()
{
    QTemporaryDir directory;
    const auto path = directory.filePath(QStringLiteral("settings.json"));
    sleekpr::infrastructure::SettingsRepository repository(path);
    const auto settings = selectionTestSettings();
    QVERIFY(repository.replace(settings).ok());
    SettingsPersistenceService settingsPersistence_window(path);
    SettingsWindow window(path, settingsPersistence_window, [](const auto&) {});
    QTRY_VERIFY(window.property("persistenceReady").toBool());
    auto external = settings;
    external.localHttpLimits.maxPreviewPages = 12;
    external.templateDocuments[QStringLiteral("default")].name = QStringLiteral("external");
    QVERIFY(repository.replace(external).ok());
    auto* offset = window.findChild<QDoubleSpinBox*>(QStringLiteral("settingsOffsetXSpin"));
    QVERIFY(offset != nullptr);
    offset->setValue(2);
    window.findChild<QPushButton*>(QStringLiteral("saveSettingsButton"))->click();
    QTRY_VERIFY(settingsPersistence_window.isIdle());
    const auto actual = repository.read().settings;
    QCOMPARE(actual.templateDocuments.value(QStringLiteral("default")).name, QStringLiteral("external"));
    QCOMPARE(actual.localHttpLimits.maxPreviewPages.value(), 12);
    QCOMPARE(actual.labelOffset.x, 2.0);
}

void LabelDesignerTests::hiddenSettingsDoesNotRenderOnTemplateChange()
{
    QTemporaryDir directory;
    const auto path = directory.filePath(QStringLiteral("settings.json"));
    sleekpr::infrastructure::SettingsRepository repository(path);
    auto settings = selectionTestSettings();
    QVERIFY(repository.replace(settings).ok());
    SettingsPersistenceService settingsPersistence_window(path);
    SettingsWindow window(path, settingsPersistence_window, {});
    QTRY_VERIFY(window.property("persistenceReady").toBool());
    window.show();
    auto* preview = window.findChild<QLabel*>(QStringLiteral("settingsPreviewLabel"));
    QTRY_VERIFY(!preview->pixmap().isNull());
    window.hide();
    const auto imageKey = preview->pixmap().cacheKey();
    settings.templateDocuments[QStringLiteral("default")].layers.first().elements.first().x = 8;
    QVERIFY(repository.replace(settings).ok());
    window.requestExternalRefresh();
    QCOMPARE(preview->pixmap().cacheKey(), imageKey);
}

void LabelDesignerTests::visibleSettingsDraftSurvivesExternalChange()
{
    QTemporaryDir directory;
    const auto path = directory.filePath(QStringLiteral("settings.json"));
    sleekpr::infrastructure::SettingsRepository repository(path);
    auto settings = selectionTestSettings();
    QVERIFY(repository.replace(settings).ok());
    SettingsPersistenceService settingsPersistence_window(path);
    SettingsWindow window(path, settingsPersistence_window, {});
    QTRY_VERIFY(window.property("persistenceReady").toBool());
    window.show();
    auto* x = window.findChild<QDoubleSpinBox*>(QStringLiteral("settingsOffsetXSpin"));
    x->setValue(2);
    settings.templateDocuments[QStringLiteral("default")].name = QStringLiteral("external");
    QVERIFY(repository.replace(settings).ok());
    window.requestExternalRefresh();
    QCOMPARE(x->value(), 2.0);
}

void LabelDesignerTests::settingsPreviewIsAsynchronous()
{
    QTemporaryDir directory;
    const auto path = directory.filePath(QStringLiteral("settings.json"));
    QVERIFY(sleekpr::infrastructure::SettingsRepository(path).replace(selectionTestSettings()).ok());
    SettingsPersistenceService settingsPersistence_window(path);
    SettingsWindow window(path, settingsPersistence_window, {});
    QTRY_VERIFY(window.property("persistenceReady").toBool());
    auto* image = window.findChild<QLabel*>(QStringLiteral("settingsPreviewLabel"));
    QVERIFY(image->pixmap().isNull());
    window.show();
    QVERIFY(image->pixmap().isNull());
    QTRY_VERIFY(!image->pixmap().isNull());
}

void LabelDesignerTests::versionEntriesAndRuntimeDiagnostics()
{
    QTemporaryDir directory;
    const auto path = directory.filePath(QStringLiteral("settings.json"));
    auto settings = selectionTestSettings();
    settings.defaultPrinter = QStringLiteral("secret-printer-do-not-copy");
    QVERIFY(sleekpr::infrastructure::SettingsRepository(path).replace(settings).ok());
    BuildInfoDialog dialog(path);
    const auto data = dialog.diagnosticJson();
    QCOMPARE(data.value(QStringLiteral("programPath")).toString(), QCoreApplication::applicationFilePath());
    QCOMPARE(data.value(QStringLiteral("settingsPath")).toString(), path);
    QVERIFY(!QJsonDocument(data).toJson().contains("secret-printer-do-not-copy"));
    SettingsPersistenceService settingsPersistence_settingsWindow(path);
    SettingsWindow settingsWindow(path, settingsPersistence_settingsWindow, {});
    QTRY_VERIFY(settingsWindow.property("persistenceReady").toBool());
    QVERIFY(settingsWindow.findChild<QPushButton*>(QStringLiteral("settingsBuildInfoButton")) != nullptr);
    PrintWorkbenchWindow workbench(path, nullptr, [] { return QStringList{}; });
    QVERIFY(workbench.findChild<QPushButton*>(QStringLiteral("workbenchBuildInfoButton")) != nullptr);
}

void LabelDesignerTests::designerEditDispatchMetrics()
{
    if (qEnvironmentVariableIsEmpty("SLEEKPR_DISPATCH_METRICS")) QSKIP("仅显式采样时运行固定数据性能记录");
    QTemporaryDir directory;
    const auto path = directory.filePath(QStringLiteral("settings.json"));
    sleekpr::infrastructure::SettingsRepository repository(path);
    auto settings = selectionTestSettings();
    auto& layer = settings.templateDocuments[QStringLiteral("default")].layers.first();
    layer.elements.clear();
    for (int i = 0; i < 32; ++i) {
        auto element = TemplateDesignerFactory::createElement(TemplateElementType::FixedText);
        element.id = QStringLiteral("perf-%1").arg(i);
        element.layerId = layer.id;
        element.text = QStringLiteral("ABCDEFGHIJK12345678");
        element.x = (i % 8) * 9;
        element.y = (i / 8) * 6;
        element.width = 9;
        element.height = 5;
        element.autoFitFont = true;
        layer.elements.append(element);
    }
    layer.elements.first().type = TemplateElementType::Rectangle;
    QVERIFY(repository.replace(settings).ok());
    SettingsPersistenceService settingsPersistence_hiddenSettings(path);
    SettingsWindow hiddenSettings(path, settingsPersistence_hiddenSettings, {});
    QTRY_VERIFY(hiddenSettings.property("persistenceReady").toBool());
    TemplateDesignerWindow window(settings, [&](const auto&) { hiddenSettings.requestExternalRefresh(); }, directory.path());
    SettingsPersistenceService persistence(path);
    const auto session = persistence.openSession(QStringLiteral("designer"));
    QVERIFY(session);
    window.attachPersistence(persistence, *session);
    QTRY_VERIFY(window.property("persistenceReady").toBool());
    window.show();
    auto* preview = window.findChild<QLabel*>(QStringLiteral("designerPreviewLabel"));
    QTRY_VERIFY(!preview->pixmap().isNull());
    auto* x = window.findChild<QDoubleSpinBox*>(QStringLiteral("elementXSpin"));
    auto* apply = window.findChild<QPushButton*>(QStringLiteral("applyElementPropertiesButton"));
    auto* input = x->findChild<QLineEdit*>();
    QVERIFY(input && apply);
    QList<double> samples, dragSamples;
    for (int i = 0; i < 20; ++i) {
        QElapsedTimer elapsed;
        elapsed.start();
        input->setFocus(); input->selectAll();
        QTest::keyClicks(input, QString::number(1 + i * 0.1, 'f', 2));
        QTest::keyClick(input, Qt::Key_Return);
        samples.append(elapsed.nsecsElapsed() / 1000000.0);
        QTest::qWait(20);
    }
    std::sort(samples.begin(), samples.end());
    QTRY_VERIFY(window.property("previewReady").toBool());
    auto* canvas = dynamic_cast<TemplatePreviewLabel*>(preview); QVERIFY(canvas);
    for (int i = 0; i < 10; ++i) {
        const auto before = x->value();
        const auto start = canvasPoint(canvas, before + 2, 2);
        const auto distance = QApplication::startDragDistance() + 2;
        QElapsedTimer elapsed; elapsed.start();
        dragCanvas(canvas, start, start + QPoint(i % 2 == 0 ? distance : -distance, 0));
        dragSamples.append(elapsed.nsecsElapsed() / 1000000.0);
        QVERIFY(x->value() != before);
        QTRY_VERIFY(window.property("previewReady").toBool());
    }
    QTRY_VERIFY(persistence.isIdle());
    std::sort(dragSamples.begin(), dragSamples.end());
    const QJsonObject result{{QStringLiteral("mode"), BuildInfo::current().configuration}, {QStringLiteral("elements"), 32},
        {QStringLiteral("samples"), samples.size()}, {QStringLiteral("dispatchP50Ms"), samples.at(9)},
        {QStringLiteral("dispatchP95Ms"), samples.at(18)},
        {QStringLiteral("dragDispatchP50Ms"), dragSamples.at(4)}, {QStringLiteral("dragDispatchP95Ms"), dragSamples.at(9)},
        {QStringLiteral("stages"), sleekpr::infrastructure::ClientPerformanceMetrics::applicationMetrics().snapshot()}};
    QSaveFile output(qEnvironmentVariable("SLEEKPR_DISPATCH_METRICS"));
    QVERIFY(output.open(QIODevice::WriteOnly));
    const auto json = QJsonDocument(result).toJson();
    QCOMPARE(output.write(json), json.size());
    QVERIFY(output.commit());
    qInfo().noquote() << QJsonDocument(result).toJson(QJsonDocument::Compact);
}

void LabelDesignerTests::initTestCase()
{
    applySleekprTheme(*qApp);
    QVERIFY(!QPixmap(QStringLiteral(":/sleekpr/ui/caret-down.png")).isNull());
    QVERIFY(!QPixmap(QStringLiteral(":/sleekpr/ui/check.png")).isNull());
}

void LabelDesignerTests::boundFieldPickerShowsChineseLabelAndSavesKey()
{
    QTemporaryDir directory;
    auto settings = selectionTestSettings();
    auto& document = settings.templateDocuments[QStringLiteral("default")];
    document.layers.first().elements.resize(1);
    auto& element = document.layers.first().elements.first();
    element.type = TemplateElementType::BoundField;
    element.fieldKey = QStringLiteral("productName");
    FieldDefinition sales;
    sales.key = QStringLiteral("salesCode");
    sales.displayName = QString::fromUtf8("销售编码");
    document.fieldSchema = {sales};
    PrintClientSettings changed;
    TemplateDesignerWindow window(settings, [&changed](const auto& next) { changed = next; }, directory.path());
    window.resize(1024, 720);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    auto* combo = window.findChild<QComboBox*>(QStringLiteral("elementFieldBindingCombo"));
    QVERIFY2(combo != nullptr, "绑定字段必须提供中文名称与原始 key 的选择器");
    QCOMPARE(combo->currentText(), QStringLiteral("productName"));
    auto* fieldName = window.findChild<QLineEdit*>(QStringLiteral("elementFieldDisplayNameEdit"));
    QVERIFY(fieldName != nullptr);
    QCOMPARE(fieldName->text(), QString::fromUtf8("商品名称"));
    QCOMPARE(combo->currentData().toString(), QStringLiteral("productName"));
    QVERIFY(combo->isVisible());
    const auto screenshot = qEnvironmentVariable("SLEEKPR_TEST_SCREENSHOT");
    if (!screenshot.isEmpty()) {
        QTest::qWait(100);
        QVERIFY(window.grab().save(screenshot + QStringLiteral(".binding.png")));
    }
    const auto index = combo->findData(QStringLiteral("salesCode"));
    QVERIFY(index >= 0);
    QCOMPARE(combo->itemText(index), QString::fromUtf8("销售编码（salesCode）"));
    combo->setCurrentIndex(index);
    auto* apply = window.findChild<QPushButton*>(QStringLiteral("applyElementPropertiesButton"));
    QVERIFY(apply != nullptr);
    apply->click();
    const auto actual = changed.templateDocuments.value(QStringLiteral("default"));
    QVERIFY(!actual.layers.isEmpty());
    QCOMPARE(actual.layers.first().elements.first().fieldKey, QStringLiteral("salesCode"));
    auto* save = window.findChild<QPushButton*>(QStringLiteral("saveTemplateToLibraryButton"));
    QVERIFY(save != nullptr);
    save->click();
    const auto stored = TemplateLibraryStore(directory.path()).loadTemplate(document.id);
    QVERIFY(stored.has_value());
    QCOMPARE(stored->layers.first().elements.first().fieldKey, QStringLiteral("salesCode"));
    auto* undo = window.findChild<QPushButton*>(QStringLiteral("designerUndoButton"));
    auto* redo = window.findChild<QPushButton*>(QStringLiteral("designerRedoButton"));
    QVERIFY(undo != nullptr && redo != nullptr);
    undo->click();
    QCOMPARE(combo->currentData().toString(), QStringLiteral("productName"));
    redo->click();
    QCOMPARE(combo->currentData().toString(), QStringLiteral("salesCode"));
}

void LabelDesignerTests::fieldPickerKeepsKeysSeparateFromLabels()
{
    FieldBindingSelector selector;
    const QList<DesignerFieldOption> options{
        {QStringLiteral("productName"), QString::fromUtf8("商品名称")},
        {QStringLiteral("otherName"), QString::fromUtf8("商品名称")},
    };
    QSignalSpy edits(&selector, &FieldBindingSelector::fieldKeyEdited);
    const auto legacy = QString::fromUtf8("商品名称（productName）");
    selector.setOptionsAndValue(options, legacy);
    auto* combo = selector.findChild<QComboBox*>();
    QVERIFY(combo != nullptr);
    QCOMPARE(selector.fieldKey(), legacy);
    QCOMPARE(combo->currentData().toString(), legacy);
    QCOMPARE(edits.count(), 0);
    combo->setCurrentIndex(combo->findData(QStringLiteral("otherName")));
    QCOMPARE(selector.fieldKey(), QStringLiteral("otherName"));
    QCOMPARE(edits.count(), 1);
    selector.setOptionsAndValue(options, QString::fromUtf8("原有中文字段"));
    QCOMPARE(selector.fieldKey(), QString::fromUtf8("原有中文字段"));
    QCOMPARE(edits.count(), 1);
}

void LabelDesignerTests::fieldPickerUsesSchemaNamesAndOnlyCurrentDocument()
{
    TemplateDocument document;
    FieldDefinition field;
    field.key = QStringLiteral("productName");
    field.displayName = QString::fromUtf8("本店商品名称");
    document.fieldSchema = {field};
    document.sampleData = QJsonObject{{QStringLiteral("storeName"), QStringLiteral("private-sample-value")}};
    TemplateElement element;
    element.type = TemplateElementType::BoundField;
    element.fieldKey = QStringLiteral("legacy_name");
    TemplateDesignerPresenter presenter;
    const auto model = presenter.elementPropertyModel(element, true, &document);
    FieldBindingSelector selector;
    selector.setOptionsAndValue(model.fieldOptions, model.value);
    auto* combo = selector.findChild<QComboBox*>();
    QVERIFY(combo != nullptr);
    QCOMPARE(combo->itemText(combo->findData(QStringLiteral("productName"))), QString::fromUtf8("本店商品名称（productName）"));
    QVERIFY(combo->findData(QStringLiteral("storeName")) >= 0);
    QCOMPARE(combo->findText(QStringLiteral("private-sample-value")), -1);
    QCOMPARE(combo->currentData().toString(), QStringLiteral("legacy_name"));
    const auto other = presenter.elementPropertyModel(element, true);
    selector.setOptionsAndValue(other.fieldOptions, other.value);
    QCOMPARE(combo->findData(QStringLiteral("storeName")), -1);
}

void LabelDesignerTests::fieldPickerAcceptsAndCancelsCustomInput()
{
    FieldBindingSelector selector;
    selector.setOptionsAndValue({{QStringLiteral("productName"), QString::fromUtf8("商品名称")}}, QStringLiteral("productName"));
    selector.resize(300, 40);
    selector.show();
    QVERIFY(QTest::qWaitForWindowExposed(&selector));
    auto* button = selector.findChild<QPushButton*>(QStringLiteral("elementCustomFieldButton"));
    QVERIFY(button != nullptr);
    QSignalSpy edits(&selector, &FieldBindingSelector::fieldKeyEdited);
    QTimer::singleShot(0, [] {
        auto* dialog = qobject_cast<QInputDialog*>(QApplication::activeModalWidget());
        if (dialog != nullptr) {
            dialog->setTextValue(QStringLiteral("custom_code"));
            dialog->accept();
        }
    });
    button->click();
    QCOMPARE(selector.fieldKey(), QStringLiteral("custom_code"));
    QCOMPARE(edits.count(), 1);
    QTimer::singleShot(0, [] {
        auto* dialog = qobject_cast<QInputDialog*>(QApplication::activeModalWidget());
        if (dialog != nullptr) {
            dialog->setTextValue(QStringLiteral("discarded"));
            dialog->reject();
        }
    });
    button->click();
    QCOMPARE(selector.fieldKey(), QStringLiteral("custom_code"));
    QCOMPARE(edits.count(), 1);
}

void LabelDesignerTests::fieldPickerRefreshIsSilentAndFitsNarrowPanel()
{
    FieldBindingSelector selector;
    const QList<DesignerFieldOption> options{{QStringLiteral("long_key"), QString(120, QChar(0x540d))}};
    QSignalSpy edits(&selector, &FieldBindingSelector::fieldKeyEdited);
    selector.setOptionsAndValue(options, QStringLiteral("long_key"));
    selector.resize(260, 40);
    selector.show();
    QVERIFY(QTest::qWaitForWindowExposed(&selector));
    selector.setOptionsAndValue(options, QStringLiteral("long_key"));
    QCOMPARE(edits.count(), 0);
    QVERIFY(selector.width() <= 260);
    for (const auto* child : selector.findChildren<QWidget*>(QString(), Qt::FindDirectChildrenOnly)) {
        QVERIFY(selector.rect().contains(child->geometry()));
    }
}

void LabelDesignerTests::bindingKeyAndDisplayNameCanBeEditedSeparately()
{
    QTemporaryDir directory;
    auto settings = selectionTestSettings();
    auto& document = settings.templateDocuments[QStringLiteral("default")];
    document.layers.first().elements.resize(1);
    auto& element = document.layers.first().elements.first();
    element.type = TemplateElementType::BoundField;
    element.fieldKey = QStringLiteral("productName");
    PrintClientSettings changed;
    TemplateDesignerWindow window(settings, [&changed](const auto& next) { changed = next; }, directory.path());
    auto* name = window.findChild<QLineEdit*>(QStringLiteral("elementFieldDisplayNameEdit"));
    QVERIFY2(name != nullptr, "绑定 key 与 displayName 必须分别编辑");
    QCOMPARE(name->text(), QString::fromUtf8("商品名称"));
    name->setText(QString::fromUtf8("门店品名"));
    auto* apply = window.findChild<QPushButton*>(QStringLiteral("applyElementPropertiesButton"));
    QVERIFY(apply != nullptr);
    apply->click();
    const auto actual = changed.templateDocuments.value(QStringLiteral("default"));
    QVERIFY(!actual.layers.isEmpty());
    QCOMPARE(actual.layers.first().elements.first().fieldKey, QStringLiteral("productName"));
    const auto json = TemplateDocumentJson::toJson(actual);
    QCOMPARE(json.value(QStringLiteral("fieldDisplayNames")).toObject().value(QStringLiteral("productName")).toString(),
        QString::fromUtf8("门店品名"));
    QVERIFY(actual.fieldSchema.isEmpty());
    auto* save = window.findChild<QPushButton*>(QStringLiteral("saveTemplateToLibraryButton"));
    QVERIFY(save != nullptr);
    save->click();
    const auto stored = TemplateLibraryStore(directory.path()).loadTemplate(document.id);
    QVERIFY(stored.has_value());
    QCOMPARE(TemplateDocumentJson::toJson(*stored).value(QStringLiteral("fieldDisplayNames")), json.value(QStringLiteral("fieldDisplayNames")));
    window.findChild<QPushButton*>(QStringLiteral("designerUndoButton"))->click();
    QCOMPARE(name->text(), QString::fromUtf8("商品名称"));
    window.findChild<QPushButton*>(QStringLiteral("designerRedoButton"))->click();
    QCOMPARE(name->text(), QString::fromUtf8("门店品名"));
}

void LabelDesignerTests::bindingNameAutoAppliesAndStaysWithinTemplate()
{
    auto settings = selectionTestSettings();
    auto& document = settings.templateDocuments[QStringLiteral("default")];
    document.layers.first().elements.resize(2);
    for (auto& element : document.layers.first().elements) {
        element.type = TemplateElementType::BoundField;
        element.fieldKey = QStringLiteral("productName");
    }
    PrintClientSettings changed;
    TemplateDesignerWindow window(settings, [&changed](const auto& next) { changed = next; });
    window.resize(1024, 720);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    auto* name = window.findChild<QLineEdit*>(QStringLiteral("elementFieldDisplayNameEdit"));
    auto* list = window.findChild<QListWidget*>(QStringLiteral("templateElementList"));
    QVERIFY(name != nullptr && list != nullptr);
    name->setText(QString::fromUtf8("店内品名"));
    QTRY_COMPARE(changed.templateDocuments.value(QStringLiteral("default")).fieldDisplayNames.value(QStringLiteral("productName")), QString::fromUtf8("店内品名"));
    list->setCurrentRow(1);
    QCOMPARE(name->text(), QString::fromUtf8("店内品名"));
    QCOMPARE(changed.templateDocuments.value(QStringLiteral("default")).layers.first().elements[1].fieldKey, QStringLiteral("productName"));
    const auto screenshot = qEnvironmentVariable("SLEEKPR_TEST_SCREENSHOT");
    if (!screenshot.isEmpty()) QVERIFY(window.grab().save(screenshot + QStringLiteral(".binding-name.png")));

    TemplateElement element = document.layers.first().elements.first();
    TemplateDesignerPresenter presenter;
    const auto otherModel = presenter.elementPropertyModel(element, true, &document);
    QCOMPARE(otherModel.fieldDisplayName, QString::fromUtf8("商品名称"));
    PrintFieldEditorPanel fields;
    fields.setDocument(changed.templateDocuments.value(QStringLiteral("default")));
    fields.setValues(QJsonObject{{"productName", "actual-value"}});
    auto* input = fields.findChild<QLineEdit*>(QStringLiteral("printField_productName"));
    auto* form = fields.findChild<QFormLayout*>();
    QVERIFY(input != nullptr && form != nullptr);
    auto* label = qobject_cast<QLabel*>(form->labelForField(input->parentWidget()));
    QVERIFY(label != nullptr && label->text().contains(QString::fromUtf8("店内品名")));
    QCOMPARE(fields.values(), QJsonObject({{"productName", "actual-value"}}));
}

void LabelDesignerTests::bindingNameCommandPreservesRulesAndBlocksLockedEdits()
{
    auto document = selectionTestSettings().templateDocuments.value(QStringLiteral("default"));
    auto& element = document.layers.first().elements.first();
    element.type = TemplateElementType::BoundField;
    element.fieldKey = QStringLiteral("productName");
    FieldDefinition field;
    field.key = QStringLiteral("productName");
    field.displayName = QString::fromUtf8("原品名");
    field.required = true;
    field.defaultValue = QStringLiteral("original-default");
    document.fieldSchema = {field};
    const auto rules = FieldSchemaJson::toJson(document.fieldSchema);
    TemplateDesignerPresenter presenter;
    auto model = presenter.elementPropertyModel(element, true, &document);
    model.x += 1;
    QVERIFY(presenter.applyElementProperties(document, element.id, model).changed);
    QVERIFY(document.fieldDisplayNames.isEmpty());
    model.fieldDisplayNameEdited = true;
    model.fieldDisplayName = QString::fromUtf8("新解释名称");
    QVERIFY(presenter.applyElementProperties(document, element.id, model).changed);
    QCOMPARE(FieldSchemaJson::toJson(document.fieldSchema), rules);
    QCOMPARE(document.layers.first().elements.first().fieldKey, QStringLiteral("productName"));
    QVERIFY(!presenter.applyElementProperties(document, element.id, model).changed);
    const auto before = TemplateDocumentJson::toJson(document);
    model.canEdit = false;
    model.fieldDisplayName = QStringLiteral("blocked");
    const auto locked = presenter.applyElementProperties(document, element.id, model);
    QVERIFY(!locked.changed && !locked.errorMessage.isEmpty());
    QCOMPARE(TemplateDocumentJson::toJson(document), before);
    model.canEdit = true;
    model.fieldDisplayName.clear();
    QVERIFY(presenter.applyElementProperties(document, element.id, model).changed);
    QVERIFY(document.fieldDisplayNames.isEmpty());
    QCOMPARE(FieldSchemaJson::toJson(document.fieldSchema), rules);
}

void LabelDesignerTests::bindingKeyChangeDoesNotRenameDataOrOtherElements()
{
    auto settings = selectionTestSettings();
    auto& document = settings.templateDocuments[QStringLiteral("default")];
    document.layers.first().elements.resize(2);
    for (auto& element : document.layers.first().elements) {
        element.type = TemplateElementType::BoundField;
        element.fieldKey = QStringLiteral("productName");
    }
    document.sampleData = QJsonObject{{"productName", "original"}, {"salesCode", "code"}};
    document.fieldDisplayNames.insert(QStringLiteral("productName"), QString::fromUtf8("品名说明"));
    const auto sample = document.sampleData;
    const auto& first = document.layers.first().elements.first();
    TemplateDesignerPresenter presenter;
    auto model = presenter.elementPropertyModel(first, true, &document);
    model.value = QStringLiteral("salesCode");
    QVERIFY(presenter.applyElementProperties(document, first.id, model).changed);
    QCOMPARE(document.layers.first().elements.first().fieldKey, QStringLiteral("salesCode"));
    QCOMPARE(document.layers.first().elements[1].fieldKey, QStringLiteral("productName"));
    QCOMPARE(document.sampleData, sample);
    QVERIFY(!document.fieldDisplayNames.contains(QStringLiteral("salesCode")));
    QCOMPARE(document.fieldDisplayNames.value(QStringLiteral("productName")), QString::fromUtf8("品名说明"));
}

void LabelDesignerTests::bindingKeySupportsDirectInputWithoutCarryingOldName()
{
    FieldBindingSelector selector;
    selector.setOptionsAndValue({{QStringLiteral("productName"), QString::fromUtf8("商品名称")},
        {QStringLiteral("salesCode"), QString::fromUtf8("销售码")}}, QStringLiteral("productName"));
    auto* combo = selector.findChild<QComboBox*>();
    auto* name = selector.findChild<QLineEdit*>(QStringLiteral("elementFieldDisplayNameEdit"));
    QVERIFY(combo != nullptr && name != nullptr && combo->isEditable());
    QVERIFY(!combo->lineEdit()->isReadOnly());
    selector.resize(300, 140);
    selector.show();
    QVERIFY(QTest::qWaitForWindowExposed(&selector));
    combo->lineEdit()->selectAll();
    QTest::keyClicks(combo->lineEdit(), "custom_code");
    QCOMPARE(selector.fieldKey(), QStringLiteral("custom_code"));
    QCOMPARE(combo->currentText(), QStringLiteral("custom_code"));
    QVERIFY(name->text().isEmpty());
    QVERIFY(!selector.isDisplayNameEdited());
    name->setText(QString::fromUtf8("自定义码"));
    QVERIFY(selector.isDisplayNameEdited());
    combo->setCurrentIndex(combo->findData(QStringLiteral("salesCode")));
    QCOMPARE(selector.fieldKey(), QStringLiteral("salesCode"));
    QCOMPARE(combo->currentText(), QStringLiteral("salesCode"));
    QCOMPARE(name->text(), QString::fromUtf8("销售码"));
    QVERIFY(!selector.isDisplayNameEdited());
}

void LabelDesignerTests::bindingFastKeyChangeKeepsPreviousName()
{
    auto settings = selectionTestSettings();
    auto& element = settings.templateDocuments[QStringLiteral("default")].layers.first().elements.first();
    element.type = TemplateElementType::BoundField;
    element.fieldKey = QStringLiteral("productName");
    PrintClientSettings changed;
    TemplateDesignerWindow window(settings, [&changed](const auto& next) { changed = next; });
    window.show();
    window.activateWindow();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    auto* name = window.findChild<QLineEdit*>(QStringLiteral("elementFieldDisplayNameEdit"));
    auto* combo = window.findChild<QComboBox*>(QStringLiteral("elementFieldBindingCombo"));
    QVERIFY(name != nullptr && combo != nullptr);
    name->setFocus();
    name->setText(QString::fromUtf8("快速修改的品名"));
    combo->lineEdit()->setFocus();
    combo->lineEdit()->selectAll();
    QTest::keyClicks(combo->lineEdit(), "salesCode");
    QTRY_COMPARE(changed.templateDocuments.value(QStringLiteral("default")).layers.first().elements.first().fieldKey, QStringLiteral("salesCode"));
    const auto document = changed.templateDocuments.value(QStringLiteral("default"));
    QCOMPARE(document.fieldDisplayNames.value(QStringLiteral("productName")), QString::fromUtf8("快速修改的品名"));
    QVERIFY(!document.fieldDisplayNames.contains(QStringLiteral("salesCode")));
    QCOMPARE(name->text(), QString::fromUtf8("销售码"));
}

void LabelDesignerTests::bindingNameKeepsExplicitSchemaNameEqualToKey()
{
    TemplateDocument document;
    FieldDefinition field;
    field.key = QStringLiteral("productName");
    field.displayName = field.key;
    document.fieldSchema = {field};
    TemplateElement element;
    element.type = TemplateElementType::BoundField;
    element.fieldKey = field.key;
    TemplateDesignerPresenter presenter;
    for (const auto& name : {QString(), QStringLiteral("  ")}) {
        document.fieldDisplayNames.insert(field.key, name);
        QCOMPARE(presenter.elementPropertyModel(element, true, &document).fieldDisplayName, field.displayName);
    }
    document.fieldDisplayNames.insert(field.key, QString::fromUtf8("自定义解释"));
    QCOMPARE(presenter.elementPropertyModel(element, true, &document).fieldDisplayName, QString::fromUtf8("自定义解释"));
    document.fieldDisplayNames.clear();
    document.fieldSchema.first().displayName.clear();
    QCOMPARE(presenter.elementPropertyModel(element, true, &document).fieldDisplayName, QString::fromUtf8("商品名称"));
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
    SettingsPersistenceService settingsPersistence_window(directory.filePath(QStringLiteral("settings.json")));
    SettingsWindow window(directory.filePath(QStringLiteral("settings.json")), settingsPersistence_window, nullptr);
    QTRY_VERIFY(window.property("persistenceReady").toBool());
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

void LabelDesignerTests::canvasSupportsMultiSelectionAndDuplicate()
{
    auto changed = selectionTestSettings();
    TemplateDesignerWindow window(changed, [&changed](const auto& next) { changed = next; });
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    QTest::qWait(50);
    auto* preview = dynamic_cast<TemplatePreviewLabel*>(window.findChild<QLabel*>(QStringLiteral("designerPreviewLabel")));
    auto* list = window.findChild<QListWidget*>(QStringLiteral("templateElementList"));
    QVERIFY(preview != nullptr && list != nullptr);
    QTest::mouseClick(preview, Qt::LeftButton, Qt::NoModifier, canvasPoint(preview, 8, 8));
    QTest::mouseClick(preview, Qt::LeftButton, Qt::ControlModifier, canvasPoint(preview, 28, 8));
    QCOMPARE(list->selectedItems().size(), 2);
    QTest::keyClick(preview, Qt::Key_D, Qt::ControlModifier);
    QTRY_COMPARE(changed.templateDocuments[QStringLiteral("default")].layers.first().elements.size(), 5);
    QCOMPARE(list->selectedItems().size(), 2);
    QTest::keyClick(preview, Qt::Key_Z, Qt::ControlModifier);
    QTRY_COMPARE(changed.templateDocuments[QStringLiteral("default")].layers.first().elements.size(), 3);
    QTest::keyClick(preview, Qt::Key_Z, Qt::ControlModifier | Qt::ShiftModifier);
    QTRY_COMPARE(changed.templateDocuments[QStringLiteral("default")].layers.first().elements.size(), 5);
}

void LabelDesignerTests::canvasGestureCommitsOneUndoStep()
{
    auto changed = selectionTestSettings();
    int notifications = 0;
    TemplateDesignerWindow window(changed, [&](const auto& next) { changed = next; ++notifications; });
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    QTest::qWait(50);
    auto* preview = dynamic_cast<TemplatePreviewLabel*>(window.findChild<QLabel*>(QStringLiteral("designerPreviewLabel")));
    QVERIFY(preview != nullptr);
    dragCanvas(preview, canvasPoint(preview, 8, 8), canvasPoint(preview, 15, 12));
    QTRY_VERIFY(changed.templateDocuments[QStringLiteral("default")].layers.first().elements.first().x > 10.0);
    QCOMPARE(notifications, 1);
    QTest::keyClick(preview, Qt::Key_Z, Qt::ControlModifier);
    QTRY_COMPARE(changed.templateDocuments[QStringLiteral("default")].layers.first().elements.first().x, 5.0);
    QVERIFY(!window.findChild<QPushButton*>(QStringLiteral("designerUndoButton"))->isEnabled());
}

void LabelDesignerTests::canvasMarqueeResizeClipboardAndAlignment()
{
    auto changed = selectionTestSettings();
    TemplateDesignerWindow window(changed, [&changed](const auto& next) { changed = next; });
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    QTest::qWait(50);
    auto* preview = dynamic_cast<TemplatePreviewLabel*>(window.findChild<QLabel*>(QStringLiteral("designerPreviewLabel")));
    auto* list = window.findChild<QListWidget*>(QStringLiteral("templateElementList"));
    QVERIFY(preview != nullptr && list != nullptr);
    dragCanvas(preview, canvasPoint(preview, 2, 2), canvasPoint(preview, 37, 14));
    QCOMPARE(list->selectedItems().size(), 2);
    auto* align = window.findChild<QAction*>(QStringLiteral("designerAlign0Action"));
    QVERIFY(align != nullptr);
    align->trigger();
    QCOMPARE(changed.templateDocuments[QStringLiteral("default")].layers.first().elements[1].x, 5.0);

    QTest::mouseClick(preview, Qt::LeftButton, Qt::NoModifier, canvasPoint(preview, 75, 25));
    QCOMPARE(list->selectedItems().size(), 0);
    list->setCurrentRow(2);
    const auto handle = preview->editorResizeHandleRect(Qt::RightEdge | Qt::BottomEdge);
    QVERIFY(!handle.isEmpty());
    dragCanvas(preview, handle.center(), handle.center() + QPoint(30, 15));
    const auto resized = changed.templateDocuments[QStringLiteral("default")].layers.first().elements[2];
    QVERIFY(resized.width > 12.0);
    QVERIFY(resized.height > 7.0);

    QTest::keyClick(preview, Qt::Key_C, Qt::ControlModifier);
    QTest::keyClick(preview, Qt::Key_V, Qt::ControlModifier);
    QTRY_COMPARE(changed.templateDocuments[QStringLiteral("default")].layers.first().elements.size(), 4);
    QTRY_COMPARE(preview->designAidCommandCount(), 4);
    const auto pasted = changed.templateDocuments[QStringLiteral("default")].layers.first().elements.last();
    QVERIFY(pasted.id != resized.id);
    QCOMPARE(pasted.width, resized.width);
    QCOMPARE(pasted.height, resized.height);
    QCOMPARE(pasted.type, resized.type);
    QCOMPARE(list->selectedItems().size(), 1);
    const auto screenshotPath = qEnvironmentVariable("SLEEKPR_TEST_SCREENSHOT");
    if (!screenshotPath.isEmpty()) QVERIFY(window.grab().save(screenshotPath + QStringLiteral(".selection.png")));

    QTest::keyClick(preview, Qt::Key_Delete);
    QCOMPARE(changed.templateDocuments[QStringLiteral("default")].layers.first().elements.size(), 3);
    QTest::keyClick(preview, Qt::Key_Z, Qt::ControlModifier);
    QTRY_COMPARE(changed.templateDocuments[QStringLiteral("default")].layers.first().elements.size(), 4);
}

void LabelDesignerTests::canvasSlowDragAndCancelAreTransactional()
{
    auto changed = selectionTestSettings();
    int notifications = 0;
    TemplateDesignerWindow window(changed, [&](const auto& next) { changed = next; ++notifications; });
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    QTest::qWait(50);
    auto* preview = dynamic_cast<TemplatePreviewLabel*>(window.findChild<QLabel*>(QStringLiteral("designerPreviewLabel")));
    QVERIFY(preview != nullptr);
    const auto start = canvasPoint(preview, 8, 8);
    const auto finish = canvasPoint(preview, 16, 14);
    QTest::mousePress(preview, Qt::LeftButton, Qt::NoModifier, start);
    QMouseEvent move(QEvent::MouseMove, finish, preview->mapToGlobal(finish), Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(preview, &move);
    QTest::qWait(320);
    QCOMPARE(notifications, 0);
    QTest::keyClick(preview, Qt::Key_Escape);
    QTest::mouseRelease(preview, Qt::LeftButton, Qt::NoModifier, finish);
    QTest::qWait(280);
    QCOMPARE(notifications, 0);
    QVERIFY(!window.findChild<QPushButton*>(QStringLiteral("designerUndoButton"))->isEnabled());
    dragCanvas(preview, start, finish);
    QCOMPARE(notifications, 1);
    QTest::keyClick(preview, Qt::Key_Z, Qt::ControlModifier);
    QTRY_COMPARE(changed.templateDocuments[QStringLiteral("default")].layers.first().elements.first().x, 5.0);
}

void LabelDesignerTests::canvasMapsDeviceCalibrationBackToMillimeters()
{
    auto changed = selectionTestSettings();
    auto& document = changed.templateDocuments[QStringLiteral("default")];
    DeviceProfile profile;
    profile.scaleX = 1.2;
    profile.scaleY = 0.8;
    profile.offsetX = 2.0;
    profile.offsetY = 1.0;
    document.deviceProfiles = {profile};
    TemplateDesignerWindow window(changed, [&changed](const auto& next) { changed = next; });
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    QTest::qWait(50);
    auto* preview = dynamic_cast<TemplatePreviewLabel*>(window.findChild<QLabel*>(QStringLiteral("designerPreviewLabel")));
    auto* snap = window.findChild<QAction*>(QStringLiteral("designerSnapAction"));
    QVERIFY(preview != nullptr && snap != nullptr);
    snap->setChecked(false);
    const auto start = canvasPoint(preview, (8.0 + 2.0) * 1.2, (8.0 + 1.0) * 0.8);
    const auto finish = canvasPoint(preview, (12.0 + 2.0) * 1.2, (10.0 + 1.0) * 0.8);
    dragCanvas(preview, start, finish);
    const auto element = changed.templateDocuments[QStringLiteral("default")].layers.first().elements.first();
    QVERIFY(qAbs(element.x - 9.0) < 0.25);
    QVERIFY(qAbs(element.y - 7.0) < 0.25);
}

void LabelDesignerTests::canvasRejectsUnsafeClipboardElements()
{
    auto changed = selectionTestSettings();
    TemplateDesignerWindow window(changed, [&changed](const auto& next) { changed = next; });
    auto* paste = window.findChild<QAction*>(QStringLiteral("designerPasteAction"));
    QVERIFY(paste != nullptr);
    auto invalid = changed.templateDocuments[QStringLiteral("default")];
    invalid.layers.first().elements.first().width = -2.0;
    const auto setClipboard = [](const TemplateDocument& document) {
        auto* mime = new QMimeData();
        mime->setData("application/x-sleekpr-label-elements+json", QJsonDocument(TemplateDocumentJson::toJson(document)).toJson());
        QApplication::clipboard()->setMimeData(mime);
    };
    setClipboard(invalid);
    paste->trigger();
    QCOMPARE(changed.templateDocuments[QStringLiteral("default")].layers.first().elements.size(), 3);

    invalid = changed.templateDocuments[QStringLiteral("default")];
    invalid.layers.first().elements.first().type = TemplateElementType::ArrayGrid;
    invalid.layers.first().elements.first().arrayGridRows = 100000;
    invalid.layers.first().elements.first().arrayGridColumns = 100000;
    setClipboard(invalid);
    paste->trigger();
    QCOMPARE(changed.templateDocuments[QStringLiteral("default")].layers.first().elements.size(), 3);
}

void LabelDesignerTests::canvasAltDragCancelRestoresOriginalAndNoopHasNoHistory()
{
    auto changed = selectionTestSettings();
    TemplateDesignerWindow window(changed, [&changed](const auto& next) { changed = next; });
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    QTest::qWait(50);
    auto* preview = dynamic_cast<TemplatePreviewLabel*>(window.findChild<QLabel*>(QStringLiteral("designerPreviewLabel")));
    auto* undo = window.findChild<QPushButton*>(QStringLiteral("designerUndoButton"));
    QVERIFY(preview != nullptr && undo != nullptr);
    const auto start = canvasPoint(preview, 8, 8);
    const auto finish = canvasPoint(preview, 17, 15);
    QTest::mousePress(preview, Qt::LeftButton, Qt::AltModifier, start);
    QMouseEvent move(QEvent::MouseMove, finish, preview->mapToGlobal(finish), Qt::NoButton, Qt::LeftButton, Qt::AltModifier);
    QApplication::sendEvent(preview, &move);
    QTest::keyClick(preview, Qt::Key_Escape);
    QTest::mouseRelease(preview, Qt::LeftButton, Qt::AltModifier, finish);
    QTest::qWait(30);
    QCOMPARE(preview->designAidCommandCount(), 3);
    QVERIFY(!undo->isEnabled());
    QTest::mousePress(preview, Qt::LeftButton, Qt::NoModifier, start);
    QApplication::sendEvent(preview, &move);
    QMouseEvent back(QEvent::MouseMove, start, preview->mapToGlobal(start), Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(preview, &back);
    QTest::mouseRelease(preview, Qt::LeftButton, Qt::NoModifier, start);
    QVERIFY(!undo->isEnabled());
    QCOMPARE(changed.templateDocuments[QStringLiteral("default")].layers.first().elements.size(), 3);
}

void LabelDesignerTests::canvasWindowCanBeDestroyedDuringDrag()
{
    auto settings = selectionTestSettings();
    int notifications = 0;
    auto window = std::make_unique<TemplateDesignerWindow>(settings, [&](const auto&) { ++notifications; });
    window->show();
    QVERIFY(QTest::qWaitForWindowExposed(window.get()));
    auto* preview = dynamic_cast<TemplatePreviewLabel*>(window->findChild<QLabel*>(QStringLiteral("designerPreviewLabel")));
    QVERIFY(preview != nullptr);
    const auto start = canvasPoint(preview, 8, 8);
    const auto finish = canvasPoint(preview, 18, 16);
    QTest::mousePress(preview, Qt::LeftButton, Qt::NoModifier, start);
    QMouseEvent move(QEvent::MouseMove, finish, preview->mapToGlobal(finish), Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(preview, &move);
    window.reset();
    QCoreApplication::processEvents();
    QCOMPARE(notifications, 0);
}

void LabelDesignerTests::canvasSmallEdgeElementHasVisibleHandlesAndMoveArea()
{
    auto settings = selectionTestSettings();
    auto& element = settings.templateDocuments[QStringLiteral("default")].layers.first().elements.first();
    element.x = 79.4;
    element.y = 29.4;
    element.width = 0.5;
    element.height = 0.5;
    TemplateDesignerWindow window(settings, nullptr);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    QTest::qWait(50);
    auto* preview = dynamic_cast<TemplatePreviewLabel*>(window.findChild<QLabel*>(QStringLiteral("designerPreviewLabel")));
    QVERIFY(preview != nullptr);
    const auto center = canvasPoint(preview, 79.65, 29.65);
    QCOMPARE(preview->editorResizeHandleAt(center), Qt::Edges{});
    const auto handle = preview->editorResizeHandleRect(Qt::RightEdge | Qt::BottomEdge);
    QVERIFY(!handle.isEmpty());
    QVERIFY(preview->rect().contains(handle));
}

void LabelDesignerTests::settingsOffersPrintWorkbenchEntry()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    SettingsPersistenceService settingsPersistence_window(directory.filePath(QStringLiteral("settings.json")));
    SettingsWindow window(directory.filePath(QStringLiteral("settings.json")), settingsPersistence_window, nullptr);
    QTRY_VERIFY(window.property("persistenceReady").toBool());
    auto* button = window.findChild<QPushButton*>(QStringLiteral("openPrintWorkbenchButton"));
    QVERIFY2(button != nullptr, "设置页必须提供独立打印工作台入口");
}

int main(int argc, char** argv)
{
    QApplication application(argc, argv);
    sleekpr::infrastructure::PreviewExecutor executor;
    LabelDesignerTests tests;
    return QTest::qExec(&tests, argc, argv);
}
#include "tst_label_designer.moc"

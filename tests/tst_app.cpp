#include <QtTest/QtTest>

#include <QApplication>
#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QTemporaryDir>

#include "sleekpr/app/TemplateDragCoordinateMapper.h"
#include "sleekpr/app/TemplateElementHitTester.h"
#include "sleekpr/app/TemplatePreviewLabel.h"
#include "sleekpr/app/TemplateDesignerWindow.h"
#include "sleekpr/app/WindowActivation.h"
#include "sleekpr/app/SettingsWindow.h"
#include "sleekpr/core/native/NativeDrawCommand.h"
#include "sleekpr/core/settings/FileSettingsStore.h"
#include "sleekpr/core/settings/TemplateElement.h"

using namespace sleekpr::app;
using namespace sleekpr::core;

class AppTests final : public QObject
{
    Q_OBJECT

private slots:
    void openingAndSavingSettingsDoesNotCreateImplicitElementOverride();
    void startupWindowActivationShowsNormalWindow();
    void dragCoordinateMapperConvertsPixelDeltaToMillimeters();
    void templatePreviewLabelReportsDragDelta();
    void templatePreviewLabelRejectsDragStartOutsideEditableArea();
    void settingsWindowDoesNotMoveElementWhenDraggingBlankPreviewArea();
    void templateElementHitTesterSelectsSmallestTopmostElement();
    void settingsWindowSelectsElementByClickingPreview();
    void settingsWindowSelectsElementFromPreviewWhenListSelectionIsCleared();
    void settingsWindowDragsElementSelectedFromPreview();
    void settingsWindowNudgesSelectedElementWithArrowKeys();
    void settingsWindowSnapsPreviewDragToGridWhenEnabled();
    void settingsWindowAddsFixedTextTemplateElement();
    void templateDesignerWindowAddsLayer();
    void templateDesignerWindowPreservesLegacyDynamicElements();
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

void AppTests::settingsWindowDoesNotMoveElementWhenDraggingBlankPreviewArea()
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
    auto* elementXSpin = window.findChild<QDoubleSpinBox*>(QStringLiteral("elementXSpin"));
    QVERIFY(previewLabel != nullptr);
    QVERIFY(elementXSpin != nullptr);

    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    const double originalX = elementXSpin->value();
    QTest::mousePress(previewLabel, Qt::LeftButton, Qt::NoModifier, QPoint(900, 320));
    QTest::mouseMove(previewLabel, QPoint(930, 330));
    QTest::mouseRelease(previewLabel, Qt::LeftButton, Qt::NoModifier, QPoint(930, 330));

    QCOMPARE(elementXSpin->value(), originalX);
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

void AppTests::settingsWindowSelectsElementByClickingPreview()
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
    auto* elementList = window.findChild<QListWidget*>();
    QVERIFY(previewLabel != nullptr);
    QVERIFY(elementList != nullptr);

    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    QTest::mouseClick(previewLabel, Qt::LeftButton, Qt::NoModifier, QPoint(50, 50));

    QCOMPARE(elementList->currentItem()->data(Qt::UserRole).toString(), QStringLiteral("qrCode"));
}

void AppTests::settingsWindowSelectsElementFromPreviewWhenListSelectionIsCleared()
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
    auto* elementList = window.findChild<QListWidget*>();
    QVERIFY(previewLabel != nullptr);
    QVERIFY(elementList != nullptr);

    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    elementList->setCurrentRow(-1);
    QVERIFY(elementList->currentItem() == nullptr);

    QTest::mouseClick(previewLabel, Qt::LeftButton, Qt::NoModifier, QPoint(50, 50));

    QVERIFY(elementList->currentItem() != nullptr);
    QCOMPARE(elementList->currentItem()->data(Qt::UserRole).toString(), QStringLiteral("qrCode"));
}

void AppTests::settingsWindowDragsElementSelectedFromPreview()
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
    auto* elementList = window.findChild<QListWidget*>();
    auto* elementXSpin = window.findChild<QDoubleSpinBox*>(QStringLiteral("elementXSpin"));
    QVERIFY(previewLabel != nullptr);
    QVERIFY(elementList != nullptr);
    QVERIFY(elementXSpin != nullptr);

    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    QVERIFY(elementList->currentItem() != nullptr);
    QVERIFY(elementList->currentItem()->data(Qt::UserRole).toString() != QStringLiteral("qrCode"));

    QTest::mousePress(previewLabel, Qt::LeftButton, Qt::NoModifier, QPoint(50, 50));
    QTest::mouseMove(previewLabel, QPoint(80, 50));
    QTest::mouseRelease(previewLabel, Qt::LeftButton, Qt::NoModifier, QPoint(80, 50));

    QCOMPARE(elementList->currentItem()->data(Qt::UserRole).toString(), QStringLiteral("qrCode"));
    QVERIFY(elementXSpin->value() > 1.8);
}

void AppTests::settingsWindowNudgesSelectedElementWithArrowKeys()
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
    auto* elementXSpin = window.findChild<QDoubleSpinBox*>(QStringLiteral("elementXSpin"));
    auto* elementYSpin = window.findChild<QDoubleSpinBox*>(QStringLiteral("elementYSpin"));
    QVERIFY(previewLabel != nullptr);
    QVERIFY(elementXSpin != nullptr);
    QVERIFY(elementYSpin != nullptr);

    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    previewLabel->setFocus();

    QTest::keyClick(previewLabel, Qt::Key_Right);
    QCOMPARE(elementXSpin->value(), 0.1);

    QTest::keyClick(previewLabel, Qt::Key_Down, Qt::ShiftModifier);
    QCOMPARE(elementYSpin->value(), 1.0);
}

void AppTests::settingsWindowSnapsPreviewDragToGridWhenEnabled()
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
    auto* snapToGridCheck = window.findChild<QCheckBox*>(QStringLiteral("snapToGridCheck"));
    auto* elementXSpin = window.findChild<QDoubleSpinBox*>(QStringLiteral("elementXSpin"));
    QVERIFY(previewLabel != nullptr);
    QVERIFY(snapToGridCheck != nullptr);
    QVERIFY(elementXSpin != nullptr);

    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    snapToGridCheck->setChecked(true);
    QTest::mousePress(previewLabel, Qt::LeftButton, Qt::NoModifier, QPoint(5, 5));
    QTest::mouseMove(previewLabel, QPoint(9, 5));
    QTest::mouseRelease(previewLabel, Qt::LeftButton, Qt::NoModifier, QPoint(9, 5));

    QCOMPARE(elementXSpin->value(), 0.5);
}

void AppTests::settingsWindowAddsFixedTextTemplateElement()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const auto settingsPath = dir.filePath("settings.json");
    FileSettingsStore store(settingsPath);
    QVERIFY(store.save(PrintClientSettings{}));

    SettingsWindow window(settingsPath, nullptr);
    auto* addFixedTextButton = window.findChild<QPushButton*>(QStringLiteral("addFixedTextButton"));
    auto* customElementTextEdit = window.findChild<QLineEdit*>(QStringLiteral("customElementTextEdit"));
    auto* saveButton = window.findChild<QPushButton*>(QStringLiteral("saveSettingsButton"));

    QVERIFY(addFixedTextButton != nullptr);
    QVERIFY(customElementTextEdit != nullptr);
    QVERIFY(saveButton != nullptr);

    addFixedTextButton->click();
    customElementTextEdit->setText(QStringLiteral("STATIC TEXT"));
    saveButton->click();

    const auto actual = store.load();
    const auto elements = actual.templateElements.value("default");
    QCOMPARE(elements.size(), 1);
    QCOMPARE(elements.first().type, TemplateElementType::FixedText);
    QCOMPARE(elements.first().text, QString("STATIC TEXT"));
    QVERIFY(!elements.first().id.isEmpty());
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

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    AppTests tests;
    return QTest::qExec(&tests, argc, argv);
}

#include "tst_app.moc"

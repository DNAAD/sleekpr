#include "sleekpr/app/ApplicationExitSmokeTest.h"
#include "sleekpr/app/ApplicationExitCoordinator.h"
#include "sleekpr/app/ApplicationExitDialog.h"
#include "sleekpr/app/SettingsWindow.h"
#include "sleekpr/app/TemplateDesignerWindow.h"
#include "sleekpr/infrastructure/settings/SettingsRepository.h"
#include <QApplication>
#include <QDebug>
#include <QDoubleSpinBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTemporaryDir>
#include <QTimer>

namespace sleekpr::app {
int runApplicationExitSmokeTest(QApplication& app)
{
    QTemporaryDir directory;
    if (!directory.isValid()) return 2;
    const auto path = directory.filePath(QStringLiteral("settings.json"));
    infrastructure::SettingsRepository repository(path);
    core::PrintClientSettings seed;
    core::TemplateDocument document; document.id = QStringLiteral("exit-smoke"); document.name = QString::fromUtf8("退出回归");
    core::TemplateLayer layer; layer.id = QStringLiteral("base");
    core::TemplateElement element; element.id = QStringLiteral("rect"); element.layerId = layer.id;
    element.type = core::TemplateElementType::Rectangle; element.x = 1; element.y = 1; element.width = 10; element.height = 5;
    layer.elements = {element}; document.layers = {layer}; seed.templateDocuments.insert(QStringLiteral("default"), document);
    if (!repository.replace(seed).ok()) return 2;
    SettingsPersistenceService service(path);
    SettingsWindow settings(path, service, {});
    TemplateDesignerWindow designer(seed, {}, directory.path());
    const auto session = service.openSession(QString::fromUtf8("标签设计器"));
    if (!session) return 2;
    designer.attachPersistence(service, *session);
    settings.setAttribute(Qt::WA_DontShowOnScreen); designer.setAttribute(Qt::WA_DontShowOnScreen);
    int step = 0, quitCalls = 0;
    ApplicationExitCoordinator coordinator(service, [](bool) {}, [&] {
        ++quitCalls;
        if (step != 4 || quitCalls != 1 || designer.hasUnsavedPersistenceChanges()) {
            qCritical() << "退出保护过早结束或回调重复"; app.exit(1); return;
        }
        qInfo() << "退出保护进程级回归通过"; app.exit(0);
    });
    coordinator.addParticipant({QStringLiteral("settings"), QString::fromUtf8("设置"),
        [&] { settings.flushPersistenceEdits(); }, [&] { return settings.exitDraftState(); },
        [&] { settings.saveForExit(); }, [&](bool value) { settings.setExitPaused(value); }, {}});
    coordinator.addParticipant({QStringLiteral("designer"), QString::fromUtf8("标签设计器"),
        [&] { designer.flushPersistenceEdits(); }, [&] { return designer.exitDraftState(); }, {},
        [&](bool value) { designer.setExitPaused(value); }, {}});
    ApplicationExitDialog dialog(coordinator); dialog.setAttribute(Qt::WA_DontShowOnScreen);
    QTimer progress, timeout;
    progress.setInterval(10); timeout.setSingleShot(true);
    const auto fail = [&](const char* message) { qCritical() << message; progress.stop(); app.exit(1); };
    QObject::connect(&timeout, &QTimer::timeout, &timeout, [&] { fail("退出保护回归超时"); });
    QObject::connect(&progress, &QTimer::timeout, &progress, [&] {
        if (!settings.property("persistenceReady").toBool() || !designer.property("persistenceReady").toBool()) return;
        auto* x = designer.findChild<QDoubleSpinBox*>(QStringLiteral("elementXSpin"));
        auto* apply = designer.findChild<QPushButton*>(QStringLiteral("applyElementPropertiesButton"));
        auto* json = designer.findChild<QPlainTextEdit*>(QStringLiteral("templateSampleDataEdit"));
        if (!x || !apply || !json) { fail("缺少实际设计器控件"); return; }
        if (step == 0) { x->setValue(2); apply->click(); step = 1; return; }
        if (step == 1) {
            if (!service.isIdle() || designer.hasUnsavedPersistenceChanges()) return;
            const auto saved = repository.read().settings.templateDocuments.value(QStringLiteral("default"));
            if (saved.layers.isEmpty() || saved.layers[0].elements[0].x != 2) { fail("保存内容未落盘"); return; }
            coordinator.requestExit(); coordinator.cancelExit(); step = 2; return;
        }
        if (step == 2) {
            if (quitCalls != 0 || !settings.isEnabled() || !designer.isEnabled()) { fail("取消退出后状态未恢复"); return; }
            json->setPlainText(QStringLiteral("{broken")); coordinator.requestExit();
            if (coordinator.state() != ExitState::Blocked) { fail("非法草稿未阻止退出"); return; }
            coordinator.cancelExit();
            if (json->toPlainText() != QStringLiteral("{broken")) { fail("返回编辑时丢失非法原文"); return; }
            json->setPlainText(QStringLiteral("{\"value\":1}"));
            designer.findChild<QPushButton*>(QStringLiteral("saveTemplateSampleDataButton"))->click(); step = 3; return;
        }
        if (step == 3 && service.isIdle() && !designer.hasUnsavedPersistenceChanges()) {
            step = 4; progress.stop(); coordinator.requestExit();
        }
    });
    timeout.start(10000); progress.start();
    // 此处是独立进程的唯一事件循环，不嵌套生产事件循环，也不创建 HTTP、托盘或真实打印引擎。
    return app.exec();
}
} // 命名空间 sleekpr::app

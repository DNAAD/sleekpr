#include "sleekpr/app/AppTheme.h"
#include "sleekpr/app/BuildInfo.h"
#include "sleekpr/app/FieldPresetManagerWindow.h"
#include "sleekpr/app/PaperSpecManagerWindow.h"
#include "sleekpr/app/PrintWorkbenchWindow.h"
#include "sleekpr/app/SettingsWindow.h"
#include "sleekpr/app/SettingsPersistenceService.h"
#include "sleekpr/app/ApplicationExitCoordinator.h"
#include "sleekpr/app/ApplicationExitDialog.h"
#include "sleekpr/app/ApplicationExitSmokeTest.h"
#include "sleekpr/app/TemplateDesignerWindow.h"
#include "sleekpr/app/WindowActivation.h"
#include "sleekpr/core/settings/FileSettingsStore.h"
#include "sleekpr/http/LocalHttpServer.h"
#include "sleekpr/infrastructure/printing/QtLabelPrintEngine.h"
#include "sleekpr/infrastructure/preview/PreviewExecutor.h"
#include "sleekpr/infrastructure/diagnostics/ClientPerformanceMetrics.h"
#include <QJsonDocument>

#include <QAction>
#include <QApplication>
#include <QDebug>
#include <QElapsedTimer>
#include <QDir>
#include <QFileInfo>
#include <QHostAddress>
#include <QMenu>
#include <QMessageBox>
#include <QPointer>
#include <QPushButton>
#include <QStandardPaths>
#include <QStyle>
#include <QSystemTrayIcon>
#include <QTemporaryDir>
#include <QTimer>

#ifdef Q_OS_WIN
#include <qt_windows.h>
#endif

#include <memory>
#include <cstdio>
#include <cstring>

namespace {

QString settingsPath()
{
    const auto dataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    // 设置文件放在系统应用数据目录，避免写入安装目录造成权限问题。
    QDir().mkpath(dataDir);
    return QDir(dataDir).filePath("settings.json");
}

QString templateLibraryDirectoryPath(const QString& settingsFilePath)
{
    // Qt 设计器和本地 HTTP 模板库共用同一目录，避免出现两个互不相见的模板来源。
    return QFileInfo(settingsFilePath).absoluteDir().filePath(QStringLiteral("templates"));
}

QString fieldPresetFilePath(const QString& settingsFilePath)
{
    // 字段预设跟随 settings.json 存放，保证 HTTP、打印和 Qt 管理界面读取同一份配置。
    return QFileInfo(settingsFilePath).absoluteDir().filePath(QStringLiteral("field-presets.json"));
}

} // 匿名命名空间

int main(int argc, char* argv[])
{
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--build-info-json") == 0) {
            // 诊断必须早于设置目录、HTTP 和托盘初始化，不能为了识别版本启动另一个客户端。
            QCoreApplication diagnostic(argc, argv);
            const auto json = QJsonDocument(sleekpr::app::BuildInfo::current().toJson()).toJson(QJsonDocument::Compact);
            const auto written = std::fwrite(json.constData(), 1, static_cast<size_t>(json.size()), stdout);
            std::fflush(stdout);
            return written == static_cast<size_t>(json.size()) ? 0 : 1;
        }
    }
    QApplication app(argc, argv);
    // 应用作用域执行器先于所有窗口创建、晚于窗口销毁，并在 QApplication 清理字体服务前收束线程。
    sleekpr::infrastructure::PreviewExecutor previewExecutor;
    QApplication::setApplicationName("sleekpr");
    QApplication::setApplicationVersion(sleekpr::app::BuildInfo::current().version);
    QApplication::setOrganizationName("zytxt");
    QApplication::setQuitOnLastWindowClosed(false);
    sleekpr::app::applySleekprTheme(app);
    qInfo().noquote() << "sleekpr build:" << QJsonDocument(sleekpr::app::BuildInfo::current().toJson()).toJson(QJsonDocument::Compact);
    // 新冒烟入口必须先于真实配置、HTTP、托盘和打印引擎创建，禁止用未知参数探测旧程序。
    if (app.arguments().contains(QStringLiteral("--smoke-test-safe-exit"))) return sleekpr::app::runApplicationExitSmokeTest(app);

    const auto designerSmokeTest = app.arguments().contains(QStringLiteral("--smoke-test-designer"));
    std::unique_ptr<QTemporaryDir> smokeSettings;
    if (designerSmokeTest) {
        // 进程级回归使用临时配置且不监听 HTTP、不显示托盘、不调用打印，不能触碰操作员的数据与设备。
        smokeSettings = std::make_unique<QTemporaryDir>();
        if (!smokeSettings->isValid()) return 2;
#ifdef Q_OS_WIN
        SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
#endif
    }
    const auto settingsFilePath = designerSmokeTest
        ? smokeSettings->filePath(QStringLiteral("settings.json")) : settingsPath();
    qInfo() << "sleekpr settings:" << settingsFilePath;
    sleekpr::app::SettingsPersistenceService persistenceService(settingsFilePath);
    sleekpr::infrastructure::QtLabelPrintEngine printEngine;
    sleekpr::http::LocalHttpServer server(
        settingsFilePath,
        &printEngine,
        sleekpr::http::LocalHttpLimits::fromSettings(sleekpr::core::FileSettingsStore(settingsFilePath).load()));
    // 设置窗口和模板设计器共用同一个纸张规格管理窗口，避免两个入口编辑不同实例导致状态不同步。
    std::unique_ptr<sleekpr::app::PaperSpecManagerWindow> paperSpecManagerWindow;
    std::unique_ptr<sleekpr::app::FieldPresetManagerWindow> fieldPresetManagerWindow;
    std::unique_ptr<sleekpr::app::TemplateDesignerWindow> templateDesignerWindow;
    std::unique_ptr<sleekpr::app::PrintWorkbenchWindow> printWorkbenchWindow;
    bool exiting = false;
    std::optional<bool> paperWasEnabled, presetWasEnabled;
    sleekpr::app::SettingsWindow settingsWindow(
        settingsFilePath,
        persistenceService,
        [&server](const sleekpr::core::PrintClientSettings& settings) {
            // 本地接口上限保存后立即影响后续请求，不需要重启托盘服务。
            server.setLimits(sleekpr::http::LocalHttpLimits::fromSettings(settings));
        },
        templateLibraryDirectoryPath(settingsFilePath));
    sleekpr::app::ApplicationExitCoordinator exitCoordinator(persistenceService,
        [&](bool value) {
            exiting = value; server.setExiting(value);
            const auto pause = [value](auto& window, std::optional<bool>& previous) {
                if (!window) return;
                if (value) { previous = window->isEnabled(); window->setEnabled(false); }
                else if (previous) { window->setEnabled(*previous); previous.reset(); }
            };
            pause(paperSpecManagerWindow, paperWasEnabled); pause(fieldPresetManagerWindow, presetWasEnabled);
        }, [&app] { app.quit(); });
    sleekpr::app::ApplicationExitDialog exitDialog(exitCoordinator);
    if (designerSmokeTest) settingsWindow.setAttribute(Qt::WA_DontShowOnScreen);
    const auto openFieldPresets = [&settingsFilePath, &fieldPresetManagerWindow, &printWorkbenchWindow, &exiting] {
        if (exiting) return;
        const auto presetPath = fieldPresetFilePath(settingsFilePath);
        if (fieldPresetManagerWindow == nullptr) {
            fieldPresetManagerWindow = std::make_unique<sleekpr::app::FieldPresetManagerWindow>(presetPath,
                [&printWorkbenchWindow] {
                    if (printWorkbenchWindow != nullptr) printWorkbenchWindow->requestExternalRefresh();
                });
        }
        fieldPresetManagerWindow->reloadFromDisk();
        sleekpr::app::showAndActivateWindow(*fieldPresetManagerWindow);
    };
    settingsWindow.setOpenFieldPresetManagerCallback(openFieldPresets);
    const auto openDesigner = [&settingsFilePath, &settingsWindow, &persistenceService, &paperSpecManagerWindow, &openFieldPresets, &templateDesignerWindow, &printWorkbenchWindow, &exiting, designerSmokeTest] {
        if (exiting) return;
        // 保存失败的隐藏窗口仍拥有草稿，不允许重复打开入口直接析构它。
        if (templateDesignerWindow != nullptr && templateDesignerWindow->hasUnsavedPersistenceChanges()) {
            sleekpr::app::showAndActivateWindow(*templateDesignerWindow);
            return;
        }
        const auto session = persistenceService.openSession(QString::fromUtf8("标签设计器"));
        if (!session) return;
        templateDesignerWindow = std::make_unique<sleekpr::app::TemplateDesignerWindow>(
            sleekpr::core::PrintClientSettings{},
            [&settingsWindow, &printWorkbenchWindow](const sleekpr::core::PrintClientSettings&) {
                // 这里只通知成功提交，写入和失败处理由编辑会话负责。
                settingsWindow.requestExternalRefresh();
                if (printWorkbenchWindow != nullptr) printWorkbenchWindow->refreshFieldDisplayNames();
            },
            templateLibraryDirectoryPath(settingsFilePath));
        templateDesignerWindow->attachPersistence(persistenceService, *session);
        if (designerSmokeTest) templateDesignerWindow->setAttribute(Qt::WA_DontShowOnScreen);
        templateDesignerWindow->setOpenPaperSpecManagerCallback([&settingsFilePath, &paperSpecManagerWindow, &templateDesignerWindow, &exiting] {
            if (exiting) return;
            const auto paperSpecPath = QFileInfo(settingsFilePath).absoluteDir().filePath(QStringLiteral("paper-specs.json"));
            if (paperSpecManagerWindow == nullptr) {
                paperSpecManagerWindow = std::make_unique<sleekpr::app::PaperSpecManagerWindow>(
                    paperSpecPath,
                    [&templateDesignerWindow] {
                        if (templateDesignerWindow != nullptr) {
                            // 纸张规格保存后立即刷新设计器下拉框和预览尺寸。
                            templateDesignerWindow->reloadPaperSpecsFromDisk();
                        }
                    });
            }
            paperSpecManagerWindow->reloadFromDisk();
            sleekpr::app::showAndActivateWindow(*paperSpecManagerWindow);
        });
        templateDesignerWindow->setOpenFieldPresetManagerCallback(openFieldPresets);
        sleekpr::app::showAndActivateWindow(*templateDesignerWindow);
    };
    settingsWindow.setOpenTemplateDesignerCallback(openDesigner);
    settingsWindow.setOpenPaperSpecManagerCallback([&settingsFilePath, &paperSpecManagerWindow, &templateDesignerWindow, &exiting] {
        if (exiting) return;
        const auto paperSpecPath = QFileInfo(settingsFilePath).absoluteDir().filePath(QStringLiteral("paper-specs.json"));
        if (paperSpecManagerWindow == nullptr) {
            paperSpecManagerWindow = std::make_unique<sleekpr::app::PaperSpecManagerWindow>(
                paperSpecPath,
                [&templateDesignerWindow] {
                    if (templateDesignerWindow != nullptr) {
                        // 即使从设置窗口打开管理器，也要同步已经打开的模板设计器。
                        templateDesignerWindow->reloadPaperSpecsFromDisk();
                    }
                });
        }
        paperSpecManagerWindow->reloadFromDisk();
        sleekpr::app::showAndActivateWindow(*paperSpecManagerWindow);
    });

    if (!designerSmokeTest && !server.listen(QHostAddress::LocalHost, 37122)) {
        // 端口被占用时必须给出桌面提示，否则托盘程序会像“没有启动”一样难排查。
        QMessageBox::critical(
            nullptr,
            QString::fromUtf8("sleekpr"),
            QString::fromUtf8("本地打印服务启动失败：127.0.0.1:37122\n\n%1").arg(server.errorString()));
        return 1;
    }
    if (!designerSmokeTest) qInfo() << "sleekpr local service: http://127.0.0.1:37122";

    const auto openWorkbench = [&settingsFilePath, &printEngine, &printWorkbenchWindow, &settingsWindow, &openDesigner, &openFieldPresets, &exitCoordinator, &exiting, designerSmokeTest] {
        if (exiting) return;
        if (printWorkbenchWindow == nullptr) {
            printWorkbenchWindow = std::make_unique<sleekpr::app::PrintWorkbenchWindow>(settingsFilePath, &printEngine);
            QPointer<sleekpr::app::ApplicationExitCoordinator> weak(&exitCoordinator);
            printWorkbenchWindow->setSubmissionStateChangedCallback([weak] { if (weak) weak->refresh(); });
            if (designerSmokeTest) printWorkbenchWindow->setAttribute(Qt::WA_DontShowOnScreen);
            printWorkbenchWindow->setOpenDesignerCallback(openDesigner);
            printWorkbenchWindow->setOpenFieldPresetManagerCallback(openFieldPresets);
            printWorkbenchWindow->setOpenSettingsCallback([&settingsWindow, &exiting] {
                if (exiting) return;
                settingsWindow.requestExternalRefresh(true);
                sleekpr::app::showAndActivateWindow(settingsWindow);
            });
        } else {
            printWorkbenchWindow->requestExternalRefresh();
        }
        sleekpr::app::showAndActivateWindow(*printWorkbenchWindow);
    };
    settingsWindow.setOpenPrintWorkbenchCallback(openWorkbench);

    exitCoordinator.addParticipant({QStringLiteral("settings"), QString::fromUtf8("设置"),
        [&] { settingsWindow.flushPersistenceEdits(); }, [&] { return settingsWindow.exitDraftState(); },
        [&] { settingsWindow.saveForExit(); }, [&](bool value) { settingsWindow.setExitPaused(value); },
        [&] { sleekpr::app::showAndActivateWindow(settingsWindow); }});
    exitCoordinator.addParticipant({QStringLiteral("designer"), QString::fromUtf8("标签设计器"),
        [&] { if (templateDesignerWindow) templateDesignerWindow->flushPersistenceEdits(); },
        [&] { return templateDesignerWindow ? templateDesignerWindow->exitDraftState() : sleekpr::app::ExitDraftState{}; }, {},
        [&](bool value) { if (templateDesignerWindow) templateDesignerWindow->setExitPaused(value); },
        [&] { if (templateDesignerWindow) sleekpr::app::showAndActivateWindow(*templateDesignerWindow); }});
    exitCoordinator.addParticipant({QStringLiteral("workbench"), QString::fromUtf8("打印工作台"), {},
        [&] { sleekpr::app::ExitDraftState state; state.activeSubmission = printWorkbenchWindow && printWorkbenchWindow->hasActivePrintSubmission(); return state; }, {},
        [&](bool value) { if (printWorkbenchWindow) printWorkbenchWindow->setExitPaused(value); }, {}});

    QAction printAction(QString::fromUtf8("打印工作台"), &app);
    QObject::connect(&printAction, &QAction::triggered, &settingsWindow, openWorkbench);

    QAction settingsAction(QString::fromUtf8("设置"), &app);
    QObject::connect(&settingsAction, &QAction::triggered, &settingsWindow, [&settingsWindow, &exiting] {
        if (exiting) return;
        settingsWindow.requestExternalRefresh(true);
        sleekpr::app::showAndActivateWindow(settingsWindow);
    });

    QAction quitAction(QString::fromUtf8("退出"), &app);
    QObject::connect(&quitAction, &QAction::triggered, &exitCoordinator, [&] { exitCoordinator.requestExit(); });

    QMenu trayMenu;
    trayMenu.addAction(&printAction);
    trayMenu.addAction(&settingsAction);
    trayMenu.addSeparator();
    trayMenu.addAction(&quitAction);

    QSystemTrayIcon tray;
    tray.setIcon(app.style()->standardIcon(QStyle::SP_FileDialogDetailedView));
    tray.setToolTip(QString::fromUtf8("sleekpr 本地打印服务"));
    tray.setContextMenu(&trayMenu);
    QObject::connect(&tray, &QSystemTrayIcon::activated, &settingsWindow, [&openWorkbench](QSystemTrayIcon::ActivationReason reason) {
        if (reason == QSystemTrayIcon::DoubleClick || reason == QSystemTrayIcon::Trigger) {
            openWorkbench();
        }
    });
    // 无托盘环境下仍保持本地服务运行，便于远程桌面或精简 Windows 环境调试。
    tray.setVisible(!designerSmokeTest && QSystemTrayIcon::isSystemTrayAvailable());
    qInfo() << "sleekpr tray available:" << QSystemTrayIcon::isSystemTrayAvailable();

    // 日常操作入口使用打印工作台，模板设计与设备配置仍通过明确入口进入各自窗口。
    QTimer::singleShot(0, &settingsWindow, openWorkbench);
    if (designerSmokeTest) {
        auto* steps = new QTimer(&app); steps->setInterval(10);
        const auto smokeClock = std::make_shared<QElapsedTimer>(); smokeClock->start();
        app.setProperty("designerSmokeStage", 0);
        QObject::connect(steps, &QTimer::timeout, &app,
            [&, steps, smokeClock, stage = 0, previous = QPointer<sleekpr::app::TemplateDesignerWindow>{}]() mutable {
                const auto fail = [&](const char* reason) { qCritical() << reason; steps->stop(); app.exit(1); };
                if (!settingsWindow.property("persistenceReady").toBool() || printWorkbenchWindow == nullptr) return;
                auto* entry = printWorkbenchWindow->findChild<QPushButton*>(QStringLiteral("workbenchOpenDesignerButton"));
                if (!entry) { fail("未找到工作台设计器入口"); return; }
                if (stage == 0) {
                    entry->click();
                    if (!templateDesignerWindow || !templateDesignerWindow->isVisible()) { fail("工作台无法打开标签设计器"); return; }
                    previous = templateDesignerWindow.get();
                    entry->click();
                    if (previous != templateDesignerWindow.get()) { fail("加载中的设计器未复用"); return; }
                    stage = 1; app.setProperty("designerSmokeStage", stage); qInfo() << "SMOKE_DESIGNER_STAGE" << stage << smokeClock->elapsed(); return;
                }
                if (!templateDesignerWindow || !templateDesignerWindow->property("persistenceReady").toBool()) return;
                // 已读取不等于已经提交全部有效编辑；重建测试必须先等窗口自己的未完成状态收束。
                if (stage == 1 || stage == 2) {
                    templateDesignerWindow->flushPersistenceEdits();
                    if (templateDesignerWindow->hasUnsavedPersistenceChanges()) return;
                }
                if (stage == 1) {
                    templateDesignerWindow->close(); entry->click();
                    if (!previous.isNull() || !templateDesignerWindow->isVisible()) { fail("设计器无法在无草稿时重建"); return; }
                    previous = templateDesignerWindow.get(); stage = 2; app.setProperty("designerSmokeStage", stage);
                    qInfo() << "SMOKE_DESIGNER_STAGE" << stage << smokeClock->elapsed(); return;
                }
                if (stage == 2) {
                    auto* button = settingsWindow.findChild<QPushButton*>(QStringLiteral("openTemplateDesignerButton"));
                    if (!button) { fail("未找到设置页设计器入口"); return; }
                    button->click();
                    if (!previous.isNull() || !templateDesignerWindow->isVisible()) { fail("设置页无法重开设计器"); return; }
                    stage = 3; app.setProperty("designerSmokeStage", stage); qInfo() << "SMOKE_DESIGNER_STAGE" << stage << smokeClock->elapsed(); return;
                }
                if (stage == 3 && templateDesignerWindow->property("previewReady").toBool()) {
                    steps->stop(); qInfo() << "标签设计器进程级回归通过"; app.exit(0);
                }
            });
        steps->start();
        QTimer::singleShot(10000, &app, [&, smokeClock] {
            qCritical() << "SMOKE_DESIGNER_TIMEOUT" << app.property("designerSmokeStage") << smokeClock->elapsed()
                << "storageIdle" << persistenceService.isIdle();
            if (templateDesignerWindow) qCritical() << "SMOKE_DESIGNER_STATE"
                << "ready" << templateDesignerWindow->property("persistenceReady")
                << "preview" << templateDesignerWindow->property("previewReady")
                << "unsaved" << templateDesignerWindow->hasUnsavedPersistenceChanges();
            app.exit(1);
        });
    }
    // Windows 在托盘程序冷启动时偶尔会丢失首次前台激活，短延迟再唤起一次保证窗口真正露出。
    if (!designerSmokeTest) QTimer::singleShot(300, &settingsWindow, [&printWorkbenchWindow, &exiting] {
        if (exiting) return;
        if (printWorkbenchWindow != nullptr) sleekpr::app::showAndActivateWindow(*printWorkbenchWindow);
    });

    const auto result = app.exec();
    previewExecutor.shutdown();
    if (qEnvironmentVariableIsSet("SLEEKPR_PERF_TRACE")) {
        qInfo().noquote() << "sleekpr performance:" << QJsonDocument(
            sleekpr::infrastructure::ClientPerformanceMetrics::applicationMetrics().snapshot()).toJson(QJsonDocument::Compact);
    }
    return result;
}

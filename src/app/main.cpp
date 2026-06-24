#include "sleekpr/app/AppTheme.h"
#include "sleekpr/app/FieldPresetManagerWindow.h"
#include "sleekpr/app/PaperSpecManagerWindow.h"
#include "sleekpr/app/SettingsWindow.h"
#include "sleekpr/app/TemplateDesignerWindow.h"
#include "sleekpr/app/WindowActivation.h"
#include "sleekpr/core/settings/FileSettingsStore.h"
#include "sleekpr/http/LocalHttpServer.h"
#include "sleekpr/infrastructure/printing/QtLabelPrintEngine.h"

#include <QAction>
#include <QApplication>
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QHostAddress>
#include <QMenu>
#include <QMessageBox>
#include <QStandardPaths>
#include <QStyle>
#include <QSystemTrayIcon>
#include <QTimer>

#include <memory>

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
    QApplication app(argc, argv);
    QApplication::setApplicationName("sleekpr");
    QApplication::setOrganizationName("zytxt");
    QApplication::setQuitOnLastWindowClosed(false);
    app.setStyleSheet(sleekpr::app::sleekprAppStyleSheet());

    const auto settingsFilePath = settingsPath();
    qInfo() << "sleekpr settings:" << settingsFilePath;
    sleekpr::infrastructure::QtLabelPrintEngine printEngine;
    // 设置窗口和模板设计器共用同一个纸张规格管理窗口，避免两个入口编辑不同实例导致状态不同步。
    std::unique_ptr<sleekpr::app::PaperSpecManagerWindow> paperSpecManagerWindow;
    std::unique_ptr<sleekpr::app::FieldPresetManagerWindow> fieldPresetManagerWindow;
    std::unique_ptr<sleekpr::app::TemplateDesignerWindow> templateDesignerWindow;
    sleekpr::app::SettingsWindow settingsWindow(settingsFilePath, nullptr, templateLibraryDirectoryPath(settingsFilePath));
    settingsWindow.setOpenTemplateDesignerCallback([&settingsFilePath, &settingsWindow, &paperSpecManagerWindow, &fieldPresetManagerWindow, &templateDesignerWindow] {
        const auto latestSettings = sleekpr::core::FileSettingsStore(settingsFilePath).load();
        templateDesignerWindow = std::make_unique<sleekpr::app::TemplateDesignerWindow>(
            latestSettings,
            [&settingsFilePath, &settingsWindow](const sleekpr::core::PrintClientSettings& settings) {
                // 设计器变更直接写回统一配置文件，设置窗口预览和 HTTP 打印接口都读取同一份模板。
                if (!sleekpr::core::FileSettingsStore(settingsFilePath).save(settings)) {
                    QMessageBox::warning(&settingsWindow, QString::fromUtf8("保存失败"), QString::fromUtf8("无法写入模板设计器设置"));
                    return;
                }
                settingsWindow.reloadFromDisk();
            },
            templateLibraryDirectoryPath(settingsFilePath));
        templateDesignerWindow->setOpenPaperSpecManagerCallback([&settingsFilePath, &paperSpecManagerWindow, &templateDesignerWindow] {
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
        templateDesignerWindow->setOpenFieldPresetManagerCallback([&settingsFilePath, &fieldPresetManagerWindow] {
            const auto presetPath = fieldPresetFilePath(settingsFilePath);
            if (fieldPresetManagerWindow == nullptr) {
                fieldPresetManagerWindow = std::make_unique<sleekpr::app::FieldPresetManagerWindow>(presetPath, nullptr);
            }
            fieldPresetManagerWindow->reloadFromDisk();
            sleekpr::app::showAndActivateWindow(*fieldPresetManagerWindow);
        });
        sleekpr::app::showAndActivateWindow(*templateDesignerWindow);
    });
    settingsWindow.setOpenPaperSpecManagerCallback([&settingsFilePath, &paperSpecManagerWindow, &templateDesignerWindow] {
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
    settingsWindow.setOpenFieldPresetManagerCallback([&settingsFilePath, &fieldPresetManagerWindow] {
        const auto presetPath = fieldPresetFilePath(settingsFilePath);
        if (fieldPresetManagerWindow == nullptr) {
            fieldPresetManagerWindow = std::make_unique<sleekpr::app::FieldPresetManagerWindow>(presetPath, nullptr);
        }
        fieldPresetManagerWindow->reloadFromDisk();
        sleekpr::app::showAndActivateWindow(*fieldPresetManagerWindow);
    });

    sleekpr::http::LocalHttpServer server(settingsFilePath, &printEngine);
    if (!server.listen(QHostAddress::LocalHost, 37122)) {
        // 端口被占用时必须给出桌面提示，否则托盘程序会像“没有启动”一样难排查。
        QMessageBox::critical(
            nullptr,
            QString::fromUtf8("sleekpr"),
            QString::fromUtf8("本地打印服务启动失败：127.0.0.1:37122\n\n%1").arg(server.errorString()));
        return 1;
    }
    qInfo() << "sleekpr local service: http://127.0.0.1:37122";

    QAction settingsAction(QString::fromUtf8("设置"), &app);
    QObject::connect(&settingsAction, &QAction::triggered, &settingsWindow, [&settingsWindow] {
        settingsWindow.reloadFromDisk();
        sleekpr::app::showAndActivateWindow(settingsWindow);
    });

    QAction quitAction(QString::fromUtf8("退出"), &app);
    QObject::connect(&quitAction, &QAction::triggered, &app, &QApplication::quit);

    QMenu trayMenu;
    trayMenu.addAction(&settingsAction);
    trayMenu.addSeparator();
    trayMenu.addAction(&quitAction);

    QSystemTrayIcon tray;
    tray.setIcon(app.style()->standardIcon(QStyle::SP_FileDialogDetailedView));
    tray.setToolTip(QString::fromUtf8("sleekpr 本地打印服务"));
    tray.setContextMenu(&trayMenu);
    QObject::connect(&tray, &QSystemTrayIcon::activated, &settingsWindow, [&settingsWindow](QSystemTrayIcon::ActivationReason reason) {
        if (reason == QSystemTrayIcon::DoubleClick || reason == QSystemTrayIcon::Trigger) {
            settingsWindow.reloadFromDisk();
            sleekpr::app::showAndActivateWindow(settingsWindow);
        }
    });
    // 无托盘环境下仍保持本地服务运行，便于远程桌面或精简 Windows 环境调试。
    tray.setVisible(QSystemTrayIcon::isSystemTrayAvailable());
    qInfo() << "sleekpr tray available:" << QSystemTrayIcon::isSystemTrayAvailable();

    // 第一阶段模板编辑入口放在设置窗口，启动后直接展示设置窗口，避免用户只能看到后台服务或托盘。
    QTimer::singleShot(0, &settingsWindow, [&settingsWindow] {
        settingsWindow.reloadFromDisk();
        sleekpr::app::showAndActivateWindow(settingsWindow);
    });
    // Windows 在托盘程序冷启动时偶尔会丢失首次前台激活，短延迟再唤起一次保证窗口真正露出。
    QTimer::singleShot(300, &settingsWindow, [&settingsWindow] {
        sleekpr::app::showAndActivateWindow(settingsWindow);
    });

    return app.exec();
}

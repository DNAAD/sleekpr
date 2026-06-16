#include "sleekpr/app/PreviewWindow.h"
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

} // 匿名命名空间

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName("sleekpr");
    QApplication::setOrganizationName("zytxt");
    QApplication::setQuitOnLastWindowClosed(false);

    const auto settingsFilePath = settingsPath();
    qInfo() << "sleekpr settings:" << settingsFilePath;
    sleekpr::infrastructure::QtLabelPrintEngine printEngine;
    sleekpr::app::PreviewWindow previewWindow(settingsFilePath);
    std::unique_ptr<sleekpr::app::TemplateDesignerWindow> templateDesignerWindow;
    sleekpr::app::SettingsWindow settingsWindow(
        settingsFilePath,
        [&previewWindow](const sleekpr::core::PrintClientSettings& settings) {
            previewWindow.refreshFromSettings(settings);
        });
    settingsWindow.setOpenTemplateDesignerCallback([&settingsFilePath, &previewWindow, &settingsWindow, &templateDesignerWindow] {
        const auto latestSettings = sleekpr::core::FileSettingsStore(settingsFilePath).load();
        templateDesignerWindow = std::make_unique<sleekpr::app::TemplateDesignerWindow>(
            latestSettings,
            [&settingsFilePath, &previewWindow, &settingsWindow](const sleekpr::core::PrintClientSettings& settings) {
                // 设计器变更直接写回统一配置文件，确保 HTTP 接口和打印预览读取到同一份模板。
                if (!sleekpr::core::FileSettingsStore(settingsFilePath).save(settings)) {
                    QMessageBox::warning(&settingsWindow, QString::fromUtf8("保存失败"), QString::fromUtf8("无法写入模板设计器设置"));
                    return;
                }
                previewWindow.refreshFromSettings(settings);
            });
        sleekpr::app::showAndActivateWindow(*templateDesignerWindow);
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

    QAction previewAction(QString::fromUtf8("预览标签"), &app);
    QObject::connect(&previewAction, &QAction::triggered, &previewWindow, [&previewWindow] {
        previewWindow.refreshPreview();
        sleekpr::app::showAndActivateWindow(previewWindow);
    });

    QAction settingsAction(QString::fromUtf8("设置"), &app);
    QObject::connect(&settingsAction, &QAction::triggered, &settingsWindow, [&settingsWindow] {
        settingsWindow.reloadFromDisk();
        sleekpr::app::showAndActivateWindow(settingsWindow);
    });

    QAction quitAction(QString::fromUtf8("退出"), &app);
    QObject::connect(&quitAction, &QAction::triggered, &app, &QApplication::quit);

    QMenu trayMenu;
    trayMenu.addAction(&previewAction);
    trayMenu.addAction(&settingsAction);
    trayMenu.addSeparator();
    trayMenu.addAction(&quitAction);

    QSystemTrayIcon tray;
    tray.setIcon(app.style()->standardIcon(QStyle::SP_FileDialogDetailedView));
    tray.setToolTip(QString::fromUtf8("sleekpr 本地打印服务"));
    tray.setContextMenu(&trayMenu);
    QObject::connect(&tray, &QSystemTrayIcon::activated, &previewWindow, [&previewWindow](QSystemTrayIcon::ActivationReason reason) {
        if (reason == QSystemTrayIcon::DoubleClick || reason == QSystemTrayIcon::Trigger) {
            previewWindow.refreshPreview();
            sleekpr::app::showAndActivateWindow(previewWindow);
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

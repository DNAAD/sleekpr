#pragma once

#include <QImage>
#include <QString>
#include <QWidget>

class QLabel;

namespace sleekpr::core {
struct PrintClientSettings;
}

namespace sleekpr::app {

class PreviewWindow final : public QWidget
{
public:
    explicit PreviewWindow(QString settingsPath, QWidget* parent = nullptr);

    // 重新读取本机设置并刷新标签图像，确保窗口中看到的是当前配置下的结果。
    void refreshPreview();

    // 设置窗口保存或临时应用后直接传入设置，避免预览等待下一次文件读取。
    void refreshFromSettings(const sleekpr::core::PrintClientSettings& settings);

private:
    QImage createPreviewImage(const sleekpr::core::PrintClientSettings& settings) const;
    void updatePreviewImage(const QImage& image);

    QString m_settingsPath;
    QLabel* m_previewLabel = nullptr;
    QLabel* m_statusLabel = nullptr;
};

} // 命名空间 sleekpr::app

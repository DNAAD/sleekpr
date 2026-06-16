#pragma once

#include "sleekpr/core/settings/PrintClientSettings.h"

#include <QString>
#include <QWidget>

#include <functional>

class QLabel;
class QListWidget;

namespace sleekpr::app {

class TemplateDesignerWindow final : public QWidget
{
public:
    using SettingsChangedCallback = std::function<void(const sleekpr::core::PrintClientSettings&)>;

    explicit TemplateDesignerWindow(
        sleekpr::core::PrintClientSettings settings,
        SettingsChangedCallback onSettingsChanged,
        QWidget* parent = nullptr);

private:
    void buildUi();
    void ensureCurrentTemplateDocument();
    void addLayer();
    void refreshLayerList();

    sleekpr::core::PrintClientSettings m_settings;
    SettingsChangedCallback m_onSettingsChanged;
    QString m_templateKey = QStringLiteral("default");
    QListWidget* m_layerList = nullptr;
    QLabel* m_statusLabel = nullptr;
};

} // 命名空间 sleekpr::app

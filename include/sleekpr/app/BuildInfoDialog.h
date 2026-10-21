#pragma once

#include <QDialog>
#include <QJsonObject>

namespace sleekpr::app {

class BuildInfoDialog final : public QDialog
{
public:
    explicit BuildInfoDialog(QString settingsPath, QWidget* parent = nullptr);
    QJsonObject diagnosticJson() const;
private:
    QString m_settingsPath;
};

} // 命名空间 sleekpr::app

#pragma once

#include "sleekpr/core/settings/TemplateElement.h"

#include <QList>
#include <QString>

namespace sleekpr::core {

struct TemplateLayer
{
    // 图层稳定标识，版本快照和元素归属都依赖它。
    QString id;
    QString name;
    bool visible = true;
    bool locked = false;
    QList<TemplateElement> elements;
};

struct TemplateVersion
{
    // 版本只保存版式快照，不保存设备 profile，避免恢复版式时覆盖设备校准。
    QString id;
    QString name;
    QString createdAt;
    QString note;
    QList<TemplateLayer> layersSnapshot;
};

struct DeviceProfile
{
    // printerName 为空表示默认 profile。
    QString id;
    QString printerName;
    double dpi = 300.0;
    double scaleX = 1.0;
    double scaleY = 1.0;
    double offsetX = 0.0;
    double offsetY = 0.0;
    QString notes;
};

struct TemplateDocument
{
    // schemaVersion 用于后续导入时拒绝未知结构。
    int schemaVersion = 1;
    QString id;
    QString name;
    QString templateKey = QStringLiteral("default");

    // 模板分类决定设计器默认工具集合；旧标签模板默认保持 label。
    QString category = QStringLiteral("label");

    // 纸张规格独立保存，模板通过 paperSpecId 引用；旧标签模板读取时可补默认规格。
    QString paperSpecId;

    QString activeVersionId;
    QList<TemplateLayer> layers;
    QList<TemplateVersion> versions;
    QList<DeviceProfile> deviceProfiles;
};

} // 命名空间 sleekpr::core

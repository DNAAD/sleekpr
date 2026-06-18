#pragma once

#include "sleekpr/core/settings/TemplateElement.h"
#include "sleekpr/core/templates/FieldSchema.h"
#include "sleekpr/core/templates/TableElement.h"

#include <QList>
#include <QJsonObject>
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

    // 表格是一级模板元素，不能拆成多个普通文本框保存。
    QList<TableElement> tables;
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

    // 模板只保存字段定义，不保存某次业务打印值；字段值由方案和本次请求合并生成。
    QList<FieldDefinition> fieldSchema;

    // 设计器样例数据只用于模板预览和预打印，正式打印必须使用接口本次传入的 values。
    QJsonObject sampleData;

    QList<TemplateVersion> versions;
    QList<DeviceProfile> deviceProfiles;
};

} // 命名空间 sleekpr::core

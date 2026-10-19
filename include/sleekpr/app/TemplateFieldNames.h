#pragma once

#include <QList>
#include <QPair>
#include <QString>

namespace sleekpr::core { struct TemplateDocument; }

namespace sleekpr::app {

class TemplateFieldNames final
{
public:
    static const QList<QPair<QString, QString>>& builtIns();
    // 只解析界面名称，不创建字段声明、不改变实际取值 key 或打印数据。
    static QString resolve(const core::TemplateDocument& document, const QString& key);
};

} // 命名空间 sleekpr::app

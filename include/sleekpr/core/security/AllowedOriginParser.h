#pragma once

#include <QString>
#include <QStringList>

namespace sleekpr::core {

class AllowedOriginParser
{
public:
    // 解析设置页中按行填写的 Origin，并去重、去掉尾随斜杠。
    static QStringList parse(const QString& text);
};

} // 命名空间 sleekpr::core

#pragma once

#include "sleekpr/core/templates/TableElement.h"

#include <QList>
#include <QString>

namespace sleekpr::app {

class TableColumnTextCodec final
{
public:
    static QString format(const QList<sleekpr::core::TableColumn>& columns);
    static QList<sleekpr::core::TableColumn> parse(const QString& text, QString* errorMessage = nullptr);
};

} // namespace sleekpr::app

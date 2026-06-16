#include "sleekpr/core/labels/TemplateOverrideKey.h"

namespace sleekpr::core {

QString templateOverrideKey(LabelTemplateKey templateKey)
{
    return templateKey == LabelTemplateKey::Silver80x30
        ? QStringLiteral("silver")
        : QStringLiteral("default");
}

LabelTemplateKey labelTemplateKeyFromOverrideKey(const QString& key)
{
    return key.compare(QStringLiteral("silver"), Qt::CaseInsensitive) == 0
        ? LabelTemplateKey::Silver80x30
        : LabelTemplateKey::Default80x30;
}

} // 命名空间 sleekpr::core

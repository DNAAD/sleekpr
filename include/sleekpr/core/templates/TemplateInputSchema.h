#pragma once

#include "sleekpr/core/templates/TemplateDocument.h"

#include <QJsonObject>
#include <QList>
#include <QString>
#include <QStringList>

namespace sleekpr::core {

struct TemplateInputField
{
    FieldDefinition definition;
    bool inferred = false;
    bool arrayPath = false;
    QStringList elementIds;
};

enum class TemplateInputProblem { Unspecified, MissingValue, InvalidType, LiteralKeyRequired, MissingArrayChild };

struct TemplateInputIssue
{
    QString fieldKey;
    QString message;
    QStringList elementIds;
    // 保留结构化原因和原始键，界面可使用显示名称格式化，接口仍使用原错误文字。
    TemplateInputProblem problem = TemplateInputProblem::Unspecified;
    QString expectedType;
    QString childFieldKey;
    qint64 itemNumber = 0;

    QString messageForDisplayNames(const QString& fieldName, const QString& childFieldName = {}) const;
};

class TemplateInputSchema
{
public:
    // 推断仅补充输入描述，不修改模板，也不把样例内容带入正式打印。
    static QList<TemplateInputField> fields(const TemplateDocument& document);
    static QList<TemplateInputIssue> validate(const TemplateDocument& document, const QJsonObject& values);
};

} // 命名空间 sleekpr::core

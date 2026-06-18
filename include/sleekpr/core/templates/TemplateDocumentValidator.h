#pragma once

#include "sleekpr/core/templates/TemplateDocument.h"

#include <QJsonObject>
#include <QList>
#include <QSizeF>
#include <QString>
#include <QStringList>

namespace sleekpr::core {

enum class TemplateValidationSeverity
{
    Warning,
    Error,
};

struct TemplateValidationIssue
{
    TemplateValidationSeverity severity = TemplateValidationSeverity::Error;
    QString code;
    QString message;
    QString layerId;
    QString elementId;
};

struct TemplateValidationResult
{
    QList<TemplateValidationIssue> issues;

    bool hasErrors() const;
    bool containsCode(const QString& code) const;
    QStringList errorMessages() const;
    QString firstErrorMessage() const;
};

class TemplateDocumentValidator
{
public:
    static constexpr int kMaxQrPayloadBytes = 46;

    TemplateValidationResult validate(
        const TemplateDocument& document,
        const QSizeF& paperSizeMm,
        const QJsonObject& fieldValues = QJsonObject{}) const;
};

} // 命名空间 sleekpr::core

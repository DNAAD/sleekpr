#pragma once

#include "sleekpr/core/templates/TemplateDocument.h"

#include <QString>
#include <QStringList>

#include <optional>

namespace sleekpr::core {

class TemplateLibraryStore
{
public:
    explicit TemplateLibraryStore(QString templateDirectoryPath);

    QStringList templateIds() const;
    std::optional<TemplateDocument> loadTemplate(const QString& templateId, QString* errorMessage = nullptr) const;
    bool saveTemplate(const TemplateDocument& document, QString* errorMessage = nullptr) const;
    bool removeTemplate(const QString& templateId) const;

private:
    QString templatePath(const QString& templateId) const;

    QString m_templateDirectoryPath;
};

} // 命名空间 sleekpr::core

#include "sleekpr/core/templates/TemplateLibraryStore.h"

#include "sleekpr/core/templates/TemplateDocumentJson.h"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QSaveFile>

#include <algorithm>
#include <utility>

namespace sleekpr::core {

namespace {

const QString& templateFileSuffix()
{
    static const QString suffix = QStringLiteral(".sleekpr-template.json");
    return suffix;
}

void setError(QString* errorMessage, const QString& message)
{
    if (errorMessage != nullptr) {
        *errorMessage = message;
    }
}

bool isSafeTemplateId(const QString& templateId)
{
    const auto normalized = templateId.trimmed();
    return !normalized.isEmpty()
        && normalized == templateId
        && !normalized.contains(QLatin1Char('/'))
        && !normalized.contains(QLatin1Char('\\'));
}

} // 命名空间

TemplateLibraryStore::TemplateLibraryStore(QString templateDirectoryPath)
    : m_templateDirectoryPath(std::move(templateDirectoryPath))
{
}

QStringList TemplateLibraryStore::templateIds() const
{
    QDir dir(m_templateDirectoryPath);
    const auto names = dir.entryList({QStringLiteral("*") + templateFileSuffix()}, QDir::Files, QDir::Name);

    QStringList ids;
    ids.reserve(names.size());
    for (const auto& name : names) {
        ids.append(name.left(name.size() - templateFileSuffix().size()));
    }
    std::sort(ids.begin(), ids.end());
    return ids;
}

std::optional<TemplateDocument> TemplateLibraryStore::loadTemplate(
    const QString& templateId,
    QString* errorMessage) const
{
    if (!isSafeTemplateId(templateId)) {
        setError(errorMessage, QStringLiteral("模板 id 不能为空，且不能包含路径分隔符"));
        return std::nullopt;
    }

    QFile file(templatePath(templateId));
    if (!file.open(QIODevice::ReadOnly)) {
        setError(errorMessage, QStringLiteral("读取模板文件失败：%1").arg(file.errorString()));
        return std::nullopt;
    }

    QJsonParseError parseError;
    const auto document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        setError(errorMessage, QStringLiteral("模板文件 JSON 格式错误：%1").arg(parseError.errorString()));
        return std::nullopt;
    }

    QString validationError;
    const auto json = document.object();
    if (!TemplateDocumentJson::validateForImport(json, &validationError)) {
        setError(errorMessage, validationError);
        return std::nullopt;
    }

    return TemplateDocumentJson::fromJson(json);
}

bool TemplateLibraryStore::saveTemplate(const TemplateDocument& document, QString* errorMessage) const
{
    if (!isSafeTemplateId(document.id)) {
        setError(errorMessage, QStringLiteral("模板 id 不能为空，且不能包含路径分隔符"));
        return false;
    }

    const auto json = TemplateDocumentJson::toJson(document);
    QString validationError;
    if (!TemplateDocumentJson::validateForImport(json, &validationError)) {
        setError(errorMessage, validationError);
        return false;
    }

    QDir dir(m_templateDirectoryPath);
    if (!dir.exists() && !QDir().mkpath(m_templateDirectoryPath)) {
        setError(errorMessage, QStringLiteral("创建模板目录失败：%1").arg(m_templateDirectoryPath));
        return false;
    }

    QSaveFile file(templatePath(document.id));
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        setError(errorMessage, QStringLiteral("写入模板文件失败：%1").arg(file.errorString()));
        return false;
    }

    // 模板文件供人工排查和版本对比使用，保存时保留缩进格式。
    file.write(QJsonDocument(json).toJson(QJsonDocument::Indented));
    if (!file.commit()) {
        setError(errorMessage, QStringLiteral("提交模板文件失败：%1").arg(file.errorString()));
        return false;
    }

    return true;
}

bool TemplateLibraryStore::removeTemplate(const QString& templateId) const
{
    if (!isSafeTemplateId(templateId)) {
        return false;
    }

    QFile file(templatePath(templateId));
    if (!file.exists()) {
        return true;
    }
    return file.remove();
}

QString TemplateLibraryStore::templatePath(const QString& templateId) const
{
    return QDir(m_templateDirectoryPath).filePath(templateId + templateFileSuffix());
}

} // 命名空间 sleekpr::core

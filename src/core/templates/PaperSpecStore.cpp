#include "sleekpr/core/templates/PaperSpecStore.h"

#include "sleekpr/core/templates/PaperSpecJson.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QSaveFile>
#include <QSet>

#include <algorithm>
#include <utility>

namespace sleekpr::core {

namespace {

void setError(QString* errorMessage, const QString& message)
{
    if (errorMessage != nullptr) {
        *errorMessage = message;
    }
}

bool isSafePaperSpecId(const QString& paperSpecId)
{
    const auto normalized = paperSpecId.trimmed();
    return !normalized.isEmpty()
        && normalized == paperSpecId
        && !normalized.contains(QLatin1Char('/'))
        && !normalized.contains(QLatin1Char('\\'));
}

QList<PaperSpec> sortedSpecs(QList<PaperSpec> specs)
{
    std::sort(specs.begin(), specs.end(), [](const PaperSpec& left, const PaperSpec& right) {
        return left.id < right.id;
    });
    return specs;
}

QJsonObject documentJsonForSpecs(const QList<PaperSpec>& specs)
{
    QJsonArray specArray;
    for (const auto& spec : sortedSpecs(specs)) {
        specArray.append(PaperSpecJson::toJson(spec));
    }

    return QJsonObject{
        {"schemaVersion", 1},
        {"paperSpecs", specArray},
    };
}

} // 匿名命名空间

PaperSpecStore::PaperSpecStore(QString filePath)
    : m_filePath(std::move(filePath))
{
}

QStringList PaperSpecStore::paperSpecIds() const
{
    QString errorMessage;
    const auto specs = loadAllSpecs(&errorMessage).value_or(QList<PaperSpec>{});

    QStringList ids;
    ids.reserve(specs.size());
    for (const auto& spec : specs) {
        ids.append(spec.id);
    }
    std::sort(ids.begin(), ids.end());
    return ids;
}

QList<PaperSpec> PaperSpecStore::paperSpecs(QString* errorMessage) const
{
    const auto specs = loadAllSpecs(errorMessage);
    if (!specs.has_value()) {
        return {};
    }
    return sortedSpecs(specs.value());
}

std::optional<PaperSpec> PaperSpecStore::loadPaperSpec(const QString& paperSpecId, QString* errorMessage) const
{
    if (!isSafePaperSpecId(paperSpecId)) {
        setError(errorMessage, QString::fromUtf8("纸张规格 id 不能为空，且不能包含路径分隔符"));
        return std::nullopt;
    }

    const auto specs = loadAllSpecs(errorMessage);
    if (!specs.has_value()) {
        return std::nullopt;
    }

    for (const auto& spec : specs.value()) {
        if (spec.id == paperSpecId) {
            return spec;
        }
    }

    setError(errorMessage, QString::fromUtf8("未找到指定纸张规格。"));
    return std::nullopt;
}

bool PaperSpecStore::savePaperSpec(const PaperSpec& spec, QString* errorMessage) const
{
    if (!isSafePaperSpecId(spec.id)) {
        setError(errorMessage, QString::fromUtf8("纸张规格 id 不能为空，且不能包含路径分隔符"));
        return false;
    }

    const auto json = PaperSpecJson::toJson(spec);
    QString validationError;
    if (!PaperSpecJson::validate(json, &validationError)) {
        setError(errorMessage, validationError);
        return false;
    }

    auto specs = loadAllSpecs(errorMessage);
    if (!specs.has_value()) {
        return false;
    }

    bool replaced = false;
    for (auto& existing : specs.value()) {
        if (existing.id == spec.id) {
            existing = spec;
            replaced = true;
            break;
        }
    }
    if (!replaced) {
        specs->append(spec);
    }

    return saveAllSpecs(specs.value(), errorMessage);
}

bool PaperSpecStore::removePaperSpec(const QString& paperSpecId, QString* errorMessage) const
{
    if (!isSafePaperSpecId(paperSpecId)) {
        setError(errorMessage, QString::fromUtf8("纸张规格 id 不能为空，且不能包含路径分隔符"));
        return false;
    }

    auto specs = loadAllSpecs(errorMessage);
    if (!specs.has_value()) {
        return false;
    }

    const auto originalSize = specs->size();
    specs->erase(
        std::remove_if(specs->begin(), specs->end(), [&paperSpecId](const PaperSpec& spec) {
            return spec.id == paperSpecId;
        }),
        specs->end());

    if (specs->size() == originalSize) {
        return true;
    }

    return saveAllSpecs(specs.value(), errorMessage);
}

std::optional<QList<PaperSpec>> PaperSpecStore::loadAllSpecs(QString* errorMessage) const
{
    QFile file(m_filePath);
    if (!file.exists()) {
        return QList<PaperSpec>{};
    }

    if (!file.open(QIODevice::ReadOnly)) {
        setError(errorMessage, QString::fromUtf8("读取纸张规格文件失败：%1").arg(file.errorString()));
        return std::nullopt;
    }

    QJsonParseError parseError;
    const auto document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        setError(errorMessage, QString::fromUtf8("纸张规格文件 JSON 格式错误：%1").arg(parseError.errorString()));
        return std::nullopt;
    }

    const auto root = document.object();
    if (root["schemaVersion"].toInt(-1) != 1) {
        setError(errorMessage, QString::fromUtf8("纸张规格文件 schemaVersion 不受支持"));
        return std::nullopt;
    }

    const auto paperSpecsValue = root["paperSpecs"];
    if (!paperSpecsValue.isArray()) {
        setError(errorMessage, QString::fromUtf8("纸张规格文件缺少 paperSpecs 数组"));
        return std::nullopt;
    }

    QList<PaperSpec> specs;
    QSet<QString> usedIds;
    for (const auto& value : paperSpecsValue.toArray()) {
        if (!value.isObject()) {
            setError(errorMessage, QString::fromUtf8("纸张规格条目必须是对象"));
            return std::nullopt;
        }

        const auto specJson = value.toObject();
        QString validationError;
        if (!PaperSpecJson::validate(specJson, &validationError)) {
            setError(errorMessage, validationError);
            return std::nullopt;
        }

        const auto spec = PaperSpecJson::fromJson(specJson);
        if (usedIds.contains(spec.id)) {
            setError(errorMessage, QString::fromUtf8("纸张规格 id 重复：%1").arg(spec.id));
            return std::nullopt;
        }

        usedIds.insert(spec.id);
        specs.append(spec);
    }

    return specs;
}

bool PaperSpecStore::saveAllSpecs(const QList<PaperSpec>& specs, QString* errorMessage) const
{
    const QFileInfo fileInfo(m_filePath);
    const auto directoryPath = fileInfo.absoluteDir().absolutePath();
    if (!QDir(directoryPath).exists() && !QDir().mkpath(directoryPath)) {
        setError(errorMessage, QString::fromUtf8("创建纸张规格目录失败：%1").arg(directoryPath));
        return false;
    }

    QSaveFile file(m_filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        setError(errorMessage, QString::fromUtf8("写入纸张规格文件失败：%1").arg(file.errorString()));
        return false;
    }

    // 纸张规格属于用户可迁移配置，保留缩进便于现场排查和人工比对。
    file.write(QJsonDocument(documentJsonForSpecs(specs)).toJson(QJsonDocument::Indented));
    if (!file.commit()) {
        setError(errorMessage, QString::fromUtf8("提交纸张规格文件失败：%1").arg(file.errorString()));
        return false;
    }

    return true;
}

} // 命名空间 sleekpr::core

#include "sleekpr/core/templates/FieldPresetStore.h"

#include "sleekpr/core/templates/FieldPresetJson.h"

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

bool isSafePresetId(const QString& presetId)
{
    const auto normalized = presetId.trimmed();
    return !normalized.isEmpty()
        && normalized == presetId
        && !normalized.contains(QLatin1Char('/'))
        && !normalized.contains(QLatin1Char('\\'));
}

QList<FieldPreset> sortedPresets(QList<FieldPreset> presets)
{
    std::sort(presets.begin(), presets.end(), [](const FieldPreset& left, const FieldPreset& right) {
        return left.id < right.id;
    });
    return presets;
}

QJsonObject documentJsonForPresets(const QList<FieldPreset>& presets)
{
    QJsonArray presetArray;
    for (const auto& preset : sortedPresets(presets)) {
        presetArray.append(FieldPresetJson::toJson(preset));
    }

    return QJsonObject{
        {"schemaVersion", 1},
        {"fieldPresets", presetArray},
    };
}

} // 匿名命名空间

FieldPresetStore::FieldPresetStore(QString filePath)
    : m_filePath(std::move(filePath))
{
}

QStringList FieldPresetStore::presetIds() const
{
    QString errorMessage;
    const auto loadedPresets = loadAllPresets(&errorMessage).value_or(QList<FieldPreset>{});

    QStringList ids;
    ids.reserve(loadedPresets.size());
    for (const auto& preset : loadedPresets) {
        ids.append(preset.id);
    }
    std::sort(ids.begin(), ids.end());
    return ids;
}

QList<FieldPreset> FieldPresetStore::presets(QString* errorMessage) const
{
    const auto loadedPresets = loadAllPresets(errorMessage);
    if (!loadedPresets.has_value()) {
        return {};
    }
    return sortedPresets(loadedPresets.value());
}

std::optional<FieldPreset> FieldPresetStore::loadPreset(const QString& presetId, QString* errorMessage) const
{
    if (!isSafePresetId(presetId)) {
        setError(errorMessage, QString::fromUtf8("字段预设 id 不能为空，且不能包含路径分隔符"));
        return std::nullopt;
    }

    const auto loadedPresets = loadAllPresets(errorMessage);
    if (!loadedPresets.has_value()) {
        return std::nullopt;
    }

    for (const auto& preset : loadedPresets.value()) {
        if (preset.id == presetId) {
            return preset;
        }
    }

    setError(errorMessage, QString::fromUtf8("未找到指定字段预设。"));
    return std::nullopt;
}

bool FieldPresetStore::savePreset(const FieldPreset& preset, QString* errorMessage) const
{
    if (!isSafePresetId(preset.id)) {
        setError(errorMessage, QString::fromUtf8("字段预设 id 不能为空，且不能包含路径分隔符"));
        return false;
    }

    const auto json = FieldPresetJson::toJson(preset);
    QString validationError;
    if (!FieldPresetJson::validate(json, &validationError)) {
        setError(errorMessage, validationError);
        return false;
    }

    auto loadedPresets = loadAllPresets(errorMessage);
    if (!loadedPresets.has_value()) {
        return false;
    }

    bool replaced = false;
    for (auto& existing : loadedPresets.value()) {
        if (existing.id == preset.id) {
            existing = preset;
            replaced = true;
            break;
        }
    }
    if (!replaced) {
        loadedPresets->append(preset);
    }

    return saveAllPresets(loadedPresets.value(), errorMessage);
}

bool FieldPresetStore::removePreset(const QString& presetId, QString* errorMessage) const
{
    if (!isSafePresetId(presetId)) {
        setError(errorMessage, QString::fromUtf8("字段预设 id 不能为空，且不能包含路径分隔符"));
        return false;
    }

    auto loadedPresets = loadAllPresets(errorMessage);
    if (!loadedPresets.has_value()) {
        return false;
    }

    const auto originalSize = loadedPresets->size();
    loadedPresets->erase(
        std::remove_if(loadedPresets->begin(), loadedPresets->end(), [&presetId](const FieldPreset& preset) {
            return preset.id == presetId;
        }),
        loadedPresets->end());

    if (loadedPresets->size() == originalSize) {
        return true;
    }

    return saveAllPresets(loadedPresets.value(), errorMessage);
}

std::optional<QList<FieldPreset>> FieldPresetStore::loadAllPresets(QString* errorMessage) const
{
    QFile file(m_filePath);
    if (!file.exists()) {
        return QList<FieldPreset>{};
    }

    if (!file.open(QIODevice::ReadOnly)) {
        setError(errorMessage, QString::fromUtf8("读取字段预设文件失败：%1").arg(file.errorString()));
        return std::nullopt;
    }

    QJsonParseError parseError;
    const auto document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        setError(errorMessage, QString::fromUtf8("字段预设文件 JSON 格式错误：%1").arg(parseError.errorString()));
        return std::nullopt;
    }

    const auto root = document.object();
    if (root["schemaVersion"].toInt(-1) != 1) {
        setError(errorMessage, QString::fromUtf8("字段预设文件 schemaVersion 不受支持"));
        return std::nullopt;
    }

    const auto presetsValue = root["fieldPresets"];
    if (!presetsValue.isArray()) {
        setError(errorMessage, QString::fromUtf8("字段预设文件缺少 fieldPresets 数组"));
        return std::nullopt;
    }

    QList<FieldPreset> loadedPresets;
    QSet<QString> usedIds;
    for (const auto& value : presetsValue.toArray()) {
        if (!value.isObject()) {
            setError(errorMessage, QString::fromUtf8("字段预设条目必须是对象"));
            return std::nullopt;
        }

        const auto presetJson = value.toObject();
        QString validationError;
        if (!FieldPresetJson::validate(presetJson, &validationError)) {
            setError(errorMessage, validationError);
            return std::nullopt;
        }

        const auto preset = FieldPresetJson::fromJson(presetJson);
        if (usedIds.contains(preset.id)) {
            setError(errorMessage, QString::fromUtf8("字段预设 id 重复：%1").arg(preset.id));
            return std::nullopt;
        }

        usedIds.insert(preset.id);
        loadedPresets.append(preset);
    }

    return loadedPresets;
}

bool FieldPresetStore::saveAllPresets(const QList<FieldPreset>& presets, QString* errorMessage) const
{
    const QFileInfo fileInfo(m_filePath);
    const auto directoryPath = fileInfo.absoluteDir().absolutePath();
    if (!QDir(directoryPath).exists() && !QDir().mkpath(directoryPath)) {
        setError(errorMessage, QString::fromUtf8("创建字段预设目录失败：%1").arg(directoryPath));
        return false;
    }

    QSaveFile file(m_filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        setError(errorMessage, QString::fromUtf8("写入字段预设文件失败：%1").arg(file.errorString()));
        return false;
    }

    // 字段预设是用户可迁移配置，保留缩进格式方便现场排查和人工比对。
    file.write(QJsonDocument(documentJsonForPresets(presets)).toJson(QJsonDocument::Indented));
    if (!file.commit()) {
        setError(errorMessage, QString::fromUtf8("提交字段预设文件失败：%1").arg(file.errorString()));
        return false;
    }

    return true;
}

} // 命名空间 sleekpr::core

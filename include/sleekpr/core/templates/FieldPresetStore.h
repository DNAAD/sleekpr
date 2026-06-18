#pragma once

#include "sleekpr/core/templates/FieldPreset.h"

#include <QList>
#include <QString>
#include <QStringList>

#include <optional>

namespace sleekpr::core {

class FieldPresetStore
{
public:
    explicit FieldPresetStore(QString filePath);

    QStringList presetIds() const;
    QList<FieldPreset> presets(QString* errorMessage = nullptr) const;
    std::optional<FieldPreset> loadPreset(const QString& presetId, QString* errorMessage = nullptr) const;
    bool savePreset(const FieldPreset& preset, QString* errorMessage = nullptr) const;
    bool removePreset(const QString& presetId, QString* errorMessage = nullptr) const;

private:
    std::optional<QList<FieldPreset>> loadAllPresets(QString* errorMessage) const;
    bool saveAllPresets(const QList<FieldPreset>& presets, QString* errorMessage) const;

    QString m_filePath;
};

} // 命名空间 sleekpr::core

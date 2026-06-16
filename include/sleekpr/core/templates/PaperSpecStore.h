#pragma once

#include "sleekpr/core/templates/PaperSpec.h"

#include <QList>
#include <QString>
#include <QStringList>

#include <optional>

namespace sleekpr::core {

class PaperSpecStore
{
public:
    explicit PaperSpecStore(QString filePath);

    QStringList paperSpecIds() const;
    QList<PaperSpec> paperSpecs(QString* errorMessage = nullptr) const;
    std::optional<PaperSpec> loadPaperSpec(const QString& paperSpecId, QString* errorMessage = nullptr) const;
    bool savePaperSpec(const PaperSpec& spec, QString* errorMessage = nullptr) const;
    bool removePaperSpec(const QString& paperSpecId, QString* errorMessage = nullptr) const;

private:
    std::optional<QList<PaperSpec>> loadAllSpecs(QString* errorMessage) const;
    bool saveAllSpecs(const QList<PaperSpec>& specs, QString* errorMessage) const;

    QString m_filePath;
};

} // 命名空间 sleekpr::core

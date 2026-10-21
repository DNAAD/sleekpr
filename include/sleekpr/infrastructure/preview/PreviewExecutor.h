#pragma once

#include "sleekpr/core/native/NativePrintDrawingPlan.h"
#include "sleekpr/core/printing/PrintPreflightReport.h"
#include "sleekpr/core/templates/TableElementLayout.h"
#include <QImage>
#include <QObject>
#include <QThreadPool>
#include <functional>
#include <optional>

namespace sleekpr::infrastructure {

struct PreviewWorkResult
{
    std::optional<core::NativePrintDrawingPlan> plan;
    QImage image;
    core::PrintPreflightReport preflight;
    double baseDpi = 0;
    std::optional<core::TableLayoutResult> tableLayout;
    QString tableId;
    QString errorCode;
    QString errorMessage;
    qint64 planningNanoseconds = 0;
    qint64 rasterNanoseconds = 0;
};

class PreviewExecutor final : public QObject
{
public:
    explicit PreviewExecutor(QObject* parent = nullptr);
    ~PreviewExecutor() override;
    void submit(std::function<PreviewWorkResult()> work, std::function<void(PreviewWorkResult)> guiCompletion);
    void shutdown();
    static PreviewExecutor& applicationInstance();
private:
    QThreadPool m_pool;
    bool m_stopping = false;
};

} // 命名空间 sleekpr::infrastructure

Q_DECLARE_METATYPE(sleekpr::infrastructure::PreviewWorkResult)

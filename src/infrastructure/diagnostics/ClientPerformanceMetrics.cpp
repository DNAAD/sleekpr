#include "sleekpr/infrastructure/diagnostics/ClientPerformanceMetrics.h"
#include <QMutexLocker>
#include <algorithm>
#include <limits>

namespace sleekpr::infrastructure {

void ClientPerformanceMetrics::record(PerformanceStage stage, qint64 nanoseconds)
{
    const int index = static_cast<int>(stage);
    if (index < 0 || index >= static_cast<int>(PerformanceStage::Count) || nanoseconds < 0) return;
    QMutexLocker lock(&m_mutex);
    auto& samples = m_stages[index];
    // 固定容量环形采样，不保存字段正文，也不让长期运行的托盘进程不断增长内存。
    samples.values[samples.next] = nanoseconds;
    samples.next = (samples.next + 1) % RetainedSamples;
    samples.retained = std::min(samples.retained + 1, RetainedSamples);
    if (samples.count < std::numeric_limits<qint64>::max()) ++samples.count;
}

QJsonObject ClientPerformanceMetrics::snapshot() const
{
    decltype(m_stages) stages;
    { QMutexLocker lock(&m_mutex); stages = m_stages; }
    constexpr std::array names{"planning", "rasterization", "saving", "publication", "externalRefresh", "deferredRefresh", "designerRefresh"};
    QJsonObject result;
    for (int i = 0; i < static_cast<int>(stages.size()); ++i) {
        auto& samples = stages[i];
        std::sort(samples.values.begin(), samples.values.begin() + samples.retained);
        const auto percentile = [&samples](int percent) {
            if (samples.retained == 0) return 0.0;
            const int rank = (samples.retained * percent + 99) / 100 - 1;
            return samples.values[rank] / 1000000.0;
        };
        result.insert(QString::fromLatin1(names[i]), QJsonObject{{QStringLiteral("count"), samples.count},
            {QStringLiteral("retained"), samples.retained}, {QStringLiteral("p50Ms"), percentile(50)},
            {QStringLiteral("p95Ms"), percentile(95)}});
    }
    return result;
}
ClientPerformanceMetrics& ClientPerformanceMetrics::applicationMetrics()
{
    static ClientPerformanceMetrics metrics;
    return metrics;
}
PerformanceScope::PerformanceScope(PerformanceStage stage, ClientPerformanceMetrics& metrics)
    : m_stage(stage), m_metrics(metrics) { m_timer.start(); }
PerformanceScope::~PerformanceScope() { m_metrics.record(m_stage, m_timer.nsecsElapsed()); }

} // 命名空间 sleekpr::infrastructure

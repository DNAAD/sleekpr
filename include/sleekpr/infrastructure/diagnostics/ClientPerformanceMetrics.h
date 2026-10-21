#pragma once

#include <QElapsedTimer>
#include <QJsonObject>
#include <QMutex>
#include <array>

namespace sleekpr::infrastructure {

enum class PerformanceStage { Planning, Rasterization, Saving, Publication, ExternalRefresh, DeferredRefresh, DesignerRefresh, SaveEnqueue, Count };

class ClientPerformanceMetrics
{
public:
    static constexpr int RetainedSamples = 512;
    void record(PerformanceStage stage, qint64 nanoseconds);
    QJsonObject snapshot() const;
    static ClientPerformanceMetrics& applicationMetrics();
private:
    struct Samples
    {
        std::array<qint64, RetainedSamples> values{};
        int retained = 0;
        int next = 0;
        qint64 count = 0;
    };
    mutable QMutex m_mutex;
    std::array<Samples, static_cast<int>(PerformanceStage::Count)> m_stages;
};

class PerformanceScope
{
public:
    explicit PerformanceScope(PerformanceStage stage,
        ClientPerformanceMetrics& metrics = ClientPerformanceMetrics::applicationMetrics());
    ~PerformanceScope();
private:
    PerformanceStage m_stage;
    ClientPerformanceMetrics& m_metrics;
    QElapsedTimer m_timer;
};

} // 命名空间 sleekpr::infrastructure

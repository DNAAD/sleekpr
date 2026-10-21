#include "sleekpr/app/LatestPreviewQueue.h"
#include <QThread>
#include <QMutex>
#include <QMutexLocker>
#include <utility>

namespace sleekpr::app {

struct LatestPreviewQueue::State
{
    struct Payload
    {
        QMutex mutex;
        std::function<infrastructure::PreviewWorkResult()> work;
        std::function<infrastructure::PreviewWorkResult()> take()
        {
            QMutexLocker lock(&mutex);
            return std::exchange(work, {});
        }
        void cancel() { const auto discarded = take(); }
    };
    struct Job
    {
        quint64 generation;
        std::function<infrastructure::PreviewWorkResult()> work;
    };
    infrastructure::PreviewExecutor* executor = nullptr;
    LatestPreviewQueue* owner = nullptr;
    quint64 generation = 0;
    bool running = false;
    std::shared_ptr<Payload> runningPayload;
    std::optional<Job> pending;
};

LatestPreviewQueue::LatestPreviewQueue(infrastructure::PreviewExecutor& executor, QObject* parent)
    : QObject(parent), m_state(std::make_shared<State>())
{
    Q_ASSERT(thread() == executor.thread());
    m_state->executor = &executor;
    m_state->owner = this;
}

LatestPreviewQueue::~LatestPreviewQueue()
{
    invalidate();
    m_state->owner = nullptr;
}

quint64 LatestPreviewQueue::submit(std::function<infrastructure::PreviewWorkResult()> work)
{
    Q_ASSERT(QThread::currentThread() == thread());
    const auto generation = ++m_state->generation;
    if (m_state->runningPayload) m_state->runningPayload->cancel();
    m_state->pending = State::Job{generation, std::move(work)};
    startPending(m_state);
    return generation;
}

void LatestPreviewQueue::invalidate()
{
    Q_ASSERT(QThread::currentThread() == thread());
    ++m_state->generation;
    if (m_state->runningPayload) m_state->runningPayload->cancel();
    m_state->pending.reset();
}

void LatestPreviewQueue::startPending(const std::shared_ptr<State>& state)
{
    if (state->running || !state->pending || !state->owner) return;
    auto job = std::move(*state->pending);
    state->pending.reset();
    state->running = true;
    const auto payload = std::make_shared<State::Payload>();
    payload->work = std::move(job.work);
    state->runningPayload = payload;
    const auto generation = job.generation;
    // 弱状态只在执行器的 GUI 回调中检查，工作线程不读取窗口指针或 GUI QPointer。
    const std::weak_ptr<State> weak = state;
    state->executor->submit([payload] {
        // 已排入全局池但尚未开始的旧任务可立即释放快照；这里只访问互斥保护的数据，不读取 GUI 状态。
        auto work = payload->take();
        if (work) return work();
        infrastructure::PreviewWorkResult canceled;
        canceled.errorCode = QStringLiteral("PREVIEW_SUPERSEDED");
        return canceled;
    }, [weak, generation](infrastructure::PreviewWorkResult result) {
        const auto state = weak.lock();
        if (!state || !state->owner) return;
        state->running = false;
        state->runningPayload.reset();
        if (generation == state->generation) emit state->owner->resultReady(generation, result);
        // 信号处理可能销毁窗口、失效队列或提交新任务，因此再次按当前状态判断。
        startPending(state);
    });
}

} // 命名空间 sleekpr::app

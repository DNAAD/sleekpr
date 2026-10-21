#pragma once

#include "sleekpr/infrastructure/preview/PreviewExecutor.h"
#include <memory>

namespace sleekpr::app {

class LatestPreviewQueue final : public QObject
{
    Q_OBJECT
public:
    explicit LatestPreviewQueue(infrastructure::PreviewExecutor& executor, QObject* parent = nullptr);
    ~LatestPreviewQueue() override;
    quint64 submit(std::function<infrastructure::PreviewWorkResult()> work);
    void invalidate();
signals:
    void resultReady(quint64 generation, const sleekpr::infrastructure::PreviewWorkResult& result);
private:
    struct State;
    std::shared_ptr<State> m_state;
    static void startPending(const std::shared_ptr<State>& state);
};

} // 命名空间 sleekpr::app

#include "sleekpr/infrastructure/preview/PreviewExecutor.h"
#include <QCoreApplication>
#include <QMetaObject>
#include <QRunnable>
#include <QThread>
#include <new>
#include <stdexcept>
#include <utility>

namespace sleekpr::infrastructure {

namespace {
PreviewExecutor* applicationExecutor = nullptr;
} // 匿名命名空间

PreviewExecutor::PreviewExecutor(QObject* parent) : QObject(parent)
{
    Q_ASSERT(QCoreApplication::instance() && thread() == QCoreApplication::instance()->thread());
    m_pool.setMaxThreadCount(2);
    if (!applicationExecutor) applicationExecutor = this;
}

PreviewExecutor::~PreviewExecutor()
{
    // 所有执行器都必须先于 QApplication 析构，避免字体和图像服务销毁后工作线程仍在使用它们。
    shutdown();
    if (applicationExecutor == this) applicationExecutor = nullptr;
}

void PreviewExecutor::shutdown()
{
    Q_ASSERT(QThread::currentThread() == thread());
    m_stopping = true;
    m_pool.waitForDone();
}

void PreviewExecutor::submit(std::function<PreviewWorkResult()> work, std::function<void(PreviewWorkResult)> completion)
{
    Q_ASSERT(QThread::currentThread() == thread());
    if (m_stopping) {
        PreviewWorkResult result;
        result.errorCode = QStringLiteral("PREVIEW_STOPPED");
        result.errorMessage = QString::fromUtf8("预览执行器正在退出");
        completion(std::move(result));
        return;
    }
    m_pool.start(QRunnable::create([this, work = std::move(work), completion = std::move(completion)]() mutable {
        PreviewWorkResult result;
        try {
            result = work();
        } catch (const std::bad_alloc&) {
            result.errorCode = QStringLiteral("PREVIEW_ALLOCATION_FAILED");
            result.errorMessage = QString::fromUtf8("预览内存分配失败");
        } catch (...) {
            result.errorCode = QStringLiteral("PREVIEW_FAILED");
            result.errorMessage = QString::fromUtf8("预览计算失败");
        }
        // 接收对象是由应用拥有的执行器，而不是窗口；析构会先收束线程，再由 QObject 丢弃未派发回调。
        QMetaObject::invokeMethod(this, [completion = std::move(completion), result = std::move(result)]() mutable {
            completion(std::move(result));
        }, Qt::QueuedConnection);
    }));
}

PreviewExecutor& PreviewExecutor::applicationInstance()
{
    if (!applicationExecutor) throw std::logic_error("尚未安装应用作用域预览执行器");
    return *applicationExecutor;
}

} // 命名空间 sleekpr::infrastructure

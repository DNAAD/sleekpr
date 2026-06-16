#include "sleekpr/infrastructure/preview/QrCodeMatrix.h"

#include <algorithm>

namespace sleekpr::infrastructure {

QrCodeMatrix::QrCodeMatrix(int size, std::vector<bool> modules)
    : matrixSize_(size)
    , modules_(std::move(modules))
    , darkModuleCount_(static_cast<int>(std::count(modules_.begin(), modules_.end(), true)))
{
}

int QrCodeMatrix::size() const
{
    return matrixSize_;
}

int QrCodeMatrix::darkModuleCount() const
{
    return darkModuleCount_;
}

bool QrCodeMatrix::hasDarkModule(int x, int y) const
{
    if (x < 0 || y < 0 || x >= matrixSize_ || y >= matrixSize_) {
        return false;
    }

    return modules_[static_cast<size_t>(y * matrixSize_ + x)];
}

bool QrCodeMatrix::operator==(const QrCodeMatrix& other) const
{
    return matrixSize_ == other.matrixSize_ && modules_ == other.modules_;
}

bool QrCodeMatrix::operator!=(const QrCodeMatrix& other) const
{
    return !(*this == other);
}

} // 命名空间 sleekpr::infrastructure

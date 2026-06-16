#pragma once

#include <vector>

namespace sleekpr::infrastructure {

class QrCodeMatrix
{
public:
    QrCodeMatrix() = default;
    QrCodeMatrix(int size, std::vector<bool> modules);

    int size() const;
    int darkModuleCount() const;
    bool hasDarkModule(int x, int y) const;

    bool operator==(const QrCodeMatrix& other) const;
    bool operator!=(const QrCodeMatrix& other) const;

private:
    int matrixSize_ = 0;
    std::vector<bool> modules_;
    int darkModuleCount_ = 0;
};

} // 命名空间 sleekpr::infrastructure

#include "sleekpr/infrastructure/printing/QrModulePixelLayout.h"

#include <algorithm>
#include <cmath>

namespace sleekpr::infrastructure {

QList<int> QrModulePixelLayout::moduleBounds(int moduleCount, int pixelSize)
{
    if (moduleCount <= 0 || pixelSize <= 0) {
        return {0};
    }

    QList<int> bounds;
    bounds.reserve(moduleCount + 1);

    for (int index = 0; index <= moduleCount; ++index) {
        // 按设计总宽度分配模块边界，保证二维码整体尺寸不被整数模块取整缩小。
        const auto raw = static_cast<double>(index) * pixelSize / moduleCount;
        bounds.append(static_cast<int>(std::round(raw)));
    }

    bounds.first() = 0;
    bounds.last() = pixelSize;

    if (pixelSize >= moduleCount) {
        for (int index = 1; index < bounds.size(); ++index) {
            bounds[index] = std::max(bounds[index], bounds[index - 1] + 1);
        }
        bounds.last() = pixelSize;
    }

    return bounds;
}

} // namespace sleekpr::infrastructure

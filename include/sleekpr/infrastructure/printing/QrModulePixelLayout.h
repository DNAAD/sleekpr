#pragma once

#include <QList>

namespace sleekpr::infrastructure {

class QrModulePixelLayout final
{
public:
    static QList<int> moduleBounds(int moduleCount, int pixelSize);
};

} // namespace sleekpr::infrastructure

#pragma once

#include <QString>

namespace sleekpr::core {

enum class PaperSpecKind
{
    Label,
    Roll,
    Sheet,
};

enum class PaperOrientation
{
    Portrait,
    Landscape,
};

struct LabelGridSpec
{
    // 多标签纸启用后，同一页会按行列切分成多个标签单元。
    bool enabled = false;
    int rows = 1;
    int columns = 1;
    double horizontalGapMm = 0.0;
    double verticalGapMm = 0.0;
};

struct PaperSpec
{
    // schemaVersion 用于后续拒绝未知纸张结构。
    int schemaVersion = 1;
    QString id;
    QString name;
    PaperSpecKind kind = PaperSpecKind::Label;
    PaperOrientation orientation = PaperOrientation::Portrait;
    double widthMm = 80.0;
    double heightMm = 30.0;
    double marginLeftMm = 0.0;
    double marginTopMm = 0.0;
    double marginRightMm = 0.0;
    double marginBottomMm = 0.0;
    double defaultDpi = 300.0;
    LabelGridSpec labelGrid;
};

} // 命名空间 sleekpr::core

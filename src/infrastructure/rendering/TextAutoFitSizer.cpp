#include "sleekpr/infrastructure/rendering/TextAutoFitSizer.h"

#include <QFont>
#include <QTextDocument>
#include <QTextOption>

#include <algorithm>
#include <cmath>

namespace sleekpr::infrastructure {
namespace {

double normalizedDpi(double dpi)
{
    return dpi > 0.0 ? dpi : 300.0;
}

double normalizedMinFontSize(const sleekpr::core::NativeDrawCommand& command)
{
    return command.autoFitMinFontSizePt > 0.0 ? command.autoFitMinFontSizePt : std::max(1.0, command.fontSizePt);
}

double normalizedMaxFontSize(const sleekpr::core::NativeDrawCommand& command, double minFontSizePt)
{
    const auto configuredMax = command.autoFitMaxFontSizePt > 0.0 ? command.autoFitMaxFontSizePt : command.fontSizePt;
    return std::max(minFontSizePt, configuredMax);
}

QFont measuredFont(const sleekpr::core::NativeDrawCommand& command, double pointSizePt, double dpiX, double dpiY)
{
    QFont font(QStringLiteral("Microsoft YaHei"));
    const auto averageDpi = (normalizedDpi(dpiX) + normalizedDpi(dpiY)) / 2.0;
    font.setPixelSize(std::max(1, static_cast<int>(std::round(pointSizePt * averageDpi / 72.0))));
    font.setBold(command.bold);
    font.setHintingPreference(QFont::PreferFullHinting);
    font.setStyleStrategy(QFont::NoAntialias);
    return font;
}

QSizeF measuredTextSize(
    const sleekpr::core::NativeDrawCommand& command,
    const QFont& font,
    QSizeF layoutSizePx)
{
    QTextDocument document;
    document.setDefaultFont(font);
    document.setDocumentMargin(0.0);

    QTextOption option;
    option.setWrapMode(command.wrapText || command.ellipsis ? QTextOption::WrapAtWordBoundaryOrAnywhere : QTextOption::NoWrap);
    option.setAlignment(Qt::AlignLeft | Qt::AlignTop);
    document.setDefaultTextOption(option);

    if (command.wrapText || command.ellipsis) {
        document.setTextWidth(std::max(1.0, layoutSizePx.width()));
    }
    document.setPlainText(command.text);
    return document.size();
}

bool textFits(
    const sleekpr::core::NativeDrawCommand& command,
    QSizeF layoutSizePx,
    double pointSizePt,
    double dpiX,
    double dpiY)
{
    const auto textSize = measuredTextSize(command, measuredFont(command, pointSizePt, dpiX, dpiY), layoutSizePx);
    return textSize.width() <= layoutSizePx.width() + 0.5
        && textSize.height() <= layoutSizePx.height() + 0.5;
}

} // 匿名命名空间

double TextAutoFitSizer::fitPointSize(
    const sleekpr::core::NativeDrawCommand& command,
    QSizeF layoutSizePx,
    double dpiX,
    double dpiY)
{
    if (!command.autoFitFont || command.type != sleekpr::core::NativeDrawCommandType::Text) {
        return command.fontSizePt;
    }
    if (layoutSizePx.width() <= 0.0 || layoutSizePx.height() <= 0.0) {
        return command.fontSizePt;
    }

    const auto minFontSizePt = normalizedMinFontSize(command);
    const auto maxFontSizePt = normalizedMaxFontSize(command, minFontSizePt);
    if (command.text.trimmed().isEmpty()) {
        return maxFontSizePt;
    }

    double low = minFontSizePt;
    double high = maxFontSizePt;
    for (int iteration = 0; iteration < 16; ++iteration) {
        const auto middle = (low + high) / 2.0;
        if (textFits(command, layoutSizePx, middle, dpiX, dpiY)) {
            low = middle;
        } else {
            high = middle;
        }
    }
    return std::clamp(low, minFontSizePt, maxFontSizePt);
}

} // 命名空间 sleekpr::infrastructure

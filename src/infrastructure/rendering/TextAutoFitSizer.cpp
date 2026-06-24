#include "sleekpr/infrastructure/rendering/TextAutoFitSizer.h"

#include <QFont>
#include <QHash>
#include <QMutex>
#include <QMutexLocker>
#include <QString>
#include <QTextDocument>
#include <QTextOption>

#include <algorithm>
#include <cmath>

namespace sleekpr::infrastructure {
namespace {

constexpr int kMaxAutoFitCacheEntries = 512;

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

QHash<QString, double>& autoFitCache()
{
    static QHash<QString, double> cache;
    return cache;
}

QMutex& autoFitCacheMutex()
{
    static QMutex mutex;
    return mutex;
}

QString cacheNumber(double value)
{
    return QString::number(value, 'f', 3);
}

QString autoFitCacheKey(
    const sleekpr::core::NativeDrawCommand& command,
    QSizeF layoutSizePx,
    double dpiX,
    double dpiY,
    double minFontSizePt,
    double maxFontSizePt)
{
    QString key;
    key.reserve(command.text.size() + 160);
    const auto append = [&key](const QString& part) {
        key += part;
        key += QLatin1Char('|');
    };

    append(command.text);
    append(command.bold ? QStringLiteral("1") : QStringLiteral("0"));
    append(command.wrapText ? QStringLiteral("1") : QStringLiteral("0"));
    append(command.ellipsis ? QStringLiteral("1") : QStringLiteral("0"));
    append(cacheNumber(command.fontSizePt));
    append(cacheNumber(minFontSizePt));
    append(cacheNumber(maxFontSizePt));
    append(cacheNumber(layoutSizePx.width()));
    append(cacheNumber(layoutSizePx.height()));
    append(cacheNumber(dpiX));
    append(cacheNumber(dpiY));
    return key;
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

    const auto cacheKey = autoFitCacheKey(command, layoutSizePx, dpiX, dpiY, minFontSizePt, maxFontSizePt);
    {
        QMutexLocker locker(&autoFitCacheMutex());
        const auto cachedIt = autoFitCache().constFind(cacheKey);
        if (cachedIt != autoFitCache().cend()) {
            return cachedIt.value();
        }
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
    const auto fittedSize = std::clamp(low, minFontSizePt, maxFontSizePt);
    {
        QMutexLocker locker(&autoFitCacheMutex());
        auto& cache = autoFitCache();
        if (cache.size() >= kMaxAutoFitCacheEntries && !cache.isEmpty()) {
            // 缓存只服务交互编辑和短期批量渲染，达到上限时丢弃任意旧项即可避免无界增长。
            cache.erase(cache.begin());
        }
        cache.insert(cacheKey, fittedSize);
    }
    return fittedSize;
}

void TextAutoFitSizer::clearCache()
{
    QMutexLocker locker(&autoFitCacheMutex());
    autoFitCache().clear();
}

int TextAutoFitSizer::cacheEntryCount()
{
    QMutexLocker locker(&autoFitCacheMutex());
    return autoFitCache().size();
}

} // 命名空间 sleekpr::infrastructure

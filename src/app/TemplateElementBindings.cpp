#include "sleekpr/app/TemplateElementBindings.h"
#include "sleekpr/app/TemplateFieldNames.h"

#include <QHash>
#include <QRegularExpression>

namespace sleekpr::app {
namespace {

const QRegularExpression& placeholderPattern()
{
    static const QRegularExpression pattern(QStringLiteral(R"(\$\{([^}]*)\})"));
    return pattern;
}

QStringList referenceKeys(const QString& text)
{
    QStringList keys;
    auto matches = placeholderPattern().globalMatch(text);
    while (matches.hasNext()) {
        const auto key = matches.next().captured(1).trimmed();
        if (!key.isEmpty() && !keys.contains(key)) keys.append(key);
    }
    return keys;
}

QString valueExpression(const core::TemplateElement& element)
{
    if (element.type == core::TemplateElementType::FixedText) return element.text;
    if (element.type == core::TemplateElementType::QrCode) {
        return element.payload.trimmed().isEmpty() && !element.fieldKey.trimmed().isEmpty()
            ? QStringLiteral("${%1}").arg(element.fieldKey.trimmed()) : element.payload;
    }
    return {};
}

QString cellExpression(const core::TemplateElement& element)
{
    return element.arrayGridCellTemplate.trimmed().isEmpty() ? QStringLiteral("${value}") : element.arrayGridCellTemplate;
}

QString rewrite(const QString& text, const QHash<QString, QString>& keys)
{
    // 只替换占位符对应的完整键，一次扫描完成；交换键名不会被第二次替换，普通文字保持原样。
    QString result;
    qsizetype position = 0;
    auto matches = placeholderPattern().globalMatch(text);
    while (matches.hasNext()) {
        const auto match = matches.next();
        result += text.mid(position, match.capturedStart() - position);
        const auto replacement = keys.constFind(match.captured(1).trimmed());
        result += replacement == keys.cend() ? match.captured() : QStringLiteral("${%1}").arg(*replacement);
        position = match.capturedEnd();
    }
    return result + text.mid(position);
}

} // 匿名命名空间

QList<DesignerFieldBinding> TemplateElementBindings::describe(const core::TemplateElement& element, const core::TemplateDocument& document)
{
    QList<DesignerFieldBinding> result;
    const auto append = [&](DesignerBindingSource source, const QString& key) {
        result.append({source, key, key, key.isEmpty() ? QString() : TemplateFieldNames::resolve(document, key), false});
    };
    if (element.type == core::TemplateElementType::ArrayGrid) {
        append(DesignerBindingSource::ArrayDataPath, element.dataPath.trimmed());
        for (const auto& key : referenceKeys(cellExpression(element))) append(DesignerBindingSource::ArrayCellTemplate, key);
    } else {
        for (const auto& key : referenceKeys(valueExpression(element))) append(DesignerBindingSource::Value, key);
    }
    return result;
}

bool TemplateElementBindings::references(const core::TemplateElement& element, DesignerBindingSource source, const QString& key)
{
    switch (source) {
    case DesignerBindingSource::Value: return referenceKeys(valueExpression(element)).contains(key);
    case DesignerBindingSource::ArrayDataPath: return element.type == core::TemplateElementType::ArrayGrid && element.dataPath.trimmed() == key;
    case DesignerBindingSource::ArrayCellTemplate: return element.type == core::TemplateElementType::ArrayGrid && referenceKeys(cellExpression(element)).contains(key);
    }
    return false;
}

QString TemplateElementBindings::apply(core::TemplateElement& updated, const core::TemplateElement& original,
    const QList<DesignerFieldBinding>& bindings, bool deferMerge)
{
    if (deferMerge) {
        for (int first = 0; first < bindings.size(); ++first) {
            for (int second = first + 1; second < bindings.size(); ++second) {
                const auto& left = bindings[first];
                const auto& right = bindings[second];
                if (left.source == right.source && left.originalKey != right.originalKey && left.key.trimmed() == right.key.trimmed()
                    && references(updated, left.source, left.originalKey) && references(updated, right.source, right.originalKey)) {
                    return QString::fromUtf8("绑定 key 暂与现有引用重复，请完成输入或移开焦点确认合并");
                }
            }
        }
    }
    QHash<QString, QString> values;
    QHash<QString, QString> cells;
    auto path = updated.dataPath;
    for (const auto& binding : bindings) {
        const auto key = binding.key.trimmed();
        const bool changed = key != binding.originalKey;
        if ((!changed && !binding.displayNameEdited) || !references(updated, binding.source, binding.originalKey)) continue;
        if (key.isEmpty() || key.contains(QLatin1Char('{')) || key.contains(QLatin1Char('}')) || key.contains(QLatin1Char('\n')) || key.contains(QLatin1Char('\r'))) {
            return QString::fromUtf8("绑定 key 不能为空，也不能包含花括号或换行");
        }
        if (!changed) continue;
        switch (binding.source) {
        case DesignerBindingSource::Value: values.insert(binding.originalKey, key); break;
        case DesignerBindingSource::ArrayDataPath: path = key; break;
        case DesignerBindingSource::ArrayCellTemplate: cells.insert(binding.originalKey, key); break;
        }
    }
    if (updated.type == core::TemplateElementType::FixedText) updated.text = rewrite(updated.text, values);
    if (updated.type == core::TemplateElementType::QrCode) {
        const auto oldKey = original.fieldKey.trimmed();
        const bool legacyBinding = original.payload.trimmed().isEmpty() && !oldKey.isEmpty()
            && (updated.payload.trimmed().isEmpty() || updated.payload == QStringLiteral("${%1}").arg(oldKey));
        if (legacyBinding) {
            // 旧二维码的单字段绑定继续使用 fieldKey，不因编辑名称或坐标改变存储方式。
            if (values.contains(oldKey)) updated.fieldKey = values.value(oldKey);
            updated.payload = original.payload;
        } else {
            updated.payload = rewrite(updated.payload, values);
        }
    }
    if (updated.type == core::TemplateElementType::ArrayGrid) {
        updated.dataPath = path;
        if (!cells.isEmpty()) updated.arrayGridCellTemplate = rewrite(cellExpression(updated), cells);
    }
    return {};
}

} // 命名空间 sleekpr::app

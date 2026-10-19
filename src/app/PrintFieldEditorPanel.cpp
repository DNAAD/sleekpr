#include "sleekpr/app/PrintFieldEditorPanel.h"
#include "sleekpr/app/TemplateFieldNames.h"
#include "sleekpr/core/templates/TemplateInputSchema.h"

#include <QCheckBox>
#include <QDoubleValidator>
#include <QFormLayout>
#include <QFrame>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QPlainTextEdit>
#include <QPointer>
#include <QScopedValueRollback>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QSizePolicy>
#include <QTabBar>
#include <QTabWidget>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextOption>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

namespace sleekpr::app {

namespace {

constexpr qsizetype maximumJsonBytes = 1024 * 1024;

QString jsonError(const QByteArray& bytes, qsizetype offset, const QString& message)
{
    // Qt 的解析偏移以 UTF-8 字节计数；界面列号必须换算为用户看到的字符位置。
    const auto prefix = QString::fromUtf8(bytes.left(std::clamp(offset, qsizetype(0), bytes.size())));
    int line = 1;
    int column = 1;
    for (const auto character : prefix) {
        if (character == QLatin1Char('\n')) {
            ++line;
            column = 1;
        } else if (!character.isLowSurrogate()) {
            ++column;
        }
    }
    return QString::fromUtf8("第 %1 行，第 %2 列：%3").arg(line).arg(column).arg(message);
}

bool parseJson(const QString& text, bool array, QJsonDocument* result, QString* error)
{
    const auto bytes = text.toUtf8();
    if (bytes.size() > maximumJsonBytes) {
        *error = jsonError(bytes, maximumJsonBytes, QString::fromUtf8("JSON 不能超过 1 MB"));
        return false;
    }
    QJsonParseError parseError;
    const auto parsed = QJsonDocument::fromJson(bytes, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        *error = jsonError(bytes, parseError.offset,
            QString::fromUtf8("JSON 语法错误：%1").arg(parseError.errorString()));
        return false;
    }
    if (array ? !parsed.isArray() : !parsed.isObject()) {
        *error = jsonError(bytes, 0, array ? QString::fromUtf8("JSON 必须是数组") : QString::fromUtf8("JSON 必须是对象"));
        return false;
    }
    *result = parsed;
    error->clear();
    return true;
}

QString objectText(const QJsonObject& object)
{
    const QJsonDocument document(object);
    const auto indented = document.toJson(QJsonDocument::Indented);
    // 排版空白不能使原本处于大小边界内的合法对象无法切换模式。
    return QString::fromUtf8(indented.size() <= maximumJsonBytes
        ? indented : document.toJson(QJsonDocument::Compact));
}

QString valueText(const QJsonValue& value)
{
    if (value.isString()) {
        return value.toString();
    }
    if (value.isDouble()) {
        return QString::number(value.toDouble(), 'g', 17);
    }
    if (value.isBool()) {
        return value.toBool() ? QStringLiteral("true") : QStringLiteral("false");
    }
    if (value.isArray()) {
        return QString::fromUtf8(QJsonDocument(value.toArray()).toJson(QJsonDocument::Indented));
    }
    if (value.isObject()) {
        return objectText(value.toObject());
    }
    return {};
}

bool numericType(sleekpr::core::FieldValueType type)
{
    using sleekpr::core::FieldValueType;
    return type == FieldValueType::Number || type == FieldValueType::Money || type == FieldValueType::Weight;
}

QJsonValue fieldValue(const QJsonObject& values, const QString& key, bool arrayPath)
{
    // 数组路径与渲染器一致，字面键优先；嵌套读取只服务显示和校验，不改写原对象。
    if (values.contains(key) || !arrayPath) {
        return values.value(key);
    }
    QJsonValue value(values);
    for (const auto& part : key.split(QLatin1Char('.'), Qt::SkipEmptyParts)) {
        if (!value.isObject()) {
            return QJsonValue(QJsonValue::Undefined);
        }
        value = value.toObject().value(part);
    }
    return value;
}

QJsonValue emptyMarker(sleekpr::core::FieldValueType type)
{
    using sleekpr::core::FieldValueType;
    if (type == FieldValueType::List) {
        return QJsonArray{};
    }
    if (type == FieldValueType::Object) {
        return QJsonObject{};
    }
    // 数字和布尔也用空字符串表达有意留空，不能把 0 或 false 当作空值。
    return QString();
}

QLabel* errorLabel(QWidget* parent, const QString& name)
{
    auto* label = new QLabel(parent);
    label->setObjectName(name);
    label->setProperty("statusRole", QStringLiteral("danger"));
    label->setWordWrap(true);
    label->setMinimumWidth(0);
    label->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    label->hide();
    return label;
}

QString fieldTitle(const sleekpr::core::FieldDefinition& definition)
{
    auto title = definition.displayName.isEmpty() ? definition.key : definition.displayName;
    if (definition.required) title += QStringLiteral(" *");
    return title;
}

} // 匿名命名空间

PrintFieldEditorPanel::PrintFieldEditorPanel(QWidget* parent)
    : QWidget(parent)
{
    setMinimumWidth(0);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setObjectName(QStringLiteral("printFieldEditorPanel"));
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(6);

    m_modeTabs = new QTabWidget(this);
    m_modeTabs->setObjectName(QStringLiteral("printFieldModeTabs"));
    m_modeTabs->setMinimumWidth(0);
    m_modeTabs->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Expanding);
    m_modeTabs->setUsesScrollButtons(true);
    m_modeTabs->setElideMode(Qt::ElideRight);

    m_formScroll = new QScrollArea(m_modeTabs);
    m_formScroll->setObjectName(QStringLiteral("printFieldsScrollArea"));
    m_formScroll->setFrameShape(QFrame::NoFrame);
    m_formScroll->setWidgetResizable(true);
    m_formScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_formScroll->setMinimumWidth(0);
    m_formContent = new QWidget(m_formScroll);
    m_formContent->setObjectName(QStringLiteral("printFieldsFormContent"));
    m_formContent->setMinimumWidth(0);
    m_formContent->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    m_formLayout = new QFormLayout(m_formContent);
    m_formLayout->setContentsMargins(8, 8, 8, 8);
    m_formLayout->setSpacing(6);
    m_formLayout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    m_formLayout->setRowWrapPolicy(QFormLayout::WrapAllRows);
    m_formLayout->setFormAlignment(Qt::AlignTop);
    m_formScroll->setWidget(m_formContent);
    m_formScroll->hide();

    m_jsonPage = new QWidget(m_modeTabs);
    m_jsonPage->setObjectName(QStringLiteral("printValuesJsonPage"));
    auto* jsonLayout = new QVBoxLayout(m_jsonPage);
    jsonLayout->setContentsMargins(8, 8, 8, 8);
    m_jsonEdit = new QPlainTextEdit(m_jsonPage);
    m_jsonEdit->setObjectName(QStringLiteral("printValuesJsonEdit"));
    m_jsonEdit->setMinimumWidth(0);
    m_jsonEdit->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Expanding);
    m_jsonEdit->setLineWrapMode(QPlainTextEdit::WidgetWidth);
    m_jsonEdit->setWordWrapMode(QTextOption::WrapAnywhere);
    jsonLayout->addWidget(m_jsonEdit);
    m_modeTabs->addTab(m_jsonPage, QStringLiteral("JSON"));
    m_modeTabs->tabBar()->hide();
    m_activePage = m_jsonPage;
    root->addWidget(m_modeTabs, 1);
    m_errorLabel = errorLabel(this, QStringLiteral("printValuesErrorLabel"));
    root->addWidget(m_errorLabel);

    connect(m_modeTabs, &QTabWidget::currentChanged, this, [this](int) { changeMode(); });
    connect(m_jsonEdit, &QPlainTextEdit::textChanged, this, [this] { editorChanged(); });
    setValues({});
}

void PrintFieldEditorPanel::setDocument(const sleekpr::core::TemplateDocument& document)
{
    const QSignalBlocker panelBlocker(this);
    const QSignalBlocker tabsBlocker(m_modeTabs);
    const QScopedValueRollback<bool> loading(m_loading, true);
    bool currentOk = false;
    const auto current = values(&currentOk);
    const bool wasJson = m_activePage == m_jsonPage;
    const bool hadFields = !m_fields.isEmpty();
    // 不能借由重建表单抹掉尚未修复的输入；调用方可先显式 setValues 替换草稿。
    if (!currentOk && !wasJson) {
        return;
    }
    if (currentOk) {
        m_baseValues = current;
    }
    rebuildForm(document);
    populateForm();
    const int formIndex = m_modeTabs->indexOf(m_formScroll);
    if (m_fields.isEmpty()) {
        if (formIndex >= 0) {
            m_modeTabs->removeTab(formIndex);
            m_formScroll->hide();
        }
        m_modeTabs->tabBar()->hide();
        m_activePage = m_jsonPage;
    } else {
        if (formIndex < 0) {
            m_modeTabs->insertTab(0, m_formScroll, QString::fromUtf8("字段"));
        }
        m_modeTabs->tabBar()->show();
        m_activePage = !currentOk || (hadFields && wasJson) ? m_jsonPage : m_formScroll;
    }
    if (currentOk) {
        const QSignalBlocker editorBlocker(m_jsonEdit);
        m_jsonEdit->setPlainText(objectText(m_baseValues));
    }
    m_modeTabs->setCurrentWidget(m_activePage);
    bool ok = false;
    m_lastEditedValues = values(&ok);
    m_lastEditValid = ok;
}

void PrintFieldEditorPanel::setValues(const QJsonObject& values)
{
    // 批量灌入属于调用方同步，内部控件和对外语义信号均不得回发。
    const QSignalBlocker panelBlocker(this);
    const QScopedValueRollback<bool> loading(m_loading, true);
    m_baseValues = values;
    populateForm();
    const QSignalBlocker jsonBlocker(m_jsonEdit);
    m_jsonEdit->setPlainText(objectText(values));
    bool ok = false;
    m_lastEditedValues = this->values(&ok);
    m_lastEditValid = ok;
}

void PrintFieldEditorPanel::refreshDisplayNames(const sleekpr::core::TemplateDocument& document)
{
    // 只刷新名称和错误文字，不重建控件或覆盖正在编辑的非法草稿。
    bool changed = false;
    for (auto& field : m_fields) {
        const auto name = TemplateFieldNames::resolve(document, field.definition.key);
        changed = changed || field.definition.displayName != name;
        field.definition.displayName = name;
        field.input->setAccessibleName(field.definition.displayName);
        field.input->setToolTip(field.definition.key);
        if (field.titleLabel != nullptr) {
            field.titleLabel->setText(fieldTitle(field.definition));
            field.titleLabel->setToolTip(field.definition.key);
        }
    }
    if (changed) values();
}

QJsonObject PrintFieldEditorPanel::values(bool* ok, QString* errorMessage) const
{
    m_invalidFieldKey.clear();
    QString error;
    QJsonObject result;
    if (m_activePage == m_jsonPage) {
        QJsonDocument parsed;
        if (parseJson(m_jsonEdit->toPlainText(), false, &parsed, &error)) {
            result = parsed.object();
        }
    } else {
        result = formValues(&error);
    }
    if (error.isEmpty() && QJsonDocument(result).toJson(QJsonDocument::Compact).size() > maximumJsonBytes) {
        error = jsonError({}, 0, QString::fromUtf8("JSON 不能超过 1 MB"));
    }
    if (error.isEmpty()) {
        for (const auto& field : m_fields) {
            const auto value = fieldValue(result, field.definition.key, field.arrayPath);
            if (value.isUndefined() || value.isNull()) continue;
            using sleekpr::core::FieldValueType;
            const auto type = field.definition.type;
            // 标量空字符串兼容有意留空；结构化字段仍必须是对应的数组或对象。
            if (type != FieldValueType::List && type != FieldValueType::Object
                && value.isString() && value.toString().trimmed().isEmpty()) continue;
            bool valid = true;
            if (numericType(type)) {
                bool numberOk = value.isDouble();
                const auto number = value.isString() ? QLocale::c().toDouble(value.toString(), &numberOk) : value.toDouble();
                valid = (value.isDouble() || value.isString()) && numberOk && std::isfinite(number);
            } else if (type == FieldValueType::Boolean) {
                valid = value.isBool();
            } else if (type == FieldValueType::List) {
                valid = value.isArray();
            } else if (type == FieldValueType::Object) {
                valid = value.isObject();
            }
            if (!valid) {
                // JSON 与表单遵守同一类型边界，不把非法数字、布尔值或结构化字段悄悄转换成默认值。
                error = QString::fromUtf8("字段 %1 的值类型不正确").arg(field.definition.displayName);
                m_invalidFieldKey = field.definition.key;
                break;
            }
        }
    }
    if (ok != nullptr) {
        *ok = error.isEmpty();
    }
    if (errorMessage != nullptr) {
        *errorMessage = error;
    }
    showError(error);
    // 出错时绝不把上一次合法对象伪装为本次输入，避免调用方继续预览或打印旧数据。
    return error.isEmpty() ? result : QJsonObject{};
}

bool PrintFieldEditorPanel::focusField(const QString& fieldKey)
{
    const auto field = std::find_if(m_fields.cbegin(), m_fields.cend(), [&fieldKey](const auto& candidate) {
        return candidate.definition.key == fieldKey;
    });
    if (field == m_fields.cend()) return false;

    m_modeTabs->setCurrentWidget(m_formScroll);
    if (m_activePage != m_formScroll) {
        // 沿用模式切换的无损校验；非法 JSON 被拒绝后必须定位原稿，不能显示旧表单值。
        focusJsonError();
        return false;
    }
    auto* target = field->input;
    if (!target->isEnabled() && field->allowEmpty != nullptr) target = field->allowEmpty;
    target->setFocus(Qt::OtherFocusReason);
    const auto reveal = [this, target = QPointer<QWidget>(target)] {
        if (target.isNull() || m_activePage != m_formScroll || !target->hasFocus()) return;
        // 错误提示会改变外层视口和字段容器高度，先完成两层排版，再读取输入框的真实几何。
        layout()->activate();
        m_formLayout->activate();
        const auto center = target->mapTo(m_formContent, target->rect().center());
        const auto viewport = m_formScroll->viewport()->size();
        const auto marginX = std::max(0, std::min(target->width() / 2 + 12, viewport.width() / 2 - 1));
        const auto marginY = std::max(0, std::min(target->height() / 2 + 12, viewport.height() / 2 - 1));
        // ensureWidgetVisible 对多行编辑器优先定位插入光标；这里必须保证整个输入框的中心可见。
        m_formScroll->ensureVisible(center.x(), center.y(), marginX, marginY);
    };
    reveal();
    // 等待本轮模式回退和 LayoutRequest 完成后复核；旧控件销毁或焦点已转移时不得再滚动。
    QTimer::singleShot(0, this, reveal);
    return true;
}

void PrintFieldEditorPanel::focusJsonError(int line, int column)
{
    m_modeTabs->setCurrentWidget(m_jsonPage);
    if (m_activePage != m_jsonPage) {
        // 非法数字中间态和结构化字段原文不能因定位请求被覆盖为旧 JSON。
        if (!m_invalidFieldKey.isEmpty()) focusField(m_invalidFieldKey);
        else m_formScroll->setFocus(Qt::OtherFocusReason);
        return;
    }

    values();
    auto cursor = m_jsonEdit->textCursor();
    if (line > 0 && column > 0) {
        const auto* document = m_jsonEdit->document();
        const auto block = document->findBlockByNumber(std::min(line - 1, document->blockCount() - 1));
        const auto text = block.text();
        int position = 0;
        // 显示列从 1 开始计数；代理对占一个可见字符，但 QTextCursor 的位置使用 UTF-16。
        for (int currentColumn = 1; currentColumn < column && position < text.size(); ++currentColumn) {
            if (text.at(position).isHighSurrogate() && position + 1 < text.size()
                && text.at(position + 1).isLowSurrogate()) ++position;
            ++position;
        }
        cursor.setPosition(block.position() + position);
    } else {
        const auto bytes = m_jsonEdit->toPlainText().toUtf8();
        qsizetype offset = 0;
        if (bytes.size() > maximumJsonBytes) {
            offset = maximumJsonBytes;
        } else {
            QJsonParseError error;
            QJsonDocument::fromJson(bytes, &error);
            if (error.error != QJsonParseError::NoError) offset = error.offset;
        }
        // 直接换算解析器的 UTF-8 字节偏移，不依赖可翻译的错误字符串或上一份诊断坐标。
        const auto prefix = QString::fromUtf8(bytes.left(std::clamp(offset, qsizetype(0), bytes.size())));
        cursor.setPosition(static_cast<int>(prefix.size()));
    }
    m_jsonEdit->setFocus(Qt::OtherFocusReason);
    m_jsonEdit->setTextCursor(cursor);
    m_jsonEdit->ensureCursorVisible();
}

QString PrintFieldEditorPanel::invalidFieldKey() const
{
    return m_invalidFieldKey;
}

void PrintFieldEditorPanel::rebuildForm(const sleekpr::core::TemplateDocument& document)
{
    m_fields.clear();
    while (m_formLayout->rowCount() > 0) {
        m_formLayout->removeRow(0);
    }
    // 字段发现和样例类型推断由核心统一负责，界面不能复制引用扫描或取用样例值。
    for (const auto& field : sleekpr::core::TemplateInputSchema::fields(document)) {
        const auto& definition = field.definition;
        FieldEditor binding;
        binding.definition = definition;
        binding.definition.displayName = TemplateFieldNames::resolve(document, definition.key);
        binding.inferred = field.inferred;
        binding.arrayPath = field.arrayPath;
        auto* container = new QWidget(m_formContent);
        container->setProperty("printFieldContainer", true);
        container->setMinimumWidth(0);
        container->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
        auto* layout = new QVBoxLayout(container);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(4);
        const auto type = definition.type;
        using sleekpr::core::FieldValueType;
        if (type == FieldValueType::Boolean) {
            auto* checkbox = new QCheckBox(container);
            // 第三态表示未填，明确区分缺失、false 和 true。
            checkbox->setTristate(true);
            binding.input = checkbox;
        } else if (type == FieldValueType::List || type == FieldValueType::Object) {
            auto* editor = new QPlainTextEdit(container);
            editor->setFixedHeight(92);
            editor->setLineWrapMode(QPlainTextEdit::WidgetWidth);
            editor->setWordWrapMode(QTextOption::WrapAnywhere);
            binding.input = editor;
        } else {
            auto* editor = new QLineEdit(container);
            editor->setClearButtonEnabled(true);
            if (numericType(type)) {
                // 数字使用 Qt 校验器且允许清空；收集时再次校验，不能把中间态变成零。
                auto* validator = new QDoubleValidator(editor);
                auto locale = QLocale::c();
                locale.setNumberOptions(QLocale::RejectGroupSeparator);
                validator->setLocale(locale);
                validator->setNotation(QDoubleValidator::ScientificNotation);
                editor->setValidator(validator);
            }
            binding.input = editor;
        }
        binding.input->setObjectName(QStringLiteral("printField_") + definition.key);
        binding.input->setAccessibleName(binding.definition.displayName);
        binding.input->setToolTip(definition.key);
        binding.input->setMinimumWidth(0);
        binding.input->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
        layout->addWidget(binding.input);
        if (binding.inferred) {
            // 单独占一行，留空选项不能挤压窄栏内的字段标题和必填标记。
            binding.allowEmpty = new QCheckBox(QString::fromUtf8("留空"), container);
            binding.allowEmpty->setObjectName(QStringLiteral("printField_") + definition.key + QStringLiteral("_allowEmpty"));
            layout->addWidget(binding.allowEmpty, 0, Qt::AlignLeft);
        }
        binding.errorLabel = errorLabel(container, QStringLiteral("printField_") + definition.key + QStringLiteral("_error"));
        layout->addWidget(binding.errorLabel);
        auto* label = new QLabel(fieldTitle(binding.definition), m_formContent);
        label->setTextFormat(Qt::PlainText);
        label->setToolTip(definition.key);
        label->setWordWrap(true);
        label->setMinimumWidth(0);
        label->setMaximumWidth(260);
        label->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
        label->setBuddy(binding.input);
        binding.titleLabel = label;
        m_formLayout->addRow(label, container);
        const int index = static_cast<int>(m_fields.size());
        m_fields.append(binding);
        if (auto* line = qobject_cast<QLineEdit*>(binding.input)) {
            connect(line, &QLineEdit::textChanged, this, [this, index] { editorChanged(index); });
        } else if (auto* checkbox = qobject_cast<QCheckBox*>(binding.input)) {
            connect(checkbox, &QCheckBox::checkStateChanged, this, [this, index] { editorChanged(index); });
        } else if (auto* editor = qobject_cast<QPlainTextEdit*>(binding.input)) {
            connect(editor, &QPlainTextEdit::textChanged, this, [this, index] { editorChanged(index); });
        }
        if (binding.allowEmpty != nullptr) {
            connect(binding.allowEmpty, &QCheckBox::toggled, this, [this, index](bool checked) {
                // 不清除输入，取消留空时恢复原稿；预设空标记的输入则保持未填。
                m_fields[index].input->setEnabled(!checked);
                editorChanged(index);
            });
        }
    }
}

void PrintFieldEditorPanel::populateForm(bool preserveEmptyInputs)
{
    for (auto& field : m_fields) {
        const QSignalBlocker blocker(field.input);
        const auto value = fieldValue(m_baseValues, field.definition.key, field.arrayPath);
        const bool isEmpty = field.inferred && value == emptyMarker(field.definition.type);
        // 模式往返时保留留空前的原始输入；显式 setValues 则必须替换旧草稿。
        const bool keepInput = preserveEmptyInputs && isEmpty && field.allowEmpty->isChecked();
        if (!keepInput) {
            const auto inputValue = isEmpty ? QJsonValue(QJsonValue::Undefined) : value;
            if (auto* line = qobject_cast<QLineEdit*>(field.input)) {
                line->setText(valueText(inputValue));
            } else if (auto* checkbox = qobject_cast<QCheckBox*>(field.input)) {
                checkbox->setCheckState(inputValue.isBool() ? (inputValue.toBool() ? Qt::Checked : Qt::Unchecked) : Qt::PartiallyChecked);
            } else if (auto* editor = qobject_cast<QPlainTextEdit*>(field.input)) {
                editor->setPlainText(valueText(inputValue));
            }
        }
        if (field.allowEmpty != nullptr) {
            const QSignalBlocker emptyBlocker(field.allowEmpty);
            field.allowEmpty->setChecked(isEmpty);
        }
        field.input->setEnabled(!isEmpty);
        field.edited = false;
        field.errorLabel->clear();
        field.errorLabel->hide();
    }
}

QJsonObject PrintFieldEditorPanel::formValues(QString* errorMessage) const
{
    auto result = m_baseValues;
    for (const auto& field : m_fields) {
        QString error;
        const auto key = field.definition.key;
        if (field.edited) {
            if (field.allowEmpty != nullptr && field.allowEmpty->isChecked()) {
                result.insert(key, emptyMarker(field.definition.type));
            } else if (const auto* line = qobject_cast<QLineEdit*>(field.input)) {
                const auto text = line->text();
                if (numericType(field.definition.type)) {
                    if (text.trimmed().isEmpty()) {
                        result.remove(key);
                    } else {
                        auto candidate = text;
                        int position = candidate.size();
                        bool numberOk = false;
                        const auto number = QLocale::c().toDouble(text, &numberOk);
                        if (!numberOk || !std::isfinite(number) || line->validator()->validate(candidate, position) != QValidator::Acceptable) {
                            error = jsonError({}, 0, QString::fromUtf8("请输入有效数字"));
                        } else {
                            result.insert(key, number);
                        }
                    }
                } else {
                    // 日期沿用模板约定保留原始文本，必填检查交给核心上下文构建器。
                    if (field.inferred && text.trimmed().isEmpty()) {
                        result.remove(key);
                    } else {
                        result.insert(key, text);
                    }
                }
            } else if (const auto* checkbox = qobject_cast<QCheckBox*>(field.input)) {
                if (checkbox->checkState() == Qt::PartiallyChecked) {
                    result.remove(key);
                } else {
                    result.insert(key, checkbox->isChecked());
                }
            } else if (const auto* editor = qobject_cast<QPlainTextEdit*>(field.input)) {
                const auto text = editor->toPlainText();
                if (text.trimmed().isEmpty()) {
                    result.remove(key);
                } else {
                    QJsonDocument parsed;
                    const bool array = field.definition.type == sleekpr::core::FieldValueType::List;
                    if (parseJson(text, array, &parsed, &error)) {
                        result.insert(key, array ? QJsonValue(parsed.array()) : QJsonValue(parsed.object()));
                    }
                }
            }
        }
        if (!error.isEmpty()) {
            error = QString::fromUtf8("%1：%2").arg(field.definition.displayName, error);
            if (errorMessage->isEmpty()) {
                *errorMessage = error;
                m_invalidFieldKey = key;
            }
        }
        field.errorLabel->setText(error);
        field.errorLabel->setVisible(!error.isEmpty());
    }
    return result;
}

void PrintFieldEditorPanel::changeMode()
{
    if (m_loading || m_modeTabs->currentWidget() == m_activePage) {
        return;
    }
    bool ok = false;
    const auto current = values(&ok);
    if (!ok) {
        // 非法原文必须留在原编辑器，拒绝切换，不能回退到上次合法数据。
        const QSignalBlocker blocker(m_modeTabs);
        m_modeTabs->setCurrentWidget(m_activePage);
        return;
    }
    const QScopedValueRollback<bool> loading(m_loading, true);
    m_baseValues = current;
    if (m_modeTabs->currentWidget() == m_jsonPage) {
        const QSignalBlocker blocker(m_jsonEdit);
        m_jsonEdit->setPlainText(objectText(m_baseValues));
    } else {
        populateForm(true);
    }
    m_activePage = m_modeTabs->currentWidget();
    showError({});
}

void PrintFieldEditorPanel::editorChanged(int fieldIndex)
{
    if (m_loading) {
        return;
    }
    if (fieldIndex >= 0 && fieldIndex < m_fields.size()) {
        m_fields[fieldIndex].edited = true;
    }
    bool ok = false;
    const auto current = values(&ok);
    // 排版空白与模式切换不是字段变化；非法输入也要通知上层废弃旧预览。
    const bool changed = !ok || !m_lastEditValid || current != m_lastEditedValues;
    m_lastEditValid = ok;
    m_lastEditedValues = current;
    if (changed) {
        emit valuesEdited();
    }
}

void PrintFieldEditorPanel::showError(const QString& message) const
{
    m_errorLabel->setText(message);
    m_errorLabel->setVisible(!message.isEmpty());
}

} // 命名空间 sleekpr::app

#pragma once

#include "sleekpr/core/templates/TemplateDocument.h"

#include <QJsonObject>
#include <QList>
#include <QWidget>

class QCheckBox;
class QFormLayout;
class QLabel;
class QPlainTextEdit;
class QScrollArea;
class QTabWidget;

namespace sleekpr::app {

class PrintFieldEditorPanel final : public QWidget
{
    Q_OBJECT

public:
    explicit PrintFieldEditorPanel(QWidget* parent = nullptr);
    void setDocument(const sleekpr::core::TemplateDocument& document);
    void refreshDisplayNames(const sleekpr::core::TemplateDocument& document);
    void setValues(const QJsonObject& values);
    QJsonObject values(bool* ok = nullptr, QString* errorMessage = nullptr) const;
    // 定位只改变视图、滚动和焦点，不替换输入，也不触发字段编辑信号。
    bool focusField(const QString& fieldKey);
    void focusJsonError(int line = 0, int column = 0);
    // 返回最近一次校验失败的首字段；语法、体积等无法可靠归属字段的错误返回空。
    QString invalidFieldKey() const;

signals:
    void valuesEdited();

private:
    struct FieldEditor
    {
        sleekpr::core::FieldDefinition definition;
        bool inferred = false;
        bool arrayPath = false;
        QWidget* input = nullptr;
        QCheckBox* allowEmpty = nullptr;
        QLabel* errorLabel = nullptr;
        QLabel* titleLabel = nullptr;
        bool edited = false;
    };

    void rebuildForm(const sleekpr::core::TemplateDocument& document);
    void populateForm(bool preserveEmptyInputs = false);
    void changeMode();
    void editorChanged(int fieldIndex = -1);
    QJsonObject formValues(QString* errorMessage) const;
    void showError(const QString& message) const;

    QTabWidget* m_modeTabs = nullptr;
    QScrollArea* m_formScroll = nullptr;
    QWidget* m_formContent = nullptr;
    QFormLayout* m_formLayout = nullptr;
    QWidget* m_jsonPage = nullptr;
    QWidget* m_activePage = nullptr;
    QPlainTextEdit* m_jsonEdit = nullptr;
    QLabel* m_errorLabel = nullptr;
    QList<FieldEditor> m_fields;
    // 完整对象始终保留，表单只覆盖实际编辑的字段，不能丢弃数组和额外占位符。
    QJsonObject m_baseValues;
    QJsonObject m_lastEditedValues;
    // 校验结果属于界面诊断状态；const 收集接口也必须更新，不能沿用上一次失败字段。
    mutable QString m_invalidFieldKey;
    bool m_lastEditValid = true;
    bool m_loading = false;
};

} // 命名空间 sleekpr::app

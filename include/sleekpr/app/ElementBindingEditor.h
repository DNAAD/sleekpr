#pragma once

#include "sleekpr/app/TemplateDesignerPropertyModel.h"
#include <QWidget>

class QTableWidget;

namespace sleekpr::app {

class ElementBindingEditor final : public QWidget
{
    Q_OBJECT
public:
    explicit ElementBindingEditor(QWidget* parent = nullptr);
    ~ElementBindingEditor() override;
    void setBindings(const QList<DesignerFieldBinding>& bindings, const QList<DesignerFieldOption>& options);
    QList<DesignerFieldBinding> bindings() const;
    bool hasActiveKeyEditor() const;

signals:
    void bindingsEdited();
    void editingFinished();

private:
    QTableWidget* m_table;
    QList<DesignerFieldBinding> m_bindings;
    QList<DesignerFieldOption> m_options;
    QStringList m_baseNames;
    bool m_loading = false;
    bool m_finishing = false;
};

} // 命名空间 sleekpr::app

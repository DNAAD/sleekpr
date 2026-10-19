#pragma once

#include "sleekpr/app/TemplateDesignerPropertyModel.h"

#include <QWidget>

class QComboBox;
class QLineEdit;

namespace sleekpr::app {

class FieldBindingSelector final : public QWidget
{
    Q_OBJECT

public:
    explicit FieldBindingSelector(QWidget* parent = nullptr);
    void setOptionsAndValue(const QList<DesignerFieldOption>& options, const QString& key);
    QString fieldKey() const;
    QString displayName() const;
    bool isDisplayNameEdited() const;

signals:
    void fieldKeyEdited();
    void fieldKeyTextEdited();
    void displayNameEdited();
    void displayNameEditingFinished();

private:
    QComboBox* m_combo;
    QLineEdit* m_displayNameEdit;
    void resetDisplayName();
    QString m_baseDisplayName;
    QList<DesignerFieldOption> m_options;
    QString m_key;
};

} // 命名空间 sleekpr::app

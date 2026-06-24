#pragma once

#include <QWidget>

class QLineEdit;
class QListWidget;
class QPushButton;

namespace sleekpr::app {

class TemplateLibraryPanel final : public QWidget
{
public:
    explicit TemplateLibraryPanel(QWidget* parent = nullptr);

    QLineEdit* searchEdit() const;
    QLineEdit* nameEdit() const;
    QListWidget* templateList() const;
    QPushButton* refreshButton() const;
    QPushButton* loadSelectedButton() const;
    QPushButton* createButton() const;
    QPushButton* deleteButton() const;
    QPushButton* duplicateButton() const;
    QPushButton* renameButton() const;
    QPushButton* loadCurrentButton() const;

private:
    QLineEdit* m_searchEdit = nullptr;
    QLineEdit* m_nameEdit = nullptr;
    QListWidget* m_templateList = nullptr;
    QPushButton* m_refreshButton = nullptr;
    QPushButton* m_loadSelectedButton = nullptr;
    QPushButton* m_createButton = nullptr;
    QPushButton* m_deleteButton = nullptr;
    QPushButton* m_duplicateButton = nullptr;
    QPushButton* m_renameButton = nullptr;
    QPushButton* m_loadCurrentButton = nullptr;
};

} // 命名空间 sleekpr::app

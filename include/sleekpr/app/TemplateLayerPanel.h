#pragma once

#include <QWidget>

class QLineEdit;
class QListWidget;
class QPushButton;

namespace sleekpr::app {

class TemplateLayerPanel final : public QWidget
{
public:
    explicit TemplateLayerPanel(QWidget* parent = nullptr);

    QLineEdit* templateLibrarySearchEdit() const;
    QLineEdit* templateLibraryNameEdit() const;
    QListWidget* templateLibraryList() const;
    QPushButton* refreshLibraryButton() const;
    QPushButton* loadSelectedLibraryButton() const;
    QPushButton* createLibraryButton() const;
    QPushButton* deleteLibraryButton() const;
    QPushButton* duplicateLibraryButton() const;
    QPushButton* renameLibraryButton() const;
    QPushButton* loadCurrentLibraryButton() const;

    QListWidget* layerList() const;
    QPushButton* addLayerButton() const;
    QPushButton* deleteLayerButton() const;
    QPushButton* lockLayerButton() const;
    QPushButton* hideLayerButton() const;
    QPushButton* moveLayerUpButton() const;
    QPushButton* moveLayerDownButton() const;
    QPushButton* saveVersionButton() const;
    QPushButton* restoreVersionButton() const;

private:
    QLineEdit* m_templateLibrarySearchEdit = nullptr;
    QLineEdit* m_templateLibraryNameEdit = nullptr;
    QListWidget* m_templateLibraryList = nullptr;
    QPushButton* m_refreshLibraryButton = nullptr;
    QPushButton* m_loadSelectedLibraryButton = nullptr;
    QPushButton* m_createLibraryButton = nullptr;
    QPushButton* m_deleteLibraryButton = nullptr;
    QPushButton* m_duplicateLibraryButton = nullptr;
    QPushButton* m_renameLibraryButton = nullptr;
    QPushButton* m_loadCurrentLibraryButton = nullptr;

    QListWidget* m_layerList = nullptr;
    QPushButton* m_addLayerButton = nullptr;
    QPushButton* m_deleteLayerButton = nullptr;
    QPushButton* m_lockLayerButton = nullptr;
    QPushButton* m_hideLayerButton = nullptr;
    QPushButton* m_moveLayerUpButton = nullptr;
    QPushButton* m_moveLayerDownButton = nullptr;
    QPushButton* m_saveVersionButton = nullptr;
    QPushButton* m_restoreVersionButton = nullptr;
};

} // 命名空间 sleekpr::app

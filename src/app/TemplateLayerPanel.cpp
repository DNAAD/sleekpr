#include "sleekpr/app/TemplateLayerPanel.h"

#include "sleekpr/app/TemplateLibraryPanel.h"

#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QSplitter>
#include <QVBoxLayout>

namespace sleekpr::app {

namespace {

QPushButton* createLayerButton(const QString& text, const QString& objectName, QWidget* parent)
{
    auto* button = new QPushButton(text, parent);
    button->setObjectName(objectName);
    return button;
}

} // 命名空间

TemplateLayerPanel::TemplateLayerPanel(QWidget* parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("designerLayerPanel"));
    setMinimumWidth(250);
    auto* layerLayout = new QVBoxLayout(this);
    layerLayout->setContentsMargins(10, 10, 10, 10);
    layerLayout->setSpacing(8);

    auto* titleLabel = new QLabel(QString::fromUtf8("图层"), this);
    titleLabel->setObjectName(QStringLiteral("designerPanelTitle"));
    m_addLayerButton = createLayerButton(QString::fromUtf8("新增图层"), QStringLiteral("addLayerButton"), this);
    m_deleteLayerButton = createLayerButton(QString::fromUtf8("删除图层"), QStringLiteral("deleteLayerButton"), this);
    m_deleteLayerButton->setProperty("buttonRole", QStringLiteral("danger"));
    m_lockLayerButton = createLayerButton(QString::fromUtf8("锁定图层"), QStringLiteral("lockLayerButton"), this);
    m_hideLayerButton = createLayerButton(QString::fromUtf8("隐藏图层"), QStringLiteral("hideLayerButton"), this);
    m_moveLayerUpButton = createLayerButton(QString::fromUtf8("上移图层"), QStringLiteral("moveLayerUpButton"), this);
    m_moveLayerDownButton = createLayerButton(QString::fromUtf8("下移图层"), QStringLiteral("moveLayerDownButton"), this);
    m_saveVersionButton = createLayerButton(QString::fromUtf8("保存版本"), QStringLiteral("saveTemplateVersionButton"), this);
    m_restoreVersionButton = createLayerButton(QString::fromUtf8("恢复版本"), QStringLiteral("restoreTemplateVersionButton"), this);

    m_layerList = new QListWidget(this);
    m_layerList->setObjectName(QStringLiteral("templateLayerList"));

    auto* layerButtonGrid = new QGridLayout();
    layerButtonGrid->setContentsMargins(0, 0, 0, 0);
    layerButtonGrid->addWidget(m_addLayerButton, 0, 0);
    layerButtonGrid->addWidget(m_deleteLayerButton, 0, 1);
    layerButtonGrid->addWidget(m_lockLayerButton, 1, 0);
    layerButtonGrid->addWidget(m_hideLayerButton, 1, 1);
    layerButtonGrid->addWidget(m_moveLayerUpButton, 2, 0);
    layerButtonGrid->addWidget(m_moveLayerDownButton, 2, 1);

    auto* documentButtonGrid = new QGridLayout();
    documentButtonGrid->setContentsMargins(0, 0, 0, 0);
    documentButtonGrid->addWidget(m_saveVersionButton, 0, 0);
    documentButtonGrid->addWidget(m_restoreVersionButton, 0, 1);

    auto* leftResourceSplitter = new QSplitter(Qt::Vertical, this);
    leftResourceSplitter->setObjectName(QStringLiteral("designerLeftResourceSplitter"));
    leftResourceSplitter->setChildrenCollapsible(false);

    auto* librarySection = new TemplateLibraryPanel(leftResourceSplitter);
    m_templateLibrarySearchEdit = librarySection->searchEdit();
    m_templateLibraryNameEdit = librarySection->nameEdit();
    m_templateLibraryList = librarySection->templateList();
    m_refreshLibraryButton = librarySection->refreshButton();
    m_loadSelectedLibraryButton = librarySection->loadSelectedButton();
    m_createLibraryButton = librarySection->createButton();
    m_deleteLibraryButton = librarySection->deleteButton();
    m_duplicateLibraryButton = librarySection->duplicateButton();
    m_renameLibraryButton = librarySection->renameButton();
    m_loadCurrentLibraryButton = librarySection->loadCurrentButton();

    auto* layerSection = new QWidget(leftResourceSplitter);
    layerSection->setObjectName(QStringLiteral("designerLayerSection"));
    auto* layerSectionLayout = new QVBoxLayout(layerSection);
    layerSectionLayout->setContentsMargins(8, 8, 8, 8);
    layerSectionLayout->setSpacing(8);
    layerSectionLayout->addWidget(titleLabel);
    layerSectionLayout->addWidget(m_layerList, 1);
    layerSectionLayout->addLayout(layerButtonGrid);
    layerSectionLayout->addLayout(documentButtonGrid);

    leftResourceSplitter->addWidget(librarySection);
    leftResourceSplitter->addWidget(layerSection);
    leftResourceSplitter->setSizes({260, 360});
    layerLayout->addWidget(leftResourceSplitter, 1);
}

QLineEdit* TemplateLayerPanel::templateLibrarySearchEdit() const { return m_templateLibrarySearchEdit; }
QLineEdit* TemplateLayerPanel::templateLibraryNameEdit() const { return m_templateLibraryNameEdit; }
QListWidget* TemplateLayerPanel::templateLibraryList() const { return m_templateLibraryList; }
QPushButton* TemplateLayerPanel::refreshLibraryButton() const { return m_refreshLibraryButton; }
QPushButton* TemplateLayerPanel::loadSelectedLibraryButton() const { return m_loadSelectedLibraryButton; }
QPushButton* TemplateLayerPanel::createLibraryButton() const { return m_createLibraryButton; }
QPushButton* TemplateLayerPanel::deleteLibraryButton() const { return m_deleteLibraryButton; }
QPushButton* TemplateLayerPanel::duplicateLibraryButton() const { return m_duplicateLibraryButton; }
QPushButton* TemplateLayerPanel::renameLibraryButton() const { return m_renameLibraryButton; }
QPushButton* TemplateLayerPanel::loadCurrentLibraryButton() const { return m_loadCurrentLibraryButton; }

QListWidget* TemplateLayerPanel::layerList() const { return m_layerList; }
QPushButton* TemplateLayerPanel::addLayerButton() const { return m_addLayerButton; }
QPushButton* TemplateLayerPanel::deleteLayerButton() const { return m_deleteLayerButton; }
QPushButton* TemplateLayerPanel::lockLayerButton() const { return m_lockLayerButton; }
QPushButton* TemplateLayerPanel::hideLayerButton() const { return m_hideLayerButton; }
QPushButton* TemplateLayerPanel::moveLayerUpButton() const { return m_moveLayerUpButton; }
QPushButton* TemplateLayerPanel::moveLayerDownButton() const { return m_moveLayerDownButton; }
QPushButton* TemplateLayerPanel::saveVersionButton() const { return m_saveVersionButton; }
QPushButton* TemplateLayerPanel::restoreVersionButton() const { return m_restoreVersionButton; }

} // 命名空间 sleekpr::app

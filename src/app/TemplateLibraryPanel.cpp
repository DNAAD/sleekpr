#include "sleekpr/app/TemplateLibraryPanel.h"

#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>

namespace sleekpr::app {

namespace {

QPushButton* createPanelButton(const QString& text, const QString& objectName, QWidget* parent)
{
    auto* button = new QPushButton(text, parent);
    button->setObjectName(objectName);
    return button;
}

} // 匿名命名空间

TemplateLibraryPanel::TemplateLibraryPanel(QWidget* parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("designerLibrarySection"));

    auto* titleLabel = new QLabel(QString::fromUtf8("模板库"), this);
    titleLabel->setObjectName(QStringLiteral("designerPanelTitle"));

    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setObjectName(QStringLiteral("templateLibrarySearchEdit"));
    m_searchEdit->setPlaceholderText(QString::fromUtf8("搜索名称、分类或模板键"));

    m_nameEdit = new QLineEdit(this);
    m_nameEdit->setObjectName(QStringLiteral("templateLibraryNameEdit"));
    m_nameEdit->setPlaceholderText(QString::fromUtf8("新建或重命名模板名称"));

    m_templateList = new QListWidget(this);
    m_templateList->setObjectName(QStringLiteral("templateLibraryList"));
    m_templateList->setMinimumHeight(84);
    m_templateList->setMaximumHeight(140);

    m_refreshButton = createPanelButton(QString::fromUtf8("刷新模板库"), QStringLiteral("refreshTemplateLibraryButton"), this);
    m_loadSelectedButton = createPanelButton(
        QString::fromUtf8("加载选中模板"),
        QStringLiteral("loadSelectedTemplateFromLibraryButton"),
        this);
    m_createButton = createPanelButton(QString::fromUtf8("新建模板"), QStringLiteral("createTemplateInLibraryButton"), this);
    m_deleteButton = createPanelButton(
        QString::fromUtf8("删除选中模板"),
        QStringLiteral("deleteSelectedTemplateFromLibraryButton"),
        this);
    m_deleteButton->setProperty("buttonRole", QStringLiteral("danger"));
    m_duplicateButton = createPanelButton(
        QString::fromUtf8("复制选中模板"),
        QStringLiteral("duplicateSelectedTemplateFromLibraryButton"),
        this);
    m_renameButton = createPanelButton(
        QString::fromUtf8("重命名模板"),
        QStringLiteral("renameSelectedTemplateFromLibraryButton"),
        this);
    m_loadCurrentButton = createPanelButton(QString::fromUtf8("从模板库加载"), QStringLiteral("loadTemplateFromLibraryButton"), this);

    auto* buttonGrid = new QGridLayout();
    buttonGrid->setContentsMargins(0, 0, 0, 0);
    buttonGrid->addWidget(m_refreshButton, 0, 0);
    buttonGrid->addWidget(m_loadSelectedButton, 0, 1);
    buttonGrid->addWidget(m_createButton, 1, 0);
    buttonGrid->addWidget(m_deleteButton, 1, 1);
    buttonGrid->addWidget(m_duplicateButton, 2, 0);
    buttonGrid->addWidget(m_renameButton, 2, 1);
    buttonGrid->addWidget(m_loadCurrentButton, 3, 0, 1, 2);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(8);
    layout->addWidget(titleLabel);
    layout->addWidget(m_searchEdit);
    layout->addWidget(m_nameEdit);
    layout->addWidget(m_templateList, 1);
    layout->addLayout(buttonGrid);
}

QLineEdit* TemplateLibraryPanel::searchEdit() const
{
    return m_searchEdit;
}

QLineEdit* TemplateLibraryPanel::nameEdit() const
{
    return m_nameEdit;
}

QListWidget* TemplateLibraryPanel::templateList() const
{
    return m_templateList;
}

QPushButton* TemplateLibraryPanel::refreshButton() const
{
    return m_refreshButton;
}

QPushButton* TemplateLibraryPanel::loadSelectedButton() const
{
    return m_loadSelectedButton;
}

QPushButton* TemplateLibraryPanel::createButton() const
{
    return m_createButton;
}

QPushButton* TemplateLibraryPanel::deleteButton() const
{
    return m_deleteButton;
}

QPushButton* TemplateLibraryPanel::duplicateButton() const
{
    return m_duplicateButton;
}

QPushButton* TemplateLibraryPanel::renameButton() const
{
    return m_renameButton;
}

QPushButton* TemplateLibraryPanel::loadCurrentButton() const
{
    return m_loadCurrentButton;
}

} // 命名空间 sleekpr::app

#include "sleekpr/app/TemplateLayerPanel.h"

#include "sleekpr/app/AppTheme.h"
#include "sleekpr/app/TemplateLibraryPanel.h"

#include <QAction>
#include <QHBoxLayout>
#include <QLabel>
#include <QLayout>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QPushButton>
#include <QSizePolicy>
#include <QTabWidget>
#include <QVBoxLayout>

namespace sleekpr::app {

namespace {

QPushButton* createLayerButton(const QString& text, const QString& objectName, QWidget* parent)
{
    auto* button = new QPushButton(text, parent);
    button->setObjectName(objectName);
    return button;
}

} // 匿名命名空间

TemplateLayerPanel::TemplateLayerPanel(QWidget* parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("designerLayerPanel"));
    setMinimumWidth(210);
    auto* layerLayout = new QVBoxLayout(this);
    layerLayout->setContentsMargins(0, 0, 0, 0);
    layerLayout->setSpacing(0);

    auto* resourceTabs = new QTabWidget(this);
    resourceTabs->setObjectName(QStringLiteral("designerResourceTabs"));
    resourceTabs->setDocumentMode(true);
    resourceTabs->setUsesScrollButtons(true);

    auto* objectSection = new QWidget(resourceTabs);
    objectSection->setObjectName(QStringLiteral("designerObjectsSection"));
    m_objectLayout = new QVBoxLayout(objectSection);
    m_objectLayout->setContentsMargins(0, 0, 0, 0);
    m_objectLayout->setSpacing(8);

    auto* layerSection = new QWidget(objectSection);
    layerSection->setObjectName(QStringLiteral("designerLayerSection"));
    layerSection->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Maximum);
    auto* layerSectionLayout = new QVBoxLayout(layerSection);
    layerSectionLayout->setContentsMargins(8, 8, 8, 0);
    layerSectionLayout->setSpacing(6);
    auto* titleLabel = new QLabel(QString::fromUtf8("图层"), layerSection);
    titleLabel->setObjectName(QStringLiteral("designerPanelTitle"));

    m_addLayerButton = createLayerButton(QString::fromUtf8("新增图层"), QStringLiteral("addLayerButton"), layerSection);
    m_deleteLayerButton = createLayerButton(QString::fromUtf8("删除图层"), QStringLiteral("deleteLayerButton"), layerSection);
    configureIconButton(m_addLayerButton, QStyle::SP_FileDialogNewFolder, QString::fromUtf8("新增图层"));
    configureIconButton(m_deleteLayerButton, QStyle::SP_TrashIcon, QString::fromUtf8("删除当前图层"));
    m_deleteLayerButton->setProperty("destructive", true);
    m_lockLayerButton = createLayerButton(QString::fromUtf8("锁定或解锁图层"), QStringLiteral("lockLayerButton"), layerSection);
    m_hideLayerButton = createLayerButton(QString::fromUtf8("隐藏或显示图层"), QStringLiteral("hideLayerButton"), layerSection);
    m_moveLayerUpButton = createLayerButton(QString::fromUtf8("上移图层"), QStringLiteral("moveLayerUpButton"), layerSection);
    m_moveLayerDownButton = createLayerButton(QString::fromUtf8("下移图层"), QStringLiteral("moveLayerDownButton"), layerSection);
    m_saveVersionButton = createLayerButton(QString::fromUtf8("保存版本"), QStringLiteral("saveTemplateVersionButton"), layerSection);
    m_restoreVersionButton = createLayerButton(QString::fromUtf8("恢复版本"), QStringLiteral("restoreTemplateVersionButton"), layerSection);

    auto* moreButton = new QPushButton(layerSection);
    moreButton->setObjectName(QStringLiteral("designerLayerMoreButton"));
    configureIconButton(moreButton, QStyle::SP_TitleBarMenuButton, QString::fromUtf8("更多图层和版本操作"));
    auto* menu = new QMenu(moreButton);
    menu->setObjectName(QStringLiteral("designerLayerMenu"));
    // 隐藏按钮保留既有对象与信号，菜单只转发点击，不复制任何业务逻辑。
    const auto addCommand = [menu](QPushButton* button, QStyle::StandardPixmap icon) {
        auto* action = menu->addAction(button->style()->standardIcon(icon), button->text());
        action->setObjectName(button->objectName() + QStringLiteral("Action"));
        action->setToolTip(button->text());
        QObject::connect(action, &QAction::triggered, button, [button] { button->click(); });
        QObject::connect(menu, &QMenu::aboutToShow, action, [action, button] {
            action->setText(button->text());
            action->setEnabled(button->isEnabled());
        });
        button->setToolTip(button->text());
        button->setAccessibleName(button->text());
        button->hide();
    };
    addCommand(m_lockLayerButton, QStyle::SP_MessageBoxWarning);
    addCommand(m_hideLayerButton, QStyle::SP_DialogNoButton);
    menu->addSeparator();
    addCommand(m_moveLayerUpButton, QStyle::SP_ArrowUp);
    addCommand(m_moveLayerDownButton, QStyle::SP_ArrowDown);
    menu->addSeparator();
    addCommand(m_saveVersionButton, QStyle::SP_DialogSaveButton);
    addCommand(m_restoreVersionButton, QStyle::SP_BrowserReload);
    moreButton->setMenu(menu);

    auto* headerLayout = new QHBoxLayout();
    headerLayout->setContentsMargins(0, 0, 0, 0);
    headerLayout->setSpacing(4);
    headerLayout->addWidget(titleLabel, 1);
    headerLayout->addWidget(m_addLayerButton);
    headerLayout->addWidget(m_deleteLayerButton);
    headerLayout->addWidget(moreButton);
    m_layerList = new QListWidget(layerSection);
    m_layerList->setObjectName(QStringLiteral("templateLayerList"));
    m_layerList->setFixedHeight(82);
    m_layerList->setAccessibleName(QString::fromUtf8("模板图层列表"));
    layerSectionLayout->addLayout(headerLayout);
    layerSectionLayout->addWidget(m_layerList);
    m_objectLayout->addWidget(layerSection);

    auto* librarySection = new TemplateLibraryPanel(resourceTabs);
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

    resourceTabs->addTab(objectSection, QString::fromUtf8("对象"));
    resourceTabs->addTab(librarySection, QString::fromUtf8("模板库"));
    resourceTabs->setCurrentIndex(0);
    layerLayout->addWidget(resourceTabs, 1);
}

void TemplateLayerPanel::setElementSection(QWidget* section)
{
    if (section == nullptr || section == m_elementSection) {
        return;
    }
    // 先解除检查器旧布局的管理，再迁移对象区域，避免布局跨父控件移动警告。
    if (auto* oldParent = section->parentWidget(); oldParent != nullptr && oldParent->layout() != nullptr) {
        oldParent->layout()->removeWidget(section);
    }
    if (m_elementSection != nullptr) {
        m_objectLayout->removeWidget(m_elementSection);
        m_elementSection->hide();
    }
    m_elementSection = section;
    m_objectLayout->addWidget(section, 1);
    section->show();
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

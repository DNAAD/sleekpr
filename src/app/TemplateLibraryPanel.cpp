#include "sleekpr/app/TemplateLibraryPanel.h"
#include "sleekpr/app/AppTheme.h"

#include <QAction>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QPushButton>
#include <QSizePolicy>
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
    m_searchEdit->setClearButtonEnabled(true);
    m_searchEdit->setAccessibleName(QString::fromUtf8("搜索模板库"));

    m_nameEdit = new QLineEdit(this);
    m_nameEdit->setObjectName(QStringLiteral("templateLibraryNameEdit"));
    m_nameEdit->setPlaceholderText(QString::fromUtf8("新建或重命名模板名称"));
    m_nameEdit->setAccessibleName(QString::fromUtf8("模板名称，用于新建或重命名"));
    m_nameEdit->setToolTip(QString::fromUtf8("新建或重命名模板时使用此名称"));

    m_templateList = new QListWidget(this);
    m_templateList->setObjectName(QStringLiteral("templateLibraryList"));
    m_templateList->setMinimumHeight(180);
    m_templateList->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_templateList->setAccessibleName(QString::fromUtf8("模板库列表"));
    m_templateList->setContextMenuPolicy(Qt::CustomContextMenu);

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

    configureIconButton(m_createButton, QStyle::SP_FileIcon, QString::fromUtf8("使用模板名称新建模板"));
    configureIconButton(m_refreshButton, QStyle::SP_BrowserReload, QString::fromUtf8("刷新模板库"));
    auto* moreButton = new QPushButton(this);
    moreButton->setObjectName(QStringLiteral("templateLibraryMoreButton"));
    configureIconButton(moreButton, QStyle::SP_TitleBarMenuButton, QString::fromUtf8("更多模板操作"));
    auto* menu = new QMenu(moreButton);
    menu->setObjectName(QStringLiteral("templateLibraryMenu"));
    // 菜单复用原按钮的点击入口，保留主窗口接线与自动化访问契约。
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
    addCommand(m_loadSelectedButton, QStyle::SP_DialogOpenButton);
    addCommand(m_duplicateButton, QStyle::SP_FileDialogNewFolder);
    addCommand(m_renameButton, QStyle::SP_FileDialogDetailedView);
    menu->addSeparator();
    addCommand(m_deleteButton, QStyle::SP_TrashIcon);
    menu->addSeparator();
    addCommand(m_loadCurrentButton, QStyle::SP_DialogOpenButton);
    moreButton->setMenu(menu);
    connect(m_templateList, &QListWidget::customContextMenuRequested, this, [this, menu](const QPoint& position) {
        if (auto* item = m_templateList->itemAt(position)) {
            m_templateList->setCurrentItem(item);
        }
        menu->exec(m_templateList->viewport()->mapToGlobal(position));
    });

    auto* headerLayout = new QHBoxLayout();
    headerLayout->setContentsMargins(0, 0, 0, 0);
    headerLayout->setSpacing(4);
    headerLayout->addWidget(titleLabel, 1);
    headerLayout->addWidget(m_createButton);
    headerLayout->addWidget(m_refreshButton);
    headerLayout->addWidget(moreButton);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(8);
    layout->addLayout(headerLayout);
    layout->addWidget(m_searchEdit);
    layout->addWidget(m_nameEdit);
    layout->addWidget(m_templateList, 1);
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

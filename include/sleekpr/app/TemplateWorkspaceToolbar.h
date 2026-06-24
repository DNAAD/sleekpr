#pragma once

#include <QWidget>

class QCheckBox;
class QComboBox;
class QPushButton;

namespace sleekpr::app {

class TemplateWorkspaceToolbar final : public QWidget
{
public:
    explicit TemplateWorkspaceToolbar(QWidget* parent = nullptr);

    QPushButton* saveToLibraryButton() const;
    QPushButton* prePrintTemplateButton() const;
    QPushButton* importTemplateButton() const;
    QPushButton* exportTemplateButton() const;
    QPushButton* undoButton() const;
    QPushButton* redoButton() const;
    QComboBox* zoomCombo() const;
    QComboBox* rulerPrecisionCombo() const;
    QCheckBox* designAidCheck() const;
    QComboBox* paperSpecCombo() const;
    QPushButton* openPaperSpecManagerButton() const;
    QPushButton* openFieldPresetManagerButton() const;

private:
    QPushButton* m_saveToLibraryButton = nullptr;
    QPushButton* m_prePrintTemplateButton = nullptr;
    QPushButton* m_importTemplateButton = nullptr;
    QPushButton* m_exportTemplateButton = nullptr;
    QPushButton* m_undoButton = nullptr;
    QPushButton* m_redoButton = nullptr;
    QComboBox* m_zoomCombo = nullptr;
    QComboBox* m_rulerPrecisionCombo = nullptr;
    QCheckBox* m_designAidCheck = nullptr;
    QComboBox* m_paperSpecCombo = nullptr;
    QPushButton* m_openPaperSpecManagerButton = nullptr;
    QPushButton* m_openFieldPresetManagerButton = nullptr;
};

} // 命名空间 sleekpr::app

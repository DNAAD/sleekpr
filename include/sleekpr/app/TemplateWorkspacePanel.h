#pragma once

#include <QWidget>

class QCheckBox;
class QComboBox;
class QLabel;
class QPlainTextEdit;
class QPushButton;
class QString;

namespace sleekpr::app {

class TemplatePreviewLabel;
class TemplateWorkspaceToolbar;

class TemplateWorkspacePanel final : public QWidget
{
public:
    explicit TemplateWorkspacePanel(const QString& initialSampleDataJson, QWidget* parent = nullptr);

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
    TemplatePreviewLabel* previewLabel() const;
    QPushButton* saveSampleDataButton() const;
    QPlainTextEdit* sampleDataEdit() const;
    QLabel* sampleDataErrorLabel() const;
    QLabel* statusLabel() const;

private:
    TemplateWorkspaceToolbar* m_toolbar = nullptr;
    TemplatePreviewLabel* m_previewLabel = nullptr;
    QPushButton* m_saveSampleDataButton = nullptr;
    QPlainTextEdit* m_sampleDataEdit = nullptr;
    QLabel* m_sampleDataErrorLabel = nullptr;
    QLabel* m_statusLabel = nullptr;
};

} // 命名空间 sleekpr::app

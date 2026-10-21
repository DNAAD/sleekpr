#pragma once
#include "sleekpr/app/ApplicationExitCoordinator.h"
#include <QDialog>
#include <QPointer>
class QLabel;
class QPlainTextEdit;
class QPushButton;
class QMessageBox;
namespace sleekpr::app {
class ApplicationExitDialog final : public QDialog
{
public:
    explicit ApplicationExitDialog(ApplicationExitCoordinator&, QWidget* parent = nullptr);
protected:
    void reject() override;
private:
    void refresh();
    ApplicationExitCoordinator& m_coordinator;
    QLabel* m_title;
    QPlainTextEdit* m_details;
    QPushButton* m_back;
    QPushButton* m_save;
    QPushButton* m_skip;
    QPushButton* m_retry;
    QPushButton* m_discard;
    QPointer<QMessageBox> m_confirmation;
};
} // 命名空间 sleekpr::app

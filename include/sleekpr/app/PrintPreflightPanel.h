#pragma once

#include "sleekpr/core/printing/PrintPreflightReport.h"

#include <QWidget>

class QListWidget;
class QListWidgetItem;
class QToolButton;

namespace sleekpr::app {

class PrintPreflightPanel final : public QWidget
{
    Q_OBJECT

public:
    explicit PrintPreflightPanel(QWidget* parent = nullptr);
    void setReport(const core::PrintPreflightReport& report);
    void setPending();

signals:
    void issueActivated(const core::PrintPreflightIssue& issue);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void updateExpansion();
    void activateIssue(QListWidgetItem* item);

    QToolButton* m_summary = nullptr;
    QListWidget* m_issues = nullptr;
    QList<core::PrintPreflightIssue> m_displayIssues;
    bool m_pending = true;
};

} // 命名空间 sleekpr::app

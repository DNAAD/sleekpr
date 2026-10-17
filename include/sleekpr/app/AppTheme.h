#pragma once

#include <QString>
#include <QStyle>

class QPushButton;
class QApplication;

namespace sleekpr::app {

QString sleekprAppStyleSheet();
void applySleekprTheme(QApplication& application);
void configureIconButton(QPushButton* button, QStyle::StandardPixmap icon, const QString& tooltip);

} // 命名空间 sleekpr::app

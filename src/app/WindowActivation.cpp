#include "sleekpr/app/WindowActivation.h"

namespace sleekpr::app {

void showAndActivateWindow(QWidget& window)
{
    // 桌面外壳入口统一使用 showNormal，避免上一次最小化或隐藏状态导致用户误以为程序没有界面。
    window.showNormal();
    // raise 和 activateWindow 负责把托盘入口、启动入口唤起的窗口带到用户当前桌面前景。
    window.raise();
    window.activateWindow();
}

} // 命名空间 sleekpr::app

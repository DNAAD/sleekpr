#pragma once

#ifndef SLEEKPR_ENABLE_TABLE_DESIGNER
#define SLEEKPR_ENABLE_TABLE_DESIGNER 0
#endif

namespace sleekpr::app {

// 当前发布版本专注标签打印，表格设计器仅在显式启用的构建中恢复。
inline constexpr bool kTableDesignerEnabled = SLEEKPR_ENABLE_TABLE_DESIGNER != 0;

} // 命名空间 sleekpr::app

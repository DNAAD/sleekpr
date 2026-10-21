# Windows 构建与启动

## 先区分正在运行的版本

- `quick-start.cmd` 默认是 Release，运行 `dist/sleekpr-release/sleekpr.exe`；Debug 运行 `build/sleekpr.exe`。Debug 构建不会更新正式包。
- 启动器发现目标程序已经存在时不会自动重编译。使用 `quick-start.cmd -Mode Debug` 前，应已构建最新 Debug；需要重建和部署但不启动时使用 `quick-start.cmd -Mode Debug -Rebuild -NoLaunch`。
- 调查“代码改了但界面没变”时，先核对进程路径、模式和产物时间，不先推断设置丢失。

## 工具环境

构建需要 MSVC x64 开发者环境、与当前构建匹配的 Qt `msvc2022_64`、CMake/CTest 和 Node.js。优先参考 [quick-start.ps1](../../quick-start.ps1) 的 `Find-VsDevCmd`、`Find-QtRoot`，通过 `vswhere`、明确指定的 Qt 根目录、`QTDIR` 或 Qt 配置发现环境，不把操作者的绝对路径复制进源码或规则。

`Qt6_DIR` 指向 Qt 的 CMake 配置目录，不等同于 Qt 根目录。普通 PowerShell 通过子进程调用 `VsDevCmd.bat` 后，环境不会回传给父 PowerShell；构建及依赖其环境的命令需在同一开发者会话或同一次 `cmd /c` 中执行。单独运行 Qt 测试程序还需要 Qt `bin` 在 `PATH` 中。

中文 PowerShell 脚本还需兼容 Windows PowerShell 5.1 的 UTF-8 BOM 读取行为；不能把脚本解析失败当成业务回归失败。

模块核验需要获准加载当前仓库脚本的会话。执行策略阻止加载时先报告原因，不修改机器/用户的永久策略；经授权可像打包入口一样仅对子进程传 `-ExecutionPolicy Bypass`。组织策略仍阻止时停止，不换解释器或编码方式规避，也不把策略拒绝算成测试失败。

## 版本信息的来源

- [BuildInfo](../../src/app/BuildInfo.cpp) 是应用、诊断和包清单共用的结构；版本来自 CMake 项目版本，revision 是完整 Git 提交或 `unknown`，configuration 是实际构建配置。
- [生成器](../../cmake/GenerateBuildInfo.cmake) 在构建前探测 Git 和有序源码文件摘要，只在内容变化时更新生成头文件，不能只在首次配置时抓一次提交号。
- `sourceDigest` 覆盖源码、资源、CMake、测试、打包内容及根构建脚本，不是 EXE 的 SHA-256，也不保证跨工具链得到相同二进制。普通文档和 Harness 影响 `workspaceDirty`，不进入当前源码指纹范围。
- `sourceDirty` 与 `workspaceDirty` 是不同维度；没有 Git 或状态不可判断时使用 `unknown`/可空状态，不能把未知状态记成 clean。标识是构建时快照，不是实时 Git 状态。
- 同一 revision 下可以有不同的未提交源码；不要只比较版本号、提交号、文件时间或是否 dirty。对比模式、源码指纹和实际路径，二进制摘要则取自包清单。

## 安全诊断与正式包

`--build-info-json` 在读取设置、启动 HTTP/托盘之前输出标识并退出。旧 EXE 可能忽略该参数并正常启动，所以启动器和验证流程先通过 [ExecutableBuildInfo](../../packaging/ExecutableBuildInfo.psm1) 检查 PE 能力标记；无标记时仅报告未知，不盲启旧程序。新增标记依赖 Windows SDK 的 RC 工具。

正式打包通过 [package-portable.ps1](../../package-portable.ps1) 的独立 `build-release` 完成测试、暂存部署和隔离冒烟；公共 `package_sleekpr_release` 目标委托同一脚本，旧 `*_release_current` 内部目标已停用。隔离探测使用包内 `qt.conf` 和本地运行库，清理开发 Qt 插件/QML 路径及 PATH 注入，不能靠开发机 Qt 安装“补齐”缺失 DLL。

`manifest.json` 记录构建身份、验证状态和文件摘要；不将清单自身纳入自身摘要。ZIP 的整体摘要记录在包外 `.zip.verification.json`，不能将源码指纹当作这两个摘要。未提交源码可如实标 dirty，不能为制作包擅自提交。

正式验收、失败回滚与已发布旧包回退的边界见 [Release 交付技能](../skills/native-release-delivery/SKILL.md)，命令见 [验证技能](../skills/verification/SKILL.md)。本机隔离通过不等于无 Qt 新机器或真实出纸通过。

## 增量构建陷阱

- MSVC `/showIncludes` 前缀受语言包影响；`VSLANG=1033` 不保证机器一定输出英文。探测结果必须按 UTF-8 读取，并供 Ninja 正确识别头文件依赖。
- 头文件依赖丢失可能混用不同类布局的旧对象，导致打开设计器时堆越界。独立编译的单元测试通过，不代表真实客户端对象也安全。
- 保留 [CMake 依赖探测](../../CMakeLists.txt) 和 [依赖回归检查](../../tests/check_msvc_header_dependencies.cmake)。反复删对象或全量清理不能替代修复依赖跟踪。
- CTest 的 `sleekpr_designer_launch_test` 使用真实客户端对象，以临时配置运行 `--smoke-test-designer`；不监听 HTTP、不显示托盘、不发送打印。`sleekpr_msvc_header_dependencies` 只在当前配置满足 MSVC/Ninja 条件时注册。

## 程序占用与验证

`LNK1168` 无法写入 `sleekpr.exe` 时先确认是否仍有客户端进程。请操作员保存模板并从托盘退出；没有明确授权不能强杀进程。

完整命令、Qt 日志与截图要求见 [验证技能](../skills/verification/SKILL.md)。测试数量以 `ctest --test-dir build -N` 的当前输出为准。

sleekpr 绿色包使用说明

1. 将整个 sleekpr-release 文件夹解压到本机固定目录，例如 D:\Apps\sleekpr。
2. 双击 sleekpr.exe 启动客户端。程序会常驻托盘，并启动本地打印服务：http://127.0.0.1:37122。
3. 不要只拷贝 sleekpr.exe，必须保留同目录下的 Qt6*.dll、platforms、imageformats、tls 等运行时文件夹。
4. 首次启动后，配置、模板、纸张规格、字段预设会保存在：
   %APPDATA%\zytxt\sleekpr
5. 迁移或备份生产机器时，请同时备份上面的配置目录。
6. 正式包包含本地 MSVC CRT；如果机器仍提示缺少运行时，请保留整个目录并核对 manifest.json，必要时运行随包的 vc_redist.x64.exe。
7. 工作台顶部显示版本及 Debug/Release。设置页和工作台的信息按钮可查看程序路径、Git 提交、源码指纹，并复制不含设置正文的诊断。
8. manifest.json 记录构建标识、逐文件 SHA-256 和本机自动验收状态；zip 旁的 verification.json 记录归档摘要。不要把清单自身纳入自身摘要。
9. 双击前也可运行 sleekpr.exe --build-info-json 读取无副作用的 JSON 标识；该命令不读取配置、不启动 HTTP 和托盘。
10. 源码未提交会明确标为 dirty，不能只看版本号判断内容相同。启动器默认不会自动重建已存在的 exe。
11. -NoTests 只生成标为 unverified 的候选包，不覆盖 sleekpr-release，不表示正式验收完成。
12. 本机自动验收不等于已完成真实打印设备出纸或无 Qt 新机器验收；首次部署请在操作者确认后再做设备验证。

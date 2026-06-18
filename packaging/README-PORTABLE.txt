sleekpr 绿色包使用说明

1. 将整个 sleekpr-release 文件夹解压到本机固定目录，例如 D:\Apps\sleekpr。
2. 双击 sleekpr.exe 启动客户端。程序会常驻托盘，并启动本地打印服务：http://127.0.0.1:37122。
3. 不要只拷贝 sleekpr.exe，必须保留同目录下的 Qt6*.dll、platforms、imageformats、tls 等运行时文件夹。
4. 首次启动后，配置、模板、纸张规格、字段预设会保存在：
   %APPDATA%\zytxt\sleekpr
5. 迁移或备份生产机器时，请同时备份上面的配置目录。
6. 如果缺少 VC++ 运行时，请运行同目录下的 vc_redist.x64.exe 后再启动。

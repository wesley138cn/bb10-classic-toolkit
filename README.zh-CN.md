# BB10 Classic Toolkit

[English](README.md)

为运行 BerryCore 的 BlackBerry Classic 补充中文终端、对话回看和 AI 工具。

这是 **BerryCore 生态的独立社区扩展项目**。终端部分基于 Term49 修改，编程助手部分是 Pi 的 QNX/ARM 适配，BerryCloud 是新增的终端聊天客户端。本仓库不代表 BerryCore 官方版本，也不表示这些修改已被上游合并。

## 已有成果

| 模块 | 我们补充的内容 | 已确认范围 |
| --- | --- | --- |
| [Term49 Classic](term49/README.md) | 中英文双字体、中文双格宽度、离线拼音、4,096 行回看缓存、上翻/下翻/返回输入按钮 | 真实手机选出中文并收到 Pi 回复；按钮翻页并返回输入 |
| [Pi Classic](pi-classic/README.md) | Pi 0.74.2 在 QNX Node 上的字符分段、正则、HTTPS 传输和启动适配 | 文件读写修改、执行命令、恢复会话、交互界面 |
| [BerryCloud](berrycloud/README.md) | 终端连续聊天、流式输出、保存恢复、导出、多行输入 | 手机直连 HTTPS、上下文追问、恢复会话 |
| [本地 Qwen](qwen-local/README.md) | 单线程推理参数、Python 本机 HTTP 适配服务 | 算术及中文短回答；记录中的单次请求约 193–200 秒 |

验证环境：BlackBerry Classic SQC100-2 / BB10 10.3.3.2049 / BerryCore 0.90.1，记录日期 2026-10-21。物理滑动手势、全部快捷键、其他机型和长期稳定性没有扩大为已验收。[详细验证边界](docs/VALIDATION.md)。

## 使用方式

这是上述功能的完整源码分享，包含最终终端源码、拼音引擎补丁、Pi 依赖锁文件与适配源码、聊天客户端、Qwen 服务、构建方法、测试与回退说明。运行时、SDK、模型与第三方依赖从原项目获取；不附带个人配置、API Key 或聊天记录。

按各模块 README 构建或安装。现有手机无需为了本仓库重新安装；请保留当前已验收版本。整理后的构建入口通过主机检查，不等于已在第二台手机完成从零安装。

## 如何理解贡献

对 BerryCore 而言，这是生态补充；对 Term49 而言，这是源码增强；对 Pi 而言，这是平台适配。将来提交 PR，且被对应维护者合并后，才能进一步称为已合入上游的贡献。计划回馈的内容见 [UPSTREAM](docs/UPSTREAM.md)。

BerryCore、Term49/Term48、Pi、libgooglepinyin、Noto 等原作者的贡献和许可见 [THIRD_PARTY_NOTICES](THIRD_PARTY_NOTICES.md)。新增工具代码及文档采用 Apache-2.0，第三方文件保留原许可。

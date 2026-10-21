BerryCloud 1.0 — BlackBerry Classic / Term49 云端终端聊天

开始聊天：
  ai cloud
继续上次聊天：
  ai cloud --resume
直接问一句：
  ai cloud "用一句话介绍黑莓"
检查连接（会发起一次 API 调用）：
  ai cloud doctor
查看设置（不显示密钥）：
  ai cloud status
更换密钥或接口：
  ai cloud setup

进入后直接输入中文问题，回车发送。不需要浏览器或电脑中转。
逐步显示回答，自动保存完整会话；Ctrl+C 取消生成，/retry 手动重试。
输入 /help 查看全部命令：
/new 新对话；/sessions 会话列表；/load 编号 恢复；/history 完整记录；
/multi 多行输入（单独一行 . 发送）；/export 导出文本；
/model 模型名 切换并新建会话；/think low|high|max；/tokens 4096；
/stream on|off；/status；/exit。

默认 GLM-5.3-Flash，Coding Plan 专用接口 open.bigmodel.cn/api/coding/paas/v4，low 推理，4096 token 预算。
该模型强制开启思考；仅展示正文，不输出思考内容。
输出预算包含思考，若无正文可 /tokens 8192 后 /retry。
不会自动重试计费请求；中断或失败回答不会污染后续上下文。
最多发送最近 20 轮且约 24000 字的上下文，完整会话保存在手机。
单次输入最多 12000 字；连接超时 20 秒、单次请求最多 180 秒。

这是终端聊天客户端，不会执行模型回复中的命令，也不会自动读手机文件。
原 ai / ai "问题" / ai web 等本地功能保持原有路由。
云端需要手机网络及账户可用额度。错误 1113 表示余额或资源包不足。
若 HTTPS 验证失败，请检查手机日期时间和 CA；程序不会跳过证书验证。

密钥与会话保存在 Term49 的 HOME/.berrycloud，文件权限 600、目录 700。
密钥不会放入源码、会话或进程命令参数。数据为本机明文文件，非加密保险箱。
导出文件也在该目录 exports/ 下；按 /status 查看实际完整路径。
退出不会删除会话；重启手机后在 Term49 输入 ai cloud 即可。

卸载入口恢复（仅在需要卸载时执行）：
  cp /accounts/1000/shared/misc/berrycore/bin/ai.before-berrycloud /accounts/1000/shared/misc/berrycore/bin/ai
配置、历史及程序可单独保留。无需重新安装 BerryCore 或本地模型。

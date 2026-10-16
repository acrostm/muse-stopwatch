<p align="right"><strong>简体中文</strong> · <a href="AGENTS.md">English</a></p>

# Agent 开发指引

这是独立的 Muse Gadget SDK／M5Stack StopWatch 适配，目标 ESP32-S3、
16 MB Flash、8 MB PSRAM，使用 ESP-IDF 6.0.1。修改前阅读 README.zh_CN.md
与 docs/DEVELOPMENT.zh_CN.md，先执行 `git status --short --branch` 并保留无关改动。

- 仓库仅限 StopWatch 适配。用 build.py 准备隔离 SDK，不修改调用者的原始检出，
  不把整套上游 SDK 放入仓库。
- 固定上游与依赖版本，补丁涉及的每个原始文件都需在 upstream-hashes.json 中
  记录未修改 SHA-256，并用干净源码验证准备流程。
- 录音必须保留本地，实际按住超过两秒并松手后才能上传。短按、取消、失败和
  未松手的录音不得进入上传或重试队列，内存不足不得退回实时发送。
- 按键回调不阻塞，音频和网络交给工作任务，LVGL 在其任务访问。可测试状态和
  协议独立于 ESP-IDF，行为修改增加回归测试，不跳过或禁用失败测试。
- 配置 CJSON_SOURCE_DIR 后运行 tools/validate.py，再构建并核验固件。分别报告
  构建、主机测试、真机测试和未验收项，构建不等于硬件验证。
- 刷写必须获得用户授权，使用已核验产物和兼容布局。只有 bootloader／分区兼容时，
  从 0x10000 更新应用才能保留 NVS；整片擦除需明确同意。
- 不提交秘密、日志、NVS 镜像、固件／ELF／MAP、上游头像或无关项目。截图仅用
  示例数据，保留上游版权，分发二进制前查看 NOTICE。
- 默认 .md 使用英文并配套 .zh_CN.md 与双向语言链接；PR 说明最终行为、兼容性
  和验证证据。
- Git 写入和公开发布需用户授权；源码发布在全部门禁通过后由版本标签触发。

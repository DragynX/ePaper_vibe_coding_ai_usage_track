# UsageMonitor

> 一块 AI 编程助手用量墨水屏。默认情况下设备自己联网、直连各家 API；也可以选择开启电脑端本地统计服务，让屏幕额外显示今日 token 和模型排行。

[English](README.md) · 简体中文

## 显示什么

高密度左右双栏。左右显示哪个平台由 `platformio.ini` 编译环境决定，每一栏都会明确写出平台名。每一栏：

- **5 小时窗口** —— 滚动的 5 小时额度：大号剩余 %、已用 % 进度条、重置倒计时。这是最大的元素。
- **1 周窗口** —— 7 天额度，同样布局，次要强调。
- **额度详情表**：紧凑显示已用、剩余、重置时间。
- **平台补充信息**：余额、套餐、额外消费、模型额度等，有什么真实数据就显示什么。
- **可选本地统计**：开启电脑端服务后显示今日 token、input/output/cache、会话数、最近活动、top models。

底部显示数据更新时间。若数据陈旧或登录过期，对应栏会提示。

### 本地统计模式

不开电脑端服务时，设备只显示云端额度数据。开启电脑端服务后，电脑读取本地 CLI 日志，通过统一的 `/v1/snapshot` JSON 接口提供增强统计。当前固件支持的平台都会走同一套入口：Claude、Codex、Copilot、MiniMax、Kimi、Zai。

服务不会伪造数据。某个平台暂时没有可读取的本地日志时，会返回 `available=false`，屏幕自动退回云端额度布局。

## 工作原理

设备持有 OAuth token，通过 HTTPS 调用各家用量接口，token 到期时自己刷新，并把轮换后的新 token 持久化到 NVS。拉取失败时继续显示上次成功的快照并标记为陈旧。

| 模块 | 职责 |
| --- | --- |
| `src/main.cpp` | 入口与顶层配置 |
| `UsageApp.*` | 编排：开机、WiFi、NTP、轮询、持久化 |
| `HttpClient.*` | HTTPS 传输（响应头、Retry-After） |
| `OAuthClient.*` | 带刷新重试的鉴权请求 |
| `TokenStore.*` | token 的 NVS 持久化 |
| `*UsageClient.*` | 各 provider 云端额度适配器 |
| `LocalStatsClient.*` | 可选电脑端本地统计客户端 |
| `UsageUI.*` | 墨水屏绘制 |
| `TextRenderer.*` | 英文位图字体 / 中文 OpenFontRender |
| `UiLang.h` | 固定文案与语言选择 |
| `QuotaMath.h` / `TimeFormat.h` / `IsoTime.h` / `HeaderField.h` / `UsageSnapshot.h` | 纯逻辑（native 测试覆盖） |
| `local_stats_service/` | 可选 Python 本地统计服务 |

## 支持的硬件

| 设备 | 屏幕 | 布局 |
| --- | --- | --- |
| reTerminal E1001 | 4 级灰 | 精简双栏 |
| reTerminal E1002 | 6 色 | 精简双栏（红/黄/绿状态） |
| reTerminal E1003 | 16 级灰 | 完整双栏仪表盘 |
| reTerminal E1003 中文 | 16 级灰 | 同上，内嵌中文字体渲染 |

## 快速开始

### 1. 安装 PlatformIO

通过 VS Code 或命令行安装 [PlatformIO](https://platformio.org/)。

### 2. 提供你的 token

先用官方 CLI 在电脑上登录，让凭证文件存在：

- Claude：`claude login` → 写入 `~/.claude/.credentials.json`
- Codex：`codex login` → 写入 `~/.codex/auth.json`

然后复制 secrets 模板并填写：

```sh
cp include/secrets.example.h src/secrets.h
```

有个辅助脚本能从本地凭证文件打印出现成的 `#define` 行：

```sh
python3 scripts/provision.py
```

把它的输出（以及你的 WiFi 信息）粘贴进 `src/secrets.h`。`src/secrets.h` 已被 git 忽略 —— 切勿提交真实 token。

> 设备无法完成浏览器 OAuth 流程。只要 refresh token 有效，它会自己刷新；一旦 refresh token 失效（你登出、改密码、或服务端清除会话），屏幕会提示你在电脑上重新登录并重新烧录。

### 3. 编译与烧录

```sh
# reTerminal E1001 / E1002 / E1003（英文）
pio run -e reterminal_e1001 --target upload
pio run -e reterminal_e1002 --target upload
pio run -e reterminal_e1003 --target upload

# reTerminal E1003，左侧 Codex，右侧 Zai/智谱
pio run -e reterminal_e1003_codex_zai --target upload

# 同样的左右平台，额外启用电脑端本地统计
pio run -e reterminal_e1003_codex_zai_local --target upload

# reTerminal E1003（简体中文）
pio run -e reterminal_e1003_zh --target upload
```

中文字体在编译期内嵌进固件，单次 `upload` 即可。查看串口输出：

```sh
pio device monitor
```

### 4. 可选 电脑端本地统计服务

在保存 CLI 日志的电脑上启动服务：

```sh
python3 local_stats_service/server.py --host 0.0.0.0 --port 8787
```

查看这台电脑的局域网 IP。macOS 使用 WiFi 时，优先试：

```sh
ipconfig getifaddr en0
```

如果没有输出，再试：

```sh
ipconfig getifaddr en1
```

也可以列出所有非本机回环地址：

```sh
ifconfig | grep "inet " | grep -v 127.0.0.1
```

使用类似 `192.168.x.x` 或 `10.x.x.x` 的局域网地址。不要填 `127.0.0.1`，它的意思是“这台电脑自己”，墨水屏设备访问不到。

在 `src/secrets.h` 里把 `UM_LOCAL_STATS_URL` 设置成这个局域网地址，例如：

```cpp
#define UM_LOCAL_STATS_URL "http://10.10.50.65:8787"
```

然后烧录带 `UM_ENABLE_LOCAL_STATS` 的环境，例如：

```sh
pio run -e reterminal_e1003_codex_zai_local --target upload
```

默认会扫描 `~/.claude/projects`、`~/.codex` 等常见 CLI 目录。也可以用环境变量覆盖某个平台的日志目录，例如 `UM_LOCAL_CLAUDE_LOG_DIR` 或 `UM_LOCAL_CODEX_LOG_DIR`。

## 开发

运行 native 单元测试（纯逻辑：额度计算、倒计时格式化、UTC/ISO8601 解析、token 过期、头/体字段选择）：

```sh
pio test -e native
```

## 安全说明

- 真实 WiFi 密码与 OAuth token 只放在 `src/secrets.h`。
- 本地统计服务用 `--host 0.0.0.0` 启动时会监听局域网，只建议在可信网络中使用。
- HTTPS 调用使用了简化的证书处理（`setInsecure()`）以方便开发。生产固件应对 `api.anthropic.com`、`platform.claude.com`、`chatgpt.com`、`auth.openai.com` 固定 CA 证书。
- 这些用量接口并非官方公开 API，而是从 CLI 推导而来，可能随 CLI 升级而变化。调用失败时固件降级到上次成功的快照。

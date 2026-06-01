# UsageMonitor

> 一块独立的桌面墨水屏，显示你的 Claude Code 和 Codex 用量额度。设备自己联网、直连各家 API，不需要陪伴 App，也不用一直开着电脑。

[English](README.md) · 简体中文

## 显示什么

左右双栏（左 Codex，右 Claude）。每一栏：

- **5 小时窗口** —— 滚动的 5 小时额度：大号剩余 %、已用 % 进度条、重置倒计时。这是最大的元素。
- **1 周窗口** —— 7 天额度，同样布局，次要强调。
- **仅 Claude**：Sonnet 和 Opus 各自的 7 天用量行。
- **仅 Codex**：credits 余额。
- **套餐**（Plus / Pro / Free），以及 Claude 的额外消费。

底部显示数据更新时间。若数据陈旧或登录过期，对应栏会提示。

### 不显示什么，为什么

设备**直连各家云端用量接口**取数，这些接口提供额度窗口、credits、套餐，但**不提供**精确 token 数、按模型的 token 总量、请求/会话次数、按自然日的累计。那些数字只存在于你电脑本地的 CLI 日志里，需要一个常驻服务才能读取。本项目刻意做成自包含的，因此只显示设备自己能取到的数据。

## 工作原理

设备持有 OAuth token，通过 HTTPS 调用各家用量接口，token 到期时自己刷新，并把轮换后的新 token 持久化到 NVS。拉取失败时继续显示上次成功的快照并标记为陈旧。

| 模块 | 职责 |
| --- | --- |
| `src/main.cpp` | 入口与顶层配置 |
| `UsageApp.*` | 编排：开机、WiFi、NTP、轮询、持久化 |
| `HttpClient.*` | HTTPS 传输（响应头、Retry-After） |
| `OAuthClient.*` | 带刷新重试的鉴权请求 |
| `TokenStore.*` | token 的 NVS 持久化 |
| `ClaudeUsageClient.*` / `CodexUsageClient.*` | 各 provider 适配器 |
| `UsageUI.*` | 墨水屏绘制 |
| `TextRenderer.*` | 英文位图字体 / 中文 OpenFontRender |
| `UiLang.h` | 固定文案与语言选择 |
| `QuotaMath.h` / `TimeFormat.h` / `IsoTime.h` / `HeaderField.h` / `UsageSnapshot.h` | 纯逻辑（native 测试覆盖） |

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

# reTerminal E1003（简体中文）
pio run -e reterminal_e1003_zh --target upload
```

中文字体在编译期内嵌进固件，单次 `upload` 即可。查看串口输出：

```sh
pio device monitor
```

## 开发

运行 native 单元测试（纯逻辑：额度计算、倒计时格式化、UTC/ISO8601 解析、token 过期、头/体字段选择）：

```sh
pio test -e native
```

## 安全说明

- 真实 WiFi 密码与 OAuth token 只放在 `src/secrets.h`。
- HTTPS 调用使用了简化的证书处理（`setInsecure()`）以方便开发。生产固件应对 `api.anthropic.com`、`platform.claude.com`、`chatgpt.com`、`auth.openai.com` 固定 CA 证书。
- 这些用量接口并非官方公开 API，而是从 CLI 推导而来，可能随 CLI 升级而变化。调用失败时固件降级到上次成功的快照。

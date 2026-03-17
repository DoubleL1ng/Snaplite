# Words-Bin（桶） - 轻量级截图与剪贴板工具

English | [中文](README.md)

## 项目简介

Words-Bin（中文名：桶）是一个运行在 Windows 上的轻量级截图与剪贴板工具，提供高效、直观的截图体验。支持区域截图、全屏截图，集成系统托盘、热键、剪贴板历史等功能。

## ? 主要功能

- **区域截图** — 冻结式交互，清晰标记选区，实时预览
- **全屏截图** — 一键捕获整个屏幕
- **剪贴板历史** — 快速访问历次截图记录
- **系统托盘** — 最小化到通知栏，左键快速唤出侧边栏
- **全局热键** — 可自定义的快捷键快速触发
- **持久化设置** — 记住用户偏好和应用状态
- **侧边栏管理** — 可钉住、可自动隐藏，边缘悬停自动呼出
- **文件管理** — 自选保存位置、自定义文件名格式

## ? 快速开始

### 系统要求

- Windows 7 或更新版本
- Qt 6.10.2（或兼容版本）
- CMake 3.20+
- MinGW 64-bit 编译器

### 编译步骤

1. **克隆仓库**
   ```bash
   git clone https://github.com/DoubleL1ng/Snaplite.git Words-Bin
   cd Words-Bin
   ```

2. **使用 Qt Creator 打开项目**
   - 打开 `CMakeLists.txt` 作为 CMake 项目
   - 选择 `Desktop Qt 6.10.2 MinGW 64-bit` 工具链

3. **编译**
   - 在 Qt Creator 中点击 `Build`，或在命令行运行：
     ```bash
     cd build/Desktop_Qt_6_10_2_MinGW_64_bit-Debug
     mingw32-make -j6
     ```

4. **运行**
   - Debug 模式：点击 Qt Creator 的 `Run` 按钮
   - 直接运行可执行文件：`build/.../Words-Bin.exe`

## ? 使用指南

### 基本操作

| 操作 | 说明 |
|------|------|
| 全局热键 | 唤起截图工具（可在设置中自定义） |
| 区域选择 | 鼠标拖拽定义截图范围，冻结背景 |
| 确认保存 | 点击预览窗口的保存按钮 |
| 取消操作 | 按 ESC 或点击关闭 |
| 托盘左键 | 快速呼出/隐藏侧边栏 |

### 侧边栏

- **钉住按钮** — 锁定侧边栏，不自动隐藏
- **GitHub** — 打开项目仓库
- **设置** — 调整应用参数
- **关闭** — 退出应用（确认后执行）

### 设置选项

- 截图保存路径
- 文件名格式（支持时间戳等变量）
- 全局热键绑定
- 边缘悬停自动呼出延迟
- 截图时自动隐藏侧边栏
- 启动时自动钉住选项

## ? 项目结构

```
Words-Bin/
├── main.cpp                 # 应用入口
├── MainWindow.h/.cpp        # 主窗口和侧边栏逻辑
├── CaptureTool.h/.cpp       # 截图选择和渲染
├── TrayIcon.h/.cpp          # 系统托盘交互
├── SettingsDialog.h/.cpp    # 设置对话框
├── AppSettings.h            # 常量和配置管理
├── CMakeLists.txt           # CMake 构建配置
├── icons/                   # 图标资源
└── libs/                    # 第三方库（jsqr 等）
```

## ? 开发信息

### 技术栈

- **框架** — Qt Widgets + Qt Gui
- **语言** — C++17
- **构建系统** — CMake
- **平台** — Windows（使用系统 API 实现全局热键）
- **产品名** — Words-Bin（中文名：桶）

### 核心模块

| 模块 | 职责 |
|------|------|
| `MainWindow` | 侧边栏管理、状态协调、多模式切换 |
| `CaptureTool` | 区域/全屏选择、冻结渲染、DPI 适配 |
| `TrayIcon` | 托盘菜单、热键注册、全局事件 |
| `SettingsDialog` | UI 设置编辑与持久化 |
| `AppSettings` | 全局常量、密钥定义、默认值 |

### 构建注意事项

- **Release vs Debug** — 推荐日常使用 Release 版本；开发调试使用 Debug 版本
- **链接器锁** — Release 编译时若 `Words-Bin.exe` 正在运行，需先关闭（使用 `Stop-Process -Name Words-Bin` 命令）
- **DPI 适配** — 代码中已处理 2K/4K 高分屏缩放

## ? 版本历史

### v0.1.0 (2026-03-16)

- ? 侧边栏交互模型优化（边缘呼出、停靠、按住行为）
- ? 新增钉住按钮，支持跨重启记忆
- ? 新增关闭确认对话框，防止误触退出
- ? 修复区域截图高 DPI 放大问题
- ? 降低区域选择抖动
- ? 修复设置项"截图时隐藏侧边栏"功能
- ? CMake 项目配置稳定化

## ? 贡献

欢迎提交 Issue 或 Pull Request！

## ? 许可证

项目使用 MIT 许可证。详见 [LICENSE](LICENSE) 文件。

## ? 反馈

如有问题、建议或功能请求，欢迎在 [Issues](https://github.com/DoubleL1ng/Snaplite/issues) 中提出。

当前 GitHub 仓库地址仍为旧地址，待线上仓库实际改名后可再同步切换：
https://github.com/DoubleL1ng/Snaplite

---

**维护者** — DoubleL1ng  
**项目链接** — https://github.com/DoubleL1ng/Snaplite

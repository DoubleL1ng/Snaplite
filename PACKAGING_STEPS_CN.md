# Words-Bin 打包步骤（Qt Creator 新手版）

本文档目标：
- 编译出 Release 版本的 Words-Bin.exe
- 使用 windeployqt 补齐 Qt 运行库
- 使用 Inno Setup 生成安装包 Setup.exe

## 1. 准备环境

需要安装以下工具：
1. Visual Studio 2022（安装工作负载：Desktop development with C++）
2. Qt Online Installer（建议安装 Qt 6.x，组件选择 `msvc2022_64`）
3. Qt Creator（通常和 Qt 一起安装）
4. Inno Setup（用于做安装包，可选）

建议保持编译器一致：
- Qt 套件选 `MSVC 2022 64-bit`
- 不建议混用 MinGW 和 MSVC

## 2. 用 Qt Creator 打开工程

1. 打开 Qt Creator
2. 选择 File -> Open File or Project
3. 选择当前目录下的 `CMakeLists.txt`
4. 在 Configure Project 页面选择 Kit：`Desktop Qt 6.x MSVC2022 64bit`
5. 点击 Configure

首次配置成功后，Qt Creator 会自动生成构建目录。

## 3. 编译 Release 可执行文件

1. 左下角 Build Configuration 切到 `Release`
2. 点击 Build（锤子图标）
3. 编译成功后，在构建目录中找到 `Words-Bin.exe`

Qt Creator 中可通过 Projects 页面查看具体 Build Directory。

## 4. 用 windeployqt 收集运行库（关键步骤）

`Words-Bin.exe` 不能单独发送给别人，必须带上 Qt 依赖库。

方法：
1. 打开“x64 Native Tools Command Prompt for VS 2022”
2. 进入 `Words-Bin.exe` 所在目录
3. 执行命令（将路径替换为你本机 Qt 安装路径）：

```bat
C:\Qt\6.8.0\msvc2022_64\bin\windeployqt.exe --release --compiler-runtime Words-Bin.exe
```

执行后，当前目录会出现：
- `Qt6Core.dll` / `Qt6Gui.dll` / `Qt6Widgets.dll`
- `platforms\qwindows.dll`
- 以及其他插件目录

到这一步就已经得到可分发的“绿色版”目录。

## 5. 先做绿色版验证（强烈建议）

1. 新建一个干净目录，例如 `D:\dist\Words-Bin`
2. 把 `Words-Bin.exe` 和 windeployqt 生成的所有文件复制进去
3. 在本机直接运行一次，确认程序可以启动
4. 最好在另一台未安装 Qt 的机器测试

如果绿色版可用，再做安装包。

## 6. 使用 Inno Setup 生成安装包

### 6.1 新建脚本

打开 Inno Setup，新建一个 `.iss` 脚本，示例：

```iss
#define MyAppName "Words-Bin"
#define MyAppVersion "0.1.0"
#define MyAppPublisher "YourName"
#define MyAppExeName "Words-Bin.exe"

[Setup]
AppId={{A1F9D9A1-5D56-4BA6-8A8A-1E4BB7AF0001}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
DefaultDirName={autopf}\{#MyAppName}
DefaultGroupName={#MyAppName}
DisableProgramGroupPage=yes
OutputDir=.
OutputBaseFilename=Words-Bin-Setup
Compression=lzma
SolidCompression=yes
ArchitecturesInstallIn64BitMode=x64
WizardStyle=modern

[Languages]
Name: "chinesesimp"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "创建桌面快捷方式"; GroupDescription: "附加任务:"; Flags: unchecked

[Files]
Source: "D:\dist\Words-Bin\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "运行 {#MyAppName}"; Flags: nowait postinstall skipifsilent
```

注意修改：
- `Source` 路径改成你的绿色版目录
- `AppPublisher` 改成你自己的名称

### 6.2 编译安装包

1. 在 Inno Setup 中点击 Build -> Compile
2. 编译完成后得到 `Words-Bin-Setup.exe`

## 7. 常见问题排查

1. 运行时报错“找不到 Qt6Core.dll”
- 原因：没有执行 windeployqt 或拷贝不完整
- 处理：回到第 4 步重新执行并完整复制目录

2. 双击程序无反应
- 原因：托盘程序启动后默认隐藏主窗口
- 处理：检查系统托盘区域是否有图标

3. Qt Creator 配置失败（Kit 不可用）
- 原因：Qt 编译器套件和 VS 环境不匹配
- 处理：确认安装了 `msvc2022_64` 版本 Qt 和 VS2022 C++ 组件

4. 打包后在别的电脑运行失败
- 原因：只拷贝了 exe，没有带 plugins 目录
- 处理：整个绿色版目录必须完整打包

## 8. 推荐发布流程

1. Qt Creator 编译 Release
2. windeployqt 收集依赖
3. 先做绿色版跨机测试
4. Inno Setup 生成安装包
5. 再次跨机验证安装包

按这个流程，你可以稳定产出可安装的 Windows 版本。

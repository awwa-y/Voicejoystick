VoiceJoystick

Qt6 + Vosk 离线语音 + 自绘摇杆，通过串口控制 STM32 舵机。

功能

- 鼠标拖动摇杆，实时发送控制帧
- 语音识别（离线，中文小模型），支持指令：左转、右转、加速、减速
- 串口通信（4800, 8N1），数据帧格式见 `CommandProcessor.cpp`
- 下位机 STM32F103 解析帧并控制舵机（0~180°）

编译环境

- Qt 6.9.3 (MinGW 64-bit)
- CMake 3.19+
- Vosk 离线库 `vosk-win64-0.3.45`
- 模型 `vosk-model-small-cn-0.22`

编译步骤

1. 克隆仓库，确保目录结构如下：
2. VoiceJoystick/
├── qt端源码
├── cmake
├── other(包含vosk库文件，以及qt6.9.3的dll文件）



2. 用 Qt Creator 打开 CMakeLists.txt，选择 **MinGW 64-bit** 套件，编译 Release。

3. 编译完成后，**必须**在 Qt 专用命令行中运行 `windeployqt6.exe --force your_exe.exe`，否则缺少 Qt 的 dll 和 platforms 插件。

4. 手动复制 `libs/vosk/libvosk.dll` 以及 MinGW 运行时 dll（`libgcc_s_seh-1.dll`, `libstdc++-6.dll`, `libwinpthread-1.dll`）到 exe 目录。

5. 把 `model` 文件夹也复制到 exe 目录。

## 常见问题与解决

### 1. 编译时找不到 `QAudioInput` / `QAudioSource`

- 原因：缺少 Multimedia 模块。  
- 解决：在 CMakeLists.txt 中 `find_package` 加上 `Multimedia`，`target_link_libraries` 加上 `Qt6::Multimedia`。

### 2. 运行时“无法定位程序输入点 __glibcxx_assert_fail”

- 原因：系统 PATH 中混入了其他 MinGW 的 `libstdc++-6.dll`，版本不匹配。  
- 解决：
- 从你的 Qt 安装目录 `mingw_64\bin` 复制 `libstdc++-6.dll` 到 exe 目录覆盖。
- 或清理 PATH 环境变量，只保留 Qt 的 MinGW 路径。

### 3. 双击 exe 提示缺少 Qt6Core.dll 或找不到平台插件

- 原因：没有正确部署 Qt 依赖。  
- 解决：用 `windeployqt6` 自动部署，或者手动复制 `platforms/qwindows.dll` 等插件。

### 4. 语音识别没反应

- 检查麦克风权限（Windows 设置 → 隐私 → 麦克风）。
- 检查 `model` 文件夹是否完整且位于 exe 同级目录。
- 观察控制台输出，确认 Vosk 模型加载成功、麦克风被正确打开。

## 串口协议

帧格式（8 字节）：0xAA, 0x01, 0x03, x_byte, y_byte, speed, checksum, 0x55


- x_byte = x + 100（x 范围 -100..100）
- y_byte = y + 100
- checksum = sum(字节1~5) 的低 8 位

## 仓库说明

- `main` 分支：当前稳定版本。
- 模型文件和 Vosk 的 dll 需要自行下载（版权原因不放入仓库），但本 README 已写明放置位置。

## 后续计划

- 添加更多语音指令（回中、微调）
- 支持动态切换模型
- 优化串口重连逻辑

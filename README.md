VoiceJoystick
这个项目是用 Qt6 加上 Vosk 离线语音识别，自己画了一个摇杆界面。通过串口控制 STM32 上的舵机。

功能
鼠标拖动摇杆，实时发送控制帧。

语音识别是离线中文小模型，支持“左转、右转、加速、减速”这几个指令。

串口通信是 4800 波特率，8N1 格式。数据帧格式在 CommandProcessor.cpp 里有定义。

下位机是 STM32F103，解析收到的帧然后控制舵机转 0 到 180 度。

编译环境
我用的是 Qt 6.9.3，MinGW 64-bit 套件。CMake 版本 3.19 以上。
Vosk 用的离线库是 vosk-win64-0.3.45，模型是 vosk-model-small-cn-0.22。

编译步骤
克隆仓库后，目录结构大概是这样：
VoiceJoystick/ 下面有 qt端源码、cmake 目录，还有一个 other 文件夹（里面放 vosk 库文件以及 Qt6.9.3 的 dll 文件）。

在 Qt Creator 里打开 CMakeLists.txt，选择 MinGW 64-bit 套件，编译 Release 版本。

编译完成后，一定要在 Qt 专用的命令行里运行
windeployqt6.exe --force your_exe.exe
不然会缺少 Qt 的 dll 和 platforms 插件。

手动复制 libs/vosk/libvosk.dll，还有 MinGW 运行时的那几个 dll（libgcc_s_seh-1.dll、libstdc++-6.dll、libwinpthread-1.dll）到 exe 所在的目录。

把 model 文件夹也复制到 exe 目录里。

常见问题与解决办法
编译时找不到 QAudioInput 或 QAudioSource
原因是没有加 Multimedia 模块。
解决方法：在 CMakeLists.txt 里的 find_package 加上 Multimedia，target_link_libraries 里也加上 Qt6::Multimedia。

运行时提示“无法定位程序输入点 __glibcxx_assert_fail”
这个通常是因为系统 PATH 里混了其他 MinGW 的 libstdc++-6.dll，版本不匹配。
解决方法：从你的 Qt 安装目录下的 mingw_64\bin 复制正确的 libstdc++-6.dll 到 exe 目录覆盖掉。或者把 PATH 环境变量清理一下，只保留 Qt 自己的 MinGW 路径。

双击 exe 提示缺少 Qt6Core.dll 或找不到平台插件
那是因为没有部署 Qt 的依赖。
用 windeployqt6 自动部署就好了。也可以手动复制 platforms/qwindows.dll 等插件。
示例命令（在 build 目录下执行）：
windeployqt6 --force mawinHcom.exe

语音识别没反应
先检查麦克风权限：Windows 设置 → 隐私 → 麦克风，确保允许。
再确认 model 文件夹是否完整，并且放在 exe 同级目录。
运行程序时看看控制台输出，确认 Vosk 模型加载成功，麦克风也被正确打开了。

串口协议
帧格式一共 8 个字节：
0xAA, 0x01, 0x03, x_byte, y_byte, speed, checksum, 0x55

x_byte = x + 100，其中 x 的范围是 -100 到 100。

y_byte = y + 100

checksum 是前面第 1 到第 5 个字节（也就是 0xAA, 0x01, 0x03, x_byte, y_byte）的和，取低 8 位。

仓库说明
main 分支是当前稳定版本。
模型文件和 Vosk 的 dll 需要自己下载（版权原因不放仓库里），不过这个 README 里已经把放在哪写清楚了。

后续打算
后面想再加点语音指令，比如回中、微调。
也打算支持动态切换模型，还有优化串口重连的逻辑。

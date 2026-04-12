cmake_minimum_required(VERSION 3.19)
project(mawinHcom LANGUAGES CXX)

# 找到需要的 Qt 组件
find_package(Qt6 6.5 REQUIRED COMPONENTS Core Widgets Multimedia SerialPort)

qt_standard_project_setup()

set(CMAKE_AUTOUIC_SEARCH_PATHS ${CMAKE_CURRENT_SOURCE_DIR})
set(CMAKE_INCLUDE_CURRENT_DIR ON)

# 只添加源文件、头文件、UI 文件，不要加 .dll/.lib/模型文件
qt_add_executable(mawinHcom
    WIN32 MACOSX_BUNDLE
    main.cpp
    mainwindow.cpp
    mainwindow.h
    mainwindow.ui
    serialwork.h
    serialwork.cpp
    log.h
    log.cpp
    joystickmanager.h
    joystickmanager.cpp
    commandprocessor.h
    commandprocessor.cpp
    VoskWorker.h
    VoskWorker.cpp
)

# Vosk 配置
set(VOSK_DIR ${CMAKE_CURRENT_SOURCE_DIR}/libs/vosk)
target_include_directories(mawinHcom PRIVATE ${VOSK_DIR})
target_link_libraries(mawinHcom PRIVATE ${VOSK_DIR}/libvosk.lib)

# 链接 Qt 模块
target_link_libraries(mawinHcom PRIVATE
    Qt6::Core
    Qt6::Widgets
    Qt6::Multimedia
    Qt6::SerialPort
)

# 静态链接 MinGW 运行时（可选，减少 DLL 依赖）
if(MINGW)
    target_link_options(mawinHcom PRIVATE -static-libgcc -static-libstdc++)
    # -static-libwinpthread 在你的 MinGW 中不支持，故不添加
endif()

# 复制 Vosk DLL 到输出目录
add_custom_command(TARGET mawinHcom POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${VOSK_DIR}/libvosk.dll
        $<TARGET_FILE_DIR:mawinHcom>
    COMMENT "Copying libvosk.dll"
)

# 复制 MinGW 运行时 DLL（如果上面没有静态链接 winpthread）
add_custom_command(TARGET mawinHcom POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${VOSK_DIR}/libgcc_s_seh-1.dll
        ${VOSK_DIR}/libstdc++-6.dll
        ${VOSK_DIR}/libwinpthread-1.dll
        $<TARGET_FILE_DIR:mawinHcom>
    COMMENT "Copying MinGW runtime DLLs"
)

# 复制模型文件夹
add_custom_command(TARGET mawinHcom POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/model
        $<TARGET_FILE_DIR:mawinHcom>/model
    COMMENT "Copying Vosk model"
)

# 部署脚本
include(GNUInstallDirs)
qt_generate_deploy_app_script(
    TARGET mawinHcom
    OUTPUT_SCRIPT deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
)
install(SCRIPT ${deploy_script})
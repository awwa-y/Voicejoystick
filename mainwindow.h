#ifndef MAINWINDOW_H
#define MAINWINDOW_H
#include <QMainWindow>
#include "devicemanager.h"
#include "joystickmanager.h"
#include <QTimer>
#include <QSerialPortInfo>
#include <QDebug>
#include <QStringList>
#include <QThread>
#include "VoskWorker.h"
#include "gatewayclient.h"
#include <QPushButton>
#include <QLabel>
#include <QTextEdit>
#include <QLineEdit>

#include <QMessageBox>
QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

// 主窗口：UI 层，串起各模块
//   摇杆控件 -> JoystickManager 的信号 -> 组帧发 DeviceManager
//   语音 -> VoskWorker（独立线程） -> 指令信号 -> 控制电机/摇杆
//   设备 -> DeviceManager 统一管理，本类只做展示和转发
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();
    QStringList getAvailablePorts();   // 枚举系统串口
    void Qslwork_init();               // 初始化串口相关 UI/逻辑
    void speedchange();                // 速度档位变化处理

    int getbaud();                     // 读取 UI 选的波特率
public:
    bool m_hasPending;                 // 有待发送的摇杆数据
    void updateDeviceTabStatus(DeviceType type, bool added);  // 设备页签状态刷新
    DeviceType getSelectedDeviceType();                       // UI 当前选的设备类型
    QString getDeviceNameByType(DeviceType type);             // 类型转显示名
    void onMotorAngleSlider_valueChanged(int value);          // 舵机角度滑条
    void onReadSensorClicked();                               // 读传感器按钮

public slots:
    void onLogMessageReceived(const QString &message);   // 日志显示到 UI
private slots:
    // 设备连接
    void onMotorConnectClicked();      // 电机连接/断开
    void onServoConnectClicked();      // 舵机连接/断开
    void onSensorConnectClicked();     // 传感器连接/断开

    void onRefreshPortsClicked();      // 刷新串口列表
    // 方向/速度按钮
    void onForwardButtonClicked();
    void onBackwardButtonClicked();
    void onSpeedUpButtonClicked();
    void onSpeedDownButtonClicked();
    // 串口事件
    void onDataReceived(int deviceId, const QByteArray &data);  // 收到设备数据
    void onSerialError(int deviceId, const QString &error);
    void onSerialOpened(int deviceId);
    void onSerialClosed();
    void changenewdata(int newx,int newy);   // 摇杆数据变化（40ms 周期发送的入口）
    void sendPendingData();                 // 定时发送积压的摇杆数据
    void speedUp();
    void speedDown();

    void onAddDeviceClicked();              // 添加设备
    void onRemoveDeviceClicked();           // 移除设备
    void updateDeviceListCombox();          // 刷新设备下拉框

    void onDeviceComboBoxChanged(int index);                       // 切换活动设备
    void onDeviceStatusChanged(int deviceId, const QString &status);
    // 收到 DeviceManager 解析出的完整协议帧（data 是帧内数据段，不是裸字节流）
    void onFrameReceived(int deviceId, quint8 cmdType, quint8 deviceType, const QByteArray &data);
    void onSendCommandClicked();                                   // 手动发命令按钮

    void onVoiceCommand(const QString &cmd);   // 识别到语音指令
    void onVoiceStatus(const QString &msg);    // 语音引擎状态（启动/停止）
    void onVoiceError(const QString &err);     // 语音错误
    void toggleVoiceRecognition(bool enabled); // 语音开关按钮
    void onGatewayConnectClicked();            // 网关连接/断开按钮
private:
    void applyDarkTheme();          // 全局深色主题样式
    void buildModernLayout();       // 卡片式布局重排(只动布局,控件与逻辑不动)
    Ui::MainWindow *ui;
    JoystickManager *m_joystick;    // 摇杆控件
    QTimer *m_sendTimer;            // 摇杆数据定时发送（40ms 周期）
    bool m_isConnected;
    QString portName;
    int baud;
    int m_pendingX, m_pendingY;     // 待发送的摇杆位置
    bool m_pendingA, m_pendingB;
    int m_speedValue;               // 当前速度
    int m_lastX;
    int m_lastY;

    bool m_motorConnected;
    bool m_servoConnected;
    bool m_sensorConnected;

    // 语音模块：worker 住在独立线程，避免模型推理卡 UI
    QThread      m_voskThread;
    VoskWorker   *m_voskWorker = nullptr;

    // 设备管理器
    DeviceManager *deviceManager = nullptr;
    // 设备类型 -> 当前选中的设备 ID（UI 操作的目标）
    QMap<DeviceType, int> devicetypeIdmap;

    // 网关客户端(边缘代理):串口帧经 TCP 转发 epoll 网关
    GatewayClient *m_gateway = nullptr;
    QLineEdit     *m_gatewayEdit = nullptr;    // 网关地址输入(代码创建,不在Designer)
    QPushButton   *m_gatewayButton = nullptr;  // 连接/断开网关
    bool m_gatewayConnected = false;
signals:
    void openSerialRequest(QString portName, int baudRate, int deviceId);
    void closeSerialRequest(int deviceId);
    void sendDataRequest(int deviceId, const QByteArray &data);

    // 转发给 VoskWorker 的开关请求（队列连接到语音线程）
    void startVoiceRecognition();
    void stopVoiceRecognition();
};

#endif

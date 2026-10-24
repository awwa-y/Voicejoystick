#ifndef DEVICEMANAGER_H
#define DEVICEMANAGER_H

#include <QObject>
#include <QMap>
#include <QList>
#include <QString>
#include <QDateTime>
#include <qthread>
#include "serialwork.h"

class ProtocolFrame;  // 前向声明：协议解析器，每设备一个实例

// 设备类型（业务侧的分类，和协议帧里的 deviceType 字段对应）
enum DeviceType {
    DEVICE_TYPE_UNKNOWN = 0,
    DEVICE_TYPE_HUMIDITY_SENSOR = 1,  // 传感器
    DEVICE_TYPE_SERVO = 2,            // 舵机
    DEVICE_TYPE_MOTOR = 3             // 电机
};

// 命令类型
enum CommandType
{
    CMD_TYPE_UNKNOWN = 0,
    CMD_TYPE_SERVO_CONTROL = 1,   // 舵机控制
    CMD_TYPE_MOTOR_CONTROL = 2,   // 电机控制
    CMD_TYPE_SENSOR_READ = 3,     // 传感器读取
    CMD_TYPE_DEVICE_STATUS = 4    // 设备状态查询
};

// 设备管理器：统一管理所有设备的注册、连接状态和线程
// 每个设备一个 SerialWork + 一个 QThread（设备隔离），
// DeviceManager 自己住在主线程，通过信号槽和各串口线程通信
// 维护两张表：devices（ID->DeviceInfo）和 deviceTypeMap（类型->ID列表）
class DeviceManager : public QObject
{
    Q_OBJECT
public:
    explicit DeviceManager(QObject *parent = nullptr);
    ~DeviceManager();

    // 添加设备：建 SerialWork 和线程，返回分配的设备 ID
    int addDevice(const QString &portName, int baudRate, const QString &deviceName, DeviceType type);
    // 移除设备：先关串口再停线程，清两张表
    bool removeDevice(int deviceId);
    // 给指定设备发原始数据（走队列连接，实际发送在设备线程里）
    bool sendData(int deviceId, const QByteArray &data);
    // 给某类型所有设备发数据（比如所有电机一起停）
    bool sendCommandByType(DeviceType deviceType, const QByteArray &data);
    // 给所有在线设备发数据
    bool broadcastCommand(const QByteArray &data);
    // 设备列表：ID -> "名字 (状态)"
    QMap<int, QString> getDeviceList();
    void setActiveDevice(int deviceId);   // 设置当前活动设备（UI 操作的目标）
    int getActiveDevice();
    bool isDeviceConnected(int deviceId);
    void setDeviceConnected(int deviceId, bool connected);
    bool isDeviceConnected(int deviceId) const;

    // 查询设备参数
    QString getDeviceStatus(int deviceId);
    DeviceType getDeviceType(int deviceId) const;
    QString getDevicePortName(int deviceId) const;
    int getDeviceBaudRate(int deviceId) const;
signals:
    void deviceStatusChanged(int deviceId, const QString &status);    // 状态变化（连接/断开/错误）
    void deviceDataReceived(int deviceId, const QByteArray &data);    // 收到设备数据
    // 设备字节流过状态机后解析出的完整协议帧（cmdType/deviceType 对应帧字段）
    void deviceFrameReceived(int deviceId, quint8 cmdType, quint8 deviceType, const QByteArray &data);
    void deviceConnected(int deviceId);
    void deviceDisconnected(int deviceId);
    void deviceError(int deviceId, const QString &error);
    void deviceActivity(int deviceId);                                // 有发送动作
    void activeDeviceChanged(int deviceId);

    // 转发给串口线程的请求（队列连接，跨线程安全）
    void openSerialRequest(QString portName, int baudRate, int deviceId);
    void closeSerialRequest(int deviceId);
    void sendDataRequest(int deviceId, const QByteArray &data);

public slots:
    void handleSerialOpened(int deviceId);                            // 串口打开成功的回调
    void handleSerialClosed(int deviceId);                            // 串口关闭的回调
    void handleDataAvailable(int deviceId);                           // 接收缓冲有数据的回调，取走并上报
    void handleSerialError(int deviceId, const QString &error);       // 串口错误的回调

private:
    // 单台设备的全部信息
    struct DeviceInfo {
        int id;
        QString name;
        DeviceType type;
        QString portName;
        int baudRate;
        QString status;
        bool isConnected;
        SerialWork *serialWork;      // 串口工作者（住在 device->thread 里）
        ProtocolFrame *parser;       // 该设备专属解析器（住主线程，设备间字节流不混流）
        QThread *thread;             // 该设备专属线程
        QDateTime lastActivity;      // 最后一次收发时间（判断超时用）
        QByteArray lastCommand;      // 最后发送的数据（调试用）
        QByteArray lastResponse;     // 最后收到的数据（调试用）
    };

    QMap<int, DeviceInfo*> devices;          // ID -> 设备信息
    QMap<DeviceType, QList<int>> deviceTypeMap;  // 类型 -> ID 列表（按类型群发用）
    int nextDeviceId;      // 自增 ID 分配器
    int activeDeviceId;    // 当前活动设备，-1 表示没有
};

#endif // DEVICEMANAGER_H

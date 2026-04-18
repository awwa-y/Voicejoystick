#ifndef DEVICEMANAGER_H
#define DEVICEMANAGER_H

#include <QObject>
#include <QMap>
#include <QString>
#include <QDateTime>
#include <qthread>
#include "serialwork.h"

enum DeviceType {
    DEVICE_TYPE_UNKNOWN = 0,
    DEVICE_TYPE_HUMIDITY_SENSOR = 1,
    DEVICE_TYPE_SERVO = 2,
    DEVICE_TYPE_MOTOR = 3
};
enum CommandType {
    CMD_TYPE_UNKNOWN = 0,
    CMD_TYPE_SERVO_CONTROL = 1,// 舵机
    CMD_TYPE_MOTOR_CONTROL = 2, // 电机
    CMD_TYPE_SENSOR_READ = 3,  // 传感器读取
    CMD_TYPE_DEVICE_STATUS = 4// 设备状态
};
class DeviceManager : public QObject
{
    Q_OBJECT
public:
    explicit DeviceManager(QObject *parent = nullptr);
    ~DeviceManager();

    int addDevice(const QString &portName, int baudRate, const QString &deviceName, DeviceType type);
    bool removeDevice(int deviceId);
    bool sendData(int deviceId, const QByteArray &data);
    bool sendCommandByType(DeviceType deviceType, const QByteArray &data);
    bool broadcastCommand(const QByteArray &data);
    QMap<int, QString> getDeviceList();
    QString getDeviceStatus(int deviceId);
    void setActiveDevice(int deviceId);
    int getActiveDevice();
    bool isDeviceConnected(int deviceId);

signals:
    void deviceStatusChanged(int deviceId, const QString &status);
    void deviceDataReceived(int deviceId, const QByteArray &data);
    void deviceConnected(int deviceId);
    void deviceDisconnected(int deviceId);
    void deviceError(int deviceId, const QString &error);
    void deviceActivity(int deviceId);
    void activeDeviceChanged(int deviceId);

    void openSerialRequest(QString portName, int baudRate, int deviceId);
    void closeSerialRequest(int deviceId);
    void sendDataRequest(int deviceId, const QByteArray &data);

public slots:
    void handleSerialOpened(int deviceId);
    void handleSerialClosed(int deviceId);
    void handleDataAvailable();
    void handleSerialError(int deviceId, const QString &error);

private:
    struct DeviceInfo {
        int id;
        QString name;
        DeviceType type;
        QString portName;
        int baudRate;
        QString status;
        bool isConnected;
        SerialWork *serialWork;
        QThread *thread;
        QDateTime lastActivity;
        QByteArray lastCommand;
        QByteArray lastResponse;
    };

    QMap<int, DeviceInfo*> devices;
    QMap<DeviceType, QList<int>> deviceTypeMap;
    int nextDeviceId;
    int activeDeviceId;
};

#endif // DEVICEMANAGER_H
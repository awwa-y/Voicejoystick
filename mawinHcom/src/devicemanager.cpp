#include "devicemanager.h"
#include <QDebug>

DeviceManager::DeviceManager(QObject *parent)
    : QObject(parent), nextDeviceId(1), activeDeviceId(-1)
{
}

DeviceManager::~DeviceManager()
{
    for (auto device : devices) {
        if (device->serialWork) {
            device->thread->quit();
            device->thread->wait();
            delete device->serialWork;
            delete device->thread;
        }
        delete device;
    }
}

int DeviceManager::addDevice(const QString &portName, int baudRate, const QString &deviceName, DeviceType type)
{
    int deviceId = nextDeviceId++;

    DeviceInfo *device = new DeviceInfo();
    device->id = deviceId;
    device->name = deviceName;
    device->type = type;
    device->portName = portName;
    device->baudRate = baudRate;
    device->status = "Connecting";
    device->isConnected = false;
    device->thread = new QThread();
    device->serialWork = new SerialWork(deviceId, this);

    // 连接DeviceManager信号到SerialWork槽
    connect(this, &DeviceManager::openSerialRequest, device->serialWork, &SerialWork::openSerial);
    connect(this, &DeviceManager::closeSerialRequest, device->serialWork, &SerialWork::closeSerial);
    connect(this, &DeviceManager::sendDataRequest, device->serialWork, &SerialWork::sendData);

    // 连接SerialWork信号到DeviceManager槽
    connect(device->serialWork, &SerialWork::serialOpened, this, &DeviceManager::handleSerialOpened);
    connect(device->serialWork, &SerialWork::serialClosed, this, &DeviceManager::handleSerialClosed);
    connect(device->serialWork, &SerialWork::errorOccurred, this, &DeviceManager::handleSerialError);

    // 连接串口数据就绪信号（高频优化）
    connect(device->serialWork, &SerialWork::newDataAvailable, this, &DeviceManager::handleDataAvailable, Qt::QueuedConnection);

    device->serialWork->moveToThread(device->thread);
    device->thread->start();

    devices[deviceId] = device;
    deviceTypeMap[type].append(deviceId);

    if (activeDeviceId == -1) {
        setActiveDevice(deviceId);
    }

    emit openSerialRequest(portName, baudRate, deviceId);
    return deviceId;
}

bool DeviceManager::removeDevice(int deviceId)
{
    if (devices.contains(deviceId)) {
        DeviceInfo *device = devices[deviceId];
        DeviceType type = device->type;

        emit closeSerialRequest(deviceId);

        device->thread->quit();
        device->thread->wait();
        delete device->serialWork;
        delete device->thread;
        delete device;

        devices.remove(deviceId);
        deviceTypeMap[type].removeOne(deviceId);

        if (activeDeviceId == deviceId && !devices.isEmpty()) {
            setActiveDevice(devices.keys().first());
        } else if (devices.isEmpty()) {
            activeDeviceId = -1;
        }

        return true;
    }
    return false;
}

bool DeviceManager::sendData(int deviceId, const QByteArray &data)
{
    if (!devices.contains(deviceId)) {
        emit deviceError(deviceId, "Device not found");
        return false;
    }

    DeviceInfo *device = devices[deviceId];

    if (!device->isConnected) {
        emit deviceError(deviceId, "Device not connected");
        return false;
    }

    // 直接发送，无需队列缓冲
    emit sendDataRequest(deviceId, data);

    // 更新设备活动时间
    device->lastActivity = QDateTime::currentDateTime();
    device->lastCommand = data;

    emit deviceActivity(deviceId);
    return true;
}

bool DeviceManager::sendCommandByType(DeviceType deviceType, const QByteArray &data)
{
    if (deviceTypeMap.contains(deviceType)) {
        bool success = false;
        QList<int> deviceIds = deviceTypeMap[deviceType];
        foreach (int deviceId, deviceIds) {
            if (sendData(deviceId, data)) {
                success = true;
            }
        }
        return success;
    }
    return false;
}

bool DeviceManager::broadcastCommand(const QByteArray &data)
{
    bool success = false;
    foreach (int deviceId, devices.keys()) {
        if (sendData(deviceId, data)) {
            success = true;
        }
    }
    return success;
}

QMap<int, QString> DeviceManager::getDeviceList()
{
    QMap<int, QString> deviceList;
    for (auto device : devices) {
        QString status = device->isConnected ? "Connected" : "Disconnected";
        deviceList[device->id] = QString("%1 (%2)").arg(device->name).arg(status);
    }
    return deviceList;
}

QString DeviceManager::getDeviceStatus(int deviceId)
{
    if (devices.contains(deviceId)) {
        return devices[deviceId]->status;
    }
    return "Unknown";
}

void DeviceManager::setActiveDevice(int deviceId)
{
    if (devices.contains(deviceId)) {
        activeDeviceId = deviceId;
        emit activeDeviceChanged(deviceId);
    }
}

int DeviceManager::getActiveDevice()
{
    return activeDeviceId;
}

bool DeviceManager::isDeviceConnected(int deviceId)
{
    if (devices.contains(deviceId)) {
        return devices[deviceId]->isConnected;
    }
    return false;
}

void DeviceManager::handleSerialOpened(int deviceId)
{
    if (devices.contains(deviceId)) {
        DeviceInfo *device = devices[deviceId];
        device->isConnected = true;
        device->status = "Connected";
        device->lastActivity = QDateTime::currentDateTime();

        qDebug() << "Device" << deviceId << "connected";
        emit deviceConnected(deviceId);
        emit deviceStatusChanged(deviceId, "Connected");
    }
}

void DeviceManager::handleSerialClosed(int deviceId)
{
    if (devices.contains(deviceId)) {
        DeviceInfo *device = devices[deviceId];
        device->isConnected = false;
        device->status = "Disconnected";

        qDebug() << "Device" << deviceId << "disconnected";
        emit deviceDisconnected(deviceId);
        emit deviceStatusChanged(deviceId, "Disconnected");
    }
}

void DeviceManager::handleDataAvailable()
{
    // 查找哪个设备有数据
    for (auto device : devices) {
        if (device->serialWork->hasData()) {
            QByteArray data = device->serialWork->readAllData();
            if (!data.isEmpty()) {
                device->lastActivity = QDateTime::currentDateTime();
                device->lastResponse = data;
                emit deviceDataReceived(device->id, data);
            }
        }
    }
}

void DeviceManager::handleSerialError(int deviceId, const QString &error)
{
    if (devices.contains(deviceId)) {
        DeviceInfo *device = devices[deviceId];
        device->status = QString("Error: %1").arg(error);
        device->isConnected = false;

        qDebug() << "Device" << deviceId << "error:" << error;
        emit deviceError(deviceId, error);
        emit deviceStatusChanged(deviceId, device->status);
    }
}
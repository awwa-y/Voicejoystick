#include "devicemanager.h"
#include "protocolframe.h"
#include "log.h"
#include <QDebug>

DeviceManager::DeviceManager(QObject *parent)
    : QObject(parent), nextDeviceId(1), activeDeviceId(-1)
{

}

// 析构：逐个停掉设备线程再删对象
// 注意顺序：thread->quit + wait 等线程退出后才能删 serialWork，
// 否则线程里还跑着它的槽函数就悬空了
DeviceManager::~DeviceManager()
{
    QMap<int, DeviceInfo*>::const_iterator it = devices.constBegin();
    while (it != devices.constEnd()) {
        DeviceInfo *device = it.value();
        if (device->serialWork) {
            device->thread->quit();
            device->thread->wait();
            delete device->serialWork;
            delete device->thread;
        }
        delete device->parser;
        delete device;
        ++it;
    }
}

// 添加设备：分配 ID、建 SerialWork 和专属线程、连好双向信号，注册进两张表
// 所有 connect 都必须在 moveToThread 之前做（跨线程连接建好后再搬，否则连不上）
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
    device->serialWork = new SerialWork(deviceId, nullptr);

    connect(this, &DeviceManager::openSerialRequest, device->serialWork, &SerialWork::openSerial);
    connect(this, &DeviceManager::closeSerialRequest, device->serialWork, &SerialWork::closeSerial);
    connect(this, &DeviceManager::sendDataRequest, device->serialWork, &SerialWork::sendData);

    connect(device->serialWork, &SerialWork::serialOpened, this, &DeviceManager::handleSerialOpened);
    connect(device->serialWork, &SerialWork::serialClosed, this, &DeviceManager::handleSerialClosed);
    connect(device->serialWork, &SerialWork::errorOccurred, this, &DeviceManager::handleSerialError);

    connect(device->serialWork, &SerialWork::newDataAvailable, this, &DeviceManager::handleDataAvailable, Qt::QueuedConnection);

        // 每设备一个解析器：多设备字节流混进同一个状态机会互相污染
        device->parser = new ProtocolFrame();
        connect(device->parser, &ProtocolFrame::frameReceived, this,
                [this, deviceId](quint8 cmd, quint8 dev, const QByteArray &data) {
                    emit deviceFrameReceived(deviceId, cmd, dev, data);
                });
        // 解析错误只记日志，不走 deviceError（那个会弹模态框，偶发校验失败会刷屏）
        connect(device->parser, &ProtocolFrame::errorReported, this, [deviceId](const QString &msg) {
            Logger::instance()->warning(QString("设备%1 协议解析错误: %2").arg(deviceId).arg(msg));
        });

        device->serialWork->moveToThread(device->thread);
    device->thread->start();

    devices[deviceId] = device;
    deviceTypeMap[type].append(deviceId);

    if (activeDeviceId == -1) {
        setActiveDevice(deviceId);
    }

    //emit openSerialRequest(portName, baudRate, deviceId);
    return deviceId;
}

// 移除设备：先发关串口请求（队列连接，让设备线程自己刷完缓冲），
// 等线程退出后删对象，最后清两张表
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
        delete device->parser;
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

// 给指定设备发数据。校验存在性和连接状态，通过信号转发到设备线程，
// 顺带记录 lastCommand 和活跃时间
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

    emit sendDataRequest(deviceId, data);

    device->lastActivity = QDateTime::currentDateTime();
    device->lastCommand = data;

    emit deviceActivity(deviceId);
    return true;
}

// 按类型群发：遍历 deviceTypeMap 里该类型的所有 ID 逐个发
// 只要有一台发成功就返回 true
bool DeviceManager::sendCommandByType(DeviceType deviceType, const QByteArray &data)
{
    if (deviceTypeMap.contains(deviceType)) {
        bool success = false;
        QList<int> deviceIds = deviceTypeMap[deviceType];
        for (QList<int>::const_iterator it = deviceIds.constBegin(); it != deviceIds.constEnd(); ++it) {
            if (sendData(*it, data)) {
                success = true;
            }
        }
        return success;
    }
    return false;
}

// 广播：给所有设备发
bool DeviceManager::broadcastCommand(const QByteArray &data)
{
    bool success = false;
    QMap<int, DeviceInfo*>::const_iterator it = devices.constBegin();
    while (it != devices.constEnd()) {
        if (sendData(it.key(), data)) {
            success = true;
        }
        ++it;
    }
    return success;
}

// 设备列表（给 UI 下拉框用）：ID -> "名字 (Connected/Disconnected)"
QMap<int, QString> DeviceManager::getDeviceList()
{
    QMap<int, QString> deviceList;
    QMap<int, DeviceInfo*>::const_iterator it = devices.constBegin();
    while (it != devices.constEnd()) {
        DeviceInfo *device = it.value();
        QString status = device->isConnected ? "Connected" : "Disconnected";
        deviceList[device->id] = QString("%1 (%2)").arg(device->name,status);
        ++it;
    }
    return deviceList;
}

// ===== 状态查询/设置 =====

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

void DeviceManager::setDeviceConnected(int deviceId, bool connected)
{
    if (devices.contains(deviceId)) {
        devices[deviceId]->isConnected = connected;
        devices[deviceId]->status = connected ? "Connected" : "Disconnected";
        emit deviceStatusChanged(deviceId, devices[deviceId]->status);
    }
}

bool DeviceManager::isDeviceConnected(int deviceId) const
{
    if (devices.contains(deviceId)) {
        return devices[deviceId]->isConnected;
    }
    return false;

}

// ===== 串口线程回调（在主线程执行，Qt::QueuedConnection 投递过来） =====

// 串口打开成功：置连接状态、刷新活跃时间、通知 UI
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

// 串口关闭：置断开状态并通知 UI
void DeviceManager::handleSerialClosed(int deviceId)
{
    if (devices.contains(deviceId)) {
        DeviceInfo *device = devices[deviceId];
        device->isConnected = false;
        device->status = "Disconnected";
        // 流结束了，丢掉攒了半截的帧，下次连接从干净状态开始
        device->parser->reset();

        qDebug() << "Device" << deviceId << "disconnected";
        emit deviceDisconnected(deviceId);
        emit deviceStatusChanged(deviceId, "Disconnected");
    }
}

// 接收缓冲有数据：从串口线程的 RingBuffer 取走，记录并上报给上层
// 取数据的动作发生在主线程，RingBuffer 的锁保证跨线程安全
void DeviceManager::handleDataAvailable(int deviceId)
{
    if (devices.contains(deviceId)) {
        DeviceInfo *device = devices[deviceId];
        if (device->serialWork->hasData()) {
            QByteArray data = device->serialWork->readAllData();
            if (!data.isEmpty()) {
                device->lastActivity = QDateTime::currentDateTime();
                device->lastResponse = data;
                emit deviceDataReceived(device->id, data);
                // 字节流过状态机，攒出完整帧会同步走 deviceFrameReceived
                device->parser->feed(data);
            }
        }
    }
}

// 串口错误：置错误状态并上报。设备拔出（ResourceError）最终也走这里
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

// ===== 简单参数查询（不存在时返回哨兵值） =====

DeviceType DeviceManager::getDeviceType(int deviceId) const
{
    if (devices.contains(deviceId)) {
        return devices[deviceId]->type;
    }
    return DEVICE_TYPE_UNKNOWN;
}

QString DeviceManager::getDevicePortName(int deviceId) const
{
    if (devices.contains(deviceId)) {
        return devices[deviceId]->portName;
    }
    return "";
}

int DeviceManager::getDeviceBaudRate(int deviceId) const
{
    if (devices.contains(deviceId)) {
        return devices[deviceId]->baudRate;
    }
    return 0;
}

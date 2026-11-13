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

    // 串口线程回调（队列连接，回调在主线程执行）
    connect(device->serialWork, &SerialWork::serialOpened, this, &DeviceManager::handleSerialOpened);
    connect(device->serialWork, &SerialWork::serialClosed, this, &DeviceManager::handleSerialClosed);
    connect(device->serialWork, &SerialWork::errorOccurred, this, &DeviceManager::handleSerialError);
    connect(device->serialWork, &SerialWork::reconnecting, this, &DeviceManager::handleSerialReconnecting);
    // 重插后系统可能给了新的 COM 号：跟着更新，否则 UI 一直显示已经不存在的端口
    connect(device->serialWork, &SerialWork::portNameChanged, this,
            [this](int deviceId, const QString &newName) {
                if (!devices.contains(deviceId)) return;
                const QString oldName = devices[deviceId]->portName;
                devices[deviceId]->portName = newName;
                Logger::instance()->warning(QString("设备%1 端口由 %2 变更为 %3")
                                                .arg(deviceId).arg(oldName, newName));
            });

    // 解析结果在串口线程就地产出，经信号跨线程投递到主线程
    connect(device->serialWork, &SerialWork::dataReceived, this,
            [this](int devId, const QByteArray &data) { emit deviceDataReceived(devId, data); },
            Qt::QueuedConnection);
    connect(device->serialWork, &SerialWork::frameReceived, this,
            [this](int devId, quint8 cmd, quint8 dev, const QByteArray &data) {
                emit deviceFrameReceived(devId, cmd, dev, data);
            },
            Qt::QueuedConnection);
    connect(device->serialWork, &SerialWork::parseError, this,
            [](int devId, const QString &msg) {
                Logger::instance()->warning(QString("设备%1 协议解析错误: %2").arg(devId).arg(msg));
            },
            Qt::QueuedConnection);

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

// 开启/关闭模拟模式（无硬件演示用）：只影响 open/close/sendData 三个入口
void DeviceManager::setSimulate(bool on)
{
    m_simulate = on;
}

// 把"打开串口"请求定向投递到指定设备的线程（替代原来对全体 SerialWork 的广播）
void DeviceManager::openSerialRequest(const QString &portName, int baudRate, int deviceId)
{
    if (!devices.contains(deviceId)) return;
    if (m_simulate) {                       // 模拟：跳过真实串口，直接置连接态
        setDeviceConnected(deviceId, true);
        emit deviceConnected(deviceId);
        return;
    }
    QMetaObject::invokeMethod(devices[deviceId]->serialWork, "openSerial", Qt::QueuedConnection,
                              Q_ARG(QString, portName), Q_ARG(int, baudRate), Q_ARG(int, deviceId));
}

// 把"关闭串口"请求定向投递到指定设备的线程（替代广播）
void DeviceManager::closeSerialRequest(int deviceId)
{
    if (!devices.contains(deviceId)) return;
    if (m_simulate) {                       // 模拟：直接置断开态
        setDeviceConnected(deviceId, false);
        emit deviceDisconnected(deviceId);
        return;
    }
    QMetaObject::invokeMethod(devices[deviceId]->serialWork, "closeSerial", Qt::QueuedConnection,
                              Q_ARG(int, deviceId));
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

    if (m_simulate) {                       // 模拟：把要发的帧打到日志，不碰真实串口
        QByteArray hexBytes = data.toHex(' ').toUpper();
        Logger::instance()->info(QString("[模拟] 设备%1 发送 %2 字节: %3")
                                     .arg(deviceId).arg(data.size())
                                     .arg(QString::fromLatin1(hexBytes.constData())));
        device->lastActivity = QDateTime::currentDateTime();
        device->lastCommand = data;
        emit deviceActivity(deviceId);
        return true;
    }

    QMetaObject::invokeMethod(device->serialWork, "sendData", Qt::QueuedConnection,
                              Q_ARG(int, deviceId), Q_ARG(QByteArray, data));

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
// 如果这台设备之前连上过，说明这次是断线恢复，额外发 deviceReconnected
void DeviceManager::handleSerialOpened(int deviceId)
{
    if (devices.contains(deviceId)) {
        DeviceInfo *device = devices[deviceId];
        const bool recovered = device->everConnected;

        device->everConnected = true;
        device->isConnected = true;
        device->reconnecting = false;
        device->retryCount = 0;
        device->status = "Connected";
        device->lastActivity = QDateTime::currentDateTime();

        qDebug() << "Device" << deviceId << (recovered ? "reconnected" : "connected");
        emit deviceConnected(deviceId);
        emit deviceStatusChanged(deviceId, "Connected");
        if (recovered) {
            emit deviceReconnected(deviceId);
        }
    }
}

// 串口关闭：置断开状态并通知 UI
void DeviceManager::handleSerialClosed(int deviceId)
{
    if (devices.contains(deviceId)) {
        DeviceInfo *device = devices[deviceId];
        device->isConnected = false;
        device->status = "Disconnected";
        // 先按"已断开"收尾；如果这次断开要自动重连，紧接着的
        // handleSerialReconnecting 会把状态和标志重新置成"重连中"
        device->reconnecting = false;
        device->retryCount = 0;
        // 解析器在 SerialWork 线程里，closeSerial 时已自行 reset

        qDebug() << "Device" << deviceId << "disconnected";
        emit deviceDisconnected(deviceId);
        emit deviceStatusChanged(deviceId, "Disconnected");
    }
}

// 断线且已排定自动重连：状态改成"重连中"，并透传给 UI
// reconnecting 标志同时是一个闸门：重连期间的串口错误不再把状态改写回 Error，
// 也不会让 UI 弹模态框
void DeviceManager::handleSerialReconnecting(int deviceId, int attempt, int delayMs)
{
    if (devices.contains(deviceId)) {
        DeviceInfo *device = devices[deviceId];
        device->reconnecting = true;
        device->retryCount = attempt;
        device->status = QString("Reconnecting(第%1次, %2ms)").arg(attempt).arg(delayMs);

        qDebug() << "Device" << deviceId << "reconnecting attempt" << attempt << "in" << delayMs << "ms";
        emit deviceReconnecting(deviceId, attempt, delayMs);
        emit deviceStatusChanged(deviceId, device->status);
    }
}

// 串口错误：置错误状态并上报。设备拔出（ResourceError）最终也走这里
// 重连过程中收到的错误只上报，不改状态——状态由 handleSerialReconnecting 维护
void DeviceManager::handleSerialError(int deviceId, const QString &error)
{
    if (devices.contains(deviceId)) {
        DeviceInfo *device = devices[deviceId];

        qDebug() << "Device" << deviceId << "error:" << error;

        if (!device->reconnecting) {
            device->status = QString("Error: %1").arg(error);
            device->isConnected = false;
        }

        emit deviceError(deviceId, error);
        if (!device->reconnecting) {
            emit deviceStatusChanged(deviceId, device->status);
        }
    }
}

// 开关自动重连：转发到设备线程（SerialWork 住在那里）
void DeviceManager::setAutoReconnect(int deviceId, bool on)
{
    if (!devices.contains(deviceId)) return;
    QMetaObject::invokeMethod(devices[deviceId]->serialWork, "setAutoReconnect", Qt::QueuedConnection,
                              Q_ARG(bool, on));
}

// 是否正处在自动重连中
bool DeviceManager::isReconnecting(int deviceId) const
{
    return devices.contains(deviceId) && devices[deviceId]->reconnecting;
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

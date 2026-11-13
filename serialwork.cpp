#include "serialwork.h"
#include "protocolframe.h"
#include <QSerialPortInfo>   // VID/PID 找回漂移的端口

// 构造：建串口对象、收发双缓冲、刷写定时器，连好三条内部信号链
// 注意此时串口没打开、定时器没启动，都等 openSerial 触发
SerialWork::SerialWork(int deviceId,QObject *parent)
    :QObject(parent), m_serial(new QSerialPort(this)),m_parser(new ProtocolFrame(this))
    , m_sendBuffer(new RingBuffer(4096, this))
{
    m_deviceId = deviceId;   // 必须初始化，否则是垃圾值，后面按ID过滤会失效

    // 解析器与串口同住设备线程：字节一到就地解析，结果经信号跨线程上抛，
    // 避免上层线程跨线程读缓冲、也避免字节流被多线程共享。
    connect(m_parser, &ProtocolFrame::frameReceived, this,
            [this](quint8 cmd, quint8 dev, const QByteArray &data) {
                emit frameReceived(m_deviceId, cmd, dev, data);
            });
    connect(m_parser, &ProtocolFrame::errorReported, this,
            [this](const QString &msg) { emit parseError(m_deviceId, msg); });

    m_flushTimer = new QTimer(this);
    m_flushTimer->setInterval(10);   // 10ms 刷写周期，发送延迟可控
    connect(m_serial, &QSerialPort::readyRead, this, &SerialWork::handleReadyRead);
    connect(m_serial, &QSerialPort::errorOccurred, this, &SerialWork::handleError);
    connect(m_flushTimer, &QTimer::timeout, this, &SerialWork::flushSendBuffer);

    // 重连定时器：单次触发，到点调 retryOpen 再试一次
    m_retryTimer = new QTimer(this);
    m_retryTimer->setSingleShot(true);
    connect(m_retryTimer, &QTimer::timeout, this, &SerialWork::retryOpen);
}

// 析构：停定时器，串口和缓冲由 Qt 父子对象树释放
SerialWork::~SerialWork()
{
    m_flushTimer->stop();
    m_retryTimer->stop();
}

// 用记住的参数真正打开指定串口（openSerial / retryOpen 共用）
// 8-N-1 无流控，和 STM32 端的约定一致
bool SerialWork::openPort(const QString &portName)
{
    m_serial->setPortName(portName);
    m_serial->setBaudRate(m_baudRate);
    m_serial->setDataBits(QSerialPort::Data8);
    m_serial->setParity(QSerialPort::NoParity);
    m_serial->setStopBits(QSerialPort::OneStop);
    m_serial->setFlowControl(QSerialPort::NoFlowControl);

    if (!m_serial->open(QIODevice::ReadWrite)) {
        return false;
    }

    m_everOpened = true;

    // 记住这个口的 USB 身份，重插后端口号漂移时靠它找回来
    const QSerialPortInfo info(portName);
    if (info.hasVendorIdentifier() && info.hasProductIdentifier()) {
        m_hasVidPid = true;
        m_vid = info.vendorIdentifier();
        m_pid = info.productIdentifier();
        m_serialNumber = info.serialNumber();
    }

    if (portName != m_portName) {
        // 换了口号（或者首次打开）：更新记忆并通知上层刷新显示
        m_portName = portName;
        emit portNameChanged(m_deviceId, m_portName);
    }

    stopReconnect();      // 连上了，退避序列归零
    m_parser->reset();    // 新连接从干净状态开始，不吃上一次留下的半截帧
    dropPendingTx();      // 断线期间积压的帧作废，不在新连接上补发旧指令
    m_flushTimer->start();
    return true;
}

// USB 转串口重插到不同 USB 口后，系统可能给出新的 COM 号。
// 原端口不存在时，按 VID/PID（再优先匹配序列号）在现有端口里找同一块板子。
// 只有唯一匹配才认，避免把两台同型号设备串了。
QString SerialWork::findPortByVidPid() const
{
    if (!m_hasVidPid) return QString();

    QStringList candidates;
    for (const QSerialPortInfo &info : QSerialPortInfo::availablePorts()) {
        if (!info.hasVendorIdentifier() || !info.hasProductIdentifier()) continue;
        if (info.vendorIdentifier() != m_vid || info.productIdentifier() != m_pid) continue;

        // 有序列号就按序列号精确匹配，避免同型号多设备混淆
        if (!m_serialNumber.isEmpty() && !info.serialNumber().isEmpty()
            && info.serialNumber() != m_serialNumber) {
            continue;
        }
        // 原端口自己还在（只是打开失败），不用改名
        if (info.portName() == m_portName) continue;
        candidates << info.portName();
    }

    if (candidates.size() == 1) return candidates.first();
    if (candidates.size() > 1) {
        qDebug() << "设备" << m_deviceId << "有多个 VID/PID 相同的候选端口，保持原名重试:" << candidates;
    }
    return QString();
}

// 打开串口（在本设备自己的线程里执行）
void SerialWork::openSerial(const QString &portName, int baudRate,int deviceId)
{
    // 信号是广播连接，每个设备的槽都会收到，只响应自己的ID
    if (deviceId != m_deviceId) return;

    m_portName = portName;
    m_baudRate = baudRate;
    m_wantOpen = true;        // 用户点了连接：意图是"保持连着"
    stopReconnect();          // 手动连接优先，清掉还在排队的旧重连

    if (m_serial->isOpen()) {
        m_serial->close();
    }

    if (openPort(m_portName)) {
        emit serialOpened(m_deviceId);
    }
    // 打开失败的错误会经 QSerialPort::errorOccurred 走到 handleError，
    // 那里按"是否曾经连过"决定要不要自动重连
}

// 关闭串口：先刷完发送缓冲再关，排队中的最后几帧不丢
// 这是"用户主动断开"，所以要把重连意图一起关掉
void SerialWork::closeSerial(int deviceId)
{
    if (deviceId != m_deviceId) return;

    m_wantOpen = false;
    stopReconnect();

    if (m_serial->isOpen()) {
        m_flushTimer->stop();

        if (m_sendBuffer->hasData()) {
            flushSendBuffer();
        }

        if (m_serial->isOpen()) {
            m_serial->close();
            m_parser->reset();   // 丢掉攒了半截的帧，下次连接从干净状态开始
            emit serialClosed(deviceId);
        }
    }
    else{
        qDebug()<<"串口已经关闭";
    }
}

// 异步发送：只写进发送 RingBuffer 就返回
// 真正写串口由 10ms 定时器调 flushSendBuffer，突发流量被定时器平滑，
// 调用方（UI 线程）不会被阻塞
void SerialWork::sendData(int deviceId,const QByteArray &data)
{
    // 只处理发给本设备的数据，其他设备的请求直接忽略
    if (deviceId != m_deviceId) return;

    if (!m_serial->isOpen()) {
        // 正在自动重连：摇杆/定时器还会持续产生帧，这些帧对新连接没有意义，
        // 静默丢掉，否则每 40ms 一条错误会把日志冲垮
        if (m_wantOpen) {
            return;
        }
        emit errorOccurred(deviceId,tr("串口未打开，无法发送数据"));
        return;
    }

    qDebug() << "SerialWork发送数据 - 设备ID:" << deviceId
             << ", 数据长度:" << data.size()
             << ", 数据:" << data.toHex(' ').toUpper();

     m_sendBuffer->write(data);
}

// 定时刷写：把发送缓冲积压的数据一次写到串口
// 写入字节数和期望不一致（驱动缓冲满）时上报错误，不静默丢数据
void SerialWork::flushSendBuffer()
{
    if (!m_serial->isOpen()) {
        return;
    }

    if (m_sendBuffer->hasData()) {
        QByteArray data = m_sendBuffer->readAll();
        qint64 written = m_serial->write(data);
        if (written != data.size()) {
            emit errorOccurred(m_deviceId,
                               tr("发送数据不完整: 期望 %1 字节，实际写入 %2 字节").arg(data.size()).arg(written));
        }
    }
}

// readyRead 回调：readAll 取走这次到达的字节，就地喂给本线程的解析器。
// 原始字节先 emit 给上层记日志；解析器攒出整帧会同步 emit frameReceived。
void SerialWork::handleReadyRead()
{
    QByteArray data = m_serial->readAll();
    if (data.isEmpty()) return;

    emit dataReceived(m_deviceId, data);
    m_parser->feed(data);
}

// ===== 断线重连 =====

// 串口错误回调：按"是否已打开"分成两类
//   A. 还没打开就报错（端口不存在/被占用）：首开失败只上报；连过之后再失败才重连
//   B. 已打开后报错：ResourceError 就是 USB 被拔掉，走重连
void SerialWork::handleError(QSerialPort::SerialPortError error)
{
    if (error == QSerialPort::NoError) return;   // 无错误不处理，避免刷屏

    const QString msg = m_serial->errorString();
    qDebug() << "设备" << m_deviceId << "串口错误:" << int(error) << msg;

    if (!m_serial->isOpen()) {
        const bool openFailed = (error == QSerialPort::ResourceError
                                 || error == QSerialPort::DeviceNotFoundError
                                 || error == QSerialPort::PermissionError);
        if (!openFailed) return;

        emit errorOccurred(m_deviceId, msg);
        scheduleReconnect(msg);   // 只有曾经连上过才会真的排重连（见 scheduleReconnect）
        return;
    }

    if (error == QSerialPort::ResourceError) {
        // 设备拔出：停刷写、关串口、丢掉半截帧和积压帧，然后排重连
        m_flushTimer->stop();
        m_serial->close();
        m_parser->reset();
        dropPendingTx();

        emit serialClosed(m_deviceId);
        // 先发 reconnecting 再发 errorOccurred：上层收到重连信号后才知道
        // "这次错误是断线重连的一部分"，不会弹模态框
        scheduleReconnect(msg);
        emit errorOccurred(m_deviceId, msg);
        return;
    }

    // 读写瞬时错误（ReadError/WriteError 等）：只上报，不动连接
    emit errorOccurred(m_deviceId, msg);
}

// 排定下一次重连：退避递增，已经排着就不要再排
// 注意 m_everOpened 这道闸：首开失败（端口选错/被占用）不自动重试，
// 否则用户点错一个 COM 口就会一直转下去，日志和 UI 全是噪音
void SerialWork::scheduleReconnect(const QString &reason)
{
    if (!m_autoReconnect || !m_wantOpen || !m_everOpened) return;
    if (m_retryTimer->isActive()) return;

    const int delay = m_policy.nextDelayMs();
    m_retryCount = m_policy.attempts();
    m_retryTimer->start(delay);

    qDebug() << "设备" << m_deviceId << "断线，第" << m_retryCount
             << "次重连将在" << delay << "ms 后开始，原因:" << reason;
    emit reconnecting(m_deviceId, m_retryCount, delay);
}

// 停掉重连并复位退避（用户手动断开、手动重连、或重连成功时调用）
void SerialWork::stopReconnect()
{
    if (m_retryTimer) m_retryTimer->stop();
    m_policy.reset();
    m_retryCount = 0;
}

// 丢弃断线期间积压的发送帧：那些帧是对旧连接的指令，补发会造成"幽灵动作"
void SerialWork::dropPendingTx()
{
    if (m_sendBuffer->hasData()) {
        const int dropped = m_sendBuffer->size();
        m_sendBuffer->clear();
        qDebug() << "设备" << m_deviceId << "丢弃断线期间积压的" << dropped << "字节";
    }
}

// 重连定时器到点：再开一次
void SerialWork::retryOpen()
{
    if (m_serial->isOpen()) return;
    if (!m_wantOpen || !m_autoReconnect) return;

    // 原端口不在了（典型是重插换了 USB 口）：先按 VID/PID 找同一块板子的新端口
    QString target = m_portName;
    if (QSerialPortInfo(m_portName).isNull()) {
        const QString found = findPortByVidPid();
        if (!found.isEmpty()) {
            qDebug() << "设备" << m_deviceId << "端口" << m_portName
                     << "已不存在，按 VID/PID 找到" << found << "，改用它重连";
            target = found;
        }
    }

    if (openPort(target)) {
        qDebug() << "设备" << m_deviceId << "重连成功:" << target;
        emit serialOpened(m_deviceId);
        return;
    }

    // 还是没开起来。QSerialPort 通常已经同步发过 errorOccurred（走 handleError 排下一次），
    // 这里再兜一次，保证退避序列不会因为驱动没发错误而卡死
    scheduleReconnect(m_serial->errorString());
}

// 自动重连开关：关掉时立刻停掉正在等待的重连
void SerialWork::setAutoReconnect(bool on)
{
    m_autoReconnect = on;
    if (!on) {
        stopReconnect();
        return;
    }
    // 打开开关时如果正断着，立刻接着重试，不用等用户再点一次连接
    if (m_wantOpen && m_everOpened && !m_serial->isOpen()) {
        scheduleReconnect(tr("手动开启自动重连"));
    }
}

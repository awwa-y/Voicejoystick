#include "serialwork.h"

// 构造：建串口对象、收发双缓冲、刷写定时器，连好三条内部信号链
// 注意此时串口没打开、定时器没启动，都等 openSerial 触发
SerialWork::SerialWork(int deviceId,QObject *parent)
    :QObject(parent), m_serial(new QSerialPort(this)),m_recieveBuffer(new RingBuffer(4096, this))
    , m_sendBuffer(new RingBuffer(4096, this))
{
    m_deviceId = deviceId;   // 必须初始化，否则是垃圾值，后面按ID过滤会失效
    m_flushTimer = new QTimer(this);
    m_flushTimer->setInterval(10);   // 10ms 刷写周期，发送延迟可控
    connect(m_serial, &QSerialPort::readyRead, this, &SerialWork::handleReadyRead);
    connect(m_serial, &QSerialPort::errorOccurred, this, &SerialWork::handleError);
    connect(m_flushTimer, &QTimer::timeout, this, &SerialWork::flushSendBuffer);

}

// 析构：停定时器，串口和缓冲由 Qt 父子对象树释放
SerialWork::~SerialWork()
{
    m_flushTimer->stop();
}

bool SerialWork::hasData()
{
    return m_recieveBuffer->hasData();
}

QByteArray SerialWork::readAllData()
{
    return m_recieveBuffer->readAll();
}

// 打开串口（在本设备自己的线程里执行）
// 8-N-1 无流控，和 STM32 端的约定一致
void SerialWork::openSerial(const QString &portName, int baudRate,int deviceId)
{
    // 信号是广播连接，每个设备的槽都会收到，只响应自己的ID
    if (deviceId != m_deviceId) return;

    m_serial->setPortName(portName);
    m_serial->setBaudRate(baudRate);
    m_serial->setDataBits(QSerialPort::Data8);
    m_serial->setParity(QSerialPort::NoParity);
    m_serial->setStopBits(QSerialPort::OneStop);
    m_serial->setFlowControl(QSerialPort::NoFlowControl);
    if (m_serial->open(QIODevice::ReadWrite)) {
        m_flushTimer->start();
        emit serialOpened(deviceId);
    } else {
        emit errorOccurred(deviceId, m_serial->errorString());
    }

}

// 关闭串口：先刷完发送缓冲再关，排队中的最后几帧不丢
void SerialWork::closeSerial(int deviceId)
{
    if (deviceId != m_deviceId) return;

    if (m_serial->isOpen()) {
        m_flushTimer->stop();

        if (m_sendBuffer->hasData()) {
            flushSendBuffer();
        }

        if (m_serial->isOpen()) {
            m_serial->close();
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

// readyRead 回调：readAll 全部搬进接收 RingBuffer
// 只发 newDataAvailable 通知（不带数据），谁需要谁来 readAllData 取，
// 省一次跨线程数据拷贝
void SerialWork::handleReadyRead()
{
    QByteArray data = m_serial->readAll();

    // 写入循环队列
    if (!data.isEmpty()) {
        m_recieveBuffer->write(data);
        emit newDataAvailable(m_deviceId);
    }
}

// 串口错误回调。TODO：按错误类型分类上报，
// 典型的 ResourceError 是 USB 设备被拔出，上层应据此走设备下线流程
void SerialWork::handleError(QSerialPort::SerialPortError error)
{

}

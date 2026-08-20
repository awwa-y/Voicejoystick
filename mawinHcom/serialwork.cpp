#include "serialwork.h"

SerialWork::SerialWork(int deviceId,QObject *parent)
    :QObject(parent), m_serial(new QSerialPort(this)),m_recieveBuffer(new RingBuffer(4096, this))
    , m_sendBuffer(new RingBuffer(4096, this))
{
    m_flushTimer = new QTimer(this);
    m_flushTimer->setInterval(10);
    connect(m_serial, &QSerialPort::readyRead, this, &SerialWork::handleReadyRead);
    connect(m_serial, &QSerialPort::errorOccurred, this, &SerialWork::handleError);
    connect(m_flushTimer, &QTimer::timeout, this, &SerialWork::flushSendBuffer);

}

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
void SerialWork::openSerial(const QString &portName, int baudRate,int deviceId)
{
    m_serial->setPortName(portName);
    m_serial->setBaudRate(baudRate);
    m_serial->setDataBits(QSerialPort::Data8);
    m_serial->setParity(QSerialPort::NoParity);
    m_serial->setStopBits(QSerialPort::OneStop);
    m_serial->setFlowControl(QSerialPort::NoFlowControl);
    if (m_serial->open(QIODevice::ReadWrite)) {
        m_flushTimer->start();  //启动定时器
        emit serialOpened(deviceId);
    } else {
        emit errorOccurred(deviceId, m_serial->errorString());
    }

}

void SerialWork::closeSerial(int deviceId)
{
    if (m_serial->isOpen()) {
        m_flushTimer->stop();

        if (m_sendBuffer->hasData()) {
            flushSendBuffer();  // 发送剩余数据
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

void SerialWork::sendData(int deviceId,const QByteArray &data)
{
    if (!m_serial->isOpen()) {
        emit errorOccurred(deviceId,tr("串口未打开，无法发送数据"));
        return;
    }

    qDebug() << "SerialWork发送数据 - 设备ID:" << deviceId 
             << ", 数据长度:" << data.size() 
             << ", 数据:" << data.toHex(' ').toUpper();

     m_sendBuffer->write(data);
}
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
void SerialWork::handleReadyRead()
{
    QByteArray data = m_serial->readAll();

    // 写入循环队列
    if (!data.isEmpty()) {
        m_recieveBuffer->write(data);
        // 仅发送通知信号，无数据拷贝
        emit newDataAvailable(m_deviceId);
    }
}

void SerialWork::handleError(QSerialPort::SerialPortError error)
{

}

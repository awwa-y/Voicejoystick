#include "serialwork.h"

SerialWork::SerialWork(int deviceId,QObject *parent)
    :QObject(parent), m_serial(new QSerialPort(this)){
    connect(m_serial, &QSerialPort::readyRead, this, &SerialWork::handleReadyRead);
    connect(m_serial, &QSerialPort::errorOccurred, this, &SerialWork::handleError);


}

SerialWork::~SerialWork()
{

}
bool SerialWork::hasData()
{
    return ringBuffer->hasData();
}

QByteArray SerialWork::readAllData()
{
    return ringBuffer->readAll();
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
        emit serialOpened(deviceId);
    } else {
        emit errorOccurred(deviceId, m_serial->errorString());
    }

}

void SerialWork::closeSerial(int deviceId)
{
    if (m_serial->isOpen()) {
        m_serial->close();
        emit serialClosed(deviceId);
        qDebug() << "Serial closed";
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

    qint64 written = m_serial->write(data);
    if (written != data.size()) {
        emit errorOccurred(deviceId,tr("发送数据不完整: 期望 %1 字节，实际写入 %2 字节").arg(data.size()).arg(written));
    }
}

void SerialWork::handleReadyRead()
{
    QByteArray data = m_serial->readAll();

    // 写入循环队列
    if (!data.isEmpty()) {
        ringBuffer->write(data);
        // 仅发送通知信号，无数据拷贝
        emit newDataAvailable();
    }
}

void SerialWork::handleError(QSerialPort::SerialPortError error)
{

}

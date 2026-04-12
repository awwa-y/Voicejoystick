#include "serialwork.h"

SerialWork::SerialWork(QObject *parent)
    :QObject(parent), m_serial(new QSerialPort(this)){
    connect(m_serial, &QSerialPort::readyRead, this, &SerialWork::handleReadyRead);
    connect(m_serial, &QSerialPort::errorOccurred, this, &SerialWork::handleError);


}

SerialWork::~SerialWork()
{

}

void SerialWork::openSerial(const QString &portName, int baudRate)
{
    m_serial->setPortName(portName);
    m_serial->setBaudRate(baudRate);
    m_serial->setDataBits(QSerialPort::Data8);
    m_serial->setParity(QSerialPort::NoParity);
    m_serial->setStopBits(QSerialPort::OneStop);
    m_serial->setFlowControl(QSerialPort::NoFlowControl);
    if (m_serial->open(QIODevice::ReadWrite)) {
        emit serialOpened();  // 通知主线程打开成功
        qDebug()<<"Serial opened:" << portName;
    } else {
        emit errorOccurred(tr("Failed to open serial port: %1").arg(m_serial->errorString()));
    }

}

void SerialWork::closeSerial()
{
    if (m_serial->isOpen()) {
        m_serial->close();
        emit serialClosed();
        qDebug() << "Serial closed";
    }
    else{
        qDebug()<<"串口已经关闭";
    }
}

void SerialWork::sendData(const QByteArray &data)
{
    if (!m_serial->isOpen()) {
        emit errorOccurred(tr("串口未打开，无法发送数据"));
        return;
    }

    qint64 written = m_serial->write(data);
    if (written != data.size()) {
        emit errorOccurred(tr("发送数据不完整: 期望 %1 字节，实际写入 %2 字节").arg(data.size()).arg(written));
    }
}

void SerialWork::handleReadyRead()
{

}

void SerialWork::handleError(QSerialPort::SerialPortError error)
{

}

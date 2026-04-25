#ifndef SERIALWORKER_H
#define SERIALWORKER_H

#include <QObject>
#include <QSerialPort>
#include <QDebug>
#include "ringbuffer.h"
class SerialWork : public QObject
{
    Q_OBJECT
public:
    explicit SerialWork(int deviceId, QObject *parent = nullptr);
    ~SerialWork();
    bool hasData();
    QByteArray readAllData();

public slots:
    void openSerial(const QString &portName, int baudRate, int deviceId);
    void closeSerial(int deviceId);
    void sendData(int deviceId, const QByteArray &data);


signals:
    void dataReceived(int deviceId, const QByteArray &data);
    void errorOccurred(int deviceId, const QString &error);
    void serialOpened(int deviceId);
    void serialClosed(int deviceId);
    void newDataAvailable(int deviceId);

private slots:
    void handleReadyRead();
    void handleError(QSerialPort::SerialPortError error);

public:
    QSerialPort *m_serial;
private:
    RingBuffer *ringBuffer;
    int m_deviceId;
};



#endif // SERIALWORKER_H

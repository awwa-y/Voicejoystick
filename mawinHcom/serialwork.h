#ifndef SERIALWORKER_H
#define SERIALWORKER_H

#include <QObject>
#include <QSerialPort>
#include <QDebug>
#include "ringbuffer.h"
#include <QTimer>
class SerialWork : public QObject
{
    Q_OBJECT
public:
    explicit SerialWork(int deviceId, QObject *parent = nullptr);
    ~SerialWork();
    bool hasData();
    void sendData(int deviceId, const QByteArray &data);
    void flushSendBuffer();
    QByteArray readAllData();

public slots:
    void openSerial(const QString &portName, int baudRate, int deviceId);
    void closeSerial(int deviceId);

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
    RingBuffer *m_recieveBuffer;
    RingBuffer *m_sendBuffer;
    int m_deviceId;
    QTimer *m_flushTimer;
};



#endif // SERIALWORKER_H

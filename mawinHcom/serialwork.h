#ifndef SERIALWORKER_H
#define SERIALWORKER_H

#include <QObject>
#include <QSerialPort>
#include <QDebug>
class SerialWork : public QObject
{
    Q_OBJECT
public:
    explicit SerialWork(QObject *parent = nullptr);
    ~SerialWork();

public slots:
    void openSerial(const QString &portName, int baudRate);
    void closeSerial();
    void sendData(const QByteArray &data);

signals:
    void dataReceived(const QByteArray &data);
    void errorOccurred(const QString &error);
    void serialOpened();
    void serialClosed();

private slots:
    void handleReadyRead();
    void handleError(QSerialPort::SerialPortError error);

public:
    QSerialPort *m_serial;
};



#endif // SERIALWORKER_H

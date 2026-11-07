#ifndef SERIALWORKER_H
#define SERIALWORKER_H

#include <QObject>
#include <QSerialPort>
#include <QDebug>
#include "ringbuffer.h"
#include <QTimer>

class ProtocolFrame;

// 单设备的串口工作者，一台设备一个实例
// 线程模型：由 DeviceManager 创建后 moveToThread 到独立 QThread，
// 设备之间互不影响，串口 IO 也不阻塞 UI 线程
//
// 收发路径：
//   收：readyRead -> handleReadyRead -> 就地喂本线程的 ProtocolFrame 解析器
//       -> 解析器出整帧 -> 本类 emit frameReceived（跨线程队列投递给上层）
//   发：sendData 只写发送 RingBuffer（立即返回），
//       10ms 定时器 flushSendBuffer 把缓冲真正写到串口
class SerialWork : public QObject
{
    Q_OBJECT
public:
    explicit SerialWork(int deviceId, QObject *parent = nullptr);
    ~SerialWork();

    void flushSendBuffer();                               // 把发送缓冲刷到串口

public slots:
    // 打开串口：115200-8-N-1 无流控，成功启动刷写定时器
    void openSerial(const QString &portName, int baudRate, int deviceId);
    // 关闭串口：先把发送缓冲刷完再关，避免尾包丢失
    void closeSerial(int deviceId);
    // 异步发送：只写发送缓冲立即返回，实际写串口由 10ms 定时器刷
    void sendData(int deviceId, const QByteArray &data);

signals:
    void dataReceived(int deviceId, const QByteArray &data);   // 收到原始字节（用于日志）
    void frameReceived(int deviceId, quint8 cmdType, quint8 deviceType, const QByteArray &data);  // 解析出的完整帧
    void parseError(int deviceId, const QString &msg);         // 协议解析错误
    void errorOccurred(int deviceId, const QString &error);   // 打开失败/发送不完整等
    void serialOpened(int deviceId);                           // 串口已打开
    void serialClosed(int deviceId);                           // 串口已关闭

private slots:
    void handleReadyRead();                                // readyRead 回调，字节进接收缓冲
    void handleError(QSerialPort::SerialPortError error);  // 串口错误回调（如设备拔出）

public:
    QSerialPort *m_serial;        // 串口对象（public 是历史遗留，外面别直接动）
private:
    ProtocolFrame *m_parser;      // 协议解析器（住在串口线程，收到字节就地解析）
    RingBuffer *m_sendBuffer;     // 发送环形缓冲，sendData 写、定时器读
    int m_deviceId;               // 所属设备 ID，信号回传时标识来源
    QTimer *m_flushTimer;         // 发送刷写定时器，10ms 周期
};

#endif // SERIALWORKER_H

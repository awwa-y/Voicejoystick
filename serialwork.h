#ifndef SERIALWORKER_H
#define SERIALWORKER_H

#include <QObject>
#include <QSerialPort>
#include <QDebug>
#include "ringbuffer.h"
#include "reconnectpolicy.h"
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
//
// 断线重连：USB 串口被拔掉时 QSerialPort 报 ResourceError，此时关口、
// 丢掉半截帧和积压的发送帧，然后按 ReconnectPolicy 的退避序列自动重试，
// 重开成功后重新 emit serialOpened，上层据此恢复设备和 UI 状态。
class SerialWork : public QObject
{
    Q_OBJECT
public:
    explicit SerialWork(int deviceId, QObject *parent = nullptr);
    ~SerialWork();

    void flushSendBuffer();                               // 把发送缓冲刷到串口

    // 自动重连开关（默认开）。关掉后拔线只掉连接，不再重试
    bool autoReconnect() const { return m_autoReconnect; }
    int reconnectAttempts() const { return m_retryCount; }

public slots:
    // 打开串口：115200-8-N-1 无流控，成功启动刷写定时器
    void openSerial(const QString &portName, int baudRate, int deviceId);
    // 关闭串口：先把发送缓冲刷完再关，避免尾包丢失
    void closeSerial(int deviceId);
    // 异步发送：只写发送缓冲立即返回，实际写串口由 10ms 定时器刷
    void sendData(int deviceId, const QByteArray &data);
    // 重连定时器到点：用上次的端口参数再开一次
    void retryOpen();
    // 开关自动重连（DeviceManager 经队列连接调用，切线程安全）
    void setAutoReconnect(bool on);

signals:
    void dataReceived(int deviceId, const QByteArray &data);   // 收到原始字节（用于日志）
    void frameReceived(int deviceId, quint8 cmdType, quint8 deviceType, const QByteArray &data);  // 解析出的完整帧
    void parseError(int deviceId, const QString &msg);         // 协议解析错误
    void errorOccurred(int deviceId, const QString &error);   // 打开失败/发送不完整等
    void serialOpened(int deviceId);                           // 串口已打开（首次或重连成功）
    void serialClosed(int deviceId);                           // 串口已关闭
    // 已排定第 attempt 次重连，delayMs 后执行。上层据此显示"重连中"
    void reconnecting(int deviceId, int attempt, int delayMs);
    // 重插后系统重新分配的端口名和原来不同（COM3 -> COM5），上层要更新显示
    void portNameChanged(int deviceId, const QString &portName);

private slots:
    void handleReadyRead();                                // readyRead 回调，字节进接收缓冲
    void handleError(QSerialPort::SerialPortError error);  // 串口错误回调（如设备拔出）

private:
    bool openPort(const QString &portName);                // 用记住的参数真正打开指定串口
    QString findPortByVidPid() const;                      // USB 转串口重插后按 VID/PID 找回漂移的端口
    void scheduleReconnect(const QString &reason);          // 排定下一次重连（去重 + 退避）
    void stopReconnect();                                  // 停掉重连并复位退避
    void dropPendingTx();                                  // 丢弃断线期间积压的发送帧

public:
    QSerialPort *m_serial;        // 串口对象（public 是历史遗留，外面别直接动）
private:
    ProtocolFrame *m_parser;      // 协议解析器（住在串口线程，收到字节就地解析）
    RingBuffer *m_sendBuffer;     // 发送环形缓冲，sendData 写、定时器读
    int m_deviceId;               // 所属设备 ID，信号回传时标识来源
    QTimer *m_flushTimer;         // 发送刷写定时器，10ms 周期

    // ---- 重连相关 ----
    QTimer *m_retryTimer;         // 单次触发的重连定时器
    ReconnectPolicy m_policy;     // 退避序列
    QString m_portName;           // 上次成功的端口名，重连时复用
    int m_baudRate = 115200;
    // 记住设备的 USB 身份：重插后端口号可能漂移，靠 VID/PID 找回
    bool m_hasVidPid = false;
    quint16 m_vid = 0;
    quint16 m_pid = 0;
    QString m_serialNumber;
    bool m_wantOpen = false;      // 用户意图：希望保持连接（显式断开后为 false）
    bool m_autoReconnect = true;  // 是否允许自动重连
    bool m_everOpened = false;    // 曾经成功打开过——只有连过才自动重连，避免错误端口无限重试
    int m_retryCount = 0;         // 已排定的重连次数（用于 UI 显示第几次）
};

#endif // SERIALWORKER_H
